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
    case UHDM_OBJECT_TYPE::uhdmreturn_stmt: {
      return_stmt* ret = (return_stmt*)stmt;
      if (const any* r = ret->VpiCondition()) return r;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmbegin: {
      begin* st = (begin*)stmt;
      if (st->Stmts()) {
        for (auto s : *st->Stmts()) {
          if (const any* r = returnWithValue(s)) return r;
        }
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmif_stmt: {
      if_stmt* st = (if_stmt*)stmt;
      if (const any* r = returnWithValue(st->VpiStmt())) return r;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmif_else: {
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
      if (op->Operands()) {
        for (auto operand : *op->Operands()) {
          if (operand->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
            constant* c = (constant*)operand;
            if (c->VpiValue().find('z') != std::string_view::npos) {
              triStatedOp = true;
              break;
            }
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
            if (op->Operands()) {
              for (auto operand : *op->Operands()) {
                if (operand->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
                  constant* c = (constant*)operand;
                  if (c->VpiValue().find('z') != std::string_view::npos) {
                    triStatedOp = true;
                    break;
                  }
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
  if (!design_->VpiElaborated()) return;  // -uhdmelab
  if (const ref_obj* lhs = object->Lhs<ref_obj>()) {
    if (const logic_net* n = lhs->Actual_group<logic_net>()) {
      if (n->VpiNetType() == vpiWire) {
        bool inProcess = false;
        const any* tmp = object;
        while (tmp) {
          if ((tmp->UhdmType() == UHDM_OBJECT_TYPE::uhdmalways) ||
              (tmp->UhdmType() == UHDM_OBJECT_TYPE::uhdminitial) ||
              (tmp->UhdmType() == UHDM_OBJECT_TYPE::uhdmfinal_stmt)) {
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
    if (const logic_typespec* tps = rt->Actual_typespec<logic_typespec>()) {
      if (const VectorOfrange* ranges = tps->Ranges()) {
        range* r0 = ranges->at(0);
        if (const constant* c = r0->Right_expr<constant>()) {
          if (c->VpiValue() == "STRING:unsized") {
            const std::string errMsg(object->VpiName());
            serializer_->GetErrorHandler()(
                ErrorType::UHDM_ILLEGAL_PACKED_DIMENSION, errMsg, c, nullptr);
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
    baseType = rt->Actual_typespec<typespec>();
  }
  if (!baseType) return;
  // The base of an enum is an integer atom type, or an integer vector type
  // with at most ONE packed dimension (LRM 6.19): not an unpacked array,
  // queue, another enum, a struct, real or string, and no range on an atom.
  {
    bool invalidBase = false;
    switch (baseType->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmint_typespec:
      case UHDM_OBJECT_TYPE::uhdminteger_typespec:
      case UHDM_OBJECT_TYPE::uhdmbyte_typespec:
      case UHDM_OBJECT_TYPE::uhdmshort_int_typespec:
      case UHDM_OBJECT_TYPE::uhdmlong_int_typespec:
      case UHDM_OBJECT_TYPE::uhdmtime_typespec:
        break;
      case UHDM_OBJECT_TYPE::uhdmlogic_typespec: {
        // Packed dimensions: its own ranges plus those of an element type
        // (`typedef T2 [3:0] T` over a vector T2 is two dimensions).
        size_t dims = 0;
        for (const logic_typespec* lt = (const logic_typespec*)baseType; lt;) {
          dims += lt->Ranges() ? lt->Ranges()->size() : 0;
          const typespec* elem =
              lt->Elem_typespec() ? lt->Elem_typespec()->Actual_typespec()
                                  : nullptr;
          lt = elem && elem->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_typespec
                   ? (const logic_typespec*)elem
                   : nullptr;
        }
        if (dims > 1) invalidBase = true;
        break;
      }
      case UHDM_OBJECT_TYPE::uhdmbit_typespec: {
        const bit_typespec* bt = (const bit_typespec*)baseType;
        if (bt->Ranges() && bt->Ranges()->size() > 1) invalidBase = true;
        break;
      }
      case UHDM_OBJECT_TYPE::uhdmarray_typespec:
      case UHDM_OBJECT_TYPE::uhdmenum_typespec:
      case UHDM_OBJECT_TYPE::uhdmstruct_typespec:
      case UHDM_OBJECT_TYPE::uhdmunion_typespec:
      case UHDM_OBJECT_TYPE::uhdmreal_typespec:
      case UHDM_OBJECT_TYPE::uhdmshort_real_typespec:
      case UHDM_OBJECT_TYPE::uhdmstring_typespec:
      case UHDM_OBJECT_TYPE::uhdmchandle_typespec:
      case UHDM_OBJECT_TYPE::uhdmclass_typespec:
        invalidBase = true;
        break;
      default:
        break;
    }
    if (invalidBase) {
      const std::string errMsg(baseType->VpiName());
      serializer_->GetErrorHandler()(ErrorType::UHDM_INVALID_ENUM_BASE_TYPE,
                                     errMsg, object, baseType);
      return;
    }
  }
  static std::regex r("^[0-9]*'");
  ExprEval eval;
  eval.setDesign(design_);

  bool invalidValue = false;
  const uint64_t baseSize =
      eval.size(baseType, invalidValue,
                object->Instance() ? object->Instance() : object->VpiParent(),
                object->VpiParent(), true);
  if (invalidValue) return;

  if (object->Enum_consts()) {
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
}

class DetectSequenceInst : public VpiListener {
 public:
  explicit DetectSequenceInst() {}
  ~DetectSequenceInst() override = default;

   void enterOperation(const operation* object, vpiHandle handle) final {
    int opType = object->VpiOpType();
    if (opType == vpiNonOverlapImplyOp || opType == vpiOverlapImplyOp) {
      rhsImplication = object->Operands()->at(1);
    }
  }

  void leaveRef_obj(const ref_obj* object, vpiHandle handle) final {
    if (decl && (seq_parent == nullptr)) {
      const any* parent = object;
      while (parent) {
        if (parent == rhsImplication) {
          seq_parent = object;
          return;
        }
        parent = parent->VpiParent();
      }
      decl = nullptr;
    }
  }

  void leaveSequence_decl(const sequence_decl *object, vpiHandle handle) final {
        decl = object; 
  }
  const sequence_decl* seqDeclDetected() const { return decl; }
  const ref_obj* parentRef() { return seq_parent; }
 private:
  const ref_obj* seq_parent = nullptr;
  const sequence_decl* decl = nullptr;
  const any* rhsImplication = nullptr;
};

void UhdmLint::leaveProperty_spec(const property_spec* prop_s,
                                  vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (const any* exp = prop_s->VpiPropertyExpr()) {
    if (exp->UhdmType() == uhdmref_obj) {
      const ref_obj* ref = any_cast<const ref_obj*>(exp);
      if (ref->Actual_group()) {
        if (ref->Actual_group()->UhdmType() == uhdmlogic_net) {
          const std::string errMsg(ref->VpiName());
          serializer_->GetErrorHandler()(ErrorType::UHDM_UNRESOLVED_PROPERTY,
                                         errMsg, ref, nullptr);
        }
      }
    }
    {
      if (prop_s->VpiClockingEvent()) {
        DetectSequenceInst seqDetector;
        vpiHandle h = NewVpiHandle(exp);
        seqDetector.listenAny(h);
        vpi_free_object(h);
        if (const sequence_decl* decl = seqDetector.seqDeclDetected()) {
          const std::string errMsg(decl->VpiName());
          serializer_->GetErrorHandler()(
              ErrorType::UHDM_NON_TEMPORAL_SEQUENCE_USE, errMsg,
              seqDetector.parentRef(), nullptr);
        }
      }
    }
  }
}

void UhdmLint::leaveSys_func_call(const sys_func_call* object,
                                  vpiHandle handle) {
  ExprEval eval;
  eval.setDesign(design_);
  if (object->VpiName() == "$past") {
    if (auto arg = object->Tf_call_args()) {
      if (arg->size() == 2) {
        any* ex = arg->at(1);
        bool invalidValue = false;
        const int64_t val = eval.get_value(
            invalidValue,
            eval.reduceExpr(ex, invalidValue, nullptr, object->VpiParent()));
        if (val <= 0 && (invalidValue == false)) {
          const std::string errMsg = std::to_string(val);
          serializer_->GetErrorHandler()(
              ErrorType::UHDM_NON_POSITIVE_VALUE, errMsg,
              ex, nullptr);
        }
      }
    }
  }
}


void UhdmLint::leavePort(const port* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  bool signedHighConn = false;
  bool signedLowConn = false;
  bool highConn = false;
  const any* reportObject = object;
  if (const any* hc = object->High_conn()) {
    if (const ref_obj* ref = any_cast<const ref_obj*> (hc)) {
      reportObject = ref;
      if (const any* actual = ref->Actual_group()) {
        if (actual->UhdmType() == uhdmlogic_var) {
          logic_var* var = (logic_var*)actual;
          highConn = true;
          if (var->VpiSigned()) {
             signedHighConn = true;
          }
        } if (actual->UhdmType() == uhdmlogic_net) {
          logic_net* var = (logic_net*)actual;
          highConn = true;
          if (var->VpiSigned()) {
             signedHighConn = true;
          }
        }
      }
    } 
  }
  if (const any* lc = object->Low_conn()) {
    if (const ref_obj* ref = any_cast<const ref_obj*>(lc)) {
      if (const any* actual = ref->Actual_group()) {
        if (actual->UhdmType() == uhdmlogic_var) {
          logic_var* var = (logic_var*)actual;
          if (var->VpiSigned()) {
            signedLowConn = true;
          }
        }
        if (actual->UhdmType() == uhdmlogic_net) {
          logic_net* var = (logic_net*)actual;
          if (var->VpiSigned()) {
            signedLowConn = true;
          }
        }
      }
    }
  }
  if (highConn && (signedLowConn != signedHighConn)) {
    std::string_view errMsg = object->VpiName();
    serializer_->GetErrorHandler()(
              ErrorType::UHDM_SIGNED_UNSIGNED_PORT_CONN, std::string(errMsg),
              reportObject, nullptr);
  }
}

// A constant expression (LRM 11.2.1) -- a declaration range, a part-select
// bound, a parameter value -- may not read a net, and a parameter may not
// read a variable either: `wire [N:0] a;` / `In[7:N]` with `input [2:0] N`,
// `parameter P = some_integer;`.  Reported on the first offending
// reference.  A net Surelog FABRICATED for a name it could not resolve (no
// typespec of its own) is not a declared net and is left alone.
static const any* nonConstantRef(const any* e, bool netsOnly) {
  if (e == nullptr) return nullptr;
  switch (e->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmref_obj: {
      const any* actual = ((const ref_obj*)e)->Actual_group();
      if (actual == nullptr) return nullptr;
      switch (actual->UhdmType()) {
        case UHDM_OBJECT_TYPE::uhdmlogic_net: {
          // A `reg` is a VARIABLE (a logic_net of type vpiReg): a loop index
          // in `a[i+1:i]` is technically illegal too, but common in older
          // RTL and accepted by other tools -- like any other variable it is
          // only reported in a parameter value.
          const logic_net* n = (const logic_net*)actual;
          if (n->Typespec() == nullptr) return nullptr;
          if (n->VpiNetType() == vpiReg) return netsOnly ? nullptr : e;
          return e;
        }
        case UHDM_OBJECT_TYPE::uhdmlogic_var:
        case UHDM_OBJECT_TYPE::uhdmint_var:
        case UHDM_OBJECT_TYPE::uhdminteger_var:
        case UHDM_OBJECT_TYPE::uhdmbit_var:
        case UHDM_OBJECT_TYPE::uhdmbyte_var:
        case UHDM_OBJECT_TYPE::uhdmlong_int_var:
        case UHDM_OBJECT_TYPE::uhdmshort_int_var:
          return netsOnly ? nullptr : e;
        default:
          return nullptr;
      }
    }
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      const operation* op = (const operation*)e;
      if (op->Operands())
        for (auto o : *op->Operands())
          if (const any* r = nonConstantRef(o, netsOnly)) return r;
      return nullptr;
    }
    default:
      return nullptr;
  }
}

void UhdmLint::checkConstantExpr(const any* expr, bool netsOnly) {
  if (const any* r = nonConstantRef(expr, netsOnly)) {
    const std::string errMsg(r->VpiName());
    serializer_->GetErrorHandler()(ErrorType::UHDM_NON_CONSTANT_EXPRESSION,
                                   errMsg, r, nullptr);
  }
}

void UhdmLint::leavePart_select(const part_select* object, vpiHandle handle) {
  checkConstantExpr(object->Left_range(), true);
  checkConstantExpr(object->Right_range(), true);
}

void UhdmLint::leaveRange(const range* object, vpiHandle handle) {
  checkConstantExpr(object->Left_expr(), true);
  checkConstantExpr(object->Right_expr(), true);
}

void UhdmLint::leaveParam_assign(const param_assign* object,
                                 vpiHandle handle) {
  checkConstantExpr(object->Rhs(), false);
}

}  // namespace UHDM
