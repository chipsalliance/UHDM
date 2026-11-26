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

#include <cerrno>
#include <cstring>
#include <regex>
#include <set>

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

// The member list of a struct or union typespec, nullptr for anything else.
static const VectorOftypespec_member* aggregateMembers(const typespec* ts) {
  if (const struct_typespec* st = any_cast<const struct_typespec*>(ts))
    return st->Members();
  if (const union_typespec* ut = any_cast<const union_typespec*>(ts))
    return ut->Members();
  return nullptr;
}

static const typespec* memberTypespec(const typespec_member* member) {
  if (const ref_typespec* rt = member->Typespec()) return rt->Actual_typespec();
  return nullptr;
}

// Surelog may hold several copies of one declared type (a forward typedef
// and its completion), so a type is matched by kind, name and position too.
static bool sameTypeDecl(const typespec* a, const typespec* b) {
  if (a == b) return true;
  if (a == nullptr || b == nullptr) return false;
  return a->UhdmType() == b->UhdmType() && !a->VpiName().empty() &&
         a->VpiName() == b->VpiName() && a->VpiFile() == b->VpiFile() &&
         a->VpiLineNo() == b->VpiLineNo() &&
         a->VpiColumnNo() == b->VpiColumnNo();
}

// True if `ts` contains `target` as a member, directly or through nested
// structs/unions and the element types of arrays.  Class handles are not
// followed: a class may legally refer to itself.
static bool containsAggregate(const typespec* ts, const typespec* target,
                              std::set<const typespec*>& visited) {
  if (ts == nullptr || !visited.insert(ts).second) return false;
  const typespec* elem = nullptr;
  if (const array_typespec* at = any_cast<const array_typespec*>(ts)) {
    if (const ref_typespec* rt = at->Elem_typespec())
      elem = rt->Actual_typespec();
  } else if (const packed_array_typespec* pt =
                 any_cast<const packed_array_typespec*>(ts)) {
    if (const ref_typespec* rt = pt->Elem_typespec())
      elem = rt->Actual_typespec();
  }
  if (elem != nullptr)
    return sameTypeDecl(elem, target) ||
           containsAggregate(elem, target, visited);
  if (const VectorOftypespec_member* members = aggregateMembers(ts)) {
    for (const typespec_member* member : *members) {
      const typespec* mts = memberTypespec(member);
      if (sameTypeDecl(mts, target) || containsAggregate(mts, target, visited))
        return true;
    }
  }
  return false;
}

