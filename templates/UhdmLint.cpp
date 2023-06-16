// -*- c++ -*-

/*

 Copyright 2019-2022 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   UhdmLint.cpp
 * Author: alaindargelas
 *
 * Created on Jan 3, 2022, 9:03 PM
 */
#include <uhdm/ExprEval.h>
#include <uhdm/UhdmLint.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

#include <cstring>
#include <regex>

namespace UHDM {

void UhdmLint::leaveBit_select(const bit_select* object, vpiHandle handle) {
  if (const ref_obj* index = object->VpiIndex<ref_obj>()) {
    if (const real_var* actual = index->Actual_group<real_var>()) {
      const std::string errMsg(actual->VpiName());
      serializer_->GetErrorHandler()(ErrorType::UHDM_REAL_TYPE_AS_SELECT,
                                     errMsg, index, nullptr);
    }
  }
}

static const any* returnWithValue(const any* stmt) {
  switch (stmt->UhdmType()) {
    case uhdmreturn_stmt: {
      return_stmt* ret = (return_stmt*)stmt;
      if (const any* r = ret->VpiCondition()) return r;
      break;
    }
    case uhdmbegin: {
      begin* st = (begin*)stmt;
      for (auto s : *st->Stmts()) {
        if (const any* r = returnWithValue(s)) return r;
      }
      break;
    }
    case uhdmif_stmt: {
      if_stmt* st = (if_stmt*)stmt;
      if (const any* r = returnWithValue(st->VpiStmt())) return r;
      break;
    }
    case uhdmif_else: {
      if_else* st = (if_else*)stmt;
      if (const any* r = returnWithValue(st->VpiStmt())) return r;
      if (const any* r = returnWithValue(st->VpiElseStmt())) return r;
      break;
    }
    default:
      break;
  }
  return nullptr;
}

void UhdmLint::leaveFunction(const function* object, vpiHandle handle) {
  if (object->Return() == nullptr) {
    if (const any* st = object->Stmt()) {
      if (const any* ret = returnWithValue(st)) {
        const std::string errMsg(object->VpiName());
        serializer_->GetErrorHandler()(
            ErrorType::UHDM_RETURN_VALUE_VOID_FUNCTION, errMsg, ret, nullptr);
      }
    }
  }
}

void UhdmLint::leaveStruct_typespec(const struct_typespec* object,
                                    vpiHandle handle) {
  if (object->VpiPacked() && (object->Members() != nullptr)) {
    for (typespec_member* member : *object->Members()) {
      if (member->Default_value()) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_ILLEGAL_DEFAULT_VALUE,
                                       std::string(""), member->Default_value(),
                                       nullptr);
      }
    }
  }
}

void UhdmLint::leaveModule_inst(const module_inst* object, vpiHandle handle) {
  if (auto assigns = object->Cont_assigns()) {
    checkMultiContAssign(assigns);
  }
}

void UhdmLint::checkMultiContAssign(
    const std::vector<UHDM::cont_assign*>* assigns) {
  for (uint32_t i = 0; i < assigns->size() - 1; i++) {
    const cont_assign* cassign = assigns->at(i);
    if (cassign->VpiStrength0() || cassign->VpiStrength1()) continue;

    const expr* lhs_exp = cassign->Lhs();
    if (const operation* op = cassign->Rhs<operation>()) {
      bool triStatedOp = false;
      for (auto operand : *op->Operands()) {
        if (operand->UhdmType() == uhdmconstant) {
          constant* c = (constant*)operand;
          if (c->VpiValue().find('z') != std::string_view::npos) {
            triStatedOp = true;
            break;
          }
        }
      }
      if (triStatedOp) continue;
    }
    for (uint32_t j = i + 1; j < assigns->size(); j++) {
      const cont_assign* as = assigns->at(j);
      if (as->VpiStrength0() || as->VpiStrength1()) continue;

      if (const UHDM::ref_obj* ref = as->Lhs<ref_obj>()) {
        if (ref->VpiName() == lhs_exp->VpiName()) {
          if (const logic_net* ln = ref->Actual_group<logic_net>()) {
            int32_t nettype = ln->VpiNetType();
            if ((nettype == vpiWor) || (nettype == vpiWand) ||
                (nettype == vpiTri) || (nettype == vpiTriAnd) ||
                (nettype == vpiTriOr) || (nettype == vpiTri0) ||
                (nettype == vpiTri1) || (nettype == vpiTriReg))
              continue;
          }
          if (const operation* op = as->Rhs<operation>()) {
            bool triStatedOp = false;
            for (auto operand : *op->Operands()) {
              if (operand->UhdmType() == uhdmconstant) {
                constant* c = (constant*)operand;
                if (c->VpiValue().find('z') != std::string_view::npos) {
                  triStatedOp = true;
                  break;
                }
              }
            }
            if (triStatedOp) continue;
          }
          // serializer_->GetErrorHandler()(ErrorType::UHDM_MULTIPLE_CONT_ASSIGN,
          //                                lhs_exp->VpiName(),
          //                                lhs_exp, lhs);
        }
      }
    }
  }
}

