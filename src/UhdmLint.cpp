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
#include <uhdm/Utils.h>
#include <uhdm/uhdm.h>

#include <cstring>
#include <regex>

namespace uhdm {

void UhdmLint::leaveBitSelect(const BitSelect* object, vpiHandle handle) {
  if (const RefObj* index = object->getIndex<RefObj>()) {
    if (const Variable* const actual = index->getActual<Variable>()) {
      if (const RefTypespec* const rt = actual->getTypespec()) {
        if (rt->getActual<RealTypespec>() != nullptr) {
          const std::string errMsg(actual->getName());
          m_serializer->getErrorHandler()(ErrorType::UHDM_REAL_TYPE_AS_SELECT,
                                          errMsg, index, nullptr);
        }
      }
    }
  }
}

static const Any* returnWithValue(const Any* stmt) {
  switch (stmt->getUhdmType()) {
    case UhdmType::ReturnStmt: {
      ReturnStmt* ret = (ReturnStmt*)stmt;
      if (const Any* r = ret->getCondition()) return r;
      break;
    }
    case UhdmType::Begin: {
      Begin* st = (Begin*)stmt;
      if (st->getStmts()) {
        for (auto s : *st->getStmts()) {
          if (const Any* r = returnWithValue(s)) return r;
        }
      }
      break;
    }
    case UhdmType::IfStmt: {
      IfStmt* st = (IfStmt*)stmt;
      if (const Any* r = returnWithValue(st->getStmt())) return r;
      break;
    }
    case UhdmType::IfElse: {
      IfElse* st = (IfElse*)stmt;
      if (const Any* r = returnWithValue(st->getStmt())) return r;
      if (const Any* r = returnWithValue(st->getElseStmt())) return r;
      break;
    }
    default:
      break;
  }
  return nullptr;
}

void UhdmLint::leaveFunction(const Function* object, vpiHandle handle) {
  if (object->getReturn() == nullptr) {
    if (const Any* st = object->getStmt()) {
      if (const Any* ret = returnWithValue(st)) {
        const std::string errMsg(object->getName());
        m_serializer->getErrorHandler()(
            ErrorType::UHDM_RETURN_VALUE_VOID_FUNCTION, errMsg, ret, nullptr);
      }
    }
  }
}

void UhdmLint::leaveStructTypespec(const StructTypespec* object,
                                   vpiHandle handle) {
  if (object->getPacked() && (object->getMembers() != nullptr)) {
    for (TypespecMember* member : *object->getMembers()) {
      if (member->getDefaultValue()) {
        m_serializer->getErrorHandler()(ErrorType::UHDM_ILLEGAL_DEFAULT_VALUE,
                                        std::string(""),
                                        member->getDefaultValue(), nullptr);
      }
    }
  }
}

void UhdmLint::leaveModule(const Module* object, vpiHandle handle) {
  if (auto assigns = object->getContAssigns()) {
    checkMultiContAssign(assigns);
  }
}

void UhdmLint::checkMultiContAssign(const std::vector<ContAssign*>* assigns) {
  for (uint32_t i = 0; i < assigns->size() - 1; i++) {
    const ContAssign* cassign = assigns->at(i);
    if (cassign->getStrength0() || cassign->getStrength1()) continue;

    const Expr* lhs_exp = cassign->getLhs();
    if (const Operation* op = cassign->getRhs<Operation>()) {
      bool triStatedOp = false;
      if (op->getOperands()) {
        for (auto operand : *op->getOperands()) {
          if (operand->getUhdmType() == UhdmType::Constant) {
            Constant* c = (Constant*)operand;
            if (c->getValue().find('z') != std::string_view::npos) {
              triStatedOp = true;
              break;
            }
          }
        }
      }
      if (triStatedOp) continue;
    }
    for (uint32_t j = i + 1; j < assigns->size(); j++) {
      const ContAssign* as = assigns->at(j);
      if (as->getStrength0() || as->getStrength1()) continue;

      if (const RefObj* ref = as->getLhs<RefObj>()) {
        if (ref->getName() == lhs_exp->getName()) {
          if (const Net* const net = ref->getActual<Net>()) {
            if (getTypespec<LogicTypespec>(net) != nullptr) {
              int32_t nettype = net->getNetType();
              if ((nettype == vpiWor) || (nettype == vpiWand) ||
                  (nettype == vpiTri) || (nettype == vpiTriAnd) ||
                  (nettype == vpiTriOr) || (nettype == vpiTri0) ||
                  (nettype == vpiTri1) || (nettype == vpiTriReg))
                continue;
            }
          }
          if (const Operation* op = as->getRhs<Operation>()) {
            bool triStatedOp = false;
            if (op->getOperands()) {
              for (auto operand : *op->getOperands()) {
                if (operand->getUhdmType() == UhdmType::Constant) {
                  Constant* c = (Constant*)operand;
                  if (c->getValue().find('z') != std::string_view::npos) {
                    triStatedOp = true;
                    break;
                  }
                }
              }
            }
            if (triStatedOp) continue;
          }
          // m_serializer->getErrorHandler()(ErrorType::UHDM_MULTIPLE_CONT_ASSIGN,
          //                                lhs_exp->VpiName(),
          //                                lhs_exp, lhs);
        }
      }
    }
  }
}

void UhdmLint::leaveAssignment(const Assignment* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (!m_design->getElaborated()) return;  // -uhdmelab
  if (const RefObj* lhs = object->getLhs<RefObj>()) {
    if (const Net* n = lhs->getActual<Net>()) {
      if (getTypespec<LogicTypespec>(n) != nullptr) {
        if (n->getNetType() == vpiWire) {
          bool inProcess = false;
          const Any* tmp = object;
          while (tmp) {
            if ((tmp->getUhdmType() == UhdmType::Always) ||
                (tmp->getUhdmType() == UhdmType::Initial) ||
                (tmp->getUhdmType() == UhdmType::FinalStmt)) {
              inProcess = true;
              break;
            }
            tmp = tmp->getParent();
          }
          if (inProcess) {
            const std::string errMsg(lhs->getName());
            m_serializer->getErrorHandler()(ErrorType::UHDM_ILLEGAL_WIRE_LHS,
                                            errMsg, lhs, 0);
          }
        }
      }
    }
  }
}

void UhdmLint::leaveNet(const Net* object, vpiHandle handle) {
  if (const LogicTypespec* const tps = getTypespec<LogicTypespec>(object)) {
    if (const RangeCollection* ranges = tps->getRanges()) {
      Range* r0 = ranges->at(0);
      if (const Constant* c = r0->getRightExpr<Constant>()) {
        if (c->getValue() == "STRING:unsized") {
          const std::string errMsg(object->getName());
          m_serializer->getErrorHandler()(
              ErrorType::UHDM_ILLEGAL_PACKED_DIMENSION, errMsg, c, nullptr);
        }
      }
    }
  }
}

void UhdmLint::leaveEnumTypespec(const EnumTypespec* object, vpiHandle handle) {
  const Typespec* baseType = nullptr;
  if (const RefTypespec* rt = object->getBaseTypespec()) {
    baseType = rt->getActual();
  }
  if (!baseType) return;
  static std::regex r("^[0-9]*'");
  ExprEval eval;
  eval.setDesign(m_design);

  bool invalidValue = false;
  const uint64_t baseSize = eval.size(
      baseType, invalidValue,
      object->getInstance() ? object->getInstance() : object->getParent(),
      object->getParent(), true);
  if (invalidValue) return;

  if (object->getEnumConsts()) {
    for (auto c : *object->getEnumConsts()) {
      const std::string_view val = c->getDecompile();
      if (c->getSize() == -1) continue;
      if (!std::regex_match(std::string(val), r)) continue;
      invalidValue = false;
      const uint64_t c_size = eval.size(c, invalidValue, object->getInstance(),
                                        object->getParent(), true);
      if (!invalidValue && (baseSize != c_size)) {
        const std::string errMsg(c->getName());
        m_serializer->getErrorHandler()(
            ErrorType::UHDM_ENUM_CONST_SIZE_MISMATCH, errMsg, c, baseType);
      }
    }
  }
}

class DetectSequenceInst final : public VpiListener {
 public:
  explicit DetectSequenceInst() {}
  ~DetectSequenceInst() override = default;

