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
 * File:   ExprEval.cpp
 * Author: alaindargelas
 *
 * Created on July 3, 2021, 8:03 PM
 */

#include <string.h>
#include <uhdm/ExprEval.h>
#include <uhdm/uhdm.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstring>
#include <iostream>
#include <locale>
#include <regex>
#include <sstream>

using namespace UHDM;

bool ExprEval::isFullySpecified(const UHDM::typespec *tps) {
  VectorOfrange *ranges = nullptr;
  UHDM_OBJECT_TYPE type = tps->UhdmType();
  switch (type) {
    case uhdmlogic_typespec: {
      logic_typespec *ltps = (logic_typespec *)tps;
      ranges = ltps->Ranges();
      break;
    }
    case uhdmarray_typespec: {
      array_typespec *ltps = (array_typespec *)tps;
      const typespec *elem = ltps->Elem_typespec();
      if (!isFullySpecified(elem)) return false;
      ranges = ltps->Ranges();
      break;
    }
    case uhdmbit_typespec: {
      bit_typespec *ltps = (bit_typespec *)tps;
      ranges = ltps->Ranges();
      break;
    }
    case uhdmenum_typespec: {
      enum_typespec *ltps = (enum_typespec *)tps;
      const typespec *base = ltps->Base_typespec();
      if (base && (!isFullySpecified(base))) return false;
      break;
    }
    case uhdmstruct_typespec: {
      struct_typespec *ltps = (struct_typespec *)tps;
      for (typespec_member *member : *ltps->Members()) {
        if (!isFullySpecified(member->Typespec())) return false;
      }
      break;
    }
    case uhdmunion_typespec: {
      union_typespec *ltps = (union_typespec *)tps;
      for (typespec_member *member : *ltps->Members()) {
        if (!isFullySpecified(member->Typespec())) return false;
      }
      break;
    }
    case uhdmpacked_array_typespec: {
      packed_array_typespec *ltps = (packed_array_typespec *)tps;
      const typespec *elem = (const typespec *)ltps->Elem_typespec();
      if (!isFullySpecified(elem)) return false;
      const typespec *ttps = ltps->Typespec();
      if (ttps && (!isFullySpecified(ttps))) return false;
      ranges = ltps->Ranges();
      break;
    }
    default:
      break;
  }
  if (ranges) {
    for (auto range : *ranges) {
      if (range->Left_expr()->UhdmType() != uhdmconstant) return false;
      if (range->Right_expr()->UhdmType() != uhdmconstant) return false;
    }
  }
  return true;
}

void tokenizeMulti(std::string_view str, std::string_view separator,
                   std::vector<std::string> &result) {
  std::string tmp;
  const unsigned int sepSize = static_cast<unsigned int>(separator.size());
  const unsigned int stringSize = static_cast<unsigned int>(str.size());
  for (unsigned int i = 0; i < stringSize; i++) {
    bool isSeparator = true;
    for (unsigned int j = 0; j < sepSize; j++) {
      if (i + j >= stringSize) break;
      if (str[i + j] != separator[j]) {
        isSeparator = false;
        break;
      }
    }
    if (isSeparator) {
      i = i + sepSize - 1;
      result.push_back(tmp);
      tmp = "";
      if (i == (str.size() - 1)) result.push_back(tmp);
    } else if (i == (str.size() - 1)) {
      tmp += str[i];
      result.push_back(tmp);
      tmp = "";
    } else {
      tmp += str[i];
    }
  }
}

any *ExprEval::getValue(const std::string &name, const any *inst,
                        const any *pexpr) {
  if ((inst == nullptr) && (pexpr == nullptr)) {
    return nullptr;
  }
  Serializer* tmps = nullptr;
  if (inst)
    tmps = inst->GetSerializer();
  else 
    tmps = pexpr->GetSerializer();
  Serializer &s = *tmps;
  any *result = nullptr;
  const any *root = inst;
  const any *tmp = inst;
  while (tmp) {
    root = tmp;
    tmp = tmp->VpiParent();
  }
  const design *des = any_cast<design *>(root);
  std::string the_name = name;
  const any *the_instance = inst;
  if (des && (name.find("::") != std::string::npos)) {
    std::vector<std::string> res;
    tokenizeMulti(name, "::", res);
    if (res.size() > 1) {
      const std::string &packName = res[0];
      const std::string &varName = res[1];
      the_name = varName;
      package *pack = nullptr;
      if (des->TopPackages()) {
        for (auto p : *des->TopPackages()) {
          if (p->VpiName() == packName) {
            pack = p;
            break;
          }
        }
      }
      the_instance = pack;
    }
  }

  while (the_instance) {
    UHDM::VectorOfparam_assign *param_assigns = nullptr;
    UHDM::VectorOftypespec *typespecs = nullptr;
    if (the_instance->UhdmType() == uhdmgen_scope_array) {
    } else if (the_instance->UhdmType() == uhdmdesign) {
      param_assigns = ((design *)the_instance)->Param_assigns();
      typespecs = ((design *)the_instance)->Typespecs();
    } else if (any_cast<scope*>(the_instance)) {
      param_assigns = ((scope *)the_instance)->Param_assigns();
      typespecs = ((scope *)the_instance)->Typespecs();
    }
    if (param_assigns) {
      for (auto p : *param_assigns) {
        if (p->Lhs() && (p->Lhs()->VpiName() == the_name)) {
          result = (any *)p->Rhs();
          break;
        }
      }
    }
    if (result == nullptr) {
      if (typespecs) {
        for (auto p : *typespecs) {
          if (p->UhdmType() == uhdmenum_typespec) {
            enum_typespec *e = (enum_typespec *)p;
            for (auto c : *e->Enum_consts()) {
              if (c->VpiName() == the_name) {
                constant *cc = s.MakeConstant();
                cc->VpiValue(c->VpiValue());
                cc->VpiSize(c->VpiSize());
                result = cc;
                break;
              }
            }
          }
        }
      }
    }
    if (result) {
      if (result->UhdmType() == uhdmoperation) {
        operation *op = (operation *)result;
        UHDM::ExprEval eval;
        eval.flattenPatternAssignments(s, op->Typespec(), (UHDM::expr *)result);
      }
    }
    if (result) break;

    the_instance = the_instance->VpiParent();
  }

  if (result) {
    UHDM_OBJECT_TYPE resultType = result->UhdmType();
    if (resultType == uhdmconstant) {
    } else if (resultType == uhdmref_obj) {
      if (result->VpiName() != name) {
        any *tmp = getValue(result->VpiName(), inst, pexpr);
        if (tmp) result = tmp;
      }
    } else if (resultType == uhdmoperation || resultType == uhdmhier_path ||
               resultType == uhdmbit_select ||
               resultType == uhdmsys_func_call) {
      bool invalidValue = false;
      any *tmp = reduceExpr(result, invalidValue, inst, pexpr);
      if (tmp) result = tmp;
    }
  }
  return result;
}

