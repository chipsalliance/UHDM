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

static std::string ownerDefName(const any* obj);

// A task or function argument declared over a type name that never
// resolved: Surelog keeps an unsupported_typespec named after it.  Class
// methods are left alone (their types are bound by a separate pass), and
// so are unelaborated modules (as for the other undeclared-name checks).
void UhdmLint::checkTfArgTypes(const task_func* tf) {
  if (tf->Io_decls() == nullptr) return;
  for (const any* p = tf->VpiParent(); p != nullptr; p = p->VpiParent()) {
    if (p->UhdmType() == uhdmclass_defn) return;
  }
  for (const io_decl* io : *tf->Io_decls()) {
    const ref_typespec* rt = io->Typespec();
    if (rt == nullptr) continue;
    const unsupported_typespec* ut =
        rt->Actual_typespec<unsupported_typespec>();
    if (ut == nullptr) continue;
    const std::string_view name = ut->VpiName();
    if (name.empty() || name.find(':') != std::string_view::npos ||
        name.find('.') != std::string_view::npos)
      continue;
    unresolvedArgTypes_.emplace_back(ut, ownerDefName(tf));
  }
}

void UhdmLint::leaveTask(const task* object, vpiHandle handle) {
  checkTfArgTypes(object);
}

void UhdmLint::leaveFunction(const function* object, vpiHandle handle) {
  checkTfArgTypes(object);
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

// LRM 6.19: an enum constant is declared in the scope of its enum, so in a
// class it may not repeat the name of a typedef, property, method or parameter
// of that class.
void UhdmLint::leaveClass_defn(const class_defn* object, vpiHandle handle) {
  std::map<std::string, const enum_const*, std::less<>> consts;
  std::set<std::string, std::less<>> enumTypes;
  auto addEnum = [&](const typespec* ts) {
    if (ts == nullptr || ts->UhdmType() != UHDM_OBJECT_TYPE::uhdmenum_typespec)
      return;
    const enum_typespec* et = (const enum_typespec*)ts;
    if (!et->VpiName().empty()) enumTypes.emplace(et->VpiName());
    if (et->Enum_consts())
      for (const enum_const* c : *et->Enum_consts())
        consts.emplace(c->VpiName(), c);
  };
  if (object->Variables())
    for (const variables* v : *object->Variables())
      if (const ref_typespec* rt = v->Typespec())
        addEnum(rt->Actual_typespec());
  if (object->Typespecs())
    for (const typespec* ts : *object->Typespecs()) addEnum(ts);
  if (consts.empty()) return;
  std::vector<const any*> others;
  if (object->Variables())
    for (const any* o : *object->Variables()) others.push_back(o);
  if (object->Typespecs())
    for (const typespec* ts : *object->Typespecs())
      if (ts->UhdmType() != UHDM_OBJECT_TYPE::uhdmenum_typespec)
        others.push_back(ts);
  if (object->Task_funcs())
    for (const any* o : *object->Task_funcs()) others.push_back(o);
  if (object->Parameters())
    for (const any* o : *object->Parameters()) others.push_back(o);
  for (const any* o : others) {
    auto it = consts.find(o->VpiName());
    if (it == consts.end()) continue;
    const enum_const* c = it->second;
    if (!reportedCalls_
             .emplace(std::string(c->VpiFile()) + "#enumname", c->VpiLineNo(),
                      c->VpiColumnNo())
             .second)
      continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_ENUM_NAME_REDECLARED,
                                   std::string(c->VpiName()), o, nullptr);
  }
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

// Any unpacked array (static, dynamic, associative or queue) an expression
// names as a whole, or nullptr.
static const any* anyUnpackedArray(const any* actual) {
  if (actual == nullptr) return nullptr;
  if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net ||
      actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var)
    return actual;
  const any* p = actual->VpiParent();
  if (p && (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net ||
            p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var))
    return p;
  return nullptr;
}

static const VectorOfrange* unpackedRanges(const any* arr) {
  if (arr->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net)
    return ((const array_net*)arr)->Ranges();
  return ((const array_var*)arr)->Ranges();
}

static const any* unpackedElement(const any* arr) {
  if (arr->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net) {
    const array_net* an = (const array_net*)arr;
    if (an->Nets() && !an->Nets()->empty()) return an->Nets()->at(0);
    return nullptr;
  }
  const array_var* av = (const array_var*)arr;
  if (av->Variables() && !av->Variables()->empty())
    return av->Variables()->at(0);
  return nullptr;
}