void UhdmLint::leaveAssignment(const assignment* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (!design_->VpiElaborated()) return; // -uhdmelab
  if (const ref_obj* lhs = object->Lhs<ref_obj>()) {
    if (const logic_net* n = lhs->Actual_group<logic_net>()) {
      if (n->VpiNetType() == vpiWire) {
        bool inProcess = false;
        const any* tmp = object;
        while (tmp) {
          if ((tmp->UhdmType() == uhdmalways) ||
              (tmp->UhdmType() == uhdminitial) ||
              (tmp->UhdmType() == uhdmfinal_stmt)) {
            inProcess = true;
            break;
          }
          tmp = tmp->VpiParent();
        }
        if (inProcess) {
          const std::string errMsg(lhs->VpiName());
          serializer_->GetErrorHandler()(ErrorType::UHDM_ILLEGAL_WIRE_LHS,
                                         errMsg, lhs, 0);
        }
      }
    }
  }
}

void UhdmLint::leaveLogic_net(const logic_net* object, vpiHandle handle) {
  if (const ref_typespec* rt = object->Typespec()) {
    if (const logic_typespec* tps = rt->Actual_group<logic_typespec>()) {
      if (const VectorOfrange* ranges = tps->Ranges()) {
        range* r0 = ranges->at(0);
        if (const constant* c = r0->Right_expr<constant>()) {
          if (c->VpiValue() == "STRING:unsized") {
            const std::string errMsg(object->VpiName());
            serializer_->GetErrorHandler()(
                ErrorType::UHDM_ILLEGAL_PACKED_DIMENSION, errMsg, c, 0);
          }
        }
      }
    }
  }
}

void UhdmLint::leaveEnum_typespec(const enum_typespec* object,
                                  vpiHandle handle) {
  const typespec* baseType = nullptr;
  if (const ref_typespec* rt = object->Base_typespec()) {
    baseType = rt->Actual_group();
  }
  if (!baseType) return;
  static std::regex r("^[0-9]*'");
  ExprEval eval;
  eval.setDesign(design_);

  bool invalidValue = false;
  const uint64_t baseSize =
      eval.size(baseType, invalidValue,
                object->Instance() ? object->Instance() : object->VpiParent(),
                object->VpiParent(), true);
  if (invalidValue) return;

  for (auto c : *object->Enum_consts()) {
    const std::string_view val = c->VpiDecompile();
    if (c->VpiSize() == -1) continue;
    if (!std::regex_match(std::string(val), r)) continue;
    invalidValue = false;
    const uint64_t c_size = eval.size(c, invalidValue, object->Instance(),
                                      object->VpiParent(), true);
    if (!invalidValue && (baseSize != c_size)) {
      const std::string errMsg(c->VpiName());
      serializer_->GetErrorHandler()(ErrorType::UHDM_ENUM_CONST_SIZE_MISMATCH,
                                     errMsg, c, baseType);
    }
  }
}

}  // namespace UHDM