any *ExprEval::getObject(const std::string &name, const any *inst,
                         const any *pexpr) {
  any *result = nullptr;
  while (pexpr) {
    if (const UHDM::scope *s = any_cast<const scope *>(pexpr)) {
      if ((result == nullptr) && s->Variables()) {
        for (auto o : *s->Variables()) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (const UHDM::task_func *s = any_cast<const task_func *>(pexpr)) {
      if ((result == nullptr) && s->Io_decls()) {
        for (auto o : *s->Io_decls()) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    pexpr = pexpr->VpiParent();
  }
  if (result == nullptr) {
    while (inst) {
      UHDM::VectorOfparam_assign *param_assigns = nullptr;
      UHDM::VectorOfvariables *variables = nullptr;
      UHDM::VectorOfarray_net *array_nets = nullptr;
      UHDM::VectorOfnet *nets = nullptr;
      if (inst->UhdmType() == uhdmgen_scope_array) {
      } else if (inst->UhdmType() == uhdmdesign) {
        param_assigns = ((design *)inst)->Param_assigns();
      } else if (any_cast<scope*>(inst)) {
        param_assigns = ((scope *)inst)->Param_assigns();
        variables = ((scope *)inst)->Variables();
        if (const instance *in = any_cast<instance *>(inst)) {
          array_nets = in->Array_nets();
          nets = in->Nets();
        }
      }
      if ((result == nullptr) && array_nets) {
        for (auto o : *array_nets) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && nets) {
        for (auto o : *nets) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && variables) {
        for (auto o : *variables) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && param_assigns) {
        for (auto o : *param_assigns) {
          const std::string &pname = o->Lhs()->VpiName();
          if (pname == name) {
            result = o;
            break;
          }
        }
      }

      if ((result == nullptr) ||
          (result && (result->UhdmType() != uhdmconstant) &&
           (result->UhdmType() != uhdmparam_assign))) {
        result = getValue(name, inst, pexpr);
      }
      if (result) break;
      if (inst) {
        if (inst->UhdmType() == uhdmmodule) {
          break;
        } else {
          inst = inst->VpiParent();
        }
      }
    }
  }

  if (result && (result->UhdmType() == uhdmref_obj)) {
    ref_obj *ref = (ref_obj *)result;
    const std::string &refname = ref->VpiName();
    if (refname != name) result = getObject(refname, inst, pexpr);
    if (result) {
      if (UHDM::param_assign *passign = any_cast<param_assign *>(result)) {
        result = (any *)passign->Rhs();
      }
    }
  }
  return result;
}

long double ExprEval::get_double(bool &invalidValue, const UHDM::expr *expr) {
  long double result = 0;
  if (const UHDM::constant *c = any_cast<const UHDM::constant *>(expr)) {
    int type = c->VpiConstType();
    std::string v = c->VpiValue();
    switch (type) {
      case vpiRealConst: {
        result = std::strtold(v.c_str() + std::string_view("REAL:").length(),
                              nullptr);
        break;
      }
      default: {
        result = static_cast<long double>(get_value(invalidValue, expr));
        break;
      }
    }
  } else {
    invalidValue = true;
  }
  return result;
}

uint64_t ExprEval::getValue(const UHDM::expr *expr) {
  uint64_t result = 0;
  if (expr && expr->UhdmType() == UHDM::uhdmconstant) {
    UHDM::constant *c = (UHDM::constant *)expr;
    const std::string &v = c->VpiValue();
    int type = c->VpiConstType();
    switch (type) {
      case vpiBinaryConst: {
        result = std::strtoll(v.c_str() + strlen("BIN:"), 0, 2);
        break;
      }
      case vpiDecConst: {
        result = std::strtoll(v.c_str() + strlen("DEC:"), 0, 10);
        break;
      }
      case vpiHexConst: {
        result = std::strtoll(v.c_str() + strlen("HEX:"), 0, 16);
        break;
      }
      case vpiOctConst: {
        result = std::strtoll(v.c_str() + strlen("OCT:"), 0, 8);
        break;
      }
      case vpiIntConst: {
        result = std::strtoll(v.c_str() + strlen("INT:"), 0, 10);
        break;
      }
      case vpiUIntConst: {
        result = std::strtoull(v.c_str() + strlen("UINT:"), 0, 10);
        break;
      }
      default: {
        if (strstr(v.c_str(), "UINT:")) {
          result = std::strtoull(v.c_str() + strlen("UINT:"), 0, 10);
        } else {
          result = std::strtoll(v.c_str() + strlen("INT:"), 0, 10);
        }
        break;
      }
    }
  }
  return result;
}

void ExprEval::recursiveFlattening(Serializer &s, VectorOfany *flattened,
                                   const VectorOfany *ordered,
                                   std::vector<const typespec *> fieldTypes) {
  // Flattening
  int index = 0;
  for (any *op : *ordered) {
    if (op->UhdmType() == uhdmtagged_pattern) {
      tagged_pattern *tp = (tagged_pattern *)op;
      const typespec *ttp = tp->Typespec();
      UHDM_OBJECT_TYPE ttpt = ttp->UhdmType();
      switch (ttpt) {
        case uhdmint_typespec: {
          any *sop = (any *)tp->Pattern();
          flattened->push_back(sop);
          break;
        }
        case uhdmstring_typespec: {
          any *sop = (any *)tp->Pattern();
          UHDM_OBJECT_TYPE sopt = sop->UhdmType();
          if (sopt == uhdmoperation) {
            VectorOfany *operands = ((operation *)sop)->Operands();
            for (auto op1 : *operands) {
              bool substituted = false;
              if (op1->UhdmType() == uhdmtagged_pattern) {
                tagged_pattern *tp1 = (tagged_pattern *)op1;
                const typespec *ttp1 = tp1->Typespec();
                UHDM_OBJECT_TYPE ttpt1 = ttp1->UhdmType();
                if (ttpt1 == uhdmstring_typespec) {
                  if (ttp1->VpiName() == "default") {
                    const any *patt = tp1->Pattern();
                    const typespec *mold = fieldTypes[index];
                    operation *subst = s.MakeOperation();
                    VectorOfany *sops = s.MakeAnyVec();
                    subst->Operands(sops);
                    subst->VpiOpType(vpiConcatOp);
                    flattened->push_back(subst);
                    UHDM_OBJECT_TYPE moldtype = mold->UhdmType();
                    if (moldtype == uhdmstruct_typespec) {
                      struct_typespec *molds = (struct_typespec *)mold;
                      for (auto mem : *molds->Members()) {
                        if (mem) sops->push_back((any *)patt);
                      }
                    } else if (moldtype == uhdmlogic_typespec) {
                      logic_typespec *molds = (logic_typespec *)mold;
                      VectorOfrange *ranges = molds->Ranges();
                      for (range *r : *ranges) {
                        uint64_t from = getValue(r->Left_expr());
                        uint64_t to = getValue(r->Right_expr());
                        if (from > to) {
                          std::swap(from, to);
                        }
                        for (uint64_t i = from; i <= to; i++) {
                          sops->push_back((any *)patt);
                        }
                        // TODO: Multidimension
                        break;
                      }
                    }
                    substituted = true;
                    break;
                  }
                }
              } else if (op1->UhdmType() == uhdmoperation) {
                //                recursiveFlattening(s, flattened,
                //                ((operation*)op1)->Operands(), fieldTypes);
                //                substituted = true;
              }
              if (!substituted) {
                flattened->push_back(sop);
                break;
              }
            }
          } else {
            flattened->push_back(sop);
          }
          break;
        }
        default:
          flattened->push_back(op);
          break;
      }
    } else {
      flattened->push_back(op);
    }
    index++;
  }
}

expr *ExprEval::flattenPatternAssignments(Serializer &s, const typespec *tps,
                                          expr *exp) {
  expr *result = exp;
  if ((!exp) || (!tps)) {
    return result;
  }
  // Reordering
  if (exp->UhdmType() == uhdmoperation) {
    operation *op = (operation *)exp;
    if (op->VpiOpType() != vpiAssignmentPatternOp) {
      return result;
    }
    if (tps->UhdmType() != uhdmstruct_typespec) {
      return result;
    }
    if (op->VpiFlattened()) {
      return result;
    }
    struct_typespec *stps = (struct_typespec *)tps;
    std::vector<std::string> fieldNames;
    std::vector<const typespec *> fieldTypes;
    for (typespec_member *memb : *stps->Members()) {
      fieldNames.push_back(memb->VpiName());
      fieldTypes.push_back(memb->Typespec());
    }
    VectorOfany *orig = op->Operands();
    VectorOfany *ordered = s.MakeAnyVec();
    std::vector<any *> tmp(fieldNames.size());
    any *defaultOp = nullptr;
    int index = 0;
    for (auto oper : *orig) {
      if (oper->UhdmType() == uhdmtagged_pattern) {
        op->VpiFlattened(true);
        tagged_pattern *tp = (tagged_pattern *)oper;
        const typespec *ttp = tp->Typespec();
        const std::string &tname = ttp->VpiName();
        bool found = false;
        if (tname == "default") {
          defaultOp = oper;
          found = true;
        }
        for (unsigned int i = 0; i < fieldNames.size(); i++) {
          if (tname == fieldNames[i]) {
            tmp[i] = oper;
            found = true;
            break;
          }
        }
        if (found == false) {
          for (unsigned int i = 0; i < fieldTypes.size(); i++) {
            if (ttp->UhdmType() == fieldTypes[i]->UhdmType()) {
              tmp[i] = oper;
              found = true;
              break;
            }
          }
        }
        if (found == false) {
          s.GetErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY, tname, exp,
                              nullptr);
          return result;
        }
      } else {
        if (index < (int)tmp.size())
          tmp[index] = oper;
        else
          s.GetErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY,
                              "Out of bound!", exp, nullptr);
      }
      index++;
    }
    index = 0;
    for (auto op : tmp) {
      if (defaultOp) {
        if (op == nullptr) {
          op = defaultOp;
        }
      }
      if (op == nullptr) {
        s.GetErrorHandler()(ErrorType::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN,
                            fieldNames[index], exp, nullptr);
        return result;
      }
      ordered->push_back(op);
      index++;
    }
    op->Operands(ordered);
    // Flattening
    VectorOfany *flattened = s.MakeAnyVec();
    recursiveFlattening(s, flattened, ordered, fieldTypes);
    op->Operands(flattened);
  }
  return result;
}

void ExprEval::prettyPrint(Serializer &s, const any *object, uint32_t indent,
                           std::ostream &out) {
  if (object == nullptr) return;
  UHDM_OBJECT_TYPE type = object->UhdmType();
  for (uint32_t i = 0; i < indent; i++) {
    out << " ";
  }
  switch (type) {
    case uhdmconstant: {
      constant *c = (constant *)object;
      out << c->VpiDecompile();
      break;
    }
    case uhdmoperation: {
      operation *oper = (operation *)object;
      int opType = oper->VpiOpType();
      switch (opType) {
        case vpiConcatOp: {
          out << "{";
          for (uint32_t i = 0; i < oper->Operands()->size(); i++) {
            prettyPrint(s, oper->Operands()->at(i), 0, out);
            if (i < oper->Operands()->size() - 1) {
              out << ",";
            }
          }
          out << "}";
          break;
        }
        default:
          break;
      }
      break;
    }
    case uhdmref_obj: {
      out << object->VpiName();
      break;
    }
    default: {
      break;
    }
  }
}

uint64_t ExprEval::size(const any *typespec, bool &invalidValue,
                        const any *inst, const any *pexpr, bool full) {
  uint64_t bits = 0;
  UHDM::VectorOfrange *ranges = nullptr;
  if (typespec) {
    UHDM_OBJECT_TYPE ttps = typespec->UhdmType();
    switch (ttps) {
      case UHDM::uhdmarray_typespec: {
        array_typespec *lts = (array_typespec *)typespec;
        ranges = lts->Ranges();
        bits = size(lts->Elem_typespec(), invalidValue, inst, pexpr, full);
        break;
      }
      case UHDM::uhdmshort_real_typespec: {
        bits = 32;
        break;
      }
      case UHDM::uhdmreal_typespec: {
        bits = 32;
        break;
      }
      case UHDM::uhdmbyte_typespec: {
        bits = 8;
        break;
      }
      case UHDM::uhdmshort_int_typespec: {
        bits = 16;
        break;
      }
      case UHDM::uhdmint_typespec: {
        bits = 32;
        break;
      }
      case UHDM::uhdmlong_int_typespec: {
        bits = 64;
        break;
      }
      case UHDM::uhdminteger_typespec: {
        integer_typespec *itps = (integer_typespec *)typespec;
        if (itps->VpiValue().find("UINT:") != std::string::npos) {
          bits = std::strtoull(
              itps->VpiValue().c_str() + std::string_view("UINT:").length(),
              nullptr, 10);
        } else {
          bits = std::strtoll(
              itps->VpiValue().c_str() + std::string_view("INT:").length(),
              nullptr, 10);
        }
        break;
      }
      case UHDM::uhdmbit_typespec: {
        bits = 1;
        UHDM::bit_typespec *lts = (UHDM::bit_typespec *)typespec;
        ranges = lts->Ranges();
        break;
      }
      case UHDM::uhdmlogic_typespec: {
        bits = 1;
        UHDM::logic_typespec *lts = (UHDM::logic_typespec *)typespec;
        ranges = lts->Ranges();
        break;
      }
      case UHDM::uhdmlogic_net: {
        bits = 1;
        UHDM::logic_net *lts = (UHDM::logic_net *)typespec;
        ranges = lts->Ranges();
        break;
      }
      case UHDM::uhdmlogic_var: {
        bits = 1;
        UHDM::logic_var *lts = (UHDM::logic_var *)typespec;
        ranges = lts->Ranges();
        break;
      }
      case UHDM::uhdmbit_var: {
        bits = 1;
        UHDM::bit_var *lts = (UHDM::bit_var *)typespec;
        ranges = lts->Ranges();
        break;
      }
      case UHDM::uhdmbyte_var: {
        bits = 8;
        break;
      }
      case UHDM::uhdmstruct_var: {
        const UHDM::typespec *tp = ((struct_var *)typespec)->Typespec();
        bits += size(tp, invalidValue, inst, pexpr, full);
        break;
      }
      case UHDM::uhdmstruct_net: {
        const UHDM::typespec *tp = ((struct_net *)typespec)->Typespec();
        bits += size(tp, invalidValue, inst, pexpr, full);
        break;
      }
      case UHDM::uhdmstruct_typespec: {
        UHDM::struct_typespec *sts = (UHDM::struct_typespec *)typespec;
        UHDM::VectorOftypespec_member *members = sts->Members();
        if (members) {
          for (UHDM::typespec_member *member : *members) {
            bits += size(member->Typespec(), invalidValue, inst, pexpr, full);
          }
        }
        break;
      }
      case UHDM::uhdmenum_typespec: {
        const UHDM::enum_typespec *sts =
            any_cast<const UHDM::enum_typespec *>(typespec);
        if (sts)
          bits = size(sts->Base_typespec(), invalidValue, inst, pexpr, full);
        break;
      }
      case UHDM::uhdmunion_typespec: {
        UHDM::union_typespec *sts = (UHDM::union_typespec *)typespec;
        UHDM::VectorOftypespec_member *members = sts->Members();
        if (members) {
          for (UHDM::typespec_member *member : *members) {
            uint64_t max =
                size(member->Typespec(), invalidValue, inst, pexpr, full);
            if (max > bits) bits = max;
          }
        }
        break;
      }
      case uhdmunsupported_typespec:
        break;
      case uhdmconstant: {
        constant *c = (constant *)typespec;
        bits = c->VpiSize();
        break;
      }
      case uhdmenum_const: {
        enum_const *c = (enum_const *)typespec;
        bits = c->VpiSize();
        break;
      }
      case uhdmref_obj: {
        ref_obj *ref = (ref_obj *)typespec;
        if (const any *act = ref->Actual_group()) {
          bits = size(act, invalidValue, inst, pexpr, full);
        }
        break;
      }
      case uhdmoperation: {
        operation *op = (operation *)typespec;
        if (op->VpiOpType() == vpiConcatOp) {
          if (auto ops = op->Operands()) {
            for (auto op : *ops) {
              bits += size(op, invalidValue, inst, pexpr, full);
            }
          }
        }
        break;
      }
      case uhdmpacked_array_typespec: {
        packed_array_typespec *tmp = (packed_array_typespec *)typespec;
        const UHDM::typespec *tps = (UHDM::typespec *)tmp->Elem_typespec();
        bits += size(tps, invalidValue, inst, pexpr, full);
        ranges = tmp->Ranges();
        break;
      }
      case uhdmtypespec_member: {
        typespec_member *tmp = (typespec_member *)typespec;
        bits += size(tmp->Typespec(), invalidValue, inst, pexpr, full);
        break;
      }
      case uhdmio_decl: {
        io_decl *decl = (io_decl *)typespec;
        const UHDM::typespec *tps = decl->Typespec();
        bits += size(tps, invalidValue, inst, pexpr, full);
        break;
      }
      default:
        break;
    }
  }
  if (ranges) {
    if (!full) {
      UHDM::range *last_range = nullptr;
      for (UHDM::range *ran : *ranges) {
        last_range = ran;
      }
      expr *lexpr = (expr *)last_range->Left_expr();
      expr *rexpr = (expr *)last_range->Right_expr();
      int64_t lv = getValue(reduceExpr(lexpr, invalidValue, inst, pexpr));

      int64_t rv = getValue(reduceExpr(rexpr, invalidValue, inst, pexpr));

      if (lv > rv)
        bits = bits * (lv - rv + 1);
      else
        bits = bits * (rv - lv + 1);
    } else {
      for (UHDM::range *ran : *ranges) {
        expr *lexpr = (expr *)ran->Left_expr();
        expr *rexpr = (expr *)ran->Right_expr();
        int64_t lv = getValue(reduceExpr(lexpr, invalidValue, inst, pexpr));

        int64_t rv = getValue(reduceExpr(rexpr, invalidValue, inst, pexpr));

        if (lv > rv)
          bits = bits * (lv - rv + 1);
        else
          bits = bits * (rv - lv + 1);
      }
    }
  }
  return bits;
}

static bool getStringVal(std::string &result, expr *val) {
  const UHDM::constant *hs0 = any_cast<const UHDM::constant *>(val);
  if (hs0) {
    s_vpi_value *sval = String2VpiValue(hs0->VpiValue());
    if (sval) {
      if (sval->format == vpiStringVal || sval->format == vpiBinStrVal) {
        result = sval->value.str;
        return true;
      }
    }
  }
  return false;
}

expr *ExprEval::reduceCompOp(operation *op, bool &invalidValue, const any *inst,
                             const any *pexpr) {
  expr *result = op;
  Serializer &s = *op->GetSerializer();
  UHDM::VectorOfany &operands = *op->Operands();
  int optype = op->VpiOpType();
  std::string s0;
  std::string s1;
  expr *reduc0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
  expr *reduc1 = reduceExpr(operands[1], invalidValue, inst, pexpr);
  bool arg0isString = getStringVal(s0, reduc0);
  bool arg1isString = getStringVal(s1, reduc1);
  bool invalidValueI = false;
  bool invalidValueD = false;
  bool invalidValueS = true;
  uint64_t val = 0;
  if (arg0isString && arg1isString) {
    invalidValueS = false;
    switch (optype) {
      case vpiEqOp:
        val = (s0 == s1);
        break;
      case vpiNeqOp:
        val = (s0 != s1);
        break;
      default:
        break;
    }
  } else {
    int64_t v0 = get_value(invalidValueI, reduc0);
    int64_t v1 = get_value(invalidValueI, reduc1);
    if ((invalidValue == false) && (invalidValueI == false)) {
      switch (optype) {
        case vpiEqOp:
          val = (v0 == v1);
          break;
        case vpiNeqOp:
          val = (v0 != v1);
          break;
        case vpiGtOp:
          val = (v0 > v1);
          break;
        case vpiGeOp:
          val = (v0 >= v1);
          break;
        case vpiLtOp:
          val = (v0 < v1);
          break;
        case vpiLeOp:
          val = (v0 <= v1);
          break;
        default:
          break;
      }
    } else {
      invalidValueD = false;
      long double v0 = get_double(invalidValueD, reduc0);
      long double v1 = get_double(invalidValueD, reduc1);
      if ((invalidValue == false) && (invalidValueD == false)) {
        switch (optype) {
          case vpiEqOp:
            val = (v0 == v1);
            break;
          case vpiNeqOp:
            val = (v0 != v1);
            break;
          case vpiGtOp:
            val = (v0 > v1);
            break;
          case vpiGeOp:
            val = (v0 >= v1);
            break;
          case vpiLtOp:
            val = (v0 < v1);
            break;
          case vpiLeOp:
            val = (v0 <= v1);
            break;
          default:
            break;
        }
      }
    }
  }
  if (invalidValueI && invalidValueD && invalidValueS) {
    invalidValue = true;
  } else {
    UHDM::constant *c = s.MakeConstant();
    c->VpiValue("UINT:" + std::to_string(val));
    c->VpiDecompile(std::to_string(val));
    c->VpiSize(64);
    c->VpiConstType(vpiUIntConst);
    result = c;
  }
  return result;
}

static uint64_t getMask(uint64_t wide) {
  uint64_t mask = 0;
  uint64_t sizeInBits = sizeof(mask) * 8;
  mask = (wide >= sizeInBits)
             ? ((uint64_t)-1)
             : ((uint64_t)((uint64_t)(((uint64_t)1) << ((uint64_t)wide))) -
                (uint64_t)1);
  return mask;
}

static std::string trimLeadingZeros(const std::string &s) {
  const uint64_t sSize = s.size();
  std::string res;
  bool nonZero = false;
  for (unsigned int i = 0; i < sSize; i++) {
    const char c = s[i];
    if (c != '0') nonZero = true;
    if (nonZero) res += c;
  }
  return res;
}

static std::string hexToBin(const std::string &s) {
  std::string out;
  for (auto i : s) {
    uint8_t n;
    if ((i <= '9') && (i >= '0'))
      n = i - '0';
    else
      n = 10 + i - 'A';
    for (int8_t j = 3; j >= 0; --j) out.push_back((n & (1 << j)) ? '1' : '0');
  }
  out = trimLeadingZeros(out);
  return out;
}

static std::string toBinary(int size, uint64_t val) {
  int constexpr bitFieldSize = 100;
  std::string tmp = std::bitset<bitFieldSize>(val).to_string();
  if (size <= 0) {
    for (unsigned int i = 0; i < bitFieldSize; i++) {
      if (tmp[i] == '1') {
        size = bitFieldSize - i;
        break;
      }
    }
  }
  std::string result;
  for (unsigned int i = bitFieldSize - size; i < bitFieldSize; i++)
    result += tmp[i];
  return result;
}

expr *ExprEval::reduceBitSelect(expr *op, unsigned int index_val,
                                bool &invalidValue, const any *inst,
                                const any *pexpr) {
  Serializer &s = *op->GetSerializer();
  expr *result = nullptr;
  expr *exp = reduceExpr(op, invalidValue, inst, pexpr);
  if (exp && (exp->UhdmType() == uhdmconstant)) {
    std::string binary;
    constant *cexp = (constant *)exp;
    if (cexp->VpiConstType() == vpiBinaryConst) {
      binary = cexp->VpiValue();
      binary = binary.erase(0, 4);
      std::reverse(binary.begin(), binary.end());
    } else {
      int64_t val = get_value(invalidValue, exp);
      binary = toBinary(exp->VpiSize(), val);
    }
    uint64_t wordSize = 1;
    if (typespec *cts = (typespec *)cexp->Typespec()) {
      if (cts->UhdmType() == uhdmint_typespec) {
        int_typespec *icts = (int_typespec *)cts;
        wordSize = std::strtoull(
            icts->VpiValue().c_str() + std::string_view("UINT:").length(),
            nullptr, 10);
      }
    }
    constant *c = s.MakeConstant();
    unsigned short lr = 0;
    unsigned short rr = 0;
    if (const typespec *tps = exp->Typespec()) {
      if (tps->UhdmType() == uhdmlogic_typespec) {
        logic_typespec *lts = (logic_typespec *)tps;
        VectorOfrange *ranges = lts->Ranges();
        if (ranges) {
          range *r = ranges->at(0);
          bool invalidValue = false;
          lr = static_cast<unsigned short>(get_value(invalidValue, r->Left_expr()));
          rr = static_cast<unsigned short>(get_value(invalidValue, r->Right_expr()));
        }
      }
    }
    c->VpiSize(static_cast<int>(wordSize));
    if (index_val < binary.size()) {
      // TODO: If range does not start at 0
      if (lr >= rr) {
        index_val = static_cast<unsigned int>(binary.size() - ((index_val + 1) * wordSize));
      }
      std::string v;
      for (unsigned int i = 0; i < wordSize; i++) {
        if ((index_val + i) < binary.size()) {
          char bitv = binary[index_val + i];
          v += std::to_string(bitv - '0');
        }
      }
      if (v.size() > UHDM_MAX_BIT_WIDTH) {
        std::string fullPath;
        if (const gen_scope_array *in = any_cast<gen_scope_array *>(inst)) {
          fullPath = in->VpiFullName();
        } else if (inst->UhdmType() == uhdmdesign) {
          fullPath = inst->VpiName();
        } else if (any_cast<scope *>(inst)) {
          fullPath = ((scope *)inst)->VpiFullName();
        }
        s.GetErrorHandler()(ErrorType::UHDM_INTERNAL_ERROR_OUT_OF_BOUND, fullPath, op,
                            nullptr);
        v = "0";
      }
      std::reverse(v.begin(), v.end());
      c->VpiValue("BIN:" + v);
      c->VpiDecompile(std::to_string(wordSize) + "'b" + v);
      c->VpiConstType(vpiBinaryConst);
    } else {
      c->VpiValue("BIN:0");
      c->VpiDecompile("1'b0");
      c->VpiConstType(vpiBinaryConst);
    }
    c->VpiFile(op->VpiFile());
    c->VpiLineNo(op->VpiLineNo());
    c->VpiColumnNo(op->VpiColumnNo());
    c->VpiEndLineNo(op->VpiEndLineNo());
    c->VpiEndColumnNo(op->VpiColumnNo() + 1);
    result = c;
  }
  return result;
}

static std::string &ltrim(std::string &str, char c) {
  auto it1 =
      std::find_if(str.begin(), str.end(), [c](char ch) { return (ch == c); });
  if (it1 != str.end()) str.erase(str.begin(), it1 + 1);
  return str;
}

int64_t ExprEval::get_value(bool &invalidValue, const UHDM::expr *expr) {
  int64_t result = 0;
  int type = 0;
  std::string v;
  if (const UHDM::constant *c = any_cast<const UHDM::constant *>(expr)) {
    type = c->VpiConstType();
    v = c->VpiValue();
  } else if (const UHDM::variables *c =
                 any_cast<const UHDM::variables *>(expr)) {
    if (c->UhdmType() == uhdmenum_var) {
      type = vpiUIntConst;
      v = c->VpiValue();
    }
  } else {
    invalidValue = true;
  }
  if (!invalidValue) {
    switch (type) {
      case vpiBinaryConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          ltrim(v, '\'');
          ltrim(v, 's');
          ltrim(v, 'b');
          try {
            result = std::strtoll(v.c_str() + std::string_view("BIN:").length(),
                                  nullptr, 2);
          } catch (...) {
            invalidValue = true;
          }
        }
        break;
      }
      case vpiDecConst: {
        try {
          result = std::strtoll(v.c_str() + std::string_view("DEC:").length(),
                                nullptr, 10);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiHexConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          ltrim(v, '\'');
          ltrim(v, 's');
          ltrim(v, 'h');
          try {
            result = std::strtoll(v.c_str() + std::string_view("HEX:").length(),
                                  nullptr, 16);
          } catch (...) {
            invalidValue = true;
          }
        }
        break;
      }
      case vpiOctConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          ltrim(v, '\'');
          ltrim(v, 's');
          ltrim(v, 'o');
          try {
            result = std::strtoll(v.c_str() + std::string_view("OCT:").length(),
                                  nullptr, 8);
          } catch (...) {
            invalidValue = true;
          }
        }
        break;
      }
      case vpiIntConst: {
        try {
          result = std::strtoll(v.c_str() + std::string_view("INT:").length(),
                                nullptr, 10);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiUIntConst: {
        try {
          result = std::strtoull(v.c_str() + std::string_view("UINT:").length(),
                                 nullptr, 10);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiScalar: {
        try {
          result = std::strtoll(v.c_str() + std::string_view("SCAL:").length(),
                                nullptr, 2);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiStringConst: {
        result = 0;
        break;
      }
      case vpiRealConst: {
        // Don't do the double precision math, leave it to client tools
        invalidValue = true;
        break;
      }
      default: {
        try {
          if (v.find("UINT:") != std::string::npos) {
            result = std::strtoull(
                v.c_str() + std::string_view("UINT:").length(), nullptr, 10);
          } else if (v.find("INT:") != std::string::npos) {
            result = std::strtoll(v.c_str() + std::string_view("INT:").length(),
                                  nullptr, 10);
          } else {
            invalidValue = true;
          }
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
    }
  }
  return result;
}

uint64_t ExprEval::get_uvalue(bool &invalidValue, const UHDM::expr *expr) {
  uint64_t result = 0;
  int type = 0;
  std::string v;
  if (const UHDM::constant *c = any_cast<const UHDM::constant *>(expr)) {
    type = c->VpiConstType();
    v = c->VpiValue();
  } else if (const UHDM::variables *c =
                 any_cast<const UHDM::variables *>(expr)) {
    if (c->UhdmType() == uhdmenum_var) {
      type = vpiUIntConst;
      v = c->VpiValue();
    }
  } else {
    invalidValue = true;
  }
  if (!invalidValue) {
    switch (type) {
      case vpiBinaryConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          ltrim(v, '\'');
          ltrim(v, 's');
          ltrim(v, 'b');
          try {
            result = std::strtoll(v.c_str() + std::string_view("BIN:").length(),
                                  nullptr, 2);
          } catch (...) {
            invalidValue = true;
          }
        }
        break;
      }
      case vpiDecConst: {
        try {
          result = std::strtoll(v.c_str() + std::string_view("DEC:").length(),
                                nullptr, 10);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiHexConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          ltrim(v, '\'');
          ltrim(v, 's');
          ltrim(v, 'h');
          try {
            result = std::strtoll(v.c_str() + std::string_view("HEX:").length(),
                                  nullptr, 16);
          } catch (...) {
            invalidValue = true;
          }
        }
        break;
      }
      case vpiOctConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          ltrim(v, '\'');
          ltrim(v, 's');
          ltrim(v, 'o');
          try {
            result = std::strtoll(v.c_str() + std::string_view("OCT:").length(),
                                  nullptr, 8);
          } catch (...) {
            invalidValue = true;
          }
        }
        break;
      }
      case vpiIntConst: {
        try {
          result = std::strtoll(v.c_str() + std::string_view("INT:").length(),
                                nullptr, 10);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiUIntConst: {
        try {
          result = std::strtoull(v.c_str() + std::string_view("UINT:").length(),
                                 nullptr, 10);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiScalar: {
        try {
          result = std::strtoll(v.c_str() + std::string_view("SCAL:").length(),
                                nullptr, 2);
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
      case vpiStringConst: {
        result = 0;
        break;
      }
      case vpiRealConst: {
        // Don't do the double precision math, leave it to client tools
        invalidValue = true;
        break;
      }
      default: {
        try {
          if (v.find("UINT:") != std::string::npos) {
            result = std::strtoull(
                v.c_str() + std::string_view("UINT:").length(), nullptr, 10);
          } else if (v.find("INT:") != std::string::npos) {
            result = std::strtoll(v.c_str() + std::string_view("INT:").length(),
                                  nullptr, 10);
          } else {
            invalidValue = true;
          }
        } catch (...) {
          invalidValue = true;
        }
        break;
      }
    }
  }
  return result;
}

expr *ExprEval::reduceExpr(const any *result, bool &invalidValue,
                           const any *inst, const any *pexpr) {
  Serializer &s = *result->GetSerializer();
  UHDM_OBJECT_TYPE objtype = result->UhdmType();
  if (objtype == uhdmoperation) {
    UHDM::operation *op = (UHDM::operation *)result;
    UHDM::VectorOfany *operands = op->Operands();
    bool constantOperands = true;
    if (operands) {
      for (auto oper : *operands) {
        UHDM_OBJECT_TYPE optype = oper->UhdmType();
        if (optype == uhdmref_obj) {
          ref_obj *ref = (ref_obj *)oper;
          const std::string &name = ref->VpiName();
          any *tmp = getValue(name, inst, pexpr);
          if (!tmp) {
            constantOperands = false;
            break;
          }
        } else if (optype == uhdmoperation) {
        } else if (optype == uhdmsys_func_call) {
        } else if (optype == uhdmfunc_call) {
        } else if (optype == uhdmbit_select) {
        } else if (optype == uhdmhier_path) {
        } else if (optype == uhdmvar_select) {
        } else if (optype == uhdmenum_var) {
        } else if (optype != uhdmconstant) {
          constantOperands = false;
          break;
        }
      }
      if (constantOperands) {
        UHDM::VectorOfany &operands = *op->Operands();
        int optype = op->VpiOpType();
        switch (optype) {
          case vpiArithRShiftOp:
          case vpiRShiftOp: {
            if (operands.size() == 2) {
              int64_t val0 =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              int64_t val1 =
                  get_value(invalidValue,
                            reduceExpr(operands[1], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) >> ((uint64_t)val1);
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val));
              c->VpiDecompile(std::to_string(val));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiLeOp:
          case vpiLtOp:
          case vpiGeOp:
          case vpiGtOp:
          case vpiNeqOp:
          case vpiEqOp: {
            if (operands.size() == 2) {
              result = reduceCompOp(op, invalidValue, inst, pexpr);
            }
            break;
          }
          case vpiPostIncOp:
          case vpiPostDecOp:
          case vpiPreDecOp:
          case vpiPreIncOp: {
            if (operands.size() == 1) {
              expr *reduc0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val = get_value(invalidValueI, reduc0);
              if ((invalidValue == false) && (invalidValueI == false)) {
                if (op->VpiOpType() == vpiPostIncOp ||
                    op->VpiOpType() == vpiPreIncOp) {
                  val++;
                } else {
                  val--;
                }
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val = get_double(invalidValueD, reduc0);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  if (op->VpiOpType() == vpiPostIncOp ||
                      op->VpiOpType() == vpiPreIncOp) {
                    val++;
                  } else {
                    val--;
                  }
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                }
              }
            }
            break;
          }
          case vpiArithLShiftOp:
          case vpiLShiftOp: {
            if (operands.size() == 2) {
              int64_t val0 =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              int64_t val1 =
                  get_value(invalidValue,
                            reduceExpr(operands[1], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) << ((uint64_t)val1);
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val));
              c->VpiDecompile(std::to_string(val));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiAddOp:
          case vpiPlusOp: {
            if (operands.size() == 2) {
              expr *expr0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
              expr *expr1 = reduceExpr(operands[1], invalidValue, inst, pexpr);
              bool unsignedOperation = true;
              for (auto exp : {expr0, expr1}) {
                if (exp) {
                  if (exp->UhdmType() == uhdmconstant) {
                    constant *c = (constant *)exp;
                    if (c->VpiConstType() == vpiIntConst ||
                        c->VpiConstType() == vpiStringConst ||
                        c->VpiConstType() == vpiRealConst ||
                        c->VpiConstType() == vpiDecConst) {
                      unsignedOperation = false;
                    }
                  }
                }
              }
              bool invalidValueI = false;
              bool invalidValueD = false;
              if (unsignedOperation) {
                uint64_t val0 = get_uvalue(invalidValueI, expr0);
                uint64_t val1 = get_uvalue(invalidValueI, expr1);
                if ((invalidValue == false) && (invalidValueI == false)) {
                  uint64_t val = val0 + val1;
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("UINT:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiUIntConst);
                  result = c;
                }
              } else {
                int64_t val0 = get_value(invalidValueI, expr0);
                int64_t val1 = get_value(invalidValueI, expr1);
                if ((invalidValue == false) && (invalidValueI == false)) {
                  int64_t val = val0 + val1;
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("INT:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiIntConst);
                  result = c;
                } else {
                  invalidValueD = false;
                  long double val0 = get_double(invalidValueD, expr0);
                  long double val1 = get_double(invalidValueD, expr1);
                  if ((invalidValue == false) && (invalidValueD == false)) {
                    long double val = val0 + val1;
                    UHDM::constant *c = s.MakeConstant();
                    c->VpiValue("REAL:" + std::to_string(val));
                    c->VpiDecompile(std::to_string(val));
                    c->VpiSize(64);
                    c->VpiConstType(vpiRealConst);
                    result = c;
                  }
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiBitOrOp: {
            if (operands.size() == 2) {
              int64_t val0 =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              int64_t val1 =
                  get_value(invalidValue,
                            reduceExpr(operands[1], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) | ((uint64_t)val1);
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val));
              c->VpiDecompile(std::to_string(val));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiBitAndOp: {
            if (operands.size() == 2) {
              int64_t val0 =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              int64_t val1 =
                  get_value(invalidValue,
                            reduceExpr(operands[1], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) & ((uint64_t)val1);
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val));
              c->VpiDecompile(std::to_string(val));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiLogOrOp: {
            if (operands.size() == 2) {
              int64_t val0 =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              int64_t val1 =
                  get_value(invalidValue,
                            reduceExpr(operands[1], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) || ((uint64_t)val1);
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val));
              c->VpiDecompile(std::to_string(val));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiLogAndOp: {
            if (operands.size() == 2) {
              int64_t val0 =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              int64_t val1 =
                  get_value(invalidValue,
                            reduceExpr(operands[1], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) && ((uint64_t)val1);
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val));
              c->VpiDecompile(std::to_string(val));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiMinusOp: {
            if (operands.size() == 1) {
              bool invalidValueI = false;
              bool invalidValueD = false;
              expr *expr0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
              int64_t val0 = get_value(invalidValueI, expr0);
              if ((invalidValue == false) && (invalidValueI == false)) {
                int64_t val = -val0;
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = -val0;
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiSubOp: {
            if (operands.size() == 2) {
              expr *expr0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
              expr *expr1 = reduceExpr(operands[1], invalidValue, inst, pexpr);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              if ((invalidValue == false) && (invalidValueI == false)) {
                int64_t val = val0 - val1;
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = val0 - val1;
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiMultOp: {
            if (operands.size() == 2) {
              expr *expr0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
              expr *expr1 = reduceExpr(operands[1], invalidValue, inst, pexpr);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              if ((invalidValue == false) && (invalidValueI == false)) {
                int64_t val = val0 * val1;
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = val0 * val1;
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiBitNegOp: {
            if (operands.size() == 1) {
              expr *operand =
                  reduceExpr(operands[0], invalidValue, inst, pexpr);
              if (operand) {
                uint64_t val = (uint64_t)get_value(invalidValue, operand);
                if (invalidValue) break;
                int size = 64;
                if (operand->UhdmType() == uhdmconstant) {
                  constant *c = (constant *)operand;
                  size = c->VpiSize();
                  if (size == 1) {
                    val = !val;
                  } else {
                    uint64_t mask = getMask(size);
                    val = ~val;
                    val = val & mask;
                  }
                } else {
                  val = ~val;
                }

                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("UINT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(size);
                c->VpiConstType(vpiUIntConst);
                result = c;
              }
            }
            break;
          }
          case vpiNotOp: {
            if (operands.size() == 1) {
              uint64_t val = !((uint64_t)get_value(
                  invalidValue,
                  reduceExpr(operands[0], invalidValue, inst, pexpr)));
              if (invalidValue) break;
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val));
              c->VpiDecompile(std::to_string(val));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiInsideOp: {
            if (operands.size() > 1) {
              int64_t val =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              if (invalidValue) break;
              for (unsigned int i = 1; i < operands.size(); i++) {
                int64_t oval = get_value(
                    invalidValue,
                    reduceExpr(operands[i], invalidValue, inst, pexpr));
                if (invalidValue) break;
                if (oval == val) {
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("UINT:1");
                  c->VpiDecompile(std::to_string(1));
                  c->VpiSize(64);
                  c->VpiConstType(vpiUIntConst);
                  result = c;
                  break;
                }
              }
            }
            break;
          }
          case vpiUnaryAndOp: {
            if (operands.size() == 1) {
              constant *cst = (constant *)(reduceExpr(operands[0], invalidValue,
                                                      inst, pexpr));
              uint64_t val = get_value(invalidValue, cst);
              if (invalidValue) break;
              uint64_t res = val & 1;
              for (int i = 1; i < cst->VpiSize(); i++) {
                res = res & ((val & (1ULL << i)) >> i);
              }
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(res));
              c->VpiDecompile(std::to_string(res));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiUnaryNandOp: {
            if (operands.size() == 1) {
              uint64_t val =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t res = val & 1;
              for (unsigned int i = 1; i < 32; i++) {
                res = res & ((val & (1ULL << i)) >> i);
              }
              res = !res;
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(res));
              c->VpiDecompile(std::to_string(res));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiUnaryOrOp: {
            if (operands.size() == 1) {
              uint64_t val =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t res = val & 1;
              for (unsigned int i = 1; i < 32; i++) {
                res = res | ((val & (1ULL << i)) >> i);
              }
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(res));
              c->VpiDecompile(std::to_string(res));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiUnaryNorOp: {
            if (operands.size() == 1) {
              uint64_t val =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t res = val & 1;
              for (unsigned int i = 1; i < 64; i++) {
                res = res | ((val & (1ULL << i)) >> i);
              }
              res = !res;
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(res));
              c->VpiDecompile(std::to_string(res));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiUnaryXorOp: {
            if (operands.size() == 1) {
              uint64_t val =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t res = val & 1;
              for (unsigned int i = 1; i < 64; i++) {
                res = res ^ ((val & (1ULL << i)) >> i);
              }
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(res));
              c->VpiDecompile(std::to_string(res));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiUnaryXNorOp: {
            if (operands.size() == 1) {
              uint64_t val =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              if (invalidValue) break;
              uint64_t res = val & 1;
              for (unsigned int i = 1; i < 64; i++) {
                res = res ^ ((val & (1ULL << i)) >> i);
              }
              res = !res;
              UHDM::constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(res));
              c->VpiDecompile(std::to_string(res));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiModOp: {
            if (operands.size() == 2) {
              expr *expr0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
              expr *expr1 = reduceExpr(operands[1], invalidValue, inst, pexpr);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              int64_t val = 0;
              if (val1 && (invalidValue == false) && (invalidValueI == false)) {
                val = val0 % val1;
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if (val1 && (invalidValue == false) &&
                    (invalidValueD == false)) {
                  long double val = 0;
                  val = std::fmod(val0, val1);
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                }
                if ((val1 == 0) && (invalidValue == false) &&
                    (invalidValueD == false)) {
                  // Divide by 0
                  std::string fullPath;
                  if (const gen_scope_array *in =
                          any_cast<gen_scope_array *>(inst)) {
                    fullPath = in->VpiFullName();
                  } else if (inst->UhdmType() == uhdmdesign) {
                    fullPath = inst->VpiName();
                  } else if (any_cast<scope*>(inst)) {
                    fullPath = ((scope *)inst)->VpiFullName();
                  }
                  s.GetErrorHandler()(ErrorType::UHDM_DIVIDE_BY_ZERO, fullPath,
                                      expr1, nullptr);
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiPowerOp: {
            if (operands.size() == 2) {
              expr *expr0 = reduceExpr(operands[0], invalidValue, inst, pexpr);
              expr *expr1 = reduceExpr(operands[1], invalidValue, inst, pexpr);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              int64_t val = 0;
              if ((invalidValue == false) && (invalidValueI == false)) {
                val = static_cast<int64_t>(std::pow<int64_t>(val0, val1));
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = 0;
                  val = pow(val0, val1);
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiDivOp: {
            if (operands.size() == 2) {
              bool divideByZero = true;
              expr *div_expr =
                  reduceExpr(operands[1], invalidValue, inst, pexpr);
              expr *num_expr =
                  reduceExpr(operands[0], invalidValue, inst, pexpr);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t divisor = get_value(invalidValueI, div_expr);
              int64_t num = get_value(invalidValueI, num_expr);
              if (divisor && (invalidValue == false) &&
                  (invalidValueI == false)) {
                divideByZero = false;
                int64_t val = num / divisor;
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double divisor = get_double(invalidValueD, div_expr);
                long double num = get_double(invalidValueD, num_expr);
                if (divisor && (invalidValue == false) &&
                    (invalidValueD == false)) {
                  divideByZero = false;
                  long double val = num / divisor;
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                }
                if (divisor) {
                  divideByZero = false;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
              if (divideByZero) {
                // Divide by 0
                std::string fullPath;
                if (const gen_scope_array *in =
                        any_cast<gen_scope_array *>(inst)) {
                  fullPath = in->VpiFullName();
                } else if (inst->UhdmType() == uhdmdesign) {
                  fullPath = inst->VpiName();
                } else if (any_cast<scope*>(inst)) {
                  fullPath = ((scope *)inst)->VpiFullName();
                }
                s.GetErrorHandler()(ErrorType::UHDM_DIVIDE_BY_ZERO, fullPath,
                                    div_expr, nullptr);
              }
            }
            break;
          }
          case vpiConditionOp: {
            if (operands.size() == 3) {
              bool localInvalidValue = false;
              expr *cond = reduceExpr(operands[0], invalidValue, inst, pexpr);
              int64_t condVal = get_value(invalidValue, cond);
              if (invalidValue) break;
              int64_t val = 0;
              expr *the_val = nullptr;
              if (condVal) {
                the_val =
                    reduceExpr(operands[1], localInvalidValue, inst, pexpr);
              } else {
                the_val =
                    reduceExpr(operands[2], localInvalidValue, inst, pexpr);
              }
              if (localInvalidValue == false) {
                val = get_value(localInvalidValue, the_val);
                if (localInvalidValue == false) {
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("INT:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiIntConst);
                  result = c;
                } else {
                  result = the_val;
                }
              } else {
                result = the_val;
              }
            }
            break;
          }
          case vpiMultiConcatOp: {
            if (operands.size() == 2) {
              int64_t n =
                  get_value(invalidValue,
                            reduceExpr(operands[0], invalidValue, inst, pexpr));
              if (invalidValue) break;
              if (n > 1000) n = 1000;  // Must be -1 or something silly
              if (n < 0) n = 0;
              expr *cv = (expr *)(operands[1]);
              if (cv->UhdmType() != uhdmconstant) {
                cv = reduceExpr(cv, invalidValue, inst, pexpr);
                if (cv->UhdmType() != uhdmconstant) {
                  break;
                }
              }
              UHDM::constant *c = s.MakeConstant();
              unsigned int width = cv->VpiSize();
              int consttype = ((UHDM::constant *)cv)->VpiConstType();
              c->VpiConstType(consttype);
              if (consttype == vpiBinaryConst) {
                std::string val = cv->VpiValue();
                std::string res;
                std::string tmp =
                    val.c_str() + std::string_view("BIN:").length();
                std::string value;
                if (width > tmp.size()) {
                  for (unsigned int i = 0; i < width - tmp.size(); i++) {
                    value += '0';
                  }
                }
                value += tmp;
                for (unsigned int i = 0; i < n; i++) {
                  res += value;
                }
                c->VpiValue("BIN:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiHexConst) {
                std::string val = cv->VpiValue();
                std::string res;
                for (unsigned int i = 0; i < n; i++) {
                  res += val.c_str() + std::string_view("HEX:").length();
                }
                c->VpiValue("HEX:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiOctConst) {
                std::string val = cv->VpiValue();
                std::string res;
                for (unsigned int i = 0; i < n; i++) {
                  res += val.c_str() + std::string_view("OCT:").length();
                }
                c->VpiValue("OCT:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiStringConst) {
                std::string val = cv->VpiValue();
                std::string res;
                for (unsigned int i = 0; i < n; i++) {
                  res += val.c_str() + std::string_view("STRING:").length();
                }
                c->VpiValue("STRING:" + res);
                c->VpiDecompile(res);
              } else {
                uint64_t val = get_value(invalidValue, cv);
                if (invalidValue) break;
                uint64_t res = 0;
                for (unsigned int i = 0; i < n; i++) {
                  res |= val << (i * width);
                }
                c->VpiValue("UINT:" + std::to_string(res));
                c->VpiDecompile(std::to_string(res));
                c->VpiConstType(vpiUIntConst);
              }
              c->VpiSize(static_cast<int>(n * width));
              // Word size
              if (width) {
                int_typespec *ts = s.MakeInt_typespec();
                ts->VpiValue("UINT:" + std::to_string(width));
                c->Typespec(ts);
              }
              result = c;
            }
            break;
          }
          case vpiConcatOp: {
            UHDM::constant *c1 = s.MakeConstant();
            std::string cval;
            int csize = 0;
            bool stringVal = false;
            for (unsigned int i = 0; i < operands.size(); i++) {
              any *oper = operands[i];
              UHDM_OBJECT_TYPE optype = oper->UhdmType();
              int operType = 0;
              if (optype == uhdmoperation) {
                operation *o = (operation *)oper;
                operType = o->VpiOpType();
              }
              if ((optype != uhdmconstant) && (operType != vpiConcatOp) &&
                  (operType != vpiMultiAssignmentPatternOp) &&
                  (operType != vpiAssignmentPatternOp)) {
                if (expr *tmp = reduceExpr(oper, invalidValue, inst, pexpr)) {
                  oper = tmp;
                }
                optype = op->UhdmType();
              }
              if (optype == uhdmconstant) {
                constant *c2 = (constant *)oper;
                std::string v = c2->VpiValue();
                unsigned int size = c2->VpiSize();
                csize += size;
                int type = c2->VpiConstType();
                switch (type) {
                  case vpiBinaryConst: {
                    std::string tmp =
                        v.c_str() + std::string_view("BIN:").length();
                    std::string value;
                    if (size > tmp.size()) {
                      for (unsigned int i = 0; i < size - tmp.size(); i++) {
                        value += '0';
                      }
                    }
                    if (op->VpiReordered()) {
                      std::reverse(tmp.begin(), tmp.end());
                    }
                    value += tmp;
                    cval += value;
                    break;
                  }
                  case vpiDecConst: {
                    if (operands.size() == 1) {
                      long long iv = std::strtoll(
                          v.c_str() + std::string_view("DEC:").length(),
                          nullptr, 10);
                      std::string bin = toBinary(size, iv);
                      if (op->VpiReordered()) {
                        std::reverse(bin.begin(), bin.end());
                      }
                      cval += bin;
                    } else {
                      c1 = nullptr;
                    }
                    break;
                  }
                  case vpiHexConst: {
                    std::string tmp =
                        hexToBin(v.c_str() + std::string_view("HEX:").length());
                    std::string value;
                    if (size > tmp.size()) {
                      for (unsigned int i = 0; i < size - tmp.size(); i++) {
                        value += '0';
                      }
                    }
                    if (op->VpiReordered()) {
                      std::reverse(tmp.begin(), tmp.end());
                    }
                    value += tmp;
                    cval += value;
                    break;
                  }
                  case vpiOctConst: {
                    long long iv = std::strtoll(
                        v.c_str() + std::string_view("OCT:").length(), nullptr,
                        8);
                    std::string bin = toBinary(size, iv);
                    if (op->VpiReordered()) {
                      std::reverse(bin.begin(), bin.end());
                    }
                    cval += bin;
                    break;
                  }
                  case vpiIntConst: {
                    if (operands.size() == 1 || (size != 64)) {
                      int64_t iv = std::strtoll(
                          v.c_str() + std::string_view("INT:").length(),
                          nullptr, 10);
                      std::string bin = toBinary(size, iv);
                      if (op->VpiReordered()) {
                        std::reverse(bin.begin(), bin.end());
                      }
                      cval += bin;
                    } else {
                      c1 = nullptr;
                    }
                    break;
                  }
                  case vpiUIntConst: {
                    if (operands.size() == 1 || (size != 64)) {
                      uint64_t iv = std::strtoull(
                          v.c_str() + std::string_view("UINT:").length(),
                          nullptr, 10);
                      std::string bin = toBinary(size, iv);
                      if (op->VpiReordered()) {
                        std::reverse(bin.begin(), bin.end());
                      }
                      cval += bin;
                    } else {
                      c1 = nullptr;
                    }
                    break;
                  }
                  case vpiStringConst: {
                    std::string tmp =
                        v.c_str() + std::string_view("STRING:").length();
                    cval += tmp;
                    stringVal = true;
                    break;
                  }
                  default: {
                    if (v.find("UINT:") != std::string::npos) {
                      uint64_t iv = std::strtoull(
                          v.c_str() + std::string_view("UINT:").length(),
                          nullptr, 10);
                      std::string bin = toBinary(size, iv);
                      if (op->VpiReordered()) {
                        std::reverse(bin.begin(), bin.end());
                      }
                      cval += bin;
                    } else {
                      int64_t iv = std::strtoll(
                          v.c_str() + std::string_view("INT:").length(),
                          nullptr, 10);
                      std::string bin = toBinary(size, iv);
                      if (op->VpiReordered()) {
                        std::reverse(bin.begin(), bin.end());
                      }
                      cval += bin;
                    }
                    break;
                  }
                }
              } else {
                c1 = nullptr;
                break;
              }
            }
            if (c1) {
              if (stringVal) {
                c1->VpiValue("STRING:" + cval);
                c1->VpiSize(static_cast<int>(cval.size() * 8));
                c1->VpiConstType(vpiStringConst);
              } else {
                if (op->VpiReordered()) {
                  std::reverse(cval.begin(), cval.end());
                }
                if (cval.size() > UHDM_MAX_BIT_WIDTH) {
                  std::string fullPath;
                  if (const gen_scope_array *in =
                          any_cast<gen_scope_array *>(inst)) {
                    fullPath = in->VpiFullName();
                  } else if (inst->UhdmType() == uhdmdesign) {
                    fullPath = inst->VpiName();
                  } else if (any_cast<scope *>(inst)) {
                    fullPath = ((scope *)inst)->VpiFullName();
                  }
                  s.GetErrorHandler()(
                      ErrorType::UHDM_INTERNAL_ERROR_OUT_OF_BOUND, fullPath, op,
                      nullptr);
                  cval = "0";
                }
                c1->VpiValue("BIN:" + cval);
                c1->VpiSize(csize);
                c1->VpiConstType(vpiBinaryConst);
              }
              result = c1;
            }
            break;
          }
          case vpiCastOp: {
            expr *oper = reduceExpr(operands[0], invalidValue, inst, pexpr);
            uint64_t val0 = get_value(invalidValue, oper);
            if (invalidValue) break;
            const typespec *tps = op->Typespec();
            if (tps) {
              UHDM_OBJECT_TYPE ttps = tps->UhdmType();
              if (ttps == uhdmint_typespec) {
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("UINT:" + std::to_string((int)val0));
                c->VpiSize(64);
                c->VpiConstType(vpiUIntConst);
                result = c;
              } else if (ttps == uhdmlong_int_typespec) {
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("UINT:" + std::to_string((long int)val0));
                c->VpiSize(64);
                c->VpiConstType(vpiUIntConst);
                result = c;
              } else if (ttps == uhdmshort_int_typespec) {
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("UINT:" + std::to_string((short int)val0));
                c->VpiSize(16);
                c->VpiConstType(vpiUIntConst);
                result = c;
              } else if (ttps == uhdminteger_typespec) {
                integer_typespec *itps = (integer_typespec *)tps;
                uint64_t cast_to = 0;
                if (itps->VpiValue().find("UINT:") != std::string::npos) {
                  cast_to =
                      std::strtoull(itps->VpiValue().c_str() +
                                        std::string_view("UINT:").length(),
                                    nullptr, 10);
                } else {
                  cast_to = std::strtoll(itps->VpiValue().c_str() +
                                             std::string_view("INT:").length(),
                                         nullptr, 10);
                }
                UHDM::constant *c = s.MakeConstant();
                uint64_t mask =
                    ((uint64_t)((uint64_t)1 << cast_to)) - ((uint64_t)1);
                uint64_t res = val0 & mask;
                c->VpiValue("UINT:" + std::to_string(res));
                c->VpiSize(static_cast<int>(cast_to));
                c->VpiConstType(vpiUIntConst);
                result = c;
              } else if (ttps == uhdmenum_typespec) {
                // TODO: Should check the value is in range of the enum and
                // issue error if not
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("UINT:" + std::to_string(val0));
                c->VpiSize(64);
                c->VpiConstType(vpiUIntConst);
                result = c;
              }
            }
            break;
          }
          case vpiMultiAssignmentPatternOp:
          case vpiAssignmentPatternOp:
            // Don't reduce these ops
            break;
          default: {
            invalidValue = true;
            break;
          }
        }
      }
    }
    return (expr *)result;
  } else if (objtype == uhdmconstant) {
    return (expr *)result;
  } else if (objtype == uhdmsys_func_call) {
    invalidValue = true;
    /*
      TODO in UHDM
    */
  } else if (objtype == uhdmfunc_call) {
    invalidValue = true;
    /*
      TODO in UHDM
    */
  } else if (objtype == uhdmref_obj) {
    ref_obj *ref = (ref_obj *)result;
    const std::string &name = ref->VpiName();
    any *tmp = getValue(name, inst, pexpr);
    if (tmp) {
      result = tmp;
    }
    return (expr *)result;
  } else if (objtype == uhdmhier_path) {
    invalidValue = true;
    /*
      TODO in UHDM
    */
  } else if (objtype == uhdmbit_select) {
    bit_select *sel = (bit_select *)result;
    const std::string &name = sel->VpiName();
    const expr *index = sel->VpiIndex();
    uint64_t index_val = get_value(
        invalidValue, reduceExpr((expr *)index, invalidValue, inst, pexpr));
    if (invalidValue == false) {
      any *object = getObject(name, inst, pexpr);
      if (object) {
        if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
          object = (any *)passign->Rhs();
        }
      }
      if (object == nullptr) {
        object = getValue(name, inst, pexpr);
      }
      if (object) {
        if (expr *tmp = reduceExpr((expr *)object, invalidValue, inst, pexpr)) {
          object = tmp;
        }
        UHDM_OBJECT_TYPE otype = object->UhdmType();
        if (otype == uhdmpacked_array_var) {
          packed_array_var *array = (packed_array_var *)object;
          VectorOfany *elems = array->Elements();
          if (index_val < elems->size()) {
            any *elem = elems->at(index_val);
            if (elem->UhdmType() == uhdmenum_var ||
                elem->UhdmType() == uhdmstruct_var ||
                elem->UhdmType() == uhdmunion_var) {
            } else {
              result = elems->at(index_val);
            }
          }
        } else if (otype == uhdmoperation) {
          operation *op = (operation *)object;
          int opType = op->VpiOpType();
          if (opType == vpiAssignmentPatternOp) {
            UHDM::VectorOfany *ops = op->Operands();
            if (ops && (index_val < ops->size())) {
              result = ops->at(index_val);
            } else if (ops) {
              bool defaultTaggedPattern = false;
              for (auto op : *ops) {
                if (op->UhdmType() == uhdmtagged_pattern) {
                  tagged_pattern *tp = (tagged_pattern *)op;
                  const typespec *tps = tp->Typespec();
                  if (tps->VpiName() == "default") {
                    defaultTaggedPattern = true;
                    break;
                  }
                }
              }
              if (!defaultTaggedPattern) invalidValue = true;
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConcatOp) {
            UHDM::VectorOfany *ops = op->Operands();
            if (ops && (index_val < ops->size())) {
              result = ops->at(index_val);
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConditionOp) {
            expr *exp = reduceExpr(op, invalidValue, inst, pexpr);
            UHDM_OBJECT_TYPE otype = exp->UhdmType();
            if (otype == uhdmoperation) {
              operation *op = (operation *)exp;
              int opType = op->VpiOpType();
              if (opType == vpiAssignmentPatternOp) {
                UHDM::VectorOfany *ops = op->Operands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                } else {
                  invalidValue = true;
                }
              } else if (opType == vpiConcatOp) {
                UHDM::VectorOfany *ops = op->Operands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                } else {
                  invalidValue = true;
                }
              }
            }
            if (object) result = object;
          } else if (opType == vpiMultiConcatOp) {
            result = reduceBitSelect(op, static_cast<unsigned int>(index_val), invalidValue, inst, pexpr);
          }
        } else if (otype == uhdmconstant) {
          result = reduceBitSelect((constant *)object, static_cast<unsigned int>(index_val), invalidValue,
                                   inst, pexpr);
        }
      }
    }
  } else if (objtype == uhdmpart_select) {
    part_select *sel = (part_select *)result;
    ref_obj *parent = (ref_obj *)sel->VpiParent();
    std::string name = parent->VpiName();
    if (name.empty()) {
      name = parent->VpiDefName();
    }
    any *object = getObject(name, inst, pexpr);
    if (object) {
      if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
        object = (any *)passign->Rhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr);
    }
    if (object && (object->UhdmType() == uhdmconstant)) {
      constant *co = (constant *)object;
      int64_t val = get_value(invalidValue, co);
      std::string binary = toBinary(co->VpiSize(), val);
      int64_t l = get_value(invalidValue, sel->Left_range());
      int64_t r = get_value(invalidValue, sel->Right_range());
      std::reverse(binary.begin(), binary.end());
      std::string sub;
      if (l > r)
        sub = binary.substr(r, l - r + 1);
      else
        sub = binary.substr(l, r - l + 1);
      UHDM::constant *c = s.MakeConstant();
      c->VpiValue("BIN:" + sub);
      c->VpiDecompile(sub);
      c->VpiSize(static_cast<int>(sub.size()));
      c->VpiConstType(vpiBinaryConst);
      result = c;
    }

  } else if (objtype == uhdmvar_select) {
    var_select *sel = (var_select *)result;
    const std::string &name = sel->VpiName();
    any *object = getObject(name, inst, pexpr);
    if (object) {
      if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
        object = (any *)passign->Rhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr);
    }
    bool selection = false;
    for (auto index : *sel->Exprs()) {
      uint64_t index_val = get_value(
          invalidValue, reduceExpr((expr *)index, invalidValue, inst, pexpr));
      if (object) {
        UHDM_OBJECT_TYPE otype = object->UhdmType();
        if (otype == uhdmoperation) {
          operation *op = (operation *)object;
          int opType = op->VpiOpType();
          if (opType == vpiAssignmentPatternOp) {
            UHDM::VectorOfany *ops = op->Operands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
              selection = true;
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConcatOp) {
            UHDM::VectorOfany *ops = op->Operands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
              selection = true;
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConditionOp) {
            expr *exp = reduceExpr(object, invalidValue, inst, pexpr);
            UHDM_OBJECT_TYPE otype = exp->UhdmType();
            if (otype == uhdmoperation) {
              operation *op = (operation *)exp;
              int opType = op->VpiOpType();
              if (opType == vpiAssignmentPatternOp) {
                UHDM::VectorOfany *ops = op->Operands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                  selection = true;
                } else {
                  invalidValue = true;
                }
              } else if (opType == vpiConcatOp) {
                UHDM::VectorOfany *ops = op->Operands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                  selection = true;
                } else {
                  invalidValue = true;
                }
              }
            }
          }
        }
      }
    }
    if (object && selection) result = object;
  }
  if (result && result->UhdmType() == uhdmref_obj) {
    bool invalidValueTmp = false;
    expr *tmp = reduceExpr(result, invalidValue, inst, pexpr);
    if (tmp && !invalidValueTmp) result = tmp;
  }
  return (expr *)result;
}

namespace UHDM {
std::string vPrint(UHDM::any *handle) {
  if (handle == nullptr) {
    std::cout << "NULL HANDLE\n";
    return "NULL HANDLE";
  }
  ExprEval eval;
  Serializer *s = handle->GetSerializer();
  std::stringstream out;
  eval.prettyPrint(*s, handle, 0, out);
  std::cout << out.str() << "\n";
  return out.str();
}
}  // namespace UHDM

std::string ExprEval::prettyPrint(UHDM::any *handle) {
  if (handle == nullptr) {
    std::cout << "NULL HANDLE\n";
    return "NULL HANDLE";
  }
  ExprEval eval;
  Serializer *s = handle->GetSerializer();
  std::stringstream out;
  eval.prettyPrint(*s, handle, 0, out);
  return out.str();
}