// The constant size of every unpacked dimension of a static array.
static bool unpackedDims(const any* arr, design* d,
                         std::vector<uint64_t>& dims) {
  const VectorOfrange* ranges = unpackedRanges(arr);
  if (ranges == nullptr || ranges->empty()) return false;
  ExprEval eval;
  eval.setDesign(d);
  for (const range* r : *ranges) {
    if (!r->Left_expr() || !r->Right_expr()) return false;
    if (r->Left_expr()->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant ||
        r->Right_expr()->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
      return false;
    bool invalid = false;
    const int64_t l = eval.get_value(invalid, (const expr*)r->Left_expr());
    const int64_t rr = eval.get_value(invalid, (const expr*)r->Right_expr());
    if (invalid) return false;
    dims.push_back((l > rr ? l - rr : rr - l) + 1);
  }
  return true;
}

// Element count and element width of a 1-D static unpacked array, when both
// are known constants.
static bool unpackedShape(const any* arr, Serializer* s, design* d,
                          uint64_t& count, uint64_t& width) {
  std::vector<uint64_t> dims;
  const any* elem = unpackedElement(arr);
  if (elem == nullptr || !unpackedDims(arr, d, dims) || dims.size() != 1)
    return false;
  ExprEval eval;
  eval.setDesign(d);
  bool invalid = false;
  count = dims[0];
  width = eval.size(elem, invalid, nullptr, nullptr, true);
  return !invalid && width > 0;
}

// What decides whether two unpacked-array element types are equivalent
// (LRM 6.22.2): integral types match on width, 2/4-state and signedness; an
// enum only matches itself; real, shortreal and string only themselves.
// Kind::Unknown (structs, classes, ...) is never compared.
struct ElemType {
  enum class Kind { Unknown, Integral, Enum, Real, ShortReal, String };
  Kind kind = Kind::Unknown;
  int state = 0;  // 2, 4, or 0 when unknown
  bool isSigned = false;
  uint64_t width = 0;
  const typespec* decl = nullptr;
};

static ElemType elemTypeOf(const any* arr, const any* elem, design* d) {
  ElemType t;
  if (elem == nullptr) return t;
  const expr* e = any_cast<const expr*>(elem);
  if (e == nullptr || e->Typespec() == nullptr) return t;
  const typespec* ts = e->Typespec()->Actual_typespec();
  if (ts == nullptr) return t;
  bool tsSigned = false;
  switch (ts->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmlogic_typespec:
      t.kind = ElemType::Kind::Integral;
      t.state = 4;
      tsSigned = ((const logic_typespec*)ts)->VpiSigned();
      break;
    case UHDM_OBJECT_TYPE::uhdmbit_typespec:
      t.kind = ElemType::Kind::Integral;
      t.state = 2;
      tsSigned = ((const bit_typespec*)ts)->VpiSigned();
      break;
    case UHDM_OBJECT_TYPE::uhdmint_typespec:
      t.kind = ElemType::Kind::Integral;
      t.state = 2;
      tsSigned = ((const int_typespec*)ts)->VpiSigned();
      t.width = 32;
      break;
    case UHDM_OBJECT_TYPE::uhdminteger_typespec:
      t.kind = ElemType::Kind::Integral;
      t.state = 4;
      tsSigned = ((const integer_typespec*)ts)->VpiSigned();
      t.width = 32;
      break;
    case UHDM_OBJECT_TYPE::uhdmshort_int_typespec:
      t.kind = ElemType::Kind::Integral;
      t.state = 2;
      tsSigned = ((const short_int_typespec*)ts)->VpiSigned();
      t.width = 16;
      break;
    case UHDM_OBJECT_TYPE::uhdmbyte_typespec:
      t.kind = ElemType::Kind::Integral;
      t.state = 2;
      tsSigned = ((const byte_typespec*)ts)->VpiSigned();
      t.width = 8;
      break;
    case UHDM_OBJECT_TYPE::uhdmlong_int_typespec:
      t.kind = ElemType::Kind::Integral;
      t.state = 2;
      tsSigned = ((const long_int_typespec*)ts)->VpiSigned();
      t.width = 64;
      break;
    case UHDM_OBJECT_TYPE::uhdmreal_typespec:
      t.kind = ElemType::Kind::Real;
      return t;
    case UHDM_OBJECT_TYPE::uhdmshort_real_typespec:
      t.kind = ElemType::Kind::ShortReal;
      return t;
    case UHDM_OBJECT_TYPE::uhdmstring_typespec:
      t.kind = ElemType::Kind::String;
      return t;
    case UHDM_OBJECT_TYPE::uhdmenum_typespec:
      t.kind = ElemType::Kind::Enum;
      t.decl = ts;
      return t;
    default:
      return t;
  }
  bool objSigned = false;
  if (const variables* v = any_cast<const variables*>(elem))
    objSigned = v->VpiSigned();
  else if (const nets* n = any_cast<const nets*>(elem))
    objSigned = n->VpiSigned();
  if (const variables* v = any_cast<const variables*>(arr))
    objSigned = objSigned || v->VpiSigned();
  t.isSigned = tsSigned || objSigned;
  if (t.width != 0) return t;
  ExprEval eval;
  eval.setDesign(d);
  bool invalid = false;
  t.width = eval.size(elem, invalid, nullptr, nullptr, true);
  if (invalid) t.width = 0;
  return t;
}

static bool elemTypesDiffer(const ElemType& a, const ElemType& b) {
  if (a.kind == ElemType::Kind::Unknown || b.kind == ElemType::Kind::Unknown)
    return false;
  if (a.kind != b.kind) return true;
  switch (a.kind) {
    case ElemType::Kind::Enum:
      return !sameTypeDecl(a.decl, b.decl);
    case ElemType::Kind::Integral:
      if (a.width && b.width && a.width != b.width) return true;
      if (a.state && b.state && a.state != b.state) return true;
      return a.isSigned != b.isSigned;
    default:
      return false;
  }
}

// A select that yields ONE element of a 1-D unpacked array (`x[0]`).
static bool isUnpackedElementSelect(const any* e) {
  if (e == nullptr || e->UhdmType() != UHDM_OBJECT_TYPE::uhdmbit_select)
    return false;
  const any* arr = anyUnpackedArray(((const bit_select*)e)->Actual_group());
  if (arr == nullptr) return false;
  const VectorOfrange* ranges = unpackedRanges(arr);
  return ranges != nullptr && ranges->size() == 1;
}

static const any* nonConstantRef(const any* e, bool netsOnly);

// A positional assignment pattern for a static unpacked array names one
// item per element of the outer dimension (LRM 10.9.1): `ms_t ms[1:0] =
// '{0, 0, 1, 1}` gives four items to two struct elements.
void UhdmLint::checkArrayPatternCount(const any* arr, const any* rhs,
                                      const any* where) {
  if (arr == nullptr || rhs == nullptr ||
      rhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmoperation)
    return;
  const operation* op = (const operation*)rhs;
  if (op->VpiOpType() != vpiAssignmentPatternOp || op->Operands() == nullptr)
    return;
  for (const any* o : *op->Operands())
    if (o->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) return;
  std::vector<uint64_t> dims;
  if (!unpackedDims(arr, design_, dims)) return;
  if (dims[0] != op->Operands()->size() && reportOnce(rhs, "array-pattern"))
    serializer_->GetErrorHandler()(
        ErrorType::UHDM_ILLEGAL_UNPACKED_ARRAY_ASSIGN,
        std::string(arr->VpiName()), where, nullptr);
}

void UhdmLint::leaveArray_var(const array_var* object, vpiHandle handle) {
  checkQueueBound(object->Ranges());
  if (wholeUnpackedArray(object) != nullptr)
    checkArrayPatternCount(object, object->Expr(), object->Expr());
}

// A bounded queue `q[$:N]` (LRM 7.10): N is a constant, known, non-negative
// expression.  Surelog keeps it as a range whose left bound is `$`.
void UhdmLint::leaveArray_net(const array_net* object, vpiHandle handle) {
  checkQueueBound(object->Ranges());
}

void UhdmLint::checkQueueBound(const VectorOfrange* ranges) {
  if (isInUhdmAllIterator() || ranges == nullptr) return;
  for (const range* r : *ranges) {
    const constant* l = any_cast<const constant*>(r->Left_expr());
    if (l == nullptr || l->VpiDecompile() != "$") continue;
    const any* right = r->Right_expr();
    if (right == nullptr) continue;
    if (const constant* c = any_cast<const constant*>(right)) {
      std::string_view v = c->VpiValue();
      if (c->VpiSize() < 0) {
        // A fill literal: `'x` / `'z` has no value, `'0` / `'1` is fine.
        if (v.find_first_of("xXzZ", 4) != std::string_view::npos &&
            reportOnce(right, "queue-bound"))
          serializer_->GetErrorHandler()(ErrorType::UHDM_UNKNOWN_BITS_VALUE,
                                         std::string(v), right, nullptr);
        continue;
      }
      if (reportOnce(right, "queue-bound"))
        checkNonPositive(right, right, 0, true);
    } else if (nonConstantRef(right, false) && reportOnce(right, "queue-bound")) {
      checkConstantExpr(right, false);
    }
  }
}

// Assigning a whole unpacked array (LRM 7.6): the right-hand side must be an
// unpacked array of the same shape, or an assignment pattern / unpacked
// array concatenation.  `assign x = y;` with y a vector, an element, an
// expression or an array of another element width or count was accepted.
void UhdmLint::checkUnpackedArrayAssign(const any* lhs, const any* rhs,
                                        const any* where) {
  if (lhs == nullptr || rhs == nullptr) return;
  // A slice of a 1-D unpacked array (`out[3:0] = in[j]`) takes an array,
  // never a single element or value.
  if (lhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmpart_select) {
    const part_select* ps = (const part_select*)lhs;
    const any* arr = anyUnpackedArray(ps->Actual_group());
    if (arr == nullptr) return;
    const VectorOfrange* ranges = unpackedRanges(arr);
    if (ranges == nullptr || ranges->size() != 1) return;
    if ((isUnpackedElementSelect(rhs) ||
         rhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) &&
        reportOnce(where, "array-assign"))
      serializer_->GetErrorHandler()(
          ErrorType::UHDM_ILLEGAL_UNPACKED_ARRAY_ASSIGN,
          std::string(ps->VpiName()), where, nullptr);
    return;
  }
  // A WHOLE-array target only: bit_select / var_select derive from ref_obj
  // and must not match (`mem[sp] <= d` is an element write).
  if (lhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return;
  const ref_obj* lref = (const ref_obj*)lhs;
  const any* lany = anyUnpackedArray(lref->Actual_group());
  if (lany == nullptr) return;
  const any* larr = wholeUnpackedArray(lref->Actual_group());
  if (larr != nullptr) checkArrayPatternCount(larr, rhs, where);
  bool bad = false;
  uint64_t lcount = 0, lwidth = 0;
  if (larr != nullptr &&
      unpackedShape(larr, serializer_, design_, lcount, lwidth)) {
    switch (rhs->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmconstant:
        bad = true;
        break;
      case UHDM_OBJECT_TYPE::uhdmoperation: {
        const int op = ((const operation*)rhs)->VpiOpType();
        // Patterns, concatenations, conditionals and casts may legally build
        // an unpacked array; any other operator yields a single value.
        if (op != vpiAssignmentPatternOp && op != vpiMultiAssignmentPatternOp &&
            op != vpiConcatOp && op != vpiMultiConcatOp &&
            op != vpiConditionOp && op != vpiCastOp)
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
              bad = actual->VpiParent() == nullptr ||
                    anyUnpackedArray(actual) == nullptr;
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
  }
  if (bad) {
    const std::string errMsg(lref->VpiName());
    serializer_->GetErrorHandler()(
        ErrorType::UHDM_ILLEGAL_UNPACKED_ARRAY_ASSIGN, errMsg, where, nullptr);
    return;
  }
  // Array to array of any kind: the unpacked shapes of two static arrays
  // must match dimension by dimension, and the element types must be
  // equivalent (LRM 7.6); `x = y[0]` assigns one element to an array.
  bool bad2 = isUnpackedElementSelect(rhs);
  if (!bad2 && rhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj) {
    const any* rany = anyUnpackedArray(((const ref_obj*)rhs)->Actual_group());
    if (rany != nullptr && rany != lany) {
      const any* rarr =
          wholeUnpackedArray(((const ref_obj*)rhs)->Actual_group());
      std::vector<uint64_t> ld, rd;
      if (larr != nullptr && rarr != nullptr &&
          unpackedDims(larr, design_, ld) && unpackedDims(rarr, design_, rd) &&
          ld != rd)
        bad2 = true;
      if (!bad2)
        bad2 =
            elemTypesDiffer(elemTypeOf(lany, unpackedElement(lany), design_),
                            elemTypeOf(rany, unpackedElement(rany), design_));
    }
  }
  if (bad2 && reportOnce(where, "array-assign"))
    serializer_->GetErrorHandler()(
        ErrorType::UHDM_ILLEGAL_UNPACKED_ARRAY_ASSIGN,
        std::string(lref->VpiName()), where, nullptr);
}

void UhdmLint::leaveCont_assign(const cont_assign* object, vpiHandle handle) {
  noteContDrivenVariable(object);
  checkUnpackedArrayAssign(object->Lhs(), object->Rhs(), object);
  checkEnumAssign(object->Lhs(), object->Rhs(), object, false);
  checkContAssignLhsSelects(object->Lhs());
  // A parameter is a constant: it cannot be the target of an assign.
  if (isInUhdmAllIterator()) return;
  if (const ref_obj* lhs = object->Lhs<ref_obj>()) {
    if (const any* actual = lhs->Actual_group()) {
      if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmparameter &&
          reportOnce(lhs, "not-assignable")) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_NOT_ASSIGNABLE,
                                       std::string(lhs->VpiName()), lhs,
                                       nullptr);
      }
    }
  }
}

// A `reg` on a bidirectional terminal: switch terminals (LRM 28.8) and inout
// ports (23.3.3.3) connect only nets.  Surelog keeps `reg` as a logic_net
// stamped vpiReg; `logic` is ambiguous (vpiLogicNet) and left alone.
static const logic_net* regNetOf(const any* expr) {
  const ref_obj* ref = any_cast<const ref_obj*>(expr);
  if (ref == nullptr) return nullptr;
  const logic_net* n = ref->Actual_group<logic_net>();
  if (n != nullptr && n->VpiNetType() == vpiReg) return n;
  return nullptr;
}

void UhdmLint::leaveSwitch_tran(const switch_tran* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (object->Prim_terms() == nullptr) return;
  for (const prim_term* t : *object->Prim_terms()) {
    // tranif/rtranif: the third terminal is the control input.
    if (t->VpiDirection() != vpiInout || t->VpiTermIndex() > 1) continue;
    if (const logic_net* n = regNetOf(t->Expr())) {
      if (reportOnce(t->Expr(), "var-on-inout")) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_VARIABLE_ON_INOUT,
                                       std::string(n->VpiName()), t->Expr(),
                                       nullptr);
      }
    }
  }
}