// LRM 7.2.1: a packed structure or union holds only integral members; LRM
// 6.18: a type cannot be defined in terms of itself (`typedef T; typedef
// struct packed { T x; } T;`).
void UhdmLint::checkAggregateTypespec(const typespec* object, bool packed) {
  const VectorOftypespec_member* members = aggregateMembers(object);
  if (members == nullptr) return;
  for (const typespec_member* member : *members) {
    const typespec* mts = memberTypespec(member);
    if (mts == nullptr) continue;
    if (packed) {
      switch (mts->UhdmType()) {
        case UHDM_OBJECT_TYPE::uhdmreal_typespec:
        case UHDM_OBJECT_TYPE::uhdmshort_real_typespec:
        case UHDM_OBJECT_TYPE::uhdmstring_typespec:
        case UHDM_OBJECT_TYPE::uhdmchandle_typespec:
        case UHDM_OBJECT_TYPE::uhdmevent_typespec:
        case UHDM_OBJECT_TYPE::uhdmclass_typespec:
          serializer_->GetErrorHandler()(
              ErrorType::UHDM_PACKED_MEMBER_NOT_INTEGRAL,
              std::string(member->VpiName()), member, nullptr);
          break;
        default:
          break;
      }
    }
    std::set<const typespec*> visited;
    if (sameTypeDecl(mts, object) || containsAggregate(mts, object, visited)) {
      serializer_->GetErrorHandler()(ErrorType::UHDM_RECURSIVE_TYPE,
                                     std::string(object->VpiName()), member,
                                     nullptr);
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
  checkAggregateTypespec(object, object->VpiPacked());
}

void UhdmLint::leaveUnion_typespec(const union_typespec* object,
                                   vpiHandle handle) {
  checkAggregateTypespec(object, object->VpiPacked());
}

void UhdmLint::leaveModule_inst(const module_inst* object, vpiHandle handle) {
  if (auto assigns = object->Cont_assigns()) {
    checkMultiContAssign(assigns);
  }
  checkScopeNameClashes(object);
}

// LRM 3.13, 27.6: a generate block's name lives in the module's scope, so it
// may not repeat the name of a net, variable, parameter, task, function,
// instance or named block there -- nor of ANOTHER generate block.  The
// iterations of one loop share a source location, and only the selected
// branch of a conditional generate is elaborated, so `if (c) begin : g`
// `end else begin : g` (legal, and common) never clashes here.  Checked on
// ELABORATED instances only; `genblkN` names are compiler-made.
void UhdmLint::checkScopeNameClashes(const module_inst* mi) {
  if (mi->Gen_scope_arrays() == nullptr || isInUhdmAllIterator()) return;
  const any* up = mi->VpiParent();
  if (!(mi->VpiTop() ||
        (up && up->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst)))
    return;
  std::set<std::string, std::less<>> names;
  auto add = [&](const any* o) {
    if (o && !o->VpiName().empty()) names.emplace(o->VpiName());
  };
  if (mi->Nets())
    for (const any* o : *mi->Nets()) add(o);
  if (mi->Array_nets())
    for (const any* o : *mi->Array_nets()) add(o);
  if (mi->Variables())
    for (const any* o : *mi->Variables()) add(o);
  if (mi->Parameters())
    for (const any* o : *mi->Parameters()) add(o);
  if (mi->Task_funcs())
    for (const any* o : *mi->Task_funcs()) add(o);
  if (mi->Modules())
    for (const any* o : *mi->Modules()) add(o);
  if (mi->Process())
    for (const process_stmt* ps : *mi->Process())
      if (const any* st = ps->Stmt())
        if (st->UhdmType() == UHDM_OBJECT_TYPE::uhdmbegin ||
            st->UhdmType() == UHDM_OBJECT_TYPE::uhdmnamed_begin ||
            st->UhdmType() == UHDM_OBJECT_TYPE::uhdmfork_stmt ||
            st->UhdmType() == UHDM_OBJECT_TYPE::uhdmnamed_fork)
          add(st);
  std::map<std::string, const gen_scope_array*, std::less<>> gens;
  for (const gen_scope_array* g : *mi->Gen_scope_arrays()) {
    std::string base(g->VpiName());
    if (const size_t b = base.find('['); b != std::string::npos) base.resize(b);
    if (base.empty() || base.rfind("genblk", 0) == 0) continue;
    bool clash = false;
    if (auto it = gens.find(base); it != gens.end()) {
      const gen_scope_array* o = it->second;
      clash =
          !(o->VpiFile() == g->VpiFile() && o->VpiLineNo() == g->VpiLineNo() &&
            o->VpiColumnNo() == g->VpiColumnNo());
    } else {
      gens.emplace(base, g);
      clash = names.count(base) != 0;
    }
    if (!clash) continue;
    if (!reportedCalls_
             .emplace(std::string(g->VpiFile()) + "#scope", g->VpiLineNo(),
                      g->VpiColumnNo())
             .second)
      continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_SCOPE_NAME_CLASH, base, g,
                                   nullptr);
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
  checkEnumAssign(object->Lhs(), object->Rhs(), object, false);
  checkContAssignLhsSelects(object->Lhs());
}

void UhdmLint::leaveAssignment(const assignment* object, vpiHandle handle) {
  checkUnpackedArrayAssign(object->Lhs(), object->Rhs(), object);
  {
    const int op = object->VpiOpType();
    checkEnumAssign(object->Lhs(), object->Rhs(), object,
                    op != 0 && op != vpiAssignmentOp);
  }
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

// The value of an enum constant (`UINT:3`, `INT:-1`, `HEX:F`, `BIN:10`),
// false when it is not a plain number.
static bool enumConstValue(std::string_view s, uint64_t& out) {
  const size_t colon = s.find(':');
  if (colon == std::string_view::npos) return false;
  const std::string_view kind = s.substr(0, colon);
  const std::string digits(s.substr(colon + 1));
  if (digits.empty() || digits.find_first_of("xXzZ?") != std::string::npos)
    return false;
  int base = 0;
  if (kind == "UINT" || kind == "DEC")
    base = 10;
  else if (kind == "INT")
    base = 10;
  else if (kind == "HEX")
    base = 16;
  else if (kind == "BIN")
    base = 2;
  else if (kind == "OCT")
    base = 8;
  else
    return false;
  if (digits.size() > (base == 2 ? 64u : 20u)) return false;
  errno = 0;
  char* end = nullptr;
  if (kind == "INT") {
    const long long v = std::strtoll(digits.c_str(), &end, 10);
    out = (uint64_t)v;
  } else {
    out = std::strtoull(digits.c_str(), &end, base);
  }
  return errno == 0 && end && *end == '\0';
}

void UhdmLint::leaveEnum_typespec(const enum_typespec* object,
                                  vpiHandle handle) {
  const typespec* baseType = nullptr;
  if (const ref_typespec* rt = object->Base_typespec()) {
    baseType = rt->Actual_typespec<typespec>();
  }
  if (!baseType) {
    // No explicit base: an `int` (LRM 6.19).
    checkEnumDuplicateValues(object, 32);
    return;
  }
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
  // At least one size digit: `'bx` is an UNSIZED based literal.
  static std::regex r("^[0-9]+'");
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
      // A SIZED literal (`4'h2`): regex_match had to match the whole text,
      // so it only ever accepted a bare `4'` and the check never ran.
      if (!std::regex_search(std::string(val), r)) continue;
      invalidValue = false;
      const uint64_t c_size = eval.size(c, invalidValue, object->Instance(),
                                        object->VpiParent(), true);
      if (!invalidValue && (baseSize != c_size)) {
        const std::string errMsg(c->VpiName());
        serializer_->GetErrorHandler()(ErrorType::UHDM_ENUM_CONST_SIZE_MISMATCH,
                                       errMsg, c, baseType);
      }
    }
    checkEnumDuplicateValues(object, baseSize);
  }
}

// LRM 6.19: the constants of one enum have UNIQUE values.  Compared within
// the base width; a value that is not a plain number (x / z, or not folded)
// is skipped.
void UhdmLint::checkEnumDuplicateValues(const enum_typespec* object,
                                        uint64_t baseSize) {
  if (object->Enum_consts() == nullptr || baseSize == 0 || baseSize > 64)
    return;
  const uint64_t mask =
      baseSize == 64 ? ~uint64_t(0) : ((uint64_t(1) << baseSize) - 1);
  // Only values known without folding are compared: Surelog can mis-fold a
  // constant EXPRESSION (`{RV_32I, RV_G | RV_C}` came out equal to
  // `{RV_32I, RV_G}`, rp32), and folds `2'dx` to 0.  A value is trusted when
  // its source text is a plain number literal (an implicit `+1` value
  // decompiles to one), or names another constant of the same enum.
  static const std::regex literal(
      "^([0-9_]+)?('[sS]?[bBoOdDhH])?[0-9a-fA-F_]+$");
  std::set<std::string_view> names;
  for (const enum_const* c : *object->Enum_consts()) names.insert(c->VpiName());
  std::map<uint64_t, const enum_const*> seen;
  for (const enum_const* c : *object->Enum_consts()) {
    const std::string text(c->VpiDecompile());
    if (!std::regex_match(text, literal) && !names.count(text)) continue;
    uint64_t v = 0;
    if (!enumConstValue(c->VpiValue(), v)) continue;
    v &= mask;
    auto [it, fresh] = seen.emplace(v, c);
    if (fresh) continue;
    const std::string errMsg = std::string(c->VpiName()) + " and " +
                               std::string(it->second->VpiName());
    serializer_->GetErrorHandler()(ErrorType::UHDM_ENUM_DUPLICATE_VALUE, errMsg,
                                   c, nullptr);
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

static bool inClass(const any* obj);

void UhdmLint::leavePart_select(const part_select* object, vpiHandle handle) {
  noteImplicitNetRef(object);
  const any* l = object->Left_range();
  const any* r = object->Right_range();
  if (nonConstantRef(l, true) || nonConstantRef(r, true)) {
    checkConstantExpr(l, true);
    checkConstantExpr(r, true);
    return;
  }
  // A variable is tolerated in BOTH bounds (`a[i+1:i]`, a constant width).
  // With one bound constant and the other variable, the WIDTH itself varies
  // (`a[15*m:0]`), which no reading of LRM 11.5.1 allows -- of a VECTOR.  A
  // slice of an unpacked array may have variable bounds when the array is a
  // queue or dynamic array (`q = q[0:n-1]`, UVM's m_uvm_queue_resize), so
  // only a packed base is checked, and never in a class.
  if (l == nullptr || r == nullptr) return;
  if (inClass(object)) return;
  if (const any* base = object->Actual_group()) {
    switch (base->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmarray_var:
      case UHDM_OBJECT_TYPE::uhdmarray_net:
        return;
      default:
        break;
    }
  }
  const any* vl = nonConstantRef(l, false);
  const any* vr = nonConstantRef(r, false);
  if ((vl == nullptr) != (vr == nullptr)) checkConstantExpr(vl ? l : r, false);
}

// LRM 10.3.1: the target of a continuous assignment may select a bit, a part
// or an element only with CONSTANT indices.  `assign arr[n] = x;` with a
// variable `n` has no fixed driver.
void UhdmLint::checkContAssignLhsSelects(const any* lhs) {
  if (lhs == nullptr) return;
  switch (lhs->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      const operation* op = (const operation*)lhs;
      if (op->Operands())
        for (const any* o : *op->Operands()) checkContAssignLhsSelects(o);
      return;
    }
    case UHDM_OBJECT_TYPE::uhdmbit_select:
      checkConstantExpr(((const bit_select*)lhs)->VpiIndex(), false);
      return;
    case UHDM_OBJECT_TYPE::uhdmvar_select: {
      const var_select* vs = (const var_select*)lhs;
      if (vs->Exprs())
        for (const any* e : *vs->Exprs()) {
          // A trailing part-select keeps its own (looser) check.
          if (e->UhdmType() == UHDM_OBJECT_TYPE::uhdmpart_select) continue;
          if (e->UhdmType() == UHDM_OBJECT_TYPE::uhdmindexed_part_select) {
            checkConstantExpr(((const indexed_part_select*)e)->Base_expr(),
                              false);
            continue;
          }
          checkConstantExpr(e, false);
        }
      return;
    }
    case UHDM_OBJECT_TYPE::uhdmindexed_part_select:
      checkConstantExpr(((const indexed_part_select*)lhs)->Base_expr(), false);
      return;
    default:
      return;
  }
}

static const any* elaboratedParamValue(const any* ex, const any* where);

void UhdmLint::leaveRange(const range* object, vpiHandle handle) {
  checkConstantExpr(object->Left_expr(), true);
  checkConstantExpr(object->Right_expr(), true);
  if (!checkUnknownBits(elaboratedParamValue(object->Left_expr(), object),
                        object))
    checkUnknownBits(elaboratedParamValue(object->Right_expr(), object),
                     object);
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
  // A fill literal ('0, '1, 'x, 'z) is not a size; an x/z value is an error.
  if (c->VpiSize() < 0) return;
  if (checkUnknownBits(c, where)) return;
  const int64_t val = eval.get_value(invalid, (const expr*)reduced);
  if (invalid) return;
  const int64_t size = val + offset;
  if (size < 0 || (size == 0 && !allowZero)) {
    serializer_->GetErrorHandler()(ErrorType::UHDM_NON_POSITIVE_VALUE,
                                   std::to_string(size), where, nullptr);
  }
}

// A size or a range bound must be a KNOWN value (LRM 6.9.1, 11.4.12.1,
// 6.24.1): a parameter holding x or z bits has none.  Only a folded constant
// (or the elaborated value of a parameter) is checked.
bool UhdmLint::checkUnknownBits(const any* ex, const any* where) {
  if (ex == nullptr || ex->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
    return false;
  const constant* c = (const constant*)ex;
  if (c->VpiSize() < 0) return false;  // '0 / '1 / 'x / 'z fill literal
  std::string_view v = c->VpiValue();
  if (!(v.rfind("BIN:", 0) == 0 || v.rfind("HEX:", 0) == 0 ||
        v.rfind("OCT:", 0) == 0))
    return false;
  if (v.find_first_of("xXzZ?", 4) == std::string_view::npos) return false;
  serializer_->GetErrorHandler()(ErrorType::UHDM_UNKNOWN_BITS_VALUE,
                                 std::string(v), where, nullptr);
  return true;
}

void UhdmLint::leaveIndexed_part_select(const indexed_part_select* object,
                                        vpiHandle handle) {
  noteImplicitNetRef(object);
  checkNonPositive(elaboratedParamValue(object->Width_expr(), object), object,
                   0, false);
}

// An element of a concatenation that contributes no bits: a replication
// with a (folded) count of 0, or a replication of such an empty
// concatenation.  Anything not KNOWN to be empty is not.
static bool emptyConcatElement(const any* e, const any* where) {
  if (e == nullptr || e->UhdmType() != UHDM_OBJECT_TYPE::uhdmoperation)
    return false;
  const operation* op = (const operation*)e;
  const VectorOfany* ops = op->Operands();
  if (ops == nullptr || ops->empty()) return false;
  if (op->VpiOpType() == vpiMultiConcatOp) {
    const any* count = elaboratedParamValue(ops->at(0), where);
    if (count && count->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
      std::string_view v = ((const constant*)count)->VpiValue();
      if (v == "UINT:0" || v == "INT:0" || v == "DEC:0" || v == "BIN:0" ||
          v == "HEX:0")
        return true;
    }
    return ops->size() == 2 && emptyConcatElement(ops->at(1), where);
  }
  if (op->VpiOpType() == vpiConcatOp) {
    for (const any* o : *ops)
      if (!emptyConcatElement(o, where)) return false;
    return true;
  }
  return false;
}

void UhdmLint::leaveOperation(const operation* object, vpiHandle handle) {
  const VectorOfany* ops = object->Operands();
  if (ops == nullptr || ops->empty()) return;
  switch (object->VpiOpType()) {
    case vpiAssignmentPatternOp:
      checkPatternCount(object);
      break;
    case vpiConcatOp: {
      // LRM 11.4.12.1: a zero replication count is legal only beside other
      // operands -- a concatenation made of nothing but empty replications
      // (`{{0{x}}}`, and the `{{0{s}}}` inside `{2{{0{s}}}}`) is an error.
      bool allEmpty = true;
      for (const any* o : *ops) {
        if (!emptyConcatElement(o, object)) {
          allEmpty = false;
          break;
        }
      }
      if (allEmpty && reportedCalls_
                          .emplace(std::string(object->VpiFile()) + "#concat",
                                   object->VpiLineNo(), object->VpiColumnNo())
                          .second)
        serializer_->GetErrorHandler()(ErrorType::UHDM_EMPTY_CONCATENATION,
                                       std::string(object->VpiDecompile()),
                                       object, nullptr);
      break;
    }
    case vpiMultiConcatOp:
      // A zero count is legal inside a larger concatenation; a negative one
      // never is.
      // `{rep{x}}`: the count may name a parameter.
      if (const any* count = elaboratedParamValue(ops->at(0), object))
        checkNonPositive(count, object, 0, true);
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
  checkPackageScopedRef(object);
}

void UhdmLint::leaveHier_path(const hier_path* object, vpiHandle handle) {
  checkImportedHierRef(object);
}

bool UhdmLint::reportOnce(const any* where, const char* tag) {
  return reportedOnce_
      .emplace(std::string(where->VpiFile()) + "#" + tag, where->VpiLineNo(),
               where->VpiColumnNo())
      .second;
}

// The name a declaration is known by, without any `P::` / `work@` scope.
static std::string_view leafName(std::string_view name) {
  const size_t pos = name.rfind("::");
  if (pos != std::string_view::npos) name.remove_prefix(pos + 2);
  return name;
}

// Calls `f` on every object package `p` declares (variables, nets,
// parameters, typedefs, enum constants, tasks and functions, classes, ...).
template <typename F>
static void forEachPackageMember(const package* p, F&& f) {
  // An anonymous enum (`enum {A, B} e;`) declares its constants through the
  // object it types.
  auto enumConsts = [&](const typespec* ts) {
    if (ts == nullptr || ts->UhdmType() != UHDM_OBJECT_TYPE::uhdmenum_typespec)
      return;
    if (const auto* consts = ((const enum_typespec*)ts)->Enum_consts()) {
      for (const enum_const* c : *consts) f(c);
    }
  };
  auto each = [&](const auto* vec) {
    if (vec == nullptr) return;
    for (const any* o : *vec) {
      f(o);
      const ref_typespec* rt = nullptr;
      if (const expr* e = any_cast<const expr*>(o)) {
        rt = e->Typespec();
      } else if (o->UhdmType() == UHDM_OBJECT_TYPE::uhdmparameter) {
        rt = ((const parameter*)o)->Typespec();
      }
      if (rt != nullptr) enumConsts(rt->Actual_typespec());
    }
  };
  each(p->Variables());
  each(p->Nets());
  each(p->Array_nets());
  each(p->Array_vars());
  each(p->Logic_vars());
  each(p->Parameters());
  each(p->Task_funcs());
  each(p->Class_defns());
  each(p->Let_decls());
  each(p->Named_events());
  each(p->Named_event_arrays());
  each(p->Property_decls());
  each(p->Sequence_decls());
  each(p->Spec_params());
  each(p->Virtual_interface_vars());
  each(p->Scopes());
  if (const auto* pas = p->Param_assigns()) {
    for (const param_assign* pa : *pas) {
      if (pa->Lhs()) f(pa->Lhs());
    }
  }
  if (const auto* tss = p->Typespecs()) {
    for (const typespec* ts : *tss) {
      f(ts);
      enumConsts(ts);
    }
  }
}

// Whether a package named `pkg` declares `member`.  A package that imports
// another one is also credited with that package's members: Surelog copies
// an export into the exporting package, and the rules that decide what is
// exported are checked elsewhere.  `known` is false when no package of that
// name exists.
// Both views: an alias typedef (`typedef base_t alias_t`) is only on the
// elaborated package.
static std::vector<const package*> allPackageViews(const design* d) {
  std::vector<const package*> res;
  if (d == nullptr) return res;
  for (const auto* vec : {d->AllPackages(), d->TopPackages()}) {
    if (vec == nullptr) continue;
    for (const package* p : *vec) res.push_back(p);
  }
  return res;
}

static bool packageHasMember(const design* d, std::string_view pkg,
                             std::string_view member, bool& known,
                             std::set<std::string, std::less<>>& visited) {
  if (!visited.emplace(pkg).second) return false;
  bool found = false;
  for (const package* p : allPackageViews(d)) {
    if (p->VpiName() != pkg) continue;
    known = true;
    std::vector<std::string_view> imported;
    forEachPackageMember(p, [&](const any* o) {
      if (o->UhdmType() == UHDM_OBJECT_TYPE::uhdmimport_typespec) {
        imported.push_back(o->VpiName());
        return;
      }
      if (leafName(o->VpiName()) == member) found = true;
    });
    if (found) return true;
    for (std::string_view other : imported) {
      bool dummy = false;
      if (packageHasMember(d, other, member, dummy, visited)) return true;
    }
  }
  return found;
}

void UhdmLint::checkPackageScopedRef(const ref_obj* ref) {
  if (ref->Actual_group() != nullptr) return;
  const std::string_view name = ref->VpiName();
  const size_t pos = name.find("::");
  if (pos == std::string_view::npos || pos == 0) return;
  const std::string_view pkg = name.substr(0, pos);
  const std::string_view member = name.substr(pos + 2);
  if (member.empty() || member.find("::") != std::string_view::npos) return;
  if (pkg == "$unit" || pkg == "std") return;
  bool known = false;
  std::set<std::string, std::less<>> visited;
  if (packageHasMember(design_, pkg, member, known, visited)) return;
  if (!known) return;
  if (!reportOnce(ref, "pkgmember")) return;
  serializer_->GetErrorHandler()(ErrorType::UHDM_NO_PACKAGE_MEMBER,
                                 std::string(name), ref, nullptr);
}

// `m.x` where `x` is visible in the instance `m` only because `m` imports it
// from a package (LRM 26.3: an imported name is not a member of the
// importing scope).  Surelog copies the imported declaration into the
// module, so the copy is recognised by sharing the package declaration's
// source position.
void UhdmLint::checkImportedHierRef(const hier_path* path) {
  const VectorOfany* elems = path->Path_elems();
  if (elems == nullptr || elems->size() < 2) return;
  const any* last = elems->back();
  if (last->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return;
  const any* before = (*elems)[elems->size() - 2];
  if (before->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return;
  const any* scope = ((const ref_obj*)before)->Actual_group();
  if (scope == nullptr ||
      (scope->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst &&
       scope->UhdmType() != UHDM_OBJECT_TYPE::uhdminterface_inst))
    return;
  const any* actual = ((const ref_obj*)last)->Actual_group();
  if (actual == nullptr || any_cast<const variables*>(actual) == nullptr)
    return;
  // A declaration of the instance's own definition is not a copy.
  if (actual->VpiFile() == scope->VpiFile() &&
      actual->VpiLineNo() >= scope->VpiLineNo() &&
      actual->VpiLineNo() <= scope->VpiEndLineNo())
    return;
  bool copied = false;
  for (const package* p : allPackageViews(design_)) {
    forEachPackageMember(p, [&](const any* o) {
      if (o->VpiName() == actual->VpiName() ||
          leafName(o->VpiName()) == actual->VpiName()) {
        if (o->VpiFile() == actual->VpiFile() &&
            o->VpiLineNo() == actual->VpiLineNo() &&
            o->VpiColumnNo() == actual->VpiColumnNo())
          copied = true;
      }
    });
    if (copied) break;
  }
  if (!copied || !reportOnce(path, "importhier")) return;
  serializer_->GetErrorHandler()(ErrorType::UHDM_IMPORTED_NAME_IN_HIER_PATH,
                                 std::string(path->VpiName()), path, nullptr);
}

void UhdmLint::leaveVar_select(const var_select* object, vpiHandle handle) {
  noteImplicitNetRef(object);
}

// ---------------------------------------------------------------------------
// Enum assignment compatibility (LRM 6.19.3): an enum variable may only be
// assigned an enum constant or a value of its own enum type; an integer, an
// expression or another enum type needs an explicit cast.

namespace {
enum class ValKind { Unknown, Enum, Other };
struct ValType {
  ValKind kind = ValKind::Unknown;
  const enum_typespec* ets = nullptr;
};
}  // namespace

static const typespec* declTypespec(const any* o) {
  if (o == nullptr) return nullptr;
  const ref_typespec* rt = nullptr;
  if (const expr* e = any_cast<const expr*>(o)) {
    rt = e->Typespec();
  } else if (const io_decl* d = any_cast<const io_decl*>(o)) {
    rt = d->Typespec();
  } else if (const typespec_member* m = any_cast<const typespec_member*>(o)) {
    rt = m->Typespec();
  }
  return rt ? rt->Actual_typespec() : nullptr;
}

// `s.f`: the member the last path element resolves to.
static const any* hierPathLeaf(const hier_path* hp) {
  if (hp->Path_elems() == nullptr || hp->Path_elems()->empty()) return nullptr;
  const any* last = hp->Path_elems()->back();
  if (last->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return nullptr;
  const any* actual = ((const ref_obj*)last)->Actual_group();
  if (actual && actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmtypespec_member)
    return actual;
  return nullptr;
}

static ValType typeOfTypespec(const typespec* ts) {
  if (ts == nullptr) return {};
  switch (ts->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmenum_typespec:
      return {ValKind::Enum, (const enum_typespec*)ts};
    case UHDM_OBJECT_TYPE::uhdmlogic_typespec:
    case UHDM_OBJECT_TYPE::uhdmbit_typespec:
    case UHDM_OBJECT_TYPE::uhdmint_typespec:
    case UHDM_OBJECT_TYPE::uhdminteger_typespec:
    case UHDM_OBJECT_TYPE::uhdmbyte_typespec:
    case UHDM_OBJECT_TYPE::uhdmshort_int_typespec:
    case UHDM_OBJECT_TYPE::uhdmlong_int_typespec:
      return {ValKind::Other, nullptr};
    default:
      return {};
  }
}

// The type of an element of the array `actual` is declared over.
static ValType elementType(const any* actual) {
  const typespec* ts = declTypespec(actual);
  if (ts && ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
    if (const ref_typespec* et = ((const array_typespec*)ts)->Elem_typespec())
      if (et->Actual_typespec()) return typeOfTypespec(et->Actual_typespec());
  }
  // Surelog often leaves the array typespec bare and types the elements.
  if (actual == nullptr) return {};
  if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var) {
    const array_var* av = (const array_var*)actual;
    if (av->Variables() && !av->Variables()->empty())
      return typeOfTypespec(declTypespec(av->Variables()->at(0)));
  } else if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net) {
    const array_net* an = (const array_net*)actual;
    if (an->Nets() && !an->Nets()->empty())
      return typeOfTypespec(declTypespec(an->Nets()->at(0)));
  }
  return {};
}

// The type of the object a write lands in (only shapes known exactly).
static ValType targetType(const any* lhs) {
  if (lhs == nullptr) return {};
  switch (lhs->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmref_obj:
      return typeOfTypespec(
          declTypespec(((const ref_obj*)lhs)->Actual_group()));
    case UHDM_OBJECT_TYPE::uhdmbit_select:
      return elementType(((const bit_select*)lhs)->Actual_group());
    case UHDM_OBJECT_TYPE::uhdmhier_path:
      return typeOfTypespec(declTypespec(hierPathLeaf((const hier_path*)lhs)));
    default:
      return {};
  }
}

// The type of a value.  Unknown whenever it cannot be told for sure.
static ValType valueType(const any* rhs) {
  if (rhs == nullptr) return {};
  switch (rhs->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmconstant: {
      const constant* c = (const constant*)rhs;
      if (const ref_typespec* rt = c->Typespec()) {
        if (rt->Actual_typespec()) return typeOfTypespec(rt->Actual_typespec());
      }
      // Surelog folds a NAMED constant (`pkg::IDLE`, a parameter) into a bare
      // constant too.  Only a constant whose source span is exactly its own
      // text was written as a literal.
      const std::string_view text = c->VpiDecompile();
      if (!text.empty() && c->VpiLineNo() == c->VpiEndLineNo() &&
          c->VpiEndColumnNo() > c->VpiColumnNo() &&
          c->VpiEndColumnNo() - c->VpiColumnNo() == text.size())
        return {ValKind::Other, nullptr};
      return {};
    }
    case UHDM_OBJECT_TYPE::uhdmref_obj: {
      const any* actual = ((const ref_obj*)rhs)->Actual_group();
      if (actual == nullptr) return {};
      if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_const) {
        const any* p = actual->VpiParent();
        if (p && p->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_typespec)
          return {ValKind::Enum, (const enum_typespec*)p};
        return {};
      }
      return typeOfTypespec(declTypespec(actual));
    }
    case UHDM_OBJECT_TYPE::uhdmbit_select:
      return elementType(((const bit_select*)rhs)->Actual_group());
    case UHDM_OBJECT_TYPE::uhdmhier_path:
      return typeOfTypespec(declTypespec(hierPathLeaf((const hier_path*)rhs)));
    case UHDM_OBJECT_TYPE::uhdmfunc_call: {
      const function* f = ((const func_call*)rhs)->Function();
      if (f == nullptr || f->Return() == nullptr) return {};
      return typeOfTypespec(declTypespec(f->Return()));
    }
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      const operation* op = (const operation*)rhs;
      const int t = op->VpiOpType();
      if (t == vpiCastOp) {
        if (const ref_typespec* rt = op->Typespec())
          return typeOfTypespec(rt->Actual_typespec());
        return {};
      }
      // Arithmetic, bitwise, logical, relational, shift and concatenation
      // operators all yield a plain integral value (LRM 6.19.4).
      if ((t >= vpiMinusOp && t <= vpiBitXNorOp) || t == vpiConcatOp ||
          t == vpiMultiConcatOp || t == vpiArithLShiftOp ||
          t == vpiArithRShiftOp || t == vpiPowerOp)
        return {ValKind::Other, nullptr};
      return {};
    }
    default:
      return {};
  }
}

// Two enum typespecs are the same type when they are the same declaration.
// Surelog clones a typespec per instance and per package import, and a
// typedef alias gets its own name and location, so compare the member lists:
// identical constants in the same order.  (Two distinct enums declared with
// identical members then compare equal -- a missed error, never a false one.)
static bool sameEnum(const enum_typespec* a, const enum_typespec* b) {
  if (a == b) return true;
  const auto* ca = a->Enum_consts();
  const auto* cb = b->Enum_consts();
  if (ca == nullptr || cb == nullptr || ca->size() != cb->size()) return false;
  for (size_t i = 0; i < ca->size(); ++i) {
    if (ca->at(i)->VpiName() != cb->at(i)->VpiName()) return false;
  }
  return true;
}

void UhdmLint::checkEnumValue(const enum_typespec* target, const any* rhs,
                              const any* where) {
  if (rhs == nullptr) return;
  // `c ? A : B`: each arm on its own.
  if (rhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
    const operation* op = (const operation*)rhs;
    if (op->VpiOpType() == vpiConditionOp && op->Operands() &&
        op->Operands()->size() == 3) {
      checkEnumValue(target, op->Operands()->at(1), where);
      checkEnumValue(target, op->Operands()->at(2), where);
      return;
    }
  }
  const ValType v = valueType(rhs);
  if (v.kind == ValKind::Unknown) return;
  if (v.kind == ValKind::Enum && sameEnum(target, v.ets)) return;
  // A constant NAMED like a member of the target: Surelog can bind a
  // generate-block enum constant to a same-named one of the parent.
  if (v.kind == ValKind::Enum &&
      rhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj &&
      target->Enum_consts()) {
    const std::string_view name = ((const ref_obj*)rhs)->VpiName();
    for (const enum_const* c : *target->Enum_consts())
      if (c->VpiName() == name) return;
  }
  if (inClass(where)) return;
  if (!reportedEnumAssigns_
           .emplace(std::string(rhs->VpiFile()), rhs->VpiLineNo(),
                    rhs->VpiColumnNo())
           .second)
    return;
  const std::string errMsg(target->VpiName().empty() ? "<anonymous>"
                                                     : target->VpiName());
  serializer_->GetErrorHandler()(ErrorType::UHDM_ENUM_ASSIGN_INCOMPATIBLE,
                                 errMsg, rhs, nullptr);
}

void UhdmLint::checkEnumAssign(const any* lhs, const any* rhs, const any* where,
                               bool compound) {
  const ValType t = targetType(lhs);
  if (t.kind != ValKind::Enum) return;
  // `e += 1` is `e = e + 1`: an integral value (LRM 6.19.4).
  if (compound) {
    if (inClass(where)) return;
    if (!reportedEnumAssigns_
             .emplace(std::string(where->VpiFile()), where->VpiLineNo(),
                      where->VpiColumnNo())
             .second)
      return;
    serializer_->GetErrorHandler()(ErrorType::UHDM_ENUM_ASSIGN_INCOMPATIBLE,
                                   std::string(t.ets->VpiName()), where,
                                   nullptr);
    return;
  }
  checkEnumValue(t.ets, rhs, where);
}

void UhdmLint::checkEnumCallArgs(const tf_call* call, const task_func* tf) {
  if (tf == nullptr || call->Tf_call_args() == nullptr ||
      tf->Io_decls() == nullptr)
    return;
  const auto& args = *call->Tf_call_args();
  const auto& ios = *tf->Io_decls();
  // Positional arguments only: a named or defaulted one does not line up.
  if (args.size() > ios.size()) return;
  for (size_t i = 0; i < args.size(); ++i) {
    if (ios[i]->VpiDirection() != vpiInput) continue;
    const ValType t = typeOfTypespec(declTypespec(ios[i]));
    if (t.kind == ValKind::Enum) checkEnumValue(t.ets, args[i], call);
  }
}

void UhdmLint::leaveReturn_stmt(const return_stmt* object, vpiHandle handle) {
  const any* p = object->VpiParent();
  while (p && p->UhdmType() != UHDM_OBJECT_TYPE::uhdmfunction)
    p = p->VpiParent();
  if (p == nullptr) return;
  const function* f = (const function*)p;
  if (f->Return() == nullptr) return;
  const ValType t = typeOfTypespec(declTypespec(f->Return()));
  if (t.kind == ValKind::Enum)
    checkEnumValue(t.ets, object->VpiCondition(), object);
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

// LRM 13.5: a call passes at most one argument per formal, and every formal
// without a default must get one.  Positional calls only are counted.
void UhdmLint::checkTfArgCount(const tf_call* call, const task_func* tf) {
  if (tf == nullptr || inClass(call)) return;
  const size_t nargs = call->Tf_call_args() ? call->Tf_call_args()->size() : 0;
  const size_t nformals = tf->Io_decls() ? tf->Io_decls()->size() : 0;
  bool bad = nargs > nformals;
  if (!bad) {
    for (size_t i = nargs; i < nformals; ++i) {
      if (tf->Io_decls()->at(i)->Expr() == nullptr) {
        bad = true;
        break;
      }
    }
  }
  if (!bad) return;
  if (!reportedCalls_
           .emplace(std::string(call->VpiFile()) + "#argc", call->VpiLineNo(),
                    call->VpiColumnNo())
           .second)
    return;
  const std::string errMsg = std::string(call->VpiName()) + " (expected " +
                             std::to_string(nformals) + ", got " +
                             std::to_string(nargs) + ")";
  serializer_->GetErrorHandler()(ErrorType::UHDM_TF_ARG_COUNT, errMsg, call,
                                 nullptr);
}

void UhdmLint::leaveFunc_call(const func_call* object, vpiHandle handle) {
  reportUndefinedTaskFunc(object, object->Function() != nullptr);
  checkEnumCallArgs(object, object->Function());
  checkTfArgCount(object, object->Function());
}

void UhdmLint::leaveTask_call(const task_call* object, vpiHandle handle) {
  // LRM 13.3: a task has no value, so it cannot be called inside an
  // expression (`y = P::t(10)`).
  if (const any* parent = object->VpiParent()) {
    bool inExpr = false;
    switch (parent->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmoperation:
        inExpr = true;
        break;
      case UHDM_OBJECT_TYPE::uhdmassignment:
        inExpr = ((const assignment*)parent)->Rhs() == object;
        break;
      case UHDM_OBJECT_TYPE::uhdmcont_assign:
        inExpr = ((const cont_assign*)parent)->Rhs() == object;
        break;
      default:
        break;
    }
    if (inExpr && object->Task() != nullptr && reportOnce(object, "taskexpr"))
      serializer_->GetErrorHandler()(ErrorType::UHDM_TASK_CALLED_AS_FUNCTION,
                                     std::string(object->VpiName()), object,
                                     nullptr);
  }
  reportUndefinedTaskFunc(object, object->Task() != nullptr);
  checkEnumCallArgs(object, object->Task());
  checkTfArgCount(object, object->Task());
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
  reportedOnce_.clear();
  declaredNames_.clear();
  elaboratedDefs_.clear();
}

// LRM 10.9.1: a POSITIONAL assignment pattern has exactly one element per
// struct member, or per element of the outermost packed dimension.  Only a
// target whose type is known exactly is checked: the variable a pattern
// initialises, or the whole-variable left-hand side it is assigned to -- and
// only a struct / logic / bit VARIABLE.  Surelog types a net, and an unpacked
// or packed-of-struct array, by its ELEMENT (`logic [7:0] arr [4]` is a
// logic_net of `logic [7:0]` in the definition view), which would count the
// wrong dimension.
static const typespec* varTypespec(const any* v) {
  if (v == nullptr) return nullptr;
  // The ELEMENT variable of an array carries the array's initialiser.
  if (const any* p = v->VpiParent()) {
    if (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var ||
        p->UhdmType() == UHDM_OBJECT_TYPE::uhdmpacked_array_var ||
        p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net ||
        p->UhdmType() == UHDM_OBJECT_TYPE::uhdmpacked_array_net)
      return nullptr;
  }
  switch (v->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmstruct_var:
    case UHDM_OBJECT_TYPE::uhdmlogic_var:
    case UHDM_OBJECT_TYPE::uhdmbit_var:
      return declTypespec(v);
    case UHDM_OBJECT_TYPE::uhdmstruct_net: {
      // A struct-typed net is typed exactly (its element would be a
      // packed_array_net / array_net, guarded above).
      const typespec* ts = declTypespec(v);
      if (ts && ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_typespec)
        return ts;
      return nullptr;
    }
    default:
      return nullptr;
  }
}

static const typespec* patternTarget(const operation* op) {
  const any* p = op->VpiParent();
  if (p == nullptr) return nullptr;
  const any* lhs = nullptr;
  switch (p->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmassignment:
      if (((const assignment*)p)->Rhs() != op) return nullptr;
      lhs = ((const assignment*)p)->Lhs();
      break;
    case UHDM_OBJECT_TYPE::uhdmcont_assign:
      if (((const cont_assign*)p)->Rhs() != op) return nullptr;
      lhs = ((const cont_assign*)p)->Lhs();
      break;
    default:
      if (const variables* v = any_cast<const variables*>(p)) {
        if (v->Expr() != op) return nullptr;
        return varTypespec(v);
      }
      return nullptr;
  }
  if (lhs == nullptr) return nullptr;
  // A net declaration's initialiser (`pair_t p = '{...};`) is a cont_assign
  // whose left-hand side is the net itself.
  if (lhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) {
    // Surelog also emits that initialiser for an UNPACKED ARRAY of structs
    // (`s_t a [2] = '{...};`), on a struct_net it creates for it: skip a
    // name the enclosing module declares as an array.
    const std::string_view name = lhs->VpiName();
    for (const any* m = p->VpiParent(); m; m = m->VpiParent()) {
      if (m->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst) continue;
      const module_inst* mi = (const module_inst*)m;
      if (mi->Array_nets())
        for (const array_net* an : *mi->Array_nets())
          if (an->VpiName() == name) return nullptr;
      if (mi->Variables())
        for (const variables* v : *mi->Variables())
          if (v->VpiName() == name &&
              v->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var)
            return nullptr;
      break;
    }
    return varTypespec(lhs);
  }
  return varTypespec(((const ref_obj*)lhs)->Actual_group());
}

static bool constRangeSize(const range* r, uint64_t& size) {
  if (r == nullptr) return false;
  const any* l = r->Left_expr();
  const any* rr = r->Right_expr();
  if (l == nullptr || rr == nullptr ||
      l->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant ||
      rr->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
    return false;
  ExprEval eval;
  bool invalid = false;
  const int64_t lv = eval.get_value(invalid, (const expr*)l);
  if (invalid) return false;
  const int64_t rv = eval.get_value(invalid, (const expr*)rr);
  if (invalid) return false;
  size = (lv > rv ? lv - rv : rv - lv) + 1;
  return true;
}

void UhdmLint::checkPatternCount(const operation* pattern) {
  const VectorOfany* ops = pattern->Operands();
  if (ops == nullptr || ops->empty() || inClass(pattern)) return;
  // `'{a: x, default: y}` is not positional.
  for (const any* o : *ops)
    if (o->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) return;
  const typespec* ts = patternTarget(pattern);
  if (ts == nullptr) return;
  uint64_t expected = 0;
  switch (ts->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmstruct_typespec: {
      const auto* m = ((const struct_typespec*)ts)->Members();
      if (m == nullptr || m->empty()) return;
      expected = m->size();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmlogic_typespec:
    case UHDM_OBJECT_TYPE::uhdmbit_typespec: {
      const VectorOfrange* rs =
          ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_typespec
              ? ((const logic_typespec*)ts)->Ranges()
              : ((const bit_typespec*)ts)->Ranges();
      if (rs == nullptr || rs->empty()) return;
      if (!constRangeSize(rs->at(0), expected)) return;
      break;
    }
    default:
      return;
  }
  if (expected == ops->size()) return;
  if (!reportedCalls_
           .emplace(std::string(pattern->VpiFile()) + "#pattern",
                    pattern->VpiLineNo(), pattern->VpiColumnNo())
           .second)
    return;
  const std::string errMsg = std::to_string(ops->size()) + " elements, " +
                             std::to_string(expected) + " required";
  serializer_->GetErrorHandler()(ErrorType::UHDM_PATTERN_ELEMENT_COUNT, errMsg,
                                 pattern, nullptr);
}

}  // namespace UHDM