  void enterOperation(const Operation* object, vpiHandle handle) final {
    int opType = object->getOpType();
    if (opType == vpiNonOverlapImplyOp || opType == vpiOverlapImplyOp) {
      m_rhsImplication = object->getOperands()->at(1);
    }
  }

  void leaveRefObj(const RefObj* object, vpiHandle handle) final {
    if (m_decl && (m_seqParent == nullptr)) {
      const Any* parent = object;
      while (parent) {
        if (parent == m_rhsImplication) {
          m_seqParent = object;
          return;
        }
        parent = parent->getParent();
      }
      m_decl = nullptr;
    }
  }

  void leaveSequenceDecl(const SequenceDecl* object, vpiHandle handle) final {
    m_decl = object;
  }

  const SequenceDecl* seqDeclDetected() const { return m_decl; }
  const RefObj* parentRef() { return m_seqParent; }

 private:
  const RefObj* m_seqParent = nullptr;
  const SequenceDecl* m_decl = nullptr;
  const Any* m_rhsImplication = nullptr;
};

void UhdmLint::leavePropertySpec(const PropertySpec* prop_s, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (const Any* exp = prop_s->getPropertyExpr()) {
    if (exp->getUhdmType() == UhdmType::RefObj) {
      const RefObj* ref = any_cast<const RefObj*>(exp);
      if (const Net* const net = ref->getActual<Net>()) {
        if (getTypespec<LogicTypespec>(net) != nullptr) {
          const std::string errMsg(ref->getName());
          m_serializer->getErrorHandler()(ErrorType::UHDM_UNRESOLVED_PROPERTY,
                                          errMsg, ref, nullptr);
        }
      }
    }
    {
      if (prop_s->getClockingEvent()) {
        DetectSequenceInst seqDetector;
        vpiHandle h = NewVpiHandle(exp);
        seqDetector.listenAny(h);
        vpi_free_object(h);
        if (const SequenceDecl* decl = seqDetector.seqDeclDetected()) {
          const std::string errMsg(decl->getName());
          m_serializer->getErrorHandler()(
              ErrorType::UHDM_NON_TEMPORAL_SEQUENCE_USE, errMsg,
              seqDetector.parentRef(), nullptr);
        }
      }
    }
  }
}

void UhdmLint::leaveSysFuncCall(const SysFuncCall* object, vpiHandle handle) {
  ExprEval eval;
  eval.setDesign(m_design);
  if (object->getName() == "$past") {
    if (auto arg = object->getArguments()) {
      if (arg->size() == 2) {
        Any* ex = arg->at(1);
        bool invalidValue = false;
        const int64_t val = eval.get_value(
            invalidValue,
            eval.reduceExpr(ex, invalidValue, nullptr, object->getParent()));
        if (val <= 0 && (invalidValue == false)) {
          const std::string errMsg = std::to_string(val);
          m_serializer->getErrorHandler()(ErrorType::UHDM_NON_POSITIVE_VALUE,
                                          errMsg, ex, nullptr);
        }
      }
    }
  }
}

void UhdmLint::leavePort(const Port* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  bool signedHighConn = false;
  bool signedLowConn = false;
  bool highConn = false;
  const Any* reportObject = object;
  if (const RefObj* const ref = object->getHighConn<RefObj>()) {
    reportObject = ref;
    if (const LogicTypespec* const lt =
            getTypespec<LogicTypespec>(ref->getActual())) {
      highConn = true;
      if (lt->getSigned()) {
        signedHighConn = true;
      }
    }
  }
  if (const RefObj* const ref = object->getLowConn<RefObj>()) {
    if (const LogicTypespec* const lt =
            getTypespec<LogicTypespec>(ref->getActual())) {
      if (lt->getSigned()) {
        signedLowConn = true;
      }
    }
  }
  if (highConn && (signedLowConn != signedHighConn)) {
    std::string_view errMsg = object->getName();
    m_serializer->getErrorHandler()(ErrorType::UHDM_SIGNED_UNSIGNED_PORT_CONN,
                                    std::string(errMsg), reportObject, nullptr);
  }
}

}  // namespace uhdm