static bool inClass(const any* obj);

static const class_defn* classOfTypespec(const typespec* ts) {
  if (const class_typespec* ct = any_cast<const class_typespec*>(ts))
    return ct->Class_defn();
  return nullptr;
}

static const class_defn* classOfVar(const any* v) {
  const class_var* cv = any_cast<const class_var*>(v);
  if (cv == nullptr || cv->Typespec() == nullptr) return nullptr;
  return classOfTypespec(cv->Typespec()->Actual_typespec());
}

// Surelog may hold several copies of one class (specializations, the
// definition and the elaborated view), so a class is matched by position.
static bool sameClass(const class_defn* a, const class_defn* b) {
  if (a == b) return true;
  if (a == nullptr || b == nullptr) return false;
  return a->VpiName() == b->VpiName() && a->VpiFile() == b->VpiFile() &&
         a->VpiLineNo() == b->VpiLineNo() &&
         a->VpiColumnNo() == b->VpiColumnNo();
}

// True if `c` is `base` or derives from it, or if the chain cannot be
// followed to its end (an unresolved base is never reported).
static bool derivesFrom(const class_defn* c, const class_defn* base) {
  for (int depth = 0; c != nullptr && depth < 64; ++depth) {
    if (sameClass(c, base)) return true;
    const extends* e = c->Extends();
    if (e == nullptr) return false;
    if (e->Class_typespec() == nullptr) return true;
    c = classOfTypespec(e->Class_typespec()->Actual_typespec());
  }
  return true;
}

// The class or type a `T::new` prefix names: the nearest enclosing scope
// declaring it, then the design's classes.
static std::string_view classLeafName(std::string_view n) {
  if (size_t p = n.rfind('@'); p != std::string_view::npos)
    n.remove_prefix(p + 1);
  if (size_t p = n.rfind("::"); p != std::string_view::npos)
    n.remove_prefix(p + 2);
  return n;
}

static const any* lookupTypeName(const any* where, std::string_view name,
                                 const design* d) {
  for (const any* p = where; p != nullptr; p = p->VpiParent()) {
    if (const instance* in = any_cast<const instance*>(p)) {
      if (in->Class_defns())
        for (const class_defn* cd : *in->Class_defns())
          if (classLeafName(cd->VpiName()) == name) return cd;
    }
    if (const scope* sc = any_cast<const scope*>(p)) {
      if (sc->Typespecs())
        for (const typespec* ts : *sc->Typespecs())
          if (ts->VpiName() == name) return ts;
    }
  }
  if (d != nullptr && d->AllClasses())
    for (const class_defn* cd : *d->AllClasses())
      if (classLeafName(cd->VpiName()) == name) return cd;
  return nullptr;
}

static bool isNonClassVariable(const any* v) {
  // A variable of an unknown type (a class Surelog did not compile, such as
  // a builtin under -nobuiltin) is a typeless logic_var: never a verdict.
  const expr* e = any_cast<const expr*>(v);
  if (e == nullptr || e->Typespec() == nullptr ||
      e->Typespec()->Actual_typespec() == nullptr)
    return false;
  switch (v->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmint_var:
    case UHDM_OBJECT_TYPE::uhdminteger_var:
    case UHDM_OBJECT_TYPE::uhdmlogic_var:
    case UHDM_OBJECT_TYPE::uhdmbit_var:
    case UHDM_OBJECT_TYPE::uhdmbyte_var:
    case UHDM_OBJECT_TYPE::uhdmshort_int_var:
    case UHDM_OBJECT_TYPE::uhdmlong_int_var:
    case UHDM_OBJECT_TYPE::uhdmreal_var:
    case UHDM_OBJECT_TYPE::uhdmshort_real_var:
    case UHDM_OBJECT_TYPE::uhdmstring_var:
    case UHDM_OBJECT_TYPE::uhdmenum_var:
    case UHDM_OBJECT_TYPE::uhdmstruct_var:
      return true;
    default:
      return false;
  }
}

