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
  noteImplicitNetRef(object);
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

// The static unpacked array an expression names as a WHOLE, or nullptr:
// Surelog binds a whole-array reference either to the array_net/array_var
// or to its element net/variable, whose parent is the array.
static const any* wholeUnpackedArray(const any* actual) {
  if (actual == nullptr) return nullptr;
  if (actual->UhdmType() != UHDM_OBJECT_TYPE::uhdmarray_net &&
      actual->UhdmType() != UHDM_OBJECT_TYPE::uhdmarray_var) {
    const any* p = actual->VpiParent();
    if (p && (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net ||
              p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var))
      actual = p;
    else
      return nullptr;
  }
  if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var) {
    const array_var* av = (const array_var*)actual;
    if (av->VpiArrayType() != vpiStaticArray && av->VpiArrayType() != 0)
      return nullptr;  // dynamic / associative / queue: no fixed shape
  }
  return actual;
}

// Element count and element width of a 1-D static unpacked array, when both
// are known constants.
static bool unpackedShape(const any* arr, Serializer* s, design* d,
                          uint64_t& count, uint64_t& width) {
  const VectorOfrange* ranges = nullptr;
  const any* elem = nullptr;
  if (arr->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net) {
    const array_net* an = (const array_net*)arr;
    ranges = an->Ranges();
    if (an->Nets() && !an->Nets()->empty()) elem = an->Nets()->at(0);
  } else {
    const array_var* av = (const array_var*)arr;
    ranges = av->Ranges();
    if (av->Variables() && !av->Variables()->empty())
      elem = av->Variables()->at(0);
  }
  if (ranges == nullptr || ranges->size() != 1 || elem == nullptr) return false;
  ExprEval eval;
  eval.setDesign(d);
  bool invalid = false;
  const range* r = ranges->at(0);
  if (!r->Left_expr() || !r->Right_expr()) return false;
  if (r->Left_expr()->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant ||
      r->Right_expr()->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
    return false;
  const int64_t l = eval.get_value(invalid, (const expr*)r->Left_expr());
  const int64_t rr = eval.get_value(invalid, (const expr*)r->Right_expr());
  if (invalid) return false;
  count = (l > rr ? l - rr : rr - l) + 1;
  width = eval.size(elem, invalid, nullptr, nullptr, true);
  return !invalid && width > 0;
}

// Assigning a whole unpacked array (LRM 7.6): the right-hand side must be an
// unpacked array of the same shape, or an assignment pattern / unpacked
// array concatenation.  `assign x = y;` with y a vector, an element, an
// expression or an array of another element width or count was accepted.
void UhdmLint::checkUnpackedArrayAssign(const any* lhs, const any* rhs,
                                        const any* where) {
  // A WHOLE-array target only: bit_select / var_select derive from ref_obj
  // and must not match (`mem[sp] <= d` is an element write).
  if (lhs == nullptr || rhs == nullptr ||
      lhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj)
    return;
  const ref_obj* lref = (const ref_obj*)lhs;
  const any* larr = wholeUnpackedArray(lref->Actual_group());
  if (larr == nullptr) return;
  uint64_t lcount = 0, lwidth = 0;
  if (!unpackedShape(larr, serializer_, design_, lcount, lwidth)) return;
  bool bad = false;
  switch (rhs->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmconstant:
      bad = true;
      break;
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      const int op = ((const operation*)rhs)->VpiOpType();
      // Patterns, concatenations, conditionals and casts may legally build
      // an unpacked array; any other operator yields a single value.
      if (op != vpiAssignmentPatternOp && op != vpiMultiAssignmentPatternOp &&
          op != vpiConcatOp && op != vpiMultiConcatOp && op != vpiConditionOp &&
          op != vpiCastOp)
        bad = true;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmref_obj: {
      const any* actual = ((const ref_obj*)rhs)->Actual_group();
      if (actual == nullptr) break;
      const any* rarr = wholeUnpackedArray(actual);
      if (rarr == nullptr) {
        // A net or variable that is not an array: a scalar or a vector.
        switch (actual->UhdmType()) {
          case UHDM_OBJECT_TYPE::uhdmlogic_net:
          case UHDM_OBJECT_TYPE::uhdmlogic_var:
          case UHDM_OBJECT_TYPE::uhdmbit_var:
          case UHDM_OBJECT_TYPE::uhdmint_var:
          case UHDM_OBJECT_TYPE::uhdminteger_var:
            bad = true;
            break;
          default:
            break;
        }
        break;
      }
      uint64_t rcount = 0, rwidth = 0;
      if (unpackedShape(rarr, serializer_, design_, rcount, rwidth))
        bad = (rcount != lcount) || (rwidth != lwidth);
      break;
    }
    default:
      break;
  }
  if (bad) {
    const std::string errMsg(lref->VpiName());
    serializer_->GetErrorHandler()(
        ErrorType::UHDM_ILLEGAL_UNPACKED_ARRAY_ASSIGN, errMsg, where, nullptr);
  }
}

void UhdmLint::leaveCont_assign(const cont_assign* object, vpiHandle handle) {
  checkUnpackedArrayAssign(object->Lhs(), object->Rhs(), object);
}

void UhdmLint::leaveAssignment(const assignment* object, vpiHandle handle) {
  checkUnpackedArrayAssign(object->Lhs(), object->Rhs(), object);
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
  noteImplicitNetRef(object);
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

// `ex` itself when it is a constant; the value of a module parameter it names,
// read from the enclosing ELABORATED instance's param_assign (an unelaborated
// definition's defaults are not what any instance uses, so nothing is
// returned there); nullptr otherwise.
static const any* elaboratedParamValue(const any* ex, const any* where) {
  if (ex == nullptr) return nullptr;
  if (ex->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) return ex;
  if (ex->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return nullptr;
  const any* actual = ((const ref_obj*)ex)->Actual_group();
  if (actual == nullptr ||
      actual->UhdmType() != UHDM_OBJECT_TYPE::uhdmparameter)
    return nullptr;
  const std::string_view name = ((const ref_obj*)ex)->VpiName();
  for (const any* p = where ? where->VpiParent() : nullptr; p;
       p = p->VpiParent()) {
    if (p->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst) continue;
    const module_inst* mi = (const module_inst*)p;
    const any* up = mi->VpiParent();
    const bool elaborated =
        mi->VpiTop() ||
        (up && up->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst);
    if (!elaborated) return nullptr;
    if (const auto pas = mi->Param_assigns()) {
      for (const param_assign* pa : *pas) {
        if (pa->Lhs() && pa->Lhs()->VpiName() == name && pa->Rhs() &&
            pa->Rhs()->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant)
          return pa->Rhs();
      }
    }
    return nullptr;
  }
  return nullptr;
}

// A size that must be positive (LRM 11.5.1 indexed part-select width,
// 11.4.12.1 replication count, 6.24.1 size cast).  `ex + offset` is the size;
// only a CONSTANT is checked: an unelaborated definition keeps `{(W-1){..}}`
// unfolded, and its parameter defaults are not the values an instance uses --
// the elaborated copy carries the folded value.
void UhdmLint::checkNonPositive(const any* ex, const any* where, int64_t offset,
                                bool allowZero) {
  if (ex == nullptr) return;
  ExprEval eval;
  eval.setDesign(design_);
  bool invalid = false;
  const any* reduced = ex;
  if (invalid || reduced == nullptr ||
      reduced->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
    return;
  const constant* c = (const constant*)reduced;
  // A fill literal ('0, '1, 'x, 'z) or an x/z value is not a size.
  if (c->VpiSize() < 0) return;
  std::string_view v = c->VpiValue();
  if (v.find_first_of("xXzZ?") != std::string_view::npos) return;
  const int64_t val = eval.get_value(invalid, (const expr*)reduced);
  if (invalid) return;
  const int64_t size = val + offset;
  if (size < 0 || (size == 0 && !allowZero)) {
    serializer_->GetErrorHandler()(ErrorType::UHDM_NON_POSITIVE_VALUE,
                                   std::to_string(size), where, nullptr);
  }
}

void UhdmLint::leaveIndexed_part_select(const indexed_part_select* object,
                                        vpiHandle handle) {
  noteImplicitNetRef(object);
  checkNonPositive(elaboratedParamValue(object->Width_expr(), object), object,
                   0, false);
}

void UhdmLint::leaveOperation(const operation* object, vpiHandle handle) {
  const VectorOfany* ops = object->Operands();
  if (ops == nullptr || ops->empty()) return;
  switch (object->VpiOpType()) {
    case vpiMultiConcatOp:
      // A zero count is legal inside a larger concatenation; a negative one
      // never is.
      checkNonPositive(ops->at(0), object, 0, true);
      break;
    case vpiCastOp: {
      // A SIZE cast `N'(x)` is compiled as a cast to an anonymous
      // logic_typespec [N-1:0].
      const ref_typespec* rt = object->Typespec();
      if (rt == nullptr) break;
      const typespec* ts = rt->Actual_typespec();
      if (ts == nullptr ||
          ts->UhdmType() != UHDM_OBJECT_TYPE::uhdmlogic_typespec ||
          !ts->VpiName().empty())
        break;
      const VectorOfrange* ranges = ((const logic_typespec*)ts)->Ranges();
      if (ranges == nullptr || ranges->size() != 1) break;
      const range* r = ranges->at(0);
      const any* l = r->Left_expr();
      const any* rr = r->Right_expr();
      if (l == nullptr || rr == nullptr ||
          l->UhdmType() != UHDM_OBJECT_TYPE::uhdmoperation ||
          rr->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
        break;
      const operation* sub = (const operation*)l;
      if (sub->VpiOpType() != vpiSubOp || sub->Operands() == nullptr ||
          sub->Operands()->size() != 2)
        break;
      const any* one = sub->Operands()->at(1);
      if (one->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant ||
          ((const constant*)one)->VpiValue() != "UINT:1" ||
          ((const constant*)rr)->VpiValue() != "UINT:0")
        break;
      // The typespec is shared by every instance and its `N` names the
      // DEFINITION's parameter.
      const any* width = elaboratedParamValue(sub->Operands()->at(0), object);
      if (width == nullptr) break;
      checkNonPositive(width, object, 0, false);
      break;
    }
    default:
      break;
  }
}

// LRM 6.10: an implicit net is inferred only from the left-hand side of a
// continuous assignment and from a port / terminal connection.  `ref` (or the
// concatenation it is part of) sits in one of those places.
static bool inImplicitNetContext(const any* ref) {
  const any* cur = ref;
  for (const any* p = ref->VpiParent(); p != nullptr;
       cur = p, p = p->VpiParent()) {
    switch (p->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmoperation: {
        const int op = ((const operation*)p)->VpiOpType();
        if (op == vpiConcatOp || op == vpiMultiConcatOp) continue;
        return false;
      }
      case UHDM_OBJECT_TYPE::uhdmcont_assign:
        return ((const cont_assign*)p)->Lhs() == cur;
      case UHDM_OBJECT_TYPE::uhdmport:
        return ((const port*)p)->High_conn() == cur;
      case UHDM_OBJECT_TYPE::uhdmprim_term:
      case UHDM_OBJECT_TYPE::uhdmgate:
      case UHDM_OBJECT_TYPE::uhdmswitch_tran:
      case UHDM_OBJECT_TYPE::uhdmudp:
        return true;
      default:
        return false;
    }
  }
  return false;
}

// Surelog binds a name it cannot resolve to a net it FABRICATES on the
// module: a logic_net with no typespec of its own.  That is the LRM's
// implicit net when some reference to the name may infer one; a name used
// only elsewhere (an always block, an expression, a parameter value, a range)
// is an undeclared identifier.
// Class bodies are not checked: Surelog leaves many members, `this` and
// inherited methods unbound there.  Nor are sequence / property bodies,
// whose local variables (LRM 16.10) it does not model.
static bool inClass(const any* obj) {
  for (const any* p = obj; p != nullptr; p = p->VpiParent()) {
    switch (p->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmclass_defn:
      case UHDM_OBJECT_TYPE::uhdmsequence_decl:
      case UHDM_OBJECT_TYPE::uhdmproperty_decl:
        return true;
      default:
        break;
    }
  }
  return false;
}

// The definition name (`work@m` -> `m`) of the module `obj` is in, or "".
static std::string ownerDefName(const any* obj) {
  for (const any* p = obj; p != nullptr; p = p->VpiParent()) {
    if (p->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst) continue;
    std::string_view def = ((const module_inst*)p)->VpiDefName();
    if (const size_t at = def.find('@'); at != std::string_view::npos)
      def.remove_prefix(at + 1);
    return std::string(def);
  }
  return "";
}

void UhdmLint::noteImplicitNetRef(const ref_obj* ref) {
  const any* actual = ref->Actual_group();
  if (actual == nullptr ||
      actual->UhdmType() != UHDM_OBJECT_TYPE::uhdmlogic_net)
    return;
  const logic_net* net = (const logic_net*)actual;
  if (net->Typespec() != nullptr) return;
  if (inClass(ref)) return;
  // In a module DEFINITION a port's net carries no typespec (it lives on the
  // port), so a net named like one of the module's ports is declared.
  const any* owner = net->VpiParent();
  while (owner && owner->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst)
    owner = owner->VpiParent();
  if (owner == nullptr) return;
  if (const auto ports = ((const module_inst*)owner)->Ports()) {
    for (const port* p : *ports)
      if (p->VpiName() == net->VpiName()) return;
  }
  if (net->VpiName().find('.') != std::string_view::npos) return;
  std::string key(net->VpiFullName());
  if (key.empty()) key = std::string(net->VpiName());
  auto [it, inserted] = implicitNets_.emplace(key, ImplicitNetUse{});
  if (inserted) {
    implicitNetOrder_.push_back(key);
    it->second.firstRef = ref;
    it->second.ownerDef = ownerDefName(net);
  }
  if (!it->second.legal && inImplicitNetContext(ref)) it->second.legal = true;
}

void UhdmLint::leaveRef_obj(const ref_obj* object, vpiHandle handle) {
  noteImplicitNetRef(object);
}

void UhdmLint::leaveVar_select(const var_select* object, vpiHandle handle) {
  noteImplicitNetRef(object);
}

void UhdmLint::reportUndefinedTaskFunc(const tf_call* call, bool bound) {
  if (bound) return;
  const std::string_view name = call->VpiName();
  if (name.empty() || name[0] == '$') return;
  // Built-ins of the `std` package (LRM 18.12, 18.13), visible everywhere.
  static constexpr std::string_view kStdBuiltins[] = {
      "randomize", "urandom",       "urandom_range",
      "srandom",   "get_randstate", "set_randstate"};
  for (std::string_view b : kStdBuiltins) {
    if (name == b) return;
  }
  if (!reportedCalls_
           .emplace(std::string(call->VpiFile()), call->VpiLineNo(),
                    call->VpiColumnNo())
           .second)
    return;
  // Reported from leaveDesign, once every declaration has been seen.
  if (inClass(call)) return;
  // `cls::type_id::create(...)`: a class scope, bound after the classes.
  if (name.find("::") != std::string_view::npos) return;
  // `q.size` without parentheses: a built-in method of the prefix object.
  if (const any* p = call->VpiParent();
      p && p->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path)
    return;
  unboundCalls_.emplace_back(call, ownerDefName(call));
}

void UhdmLint::leaveFunc_call(const func_call* object, vpiHandle handle) {
  reportUndefinedTaskFunc(object, object->Function() != nullptr);
}

void UhdmLint::leaveTask_call(const task_call* object, vpiHandle handle) {
  reportUndefinedTaskFunc(object, object->Task() != nullptr);
}

void UhdmLint::leaveAny(const any* object, vpiHandle handle) {
  switch (object->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmmethod_func_call: {
      // A `with (...)` clause names the iterator (`item` unless the call
      // gives one as its argument) without declaring it.
      const method_func_call* m = (const method_func_call*)object;
      if (m->With() == nullptr) return;
      declaredNames_.emplace("item");
      if (const auto args = m->Tf_call_args()) {
        for (const any* a : *args) {
          if (!a->VpiName().empty()) declaredNames_.emplace(a->VpiName());
        }
      }
      return;
    }
    // References and expressions name what they USE, not what they declare.
    case UHDM_OBJECT_TYPE::uhdmref_obj:
    case UHDM_OBJECT_TYPE::uhdmref_typespec:
    case UHDM_OBJECT_TYPE::uhdmref_module:
    case UHDM_OBJECT_TYPE::uhdmbit_select:
    case UHDM_OBJECT_TYPE::uhdmpart_select:
    case UHDM_OBJECT_TYPE::uhdmindexed_part_select:
    case UHDM_OBJECT_TYPE::uhdmvar_select:
    case UHDM_OBJECT_TYPE::uhdmhier_path:
    case UHDM_OBJECT_TYPE::uhdmfunc_call:
    case UHDM_OBJECT_TYPE::uhdmtask_call:
    case UHDM_OBJECT_TYPE::uhdmmethod_task_call:
    case UHDM_OBJECT_TYPE::uhdmsys_func_call:
    case UHDM_OBJECT_TYPE::uhdmsys_task_call:
    case UHDM_OBJECT_TYPE::uhdmoperation:
    case UHDM_OBJECT_TYPE::uhdmconstant:
      return;
    case UHDM_OBJECT_TYPE::uhdmmodule_inst: {
      // A module's own name is declared too (`$printtimescale(top)`).
      const module_inst* mi = (const module_inst*)object;
      const std::string def = ownerDefName(mi);
      if (!def.empty()) {
        declaredNames_.emplace(def);
        const any* up = mi->VpiParent();
        if (mi->VpiTop() ||
            (up && up->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst))
          elaboratedDefs_.emplace(def);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmlogic_net:
      // The fabricated net itself declares nothing.
      if (((const logic_net*)object)->Typespec() == nullptr) return;
      break;
    default:
      break;
  }
  const std::string_view name = object->VpiName();
  if (!name.empty()) declaredNames_.emplace(name);
}

void UhdmLint::leaveDesign(const design* object, vpiHandle handle) {
  // Built-in scope names (`std::randomize`, `this.x`, `super.f()`).
  for (std::string_view builtin : {"std", "this", "super"})
    declaredNames_.emplace(builtin);
  for (const std::string& key : implicitNetOrder_) {
    const ImplicitNetUse& use = implicitNets_[key];
    if (use.legal) continue;
    const std::string_view name = use.firstRef->VpiName();
    if (declaredNames_.find(name) != declaredNames_.end()) continue;
    if (elaboratedDefs_.find(use.ownerDef) == elaboratedDefs_.end()) continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_UNDECLARED_IDENTIFIER,
                                   std::string(use.firstRef->VpiName()),
                                   use.firstRef, nullptr);
  }
  for (const auto& [call, ownerDef] : unboundCalls_) {
    if (declaredNames_.find(call->VpiName()) != declaredNames_.end()) continue;
    if (elaboratedDefs_.find(ownerDef) == elaboratedDefs_.end()) continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_UNDEFINED_TASK_FUNC,
                                   std::string(call->VpiName()), call, nullptr);
  }
  unboundCalls_.clear();
  implicitNets_.clear();
  implicitNetOrder_.clear();
  reportedCalls_.clear();
  declaredNames_.clear();
  elaboratedDefs_.clear();
}

}  // namespace UHDM