// LRM 8.7 / 8.15 / 8.21: `new` constructs only into a class handle, never
// for a virtual class or a non-class type; a class handle takes only a
// handle of its own class or of a class derived from it.
void UhdmLint::checkClassAssign(const any* lhs, const any* rhs,
                                const any* where) {
  if (lhs == nullptr || rhs == nullptr ||
      lhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj)
    return;
  // Inside a class body Surelog binds type parameters to their defaults and
  // a shadowed name to the outer declaration, so no verdict is safe there.
  if (inClass(where)) return;
  const ref_obj* lref = (const ref_obj*)lhs;
  const any* lact = lref->Actual_group();
  if (lact == nullptr) return;
  const class_defn* lcls = classOfVar(lact);
  const class_defn* rcls = nullptr;
  auto reportNew = [&](const std::string& msg) {
    if (reportOnce(where, "class-new"))
      serializer_->GetErrorHandler()(ErrorType::UHDM_ILLEGAL_CLASS_NEW, msg,
                                     where, nullptr);
  };
  if (const method_func_call* nw = any_cast<const method_func_call*>(rhs)) {
    if (nw->VpiName() != "new") return;
    if (const ref_obj* prefix = nw->Prefix<ref_obj>()) {
      const any* named = lookupTypeName(nw, prefix->VpiName(), design_);
      if (named == nullptr) return;
      if (const class_defn* cd = any_cast<const class_defn*>(named)) {
        rcls = cd;
      } else if (const typespec* ts = any_cast<const typespec*>(named)) {
        if (ts->UhdmType() != UHDM_OBJECT_TYPE::uhdmclass_typespec) {
          reportNew("\"" + std::string(prefix->VpiName()) +
                    "\" is not a class type");
          return;
        }
        rcls = classOfTypespec(ts);
      }
      if (rcls != nullptr && rcls->VpiVirtual()) {
        reportNew("virtual class \"" +
                  std::string(classLeafName(rcls->VpiName())) +
                  "\" cannot be constructed");
        return;
      }
    } else {
      // A dynamic array is sized with `new[n]`, which always carries the
      // size; a bare `new` is a class constructor.
      const bool arrayTarget =
          lact->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var &&
          (nw->Tf_call_args() == nullptr || nw->Tf_call_args()->empty());
      if (isNonClassVariable(lact) || arrayTarget) {
        reportNew("\"" + std::string(lref->VpiName()) +
                  "\" is not a class handle");
      } else if (lcls != nullptr && lcls->VpiVirtual()) {
        reportNew("virtual class \"" +
                  std::string(classLeafName(lcls->VpiName())) +
                  "\" cannot be constructed");
      }
      return;
    }
  } else if (const ref_obj* rref = any_cast<const ref_obj*>(rhs)) {
    if (rref->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return;
    rcls = classOfVar(rref->Actual_group());
  }
  if (lcls == nullptr || rcls == nullptr || derivesFrom(rcls, lcls)) return;
  if (reportOnce(where, "class-compat"))
    serializer_->GetErrorHandler()(ErrorType::UHDM_INCOMPATIBLE_CLASS_HANDLE,
                                   std::string(lref->VpiName()), where,
                                   nullptr);
}

void UhdmLint::leaveAssignment(const assignment* object, vpiHandle handle) {
  noteProcDrivenVariable(object);
  checkUnpackedArrayAssign(object->Lhs(), object->Rhs(), object);
  if (object->VpiOpType() == 0 || object->VpiOpType() == vpiAssignmentOp)
    checkStreamFits(object->Lhs(), object->Rhs(), object);
  {
    const int op = object->VpiOpType();
    checkEnumAssign(object->Lhs(), object->Rhs(), object,
                    op != 0 && op != vpiAssignmentOp);
  }
  if (isInUhdmAllIterator()) return;
  checkClassAssign(object->Lhs(), object->Rhs(), object);
  if (!design_->VpiElaborated()) return;  // -uhdmelab
  if (const ref_obj* lhs = object->Lhs<ref_obj>()) {
    const logic_net* n = lhs->Actual_group<logic_net>();
    // A select of an array of wires (`icim[0] <= ...`).
    if (n == nullptr) {
      if (const array_net* an = lhs->Actual_group<array_net>()) {
        if (an->Nets() != nullptr && !an->Nets()->empty())
          n = any_cast<const logic_net*>(an->Nets()->front());
      }
    }
    if (n != nullptr) {
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
    // A property expression that is a bare name bound to a net is a
    // boolean property over that signal (`@(posedge clk) out`, LRM 16.12),
    // not an unresolved property name: UHDM_UNRESOLVED_PROPERTY is no
    // longer reported for it.
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

// The kind of an operand that only some operators accept (LRM 11.3.1):
// real and string values, and a call to a void function.
namespace {
enum class OperandKind { Other, Real, String, Void };

OperandKind typespecKind(const ref_typespec* rt) {
  if (rt == nullptr) return OperandKind::Other;
  const typespec* ts = rt->Actual_typespec();
  if (ts == nullptr) return OperandKind::Other;
  switch (ts->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmreal_typespec:
      return OperandKind::Real;
    case UHDM_OBJECT_TYPE::uhdmstring_typespec:
      return OperandKind::String;
    default:
      return OperandKind::Other;
  }
}

OperandKind operandKind(const any* o) {
  if (o == nullptr) return OperandKind::Other;
  switch (o->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmconstant: {
      const constant* c = static_cast<const constant*>(o);
      if (c->VpiConstType() == vpiRealConst) return OperandKind::Real;
      return typespecKind(c->Typespec());
    }
    case UHDM_OBJECT_TYPE::uhdmref_obj: {
      const ref_obj* ref = static_cast<const ref_obj*>(o);
      const any* actual = ref->Actual_group();
      // A parameter value in the definition view is left unbound: look the
      // name up among the enclosing scope's parameters.
      if (actual == nullptr) {
        for (const any* p = ref->VpiParent(); p != nullptr && actual == nullptr;
             p = p->VpiParent()) {
          const scope* sc = any_cast<const scope*>(p);
          if (sc == nullptr || sc->Parameters() == nullptr) continue;
          for (const any* prm : *sc->Parameters()) {
            if (prm->UhdmType() == UHDM_OBJECT_TYPE::uhdmparameter &&
                prm->VpiName() == ref->VpiName()) {
              actual = prm;
              break;
            }
          }
          break;  // only the innermost scope
        }
      }
      if (actual == nullptr) return OperandKind::Other;
      switch (actual->UhdmType()) {
        case UHDM_OBJECT_TYPE::uhdmreal_var:
          return OperandKind::Real;
        case UHDM_OBJECT_TYPE::uhdmstring_var:
          return OperandKind::String;
        case UHDM_OBJECT_TYPE::uhdmparameter:
          return typespecKind(
              static_cast<const parameter*>(actual)->Typespec());
        default:
          return OperandKind::Other;
      }
    }
    case UHDM_OBJECT_TYPE::uhdmfunc_call: {
      const function* f = static_cast<const func_call*>(o)->Function();
      if (f != nullptr && f->Return() == nullptr) return OperandKind::Void;
      return OperandKind::Other;
    }
    default:
      return OperandKind::Other;
  }
}
}  // namespace

void UhdmLint::leaveSys_func_call(const sys_func_call* object,
                                  vpiHandle handle) {
  // LRM 11.7: $signed / $unsigned take an integral argument.
  if ((object->VpiName() == "$signed" || object->VpiName() == "$unsigned") &&
      object->Tf_call_args() != nullptr) {
    for (const any* a : *object->Tf_call_args()) {
      if (operandKind(a) == OperandKind::Real && reportOnce(a, "operand-type"))
        serializer_->GetErrorHandler()(ErrorType::UHDM_INVALID_OPERAND_TYPE,
                                       "real", a, nullptr);
    }
  }
  // A system TASK has no value (LRM 20.1): `val = $display;`.
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
    if (inExpr) {
      const std::string_view n = object->VpiName();
      for (std::string_view t :
           {"$display", "$displayb", "$displayh", "$displayo", "$write",
            "$writeb", "$writeh", "$writeo", "$strobe", "$monitor",
            "$monitoron", "$monitoroff", "$finish", "$stop", "$exit",
            "$fdisplay", "$fwrite", "$fstrobe", "$fmonitor", "$fclose",
            "$fflush", "$readmemb", "$readmemh", "$writememb", "$writememh",
            "$dumpfile", "$dumpvars", "$dumpon", "$dumpoff", "$dumpall",
            "$dumpflush", "$printtimescale", "$timeformat", "$info",
            "$warning", "$error", "$fatal"}) {
        if (n == t) {
          if (reportOnce(object, "systaskexpr"))
            serializer_->GetErrorHandler()(
                ErrorType::UHDM_TASK_CALLED_AS_FUNCTION, std::string(n),
                object, nullptr);
          break;
        }
      }
    }
  }
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
  if (object->VpiDirection() == vpiInout) {
    if (const logic_net* n = regNetOf(object->Low_conn())) {
      if (reportOnce(n, "var-on-inout")) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_VARIABLE_ON_INOUT,
                                       std::string(n->VpiName()), object,
                                       nullptr);
      }
    }
    // An inout actual must be a net lvalue: a replication or any other
    // computed expression cannot be driven back.
    if (const operation* op = object->High_conn<operation>()) {
      const int t = op->VpiOpType();
      if (t != vpiConcatOp && t != vpiNullOp &&
          reportOnce(op, "not-assignable")) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_NOT_ASSIGNABLE,
                                       std::string(object->VpiName()), op,
                                       nullptr);
      }
    }
  }
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
// A port's net in a module DEFINITION carries no typespec of its own (the
// port does), so the missing typespec alone does not mark it fabricated.
static bool isPortNet(const logic_net* n) {
  const any* p = n->VpiParent();
  if (p == nullptr || p->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst)
    return false;
  if (const VectorOfport* ports = ((const module_inst*)p)->Ports())
    for (const port* pt : *ports)
      if (pt->VpiName() == n->VpiName()) return true;
  return false;
}

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
          if (n->Typespec() == nullptr && !isPortNet(n)) return nullptr;
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

// LRM 10.6: the target of a procedural continuous assignment (`assign`,
// `deassign`, `force`, `release`) is fixed when the statement executes; a
// select of it with a variable index (`assign v[idx] = 1'b1;`) is illegal.
void UhdmLint::checkProcContTarget(const any* lhs, const any* where) {
  if (lhs == nullptr) return;
  const any* index = nullptr;
  std::string_view name;
  switch (lhs->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmbit_select: {
      const bit_select* bs = (const bit_select*)lhs;
      index = nonConstantRef(bs->VpiIndex(), false);
      name = bs->VpiName();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmpart_select: {
      const part_select* ps = (const part_select*)lhs;
      index = nonConstantRef(ps->Left_range(), false);
      if (index == nullptr) index = nonConstantRef(ps->Right_range(), false);
      name = ps->VpiName();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmindexed_part_select: {
      const indexed_part_select* ips = (const indexed_part_select*)lhs;
      index = nonConstantRef(ips->Base_expr(), false);
      name = ips->VpiName();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmvar_select: {
      const var_select* vs = (const var_select*)lhs;
      if (vs->Exprs())
        for (const any* e : *vs->Exprs())
          if ((index = nonConstantRef(e, false)) != nullptr) break;
      name = vs->VpiName();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      const operation* op = (const operation*)lhs;
      if (op->VpiOpType() == vpiConcatOp && op->Operands())
        for (const any* o : *op->Operands()) checkProcContTarget(o, where);
      return;
    }
    default:
      return;
  }
  if (index != nullptr && reportOnce(lhs, "proc-cont-select"))
    serializer_->GetErrorHandler()(
        ErrorType::UHDM_NON_CONSTANT_PROC_CONT_SELECT, std::string(name), lhs,
        nullptr);
}

void UhdmLint::leaveAssign_stmt(const assign_stmt* object, vpiHandle handle) {
  checkProcContTarget(object->Lhs(), object);
}

void UhdmLint::leaveDeassign(const deassign* object, vpiHandle handle) {
  checkProcContTarget(object->Lhs(), object);
}

void UhdmLint::leaveForce(const force* object, vpiHandle handle) {
  checkProcContTarget(object->Lhs(), object);
}

void UhdmLint::leaveRelease(const release* object, vpiHandle handle) {
  checkProcContTarget(object->Lhs(), object);
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

// A function called in a constant expression without some of its trailing
// arguments uses their DEFAULTS (LRM 13.5.3), which must then be constant
// too: `localparam r = k(3);` over `function k(int i, int j = a + b)`.
static const any* nonConstantDefaultArg(const any* e) {
  if (e == nullptr) return nullptr;
  switch (e->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      const operation* op = (const operation*)e;
      if (op->Operands())
        for (const any* o : *op->Operands())
          if (const any* r = nonConstantDefaultArg(o)) return r;
      return nullptr;
    }
    case UHDM_OBJECT_TYPE::uhdmfunc_call: {
      const func_call* fc = (const func_call*)e;
      const size_t given = fc->Tf_call_args() ? fc->Tf_call_args()->size() : 0;
      if (fc->Tf_call_args())
        for (const any* a : *fc->Tf_call_args())
          if (const any* r = nonConstantDefaultArg(a)) return r;
      const function* f = fc->Function();
      if (f == nullptr || f->Io_decls() == nullptr) return nullptr;
      for (size_t i = given; i < f->Io_decls()->size(); ++i)
        if (const any* r = nonConstantRef(f->Io_decls()->at(i)->Expr(), false))
          return r;
      return nullptr;
    }
    default:
      return nullptr;
  }
}

void UhdmLint::leaveParam_assign(const param_assign* object,
                                 vpiHandle handle) {
  checkConstantExpr(object->Rhs(), false);
  if (const any* r = nonConstantDefaultArg(object->Rhs());
      r != nullptr && reportOnce(r, "nonconst-default-arg"))
    serializer_->GetErrorHandler()(ErrorType::UHDM_NON_CONSTANT_EXPRESSION,
                                   std::string(r->VpiName()), r, nullptr);
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

void UhdmLint::checkOperandTypes(const operation* op) {
  const VectorOfany* ops = op->Operands();
  if (ops == nullptr) return;
  bool integralOnly = false;  // no real operand
  bool wildcard = false;      // no string operand either
  switch (op->VpiOpType()) {
    case vpiBitNegOp:
    case vpiUnaryAndOp:
    case vpiUnaryNandOp:
    case vpiUnaryOrOp:
    case vpiUnaryNorOp:
    case vpiUnaryXorOp:
    case vpiUnaryXNorOp:
    case vpiModOp:
    case vpiLShiftOp:
    case vpiRShiftOp:
    case vpiArithLShiftOp:
    case vpiArithRShiftOp:
    case vpiBitAndOp:
    case vpiBitOrOp:
    case vpiBitXorOp:
    case vpiBitXnorOp:
    case vpiConcatOp:
    case vpiMultiConcatOp:
      integralOnly = true;
      break;
    case vpiWildEqOp:
    case vpiWildNeqOp:
      integralOnly = true;
      wildcard = true;
      break;
    case vpiCastOp:
    case vpiAssignmentPatternOp:
    case vpiMultiAssignmentPatternOp:
      return;
    default:
      break;
  }
  for (size_t i = 0; i < ops->size(); i++) {
    // The count of `{n{...}}` is checked elsewhere.
    if (op->VpiOpType() == vpiMultiConcatOp && i == 0) continue;
    const any* o = ops->at(i);
    const OperandKind k = operandKind(o);
    ErrorType err = ErrorType::UHDM_INVALID_OPERAND_TYPE;
    std::string what;
    if (k == OperandKind::Void) {
      err = ErrorType::UHDM_VOID_FUNCTION_VALUE;
      what = std::string(static_cast<const func_call*>(o)->VpiName());
    } else if (k == OperandKind::Real && integralOnly) {
      what = "real";
    } else if (k == OperandKind::String && wildcard) {
      what = "string";
    } else {
      continue;
    }
    if (reportOnce(op, "operand-type"))
      serializer_->GetErrorHandler()(err, what, op, nullptr);
    return;
  }
}

// LRM 15.5: an event is not a value; it cannot be passed to a formal that
// is not itself an event.
void UhdmLint::checkEventArgs(const tf_call* call, const task_func* tf) {
  if (call->Tf_call_args() == nullptr) return;
  const VectorOfio_decl* ios = tf ? tf->Io_decls() : nullptr;
  size_t i = 0;
  for (const any* a : *call->Tf_call_args()) {
    const size_t idx = i++;
    const ref_obj* r = any_cast<const ref_obj*>(a);
    if (r == nullptr || r->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj)
      continue;
    const any* actual = r->Actual_group();
    if (actual == nullptr) continue;
    // A whole unpacked array bound to a formal that is not an array.
    if (ios != nullptr && idx < ios->size() &&
        (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var ||
         actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net)) {
      const io_decl* io = ios->at(idx);
      const typespec* ts =
          io->Typespec() ? io->Typespec()->Actual_typespec() : nullptr;
      if ((io->Ranges() == nullptr || io->Ranges()->empty()) && ts &&
          (ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_typespec ||
           ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_typespec ||
           ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmint_typespec ||
           ts->UhdmType() == UHDM_OBJECT_TYPE::uhdminteger_typespec) &&
          reportOnce(a, "array-arg"))
        serializer_->GetErrorHandler()(
            ErrorType::UHDM_ILLEGAL_UNPACKED_ARRAY_ASSIGN,
            std::string(r->VpiName()), a, nullptr);
      continue;
    }
    if (actual->UhdmType() != UHDM_OBJECT_TYPE::uhdmnamed_event) continue;
    if (ios != nullptr && idx < ios->size()) {
      if (const ref_typespec* rt = ios->at(idx)->Typespec()) {
        if (const typespec* ts = rt->Actual_typespec())
          if (ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmevent_typespec) continue;
      }
    }
    if (reportOnce(a, "event-value"))
      serializer_->GetErrorHandler()(ErrorType::UHDM_EVENT_AS_VALUE,
                                     std::string(r->VpiName()), a, nullptr);
  }
}

void UhdmLint::leaveOperation(const operation* object, vpiHandle handle) {
  checkOperandTypes(object);
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
  // A SELECT of a name Surelog could not bind at all (`$fwrite(f, "%c",
  // undeclared[7:0])`): no net is fabricated for it, so the implicit-net
  // bookkeeping never sees it.  Reported from leaveDesign when the name is
  // declared nowhere in the design.
  if (actual == nullptr) {
    // A name in a $unit typedef (`typedef struct packed { logic [A-1:0]
    // a; } t;` at file scope) that nothing declares.
    if (ref->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj) {
      const std::string_view name = ref->VpiName();
      if (name.empty() || name.find('.') != std::string_view::npos ||
          name.find(':') != std::string_view::npos)
        return;
      const any* prev = ref;
      for (const any* p = ref->VpiParent(); p != nullptr;
           prev = p, p = p->VpiParent()) {
        if (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
          if (any_cast<const typespec*>(prev) != nullptr &&
              reportedOnce_
                  .emplace(std::string(ref->VpiFile()) + "#unit-type-ref",
                           ref->VpiLineNo(), ref->VpiColumnNo())
                  .second)
            unboundSelects_.emplace_back(ref, "$unit");
          return;
        }
        if (any_cast<const typespec*>(p) == nullptr &&
            p->UhdmType() != UHDM_OBJECT_TYPE::uhdmrange &&
            p->UhdmType() != UHDM_OBJECT_TYPE::uhdmoperation &&
            p->UhdmType() != UHDM_OBJECT_TYPE::uhdmtypespec_member &&
            p->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_typespec)
          return;
      }
      return;
    }
    switch (ref->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmbit_select:
      case UHDM_OBJECT_TYPE::uhdmpart_select:
      case UHDM_OBJECT_TYPE::uhdmindexed_part_select: {
        const std::string_view name = ref->VpiName();
        if (name.empty() || name.find('.') != std::string_view::npos ||
            name.find(':') != std::string_view::npos || inClass(ref))
          return;
        if (const any* p = ref->VpiParent();
            p == nullptr || p->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path)
          return;
        // Only a READ in an always / initial / final body: continuous
        // assignment targets, function and task bodies (out-of-class method
        // definitions) and generate-scope names are bound too unreliably.
        bool inProcess = false;
        const any* prev = ref;
        for (const any* p = ref->VpiParent(); p != nullptr;
             prev = p, p = p->VpiParent()) {
          const UHDM_OBJECT_TYPE t = p->UhdmType();
          if (t == UHDM_OBJECT_TYPE::uhdmassignment &&
              ((const assignment*)p)->Lhs() == prev)
            return;
          if (t == UHDM_OBJECT_TYPE::uhdmfunction ||
              t == UHDM_OBJECT_TYPE::uhdmtask ||
              t == UHDM_OBJECT_TYPE::uhdmgen_scope ||
              t == UHDM_OBJECT_TYPE::uhdmcont_assign)
            return;
          if (t == UHDM_OBJECT_TYPE::uhdmalways ||
              t == UHDM_OBJECT_TYPE::uhdminitial ||
              t == UHDM_OBJECT_TYPE::uhdmfinal_stmt) {
            inProcess = true;
            break;
          }
          if (t == UHDM_OBJECT_TYPE::uhdmmodule_inst) break;
        }
        if (!inProcess) return;
        if (!reportedOnce_
                 .emplace(std::string(ref->VpiFile()) + "#unbound-select",
                          ref->VpiLineNo(), ref->VpiColumnNo())
                 .second)
          return;
        unboundSelects_.emplace_back(ref, ownerDefName(ref));
        return;
      }
      default:
        return;
    }
  }
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
  if (member.empty() || member.find("::") != std::string_view::npos ||
      member.find('@') != std::string_view::npos)  // @@BAD_SYMBOL@@
    return;
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

// The number of packed dimensions a typespec has, or -1 when it cannot be
// told for sure (a typedef, a struct, an element chain, ...).
static int packedDimsOf(const typespec* ts) {
  if (ts == nullptr || !ts->VpiName().empty()) return -1;
  switch (ts->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmlogic_typespec: {
      const logic_typespec* lt = (const logic_typespec*)ts;
      if (lt->Elem_typespec() != nullptr) return -1;
      return lt->Ranges() ? (int)lt->Ranges()->size() : 0;
    }
    case UHDM_OBJECT_TYPE::uhdmbit_typespec: {
      const bit_typespec* bt = (const bit_typespec*)ts;
      if (bt->Bit_typespec() != nullptr) return -1;
      return bt->Ranges() ? (int)bt->Ranges()->size() : 0;
    }
    case UHDM_OBJECT_TYPE::uhdmint_typespec:
    case UHDM_OBJECT_TYPE::uhdminteger_typespec:
    case UHDM_OBJECT_TYPE::uhdmshort_int_typespec:
    case UHDM_OBJECT_TYPE::uhdmlong_int_typespec:
    case UHDM_OBJECT_TYPE::uhdmbyte_typespec:
      return 1;
    default:
      return -1;
  }
}

static const typespec* ownTypespec(const any* o) {
  const expr* e = any_cast<const expr*>(o);
  if (e == nullptr || e->Typespec() == nullptr) return nullptr;
  return e->Typespec()->Actual_typespec();
}

// True if a module port is named like `obj`: a port's net may carry only the
// element type of the port's declared type (see the reader's CLAUDE.md).
static bool namedLikePort(const any* obj) {
  for (const any* p = obj->VpiParent(); p != nullptr; p = p->VpiParent()) {
    if (p->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst) continue;
    if (const auto ports = ((const module_inst*)p)->Ports())
      for (const port* pt : *ports)
        if (pt->VpiName() == obj->VpiName()) return true;
    return false;
  }
  return false;
}

// The total number of unpacked and packed dimensions of a declared object,
// or -1 when unknown.
static int totalDims(const any* actual) {
  if (actual == nullptr || namedLikePort(actual)) return -1;
  switch (actual->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmarray_net:
    case UHDM_OBJECT_TYPE::uhdmarray_var: {
      const VectorOfrange* ranges = unpackedRanges(actual);
      const any* elem = unpackedElement(actual);
      if (ranges == nullptr || elem == nullptr) return -1;
      const int p = packedDimsOf(ownTypespec(elem));
      return p < 0 ? -1 : (int)ranges->size() + p;
    }
    case UHDM_OBJECT_TYPE::uhdmlogic_net:
    case UHDM_OBJECT_TYPE::uhdmlogic_var:
    case UHDM_OBJECT_TYPE::uhdmbit_var:
    case UHDM_OBJECT_TYPE::uhdmint_var:
    case UHDM_OBJECT_TYPE::uhdminteger_var:
    case UHDM_OBJECT_TYPE::uhdmbyte_var:
    case UHDM_OBJECT_TYPE::uhdmshort_int_var:
    case UHDM_OBJECT_TYPE::uhdmlong_int_var:
      return packedDimsOf(ownTypespec(actual));
    default:
      return -1;
  }
}

// `vector[0][0]` on a `reg [15:0] vector`: one index more than the object
// has dimensions (LRM 7.4.5, 11.5.1).
void UhdmLint::leaveVar_select(const var_select* object, vpiHandle handle) {
  noteImplicitNetRef(object);
  if (isInUhdmAllIterator() || object->Exprs() == nullptr || inClass(object))
    return;
  const int dims = totalDims(object->Actual_group());
  if (dims >= 0 && (int)object->Exprs()->size() > dims &&
      reportOnce(object, "too-many-indices"))
    serializer_->GetErrorHandler()(ErrorType::UHDM_TOO_MANY_INDICES,
                                   std::string(object->VpiName()), object,
                                   nullptr);
}

void UhdmLint::leaveForeach_stmt(const foreach_stmt* object,
                                 vpiHandle handle) {
  if (isInUhdmAllIterator() || object->VpiLoopVars() == nullptr ||
      inClass(object))
    return;
  const any* arr = object->Variable();
  const any* actual = nullptr;
  if (const ref_var* rv = any_cast<const ref_var*>(arr))
    actual = rv->Actual_group();
  else if (const ref_obj* ro = any_cast<const ref_obj*>(arr))
    actual = ro->Actual_group();
  if (actual == nullptr) return;
  const int dims = totalDims(actual);
  if (dims >= 0 && (int)object->VpiLoopVars()->size() > dims &&
      reportOnce(object, "too-many-indices"))
    serializer_->GetErrorHandler()(ErrorType::UHDM_TOO_MANY_INDICES,
                                   std::string(arr->VpiName()), object,
                                   nullptr);
}

// LRM 6.5: a variable written by a continuous assignment has no other
// driver -- neither another continuous assignment nor a procedural write.
static bool isVariableObject(const any* a) {
  if (a == nullptr) return false;
  switch (a->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmlogic_var:
    case UHDM_OBJECT_TYPE::uhdmbit_var:
    case UHDM_OBJECT_TYPE::uhdmint_var:
    case UHDM_OBJECT_TYPE::uhdminteger_var:
    case UHDM_OBJECT_TYPE::uhdmbyte_var:
    case UHDM_OBJECT_TYPE::uhdmshort_int_var:
    case UHDM_OBJECT_TYPE::uhdmlong_int_var:
    case UHDM_OBJECT_TYPE::uhdmreal_var:
    case UHDM_OBJECT_TYPE::uhdmenum_var:
    case UHDM_OBJECT_TYPE::uhdmstruct_var:
      return true;
    default:
      return false;
  }
}

void UhdmLint::noteContDrivenVariable(const cont_assign* object) {
  if (isInUhdmAllIterator() || inClass(object)) return;
  const ref_obj* lhs = any_cast<const ref_obj*>(object->Lhs());
  if (lhs == nullptr || lhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj)
    return;
  const any* actual = lhs->Actual_group();
  if (!isVariableObject(actual)) return;
  auto [it, inserted] = contDrivenVars_.emplace(actual, object);
  if (inserted) {
    contDrivenOrder_.push_back(actual);
    return;
  }
  // The same source assign seen twice (an interface array element's own
  // assign reaches one variable through each view) is one driver.
  const cont_assign* first = it->second;
  if (first->VpiFile() == object->VpiFile() &&
      first->VpiLineNo() == object->VpiLineNo() &&
      first->VpiColumnNo() == object->VpiColumnNo())
    return;
  if (reportOnce(object, "var-drivers")) {
    serializer_->GetErrorHandler()(ErrorType::UHDM_VARIABLE_MULTIPLE_DRIVERS,
                                   std::string(lhs->VpiName()), object,
                                   nullptr);
  }
}

void UhdmLint::noteProcDrivenVariable(const assignment* object) {
  if (isInUhdmAllIterator() || inClass(object)) return;
  bool inProcess = false;
  for (const any* p = object->VpiParent(); p != nullptr; p = p->VpiParent()) {
    const UHDM_OBJECT_TYPE t = p->UhdmType();
    if (t == UHDM_OBJECT_TYPE::uhdmfunction ||
        t == UHDM_OBJECT_TYPE::uhdmtask)
      return;
    if (t == UHDM_OBJECT_TYPE::uhdmalways ||
        t == UHDM_OBJECT_TYPE::uhdminitial ||
        t == UHDM_OBJECT_TYPE::uhdmfinal_stmt) {
      inProcess = true;
      break;
    }
  }
  if (!inProcess) return;
  const ref_obj* lhs = any_cast<const ref_obj*>(object->Lhs());
  if (lhs == nullptr) return;
  const any* actual = lhs->Actual_group();
  if (isVariableObject(actual)) procDrivenVars_.emplace(actual, object);
}

void UhdmLint::reportContProcDrivenVariables() {
  for (const any* var : contDrivenOrder_) {
    if (procDrivenVars_.find(var) == procDrivenVars_.end()) continue;
    const cont_assign* ca = contDrivenVars_[var];
    if (reportOnce(ca, "var-drivers"))
      serializer_->GetErrorHandler()(ErrorType::UHDM_VARIABLE_MULTIPLE_DRIVERS,
                                     std::string(var->VpiName()), ca, nullptr);
  }
  contDrivenVars_.clear();
  contDrivenOrder_.clear();
  procDrivenVars_.clear();
}

// An assignment pattern (LRM 10.9) builds an array or a structure; a scalar
// `bit x = '{1'b1};` has neither elements nor members.
void UhdmLint::checkPatternToScalar(const any* var, const any* rhs) {
  const operation* op = any_cast<const operation*>(rhs);
  if (op == nullptr || (op->VpiOpType() != vpiAssignmentPatternOp &&
                        op->VpiOpType() != vpiMultiAssignmentPatternOp))
    return;
  if (var == nullptr || (var->UhdmType() != UHDM_OBJECT_TYPE::uhdmbit_var &&
                         var->UhdmType() != UHDM_OBJECT_TYPE::uhdmlogic_var))
    return;
  // The element variable of an unpacked array shares the array's pattern.
  if (const any* p = var->VpiParent())
    if (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var ||
        p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net)
      return;
  if (packedDimsOf(ownTypespec(var)) != 0) return;
  if (reportOnce(rhs, "pattern-scalar"))
    serializer_->GetErrorHandler()(ErrorType::UHDM_PATTERN_TO_SCALAR,
                                   std::string(var->VpiName()), rhs, nullptr);
}

void UhdmLint::leaveBit_var(const bit_var* object, vpiHandle handle) {
  if (!isInUhdmAllIterator()) checkPatternToScalar(object, object->Expr());
}

void UhdmLint::leaveLogic_var(const logic_var* object, vpiHandle handle) {
  if (!isInUhdmAllIterator()) checkPatternToScalar(object, object->Expr());
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
  // `c.e`: a property of an object reached through a class handle.  Only for
  // a class with no parameters: Surelog binds a parameterised class's type
  // parameters to their defaults.
  if (actual && hp->Path_elems()->size() == 2 &&
      any_cast<const variables*>(actual)) {
    const any* first = hp->Path_elems()->front();
    const ref_obj* root = any_cast<const ref_obj*>(first);
    const any* cls = actual->VpiParent();
    if (root && root->Actual_group() &&
        root->Actual_group()->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_var &&
        cls && cls->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_defn &&
        (((const class_defn*)cls)->Parameters() == nullptr ||
         ((const class_defn*)cls)->Parameters()->empty()))
      return actual;
  }
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

// The width of a plain integral variable, 0 when it is anything else (an
// array, a dynamic type, a struct, a width that is not constant).
static uint64_t integralBits(const any* v) {
  if (v == nullptr || (any_cast<const variables*>(v) == nullptr &&
                       any_cast<const nets*>(v) == nullptr))
    return 0;
  if (const any* p = v->VpiParent())
    if (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var ||
        p->UhdmType() == UHDM_OBJECT_TYPE::uhdmpacked_array_var ||
        p->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_net ||
        p->UhdmType() == UHDM_OBJECT_TYPE::uhdmpacked_array_net)
      return 0;
  const typespec* ts = declTypespec(v);
  if (ts == nullptr) return 0;
  switch (ts->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmbyte_typespec: return 8;
    case UHDM_OBJECT_TYPE::uhdmshort_int_typespec: return 16;
    case UHDM_OBJECT_TYPE::uhdmint_typespec:
    case UHDM_OBJECT_TYPE::uhdminteger_typespec: return 32;
    case UHDM_OBJECT_TYPE::uhdmlong_int_typespec: return 64;
    case UHDM_OBJECT_TYPE::uhdmlogic_typespec:
    case UHDM_OBJECT_TYPE::uhdmbit_typespec: {
      ExprEval eval;
      bool invalid = false;
      const uint64_t w = eval.size(ts, invalid, nullptr, nullptr, true);
      return invalid ? 0 : w;
    }
    default:
      return 0;
  }
}

// `int d = {<<{a, b, c}}`: a stream used as a value must fit its target, which
// is padded on the right when wider but never truncated (LRM 11.4.14).  Only
// a stream over plain integral variables into a plain integral variable.
void UhdmLint::checkStreamFits(const any* lhs, const any* rhs,
                               const any* where) {
  if (lhs == nullptr || rhs == nullptr ||
      rhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmoperation)
    return;
  const operation* op = (const operation*)rhs;
  if (op->VpiOpType() != vpiStreamLROp && op->VpiOpType() != vpiStreamRLOp)
    return;
  const any* target = lhs;
  if (lhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj)
    target = ((const ref_obj*)lhs)->Actual_group();
  const uint64_t tbits = integralBits(target);
  if (tbits == 0 || op->Operands() == nullptr || op->Operands()->empty())
    return;
  // The slice size, if any, comes first; the stream concatenation last.
  const any* body = op->Operands()->back();
  std::vector<const any*> items;
  if (body->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation &&
      ((const operation*)body)->VpiOpType() == vpiConcatOp) {
    if (((const operation*)body)->Operands())
      for (const any* o : *((const operation*)body)->Operands())
        items.push_back(o);
  } else {
    items.push_back(body);
  }
  uint64_t sbits = 0;
  for (const any* o : items) {
    if (o->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return;
    const uint64_t w = integralBits(((const ref_obj*)o)->Actual_group());
    if (w == 0) return;
    sbits += w;
  }
  if (sbits <= tbits || inClass(where) || !reportOnce(where, "stream-fit"))
    return;
  serializer_->GetErrorHandler()(
      ErrorType::UHDM_STREAM_TOO_WIDE,
      std::to_string(sbits) + " bits into " + std::to_string(tbits), rhs,
      nullptr);
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

// `m1.e`, written in an elaborated instance: the variable reached by walking
// child instances down from the instance holding the statement.  Surelog
// leaves the path elements of such a hier_path unbound.
// A statement of a module body is shared by its definition and every
// elaborated instance; the first elaborated instance of the definition stands
// for all of them (the child instances below it are the same in each).
const module_inst* UhdmLint::elabInstanceOf(const module_inst* mi) {
  if (mi->Modules() != nullptr) return mi;
  if (!elabByDefBuilt_) {
    elabByDefBuilt_ = true;
    std::vector<const module_inst*> work;
    if (design_ && design_->TopModules())
      for (const module_inst* t : *design_->TopModules()) work.push_back(t);
    while (!work.empty()) {
      const module_inst* m = work.back();
      work.pop_back();
      elabByDef_.emplace(std::string(m->VpiDefName()), m);
      if (m->Modules())
        for (const module_inst* c : *m->Modules()) work.push_back(c);
    }
  }
  auto it = elabByDef_.find(mi->VpiDefName());
  return it == elabByDef_.end() ? nullptr : it->second;
}

const any* UhdmLint::instHierVar(const hier_path* hp, const any* where) {
  if (hp == nullptr || hp->Path_elems() == nullptr ||
      hp->Path_elems()->size() < 2)
    return nullptr;
  const any* p = where;
  while (p && p->UhdmType() != UHDM_OBJECT_TYPE::uhdmmodule_inst)
    p = p->VpiParent();
  if (p == nullptr) return nullptr;
  const module_inst* mi = elabInstanceOf((const module_inst*)p);
  if (mi == nullptr) return nullptr;
  const auto& elems = *hp->Path_elems();
  for (size_t i = 0; i + 1 < elems.size(); ++i) {
    if (elems[i]->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return nullptr;
    const std::string_view name = elems[i]->VpiName();
    const module_inst* next = nullptr;
    if (mi->Modules())
      for (const module_inst* c : *mi->Modules())
        if (c->VpiName() == name) next = c;
    if (next == nullptr) return nullptr;
    mi = next;
  }
  const any* last = elems.back();
  if (last->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_obj) return nullptr;
  if (mi->Variables())
    for (const any* v : *mi->Variables())
      if (v->VpiName() == last->VpiName()) return v;
  return nullptr;
}

// The module instance an enum typespec was declared in, or null when it comes
// from anywhere else (a package, $unit, a class).
static const module_inst* enumDeclInstance(const enum_typespec* ts) {
  if (ts->VpiName().find("::") != std::string_view::npos) return nullptr;
  for (const any* p = ts->VpiParent(); p; p = p->VpiParent()) {
    if (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst)
      return (const module_inst*)p;
    if (any_cast<const typespec*>(p) == nullptr &&
        any_cast<const variables*>(p) == nullptr &&
        any_cast<const nets*>(p) == nullptr &&
        p->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_typespec)
      return nullptr;
  }
  return nullptr;
}

void UhdmLint::checkEnumAssign(const any* lhs, const any* rhs, const any* where,
                               bool compound) {
  // `m1.e = m2.e`: an enum declared inside a module is a distinct type in
  // every instance of it (LRM 6.22.1), even though the members match.
  if (!compound && lhs && rhs &&
      lhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path &&
      rhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path) {
    const any* lv = instHierVar((const hier_path*)lhs, where);
    const any* rv = instHierVar((const hier_path*)rhs, where);
    const ValType lt = typeOfTypespec(declTypespec(lv));
    const ValType rt = typeOfTypespec(declTypespec(rv));
    if (lv && rv && lt.kind == ValKind::Enum && rt.kind == ValKind::Enum &&
        lt.ets != rt.ets) {
      const module_inst* li = enumDeclInstance(lt.ets);
      const module_inst* ri = enumDeclInstance(rt.ets);
      if (li && ri && li != ri && !inClass(where) &&
          reportedEnumAssigns_
              .emplace(std::string(rhs->VpiFile()), rhs->VpiLineNo(),
                       rhs->VpiColumnNo())
              .second) {
        const std::string errMsg(lt.ets->VpiName().empty() ? "<anonymous>"
                                                           : lt.ets->VpiName());
        serializer_->GetErrorHandler()(ErrorType::UHDM_ENUM_ASSIGN_INCOMPATIBLE,
                                       errMsg, rhs, nullptr);
        return;
      }
    }
  }
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
  checkEventArgs(object, object->Function());
  checkEnumCallArgs(object, object->Function());
  checkTfArgCount(object, object->Function());
}

void UhdmLint::leaveTask_call(const task_call* object, vpiHandle handle) {
  checkEventArgs(object, object->Task());
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
    // A type name nothing resolved names a use, not a declaration.
    case UHDM_OBJECT_TYPE::uhdmunsupported_typespec:
      return;
    case UHDM_OBJECT_TYPE::uhdmmodule_inst: {
      // A module's own name is declared too (`$printtimescale(top)`).
      // Module and instance names are SCOPE names: they may name a scope
      // for a system task, never a value (see usedAsValue).
      const module_inst* mi = (const module_inst*)object;
      const std::string def = ownerDefName(mi);
      if (!def.empty()) {
        scopeNames_.emplace(def);
        const any* up = mi->VpiParent();
        if (mi->VpiTop() ||
            (up && up->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst))
          elaboratedDefs_.emplace(def);
      }
      if (!mi->VpiName().empty()) scopeNames_.emplace(mi->VpiName());
      return;
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

// True unless `ref` is an argument of a system task or function that takes
// a scope (`$dumpvars(0, top)`, `$printtimescale(top)`): a module or
// instance name there is legal, as a value (`$bits(top)`) it is not.
static bool usedAsValue(const any* ref) {
  // `@top.ev` reaches us as a ref named `top` whose span still covers the
  // whole dotted path: a hierarchical name, not a value.
  if (ref->VpiEndLineNo() != ref->VpiLineNo() ||
      static_cast<size_t>(ref->VpiEndColumnNo() - ref->VpiColumnNo()) >
          ref->VpiName().size())
    return false;
  for (const any* p = ref->VpiParent(); p != nullptr; p = p->VpiParent()) {
    switch (p->UhdmType()) {
      case UHDM_OBJECT_TYPE::uhdmsys_func_call: {
        const std::string_view n = p->VpiName();
        for (std::string_view v :
             {"$bits", "$size", "$left", "$right", "$high", "$low",
              "$increment", "$dimensions", "$unpacked_dimensions", "$clog2",
              "$signed", "$unsigned", "$countones", "$onehot", "$onehot0",
              "$isunknown"})
          if (n == v) return true;
        return false;
      }
      case UHDM_OBJECT_TYPE::uhdmmodule_inst:
      case UHDM_OBJECT_TYPE::uhdmdesign:
        return true;
      default:
        break;
    }
  }
  return true;
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
    if (scopeNames_.find(name) != scopeNames_.end() &&
        !usedAsValue(use.firstRef))
      continue;
    if (elaboratedDefs_.find(use.ownerDef) == elaboratedDefs_.end()) continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_UNDECLARED_IDENTIFIER,
                                   std::string(use.firstRef->VpiName()),
                                   use.firstRef, nullptr);
  }
  for (const auto& [call, ownerDef] : unboundCalls_) {
    if (declaredNames_.find(call->VpiName()) != declaredNames_.end() ||
        scopeNames_.find(call->VpiName()) != scopeNames_.end())
      continue;
    if (elaboratedDefs_.find(ownerDef) == elaboratedDefs_.end()) continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_UNDEFINED_TASK_FUNC,
                                   std::string(call->VpiName()), call, nullptr);
  }
  for (const auto& [ref, ownerDef] : unboundSelects_) {
    if (declaredNames_.find(ref->VpiName()) != declaredNames_.end() ||
        scopeNames_.find(ref->VpiName()) != scopeNames_.end())
      continue;
    if (ownerDef != "$unit" &&
        elaboratedDefs_.find(ownerDef) == elaboratedDefs_.end())
      continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_UNDECLARED_IDENTIFIER,
                                   std::string(ref->VpiName()), ref, nullptr);
  }
  unboundSelects_.clear();
  for (const auto& [ut, ownerDef] : unresolvedArgTypes_) {
    if (declaredNames_.find(ut->VpiName()) != declaredNames_.end() ||
        scopeNames_.find(ut->VpiName()) != scopeNames_.end())
      continue;
    if (elaboratedDefs_.find(ownerDef) == elaboratedDefs_.end()) continue;
    if (!reportedOnce_
             .emplace(std::string(ut->VpiFile()) + "#undefined-type",
                      ut->VpiLineNo(), ut->VpiColumnNo())
             .second)
      continue;
    serializer_->GetErrorHandler()(ErrorType::UHDM_UNDEFINED_TYPE,
                                   std::string(ut->VpiName()), ut, nullptr);
  }
  unresolvedArgTypes_.clear();
  reportContProcDrivenVariables();
  unboundCalls_.clear();
  implicitNets_.clear();
  implicitNetOrder_.clear();
  reportedCalls_.clear();
  reportedOnce_.clear();
  declaredNames_.clear();
  scopeNames_.clear();
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
