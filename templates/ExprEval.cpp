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
#include <uhdm/ElaboratorListener.h>
#include <uhdm/ExprEval.h>
#include <uhdm/NumUtils.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstring>
#include <iostream>
#include <locale>
#include <regex>
#include <sstream>
#include <string_view>

using namespace UHDM;

[[nodiscard]] static std::string_view ltrim(std::string_view str, char c) {
  auto pos = str.find(c);
  if (pos != std::string_view::npos) str = str.substr(pos + 1);
  return str;
}

[[nodiscard]] std::string_view rtrim(std::string_view str, char c) {
  auto pos = str.rfind(c);
  if (pos != std::string_view::npos) str = str.substr(0, pos);
  return str;
}

class DetectRefObj : public VpiListener {
 public:
  explicit DetectRefObj() {}
  ~DetectRefObj() override = default;
  void leaveRef_obj(const ref_obj *object, vpiHandle handle) final {
    hasRef_obj = true;
  }
  void leaveHier_path(const hier_path *object, vpiHandle handle) final {
    hasRef_obj = true;
  }
  bool refObjDetected() { return hasRef_obj; }

 private:
  bool hasRef_obj = false;
};

bool ExprEval::isFullySpecified(const UHDM::typespec *tps) {
  if (tps == nullptr) {
    return true;
  }
  DetectRefObj detector;
  vpiHandle h_rhs = NewVpiHandle(tps);
  detector.listenAny(h_rhs);
  vpi_free_object(h_rhs);
  if (detector.refObjDetected()) {
    return false;
  }
  return true;
}

static std::vector<std::string_view> tokenizeMulti(
    std::string_view str, std::string_view multichar_separator) {
  std::vector<std::string_view> result;
  if (str.empty()) return result;

  size_t start = 0;
  size_t end = 0;
  const size_t sepSize = multichar_separator.size();
  const size_t stringSize = str.size();
  for (size_t i = 0; i < stringSize; i++) {
    bool isSeparator = true;
    for (size_t j = 0; j < sepSize; j++) {
      if (i + j >= stringSize) break;
      if (str[i + j] != multichar_separator[j]) {
        isSeparator = false;
        break;
      }
    }
    if (isSeparator) {
      result.emplace_back(str.data() + start, end - start);
      start = end = end + sepSize;
      i = i + sepSize - 1;
    } else {
      ++end;
    }
  }
  result.emplace_back(str.data() + start, end - start);
  return result;
}

any *ExprEval::getValue(std::string_view name, const any *inst,
                        const any *pexpr, bool muteError) {
  any *result = nullptr;
  if ((inst == nullptr) && (pexpr == nullptr)) {
    return nullptr;
  }
  Serializer *tmps = nullptr;
  if (inst)
    tmps = inst->GetSerializer();
  else
    tmps = pexpr->GetSerializer();
  Serializer &s = *tmps;
  const any *root = inst;
  const any *tmp = inst;
  while (tmp) {
    root = tmp;
    tmp = tmp->VpiParent();
  }
  const design *des = any_cast<design *>(root);
  if (des) m_design = des;
  std::string_view the_name = name;
  const any *the_instance = inst;
  if (m_design && (name.find("::") != std::string::npos)) {
    std::vector<std::string_view> res = tokenizeMulti(name, "::");
    if (res.size() > 1) {
      const std::string_view packName = res[0];
      const std::string_view varName = res[1];
      the_name = varName;
      package *pack = nullptr;
      if (m_design->TopPackages()) {
        for (auto p : *m_design->TopPackages()) {
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
    } else if (any_cast<scope *>(the_instance)) {
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
        expr *res = eval.flattenPatternAssignments(s, op->Typespec(),
                                                   (UHDM::expr *)result);
        if (res->UhdmType() == uhdmoperation) {
          ((operation *)result)->Operands(((operation *)res)->Operands());
        }
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
        any *tmp = getValue(result->VpiName(), inst, pexpr, muteError);
        if (tmp) result = tmp;
      }
    } else if (resultType == uhdmoperation || resultType == uhdmhier_path ||
               resultType == uhdmbit_select ||
               resultType == uhdmsys_func_call) {
      bool invalidValue = false;
      any *tmp = reduceExpr(result, invalidValue, inst, pexpr, muteError);
      if (tmp) result = tmp;
    }
  }
  if ((result == nullptr) && getValueFunctor) {
    result = getValueFunctor(name, inst, pexpr);
  }
  return result;
}

any *ExprEval::getObject(std::string_view name, const any *inst,
                         const any *pexpr, bool muteError) {
  any *result = nullptr;
  while (pexpr) {
    if (const UHDM::scope *s = any_cast<const scope *>(pexpr)) {
      if (s->Variables()) {
        for (auto o : *s->Variables()) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (const UHDM::task_func *s = any_cast<const task_func *>(pexpr)) {
      if (s->Io_decls()) {
        for (auto o : *s->Io_decls()) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (pexpr->UhdmType() == uhdmforeach_stmt) {
      foreach_stmt *for_stmt = (foreach_stmt *)pexpr;
      if (VectorOfany *loopvars = for_stmt->VpiLoopVars()) {
        for (auto var : *loopvars) {
          if (var->VpiName() == name) {
            result = var;
            break;
          }
        }
      }
    }
    if (pexpr->UhdmType() == uhdmclass_defn) {
      const class_defn *defn = (class_defn *)pexpr;
      while (defn) {
        if (defn->Variables()) {
          for (variables *member : *defn->Variables()) {
            if (member->VpiName() == name) {
              result = member;
              break;
            }
          }
        }
        const class_defn *tmp = defn;
        defn = nullptr;
        if (const extends *ext = tmp->Extends()) {
          if (const class_typespec *tp = ext->Class_typespec()) {
            defn = tp->Class_defn();
          }
        }
        if (result) break;
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
      UHDM::VectorOftypespec *typespecs = nullptr;
      UHDM::VectorOfscope *scopes = nullptr;
      if (inst->UhdmType() == uhdmgen_scope_array) {
      } else if (inst->UhdmType() == uhdmdesign) {
        param_assigns = ((design *)inst)->Param_assigns();
        typespecs = ((design *)inst)->Typespecs();
      } else if (any_cast<scope *>(inst)) {
        param_assigns = ((scope *)inst)->Param_assigns();
        variables = ((scope *)inst)->Variables();
        typespecs = ((scope *)inst)->Typespecs();
        scopes = ((scope *)inst)->Scopes();
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
          const std::string_view pname = o->Lhs()->VpiName();
          if (pname == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && typespecs) {
        for (auto o : *typespecs) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && scopes) {
        for (auto o : *scopes) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) ||
          (result && (result->UhdmType() != uhdmconstant) &&
           (result->UhdmType() != uhdmparam_assign))) {
        any *tmpresult = getValue(name, inst, pexpr, muteError);
        if (tmpresult) result = tmpresult;
      }
      if (result) break;
      if (inst) {
        if (inst->UhdmType() == uhdmmodule_inst) {
          break;
        } else {
          inst = inst->VpiParent();
        }
      }
    }
  }

  if (result && (result->UhdmType() == uhdmref_obj)) {
    ref_obj *ref = (ref_obj *)result;
    const std::string_view refname = ref->VpiName();
    if (refname != name) result = getObject(refname, inst, pexpr, muteError);
    if (result) {
      if (UHDM::param_assign *passign = any_cast<param_assign *>(result)) {
        result = (any *)passign->Rhs();
      }
    }
  }
  if ((result == nullptr) && getObjectFunctor) {
    return getObjectFunctor(name, inst, pexpr);
  }
  return result;
}

long double ExprEval::get_double(bool &invalidValue, const UHDM::expr *expr) {
  long double result = 0;
  if (const UHDM::constant *c = any_cast<const UHDM::constant *>(expr)) {
    int type = c->VpiConstType();
    std::string_view sv = c->VpiValue();
    switch (type) {
      case vpiRealConst: {
        sv.remove_prefix(std::string_view("REAL:").length());
        invalidValue = NumUtils::parseLongDouble(sv, &result) == nullptr;
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
    std::string_view sv = c->VpiValue();
    int type = c->VpiConstType();
    switch (type) {
      case vpiBinaryConst: {
        sv.remove_prefix(std::string_view("BIN:").length());
        if (NumUtils::parseBinary(sv, &result) == nullptr) {
          result = 0;
        }
        break;
      }
      case vpiDecConst: {
        sv.remove_prefix(std::string_view("DEC:").length());
        if (NumUtils::parseIntLenient(sv, &result) == nullptr) {
          result = 0;
        }
        break;
      }
      case vpiHexConst: {
        sv.remove_prefix(std::string_view("HEX:").length());
        if (NumUtils::parseHex(sv, &result) == nullptr) {
          result = 0;
        }
        break;
      }
      case vpiOctConst: {
        sv.remove_prefix(std::string_view("OCT:").length());
        if (NumUtils::parseOctal(sv, &result) == nullptr) {
          result = 0;
        }
        break;
      }
      case vpiIntConst: {
        sv.remove_prefix(std::string_view("INT:").length());
        if (NumUtils::parseIntLenient(sv, &result) == nullptr) {
          result = 0;
        }
        break;
      }
      case vpiUIntConst: {
        sv.remove_prefix(std::string_view("UINT:").length());
        if (NumUtils::parseUint64(sv, &result) == nullptr) {
          result = 0;
        }
        break;
      }
      default: {
        if (sv.find("UINT:") == 0) {
          sv.remove_prefix(std::string_view("UINT:").length());
          if (NumUtils::parseUint64(sv, &result) == nullptr) {
            result = 0;
          }
        } else {
          sv.remove_prefix(std::string_view("INT:").length());
          if (NumUtils::parseIntLenient(sv, &result) == nullptr) {
            result = 0;
          }
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
        case uhdminteger_typespec: {
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
      tps = op->Typespec();
    }
    if (tps == nullptr) {
      return result;
    }
    if (tps->UhdmType() != uhdmstruct_typespec) {
      return result;
    }
    if (op->VpiFlattened()) {
      return result;
    }
    struct_typespec *stps = (struct_typespec *)tps;
    std::vector<std::string_view> fieldNames;
    std::vector<const typespec *> fieldTypes;
    for (typespec_member *memb : *stps->Members()) {
      fieldNames.emplace_back(memb->VpiName());
      fieldTypes.emplace_back(memb->Typespec());
    }
    VectorOfany *orig = op->Operands();
    VectorOfany *ordered = s.MakeAnyVec();
    std::vector<any *> tmp(fieldNames.size());
    any *defaultOp = nullptr;
    int index = 0;
    bool flatten = false;
    for (auto oper : *orig) {
      if (oper->UhdmType() == uhdmtagged_pattern) {
        tagged_pattern *tp = (tagged_pattern *)oper;
        const typespec *ttp = tp->Typespec();
        const std::string_view tname = ttp->VpiName();
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
          if (!m_muteError) {
            const std::string errMsg(tname);
            s.GetErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY, errMsg,
                                exp, nullptr);
          }
          return result;
        }
      } else {
        if (index < (int)tmp.size()) {
          tmp[index] = oper;
        } else {
          if (!m_muteError) {
            s.GetErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY,
                                "Out of bound!", exp, nullptr);
          }
        }
      }
      index++;
    }
    index = 0;
    ElaboratorListener listener(&s, false, m_muteError);
    for (auto op : tmp) {
      if (defaultOp) {
        if (op == nullptr) {
          op = clone_tree((any *)defaultOp, s, &listener);
        }
      }
      if (op == nullptr) {
        if (!m_muteError) {
          const std::string errMsg(fieldNames[index]);
          s.GetErrorHandler()(ErrorType::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN,
                              errMsg, exp, nullptr);
        }
        return result;
      }
      if (op->UhdmType() == uhdmtagged_pattern) {
        tagged_pattern *tp = (tagged_pattern *)op;
        const any *patt = tp->Pattern();
        if (patt->UhdmType() == uhdmconstant) {
          constant *c = (constant *)patt;
          if (c->VpiSize() == -1) {
            bool invalidValue = false;
            uint64_t uval = get_uvalue(invalidValue, c);
            if (uval == 1) {
              uint64_t size = ExprEval::size(fieldTypes[index], invalidValue,
                                             nullptr, exp, true, true);
              uint64_t mask = NumUtils::getMask(size);
              uval = mask;
              c->VpiValue("UINT:" + std::to_string(uval));
              c->VpiDecompile(std::to_string(uval));
              c->VpiConstType(vpiUIntConst);
              c->VpiSize(static_cast<int>(size));
            } else if (uval == 0) {
              uint64_t size = ExprEval::size(fieldTypes[index], invalidValue,
                                             nullptr, exp, true, true);
              c->VpiValue("UINT:" + std::to_string(uval));
              c->VpiDecompile(std::to_string(uval));
              c->VpiConstType(vpiUIntConst);
              c->VpiSize(static_cast<int>(size));
            }
          }
        }
      }
      ordered->push_back(op);
      index++;
    }
    operation *opres = (operation *)clone_tree((any *)op, s, &listener);
    opres->Operands(ordered);
    if (flatten) {
      opres->VpiFlattened(true);
    }
    // Flattening
    VectorOfany *flattened = s.MakeAnyVec();
    recursiveFlattening(s, flattened, ordered, fieldTypes);
    opres->Operands(flattened);
    result = opres;
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
    case uhdmparameter: {
      parameter *p = (parameter *)object;
      std::string_view val = p->VpiValue();
      val = ltrim(val, ':');
      out << val;
      break;
    }
    case uhdmenum_const: {
      enum_const *c = (enum_const *)object;
      std::string_view val = c->VpiValue();
      val = ltrim(val, ':');
      out << val;
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
                        const any *inst, const any *pexpr, bool full,
                        bool muteError) {
  uint64_t bits = 0;
  UHDM::VectorOfrange *ranges = nullptr;
  if (typespec) {
    UHDM_OBJECT_TYPE ttps = typespec->UhdmType();
    switch (ttps) {
      case UHDM::uhdmhier_path: {
        typespec = decodeHierPath((hier_path *)typespec, invalidValue, inst,
                                  nullptr, true);
        if (typespec)
          bits = size(typespec, invalidValue, inst, pexpr, full);
        else
          invalidValue = true;
        break;
      }
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
        int_typespec *its = (int_typespec *)typespec;
        bits = 32;
        ranges = its->Ranges();
        if (ranges) {
          bits = 1;
        }
        break;
      }
      case UHDM::uhdmlong_int_typespec: {
        bits = 64;
        break;
      }
      case UHDM::uhdminteger_typespec: {
        integer_typespec *itps = (integer_typespec *)typespec;
        std::string_view val = itps->VpiValue();
        if (val.empty()) {
          bits = 32;
        } else if (val.find("UINT:") == 0) {
          val.remove_prefix(std::string_view("UINT:").length());
          if (NumUtils::parseUint64(val, &bits) == nullptr) {
            bits = 32;
          }
        } else if (val.find("INT:") == 0) {
          val.remove_prefix(std::string_view("INT:").length());
          if (NumUtils::parseIntLenient(val, &bits) == nullptr) {
            bits = 32;
          }
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
      case UHDM::uhdmstring_typespec: {
        bits = 0;
        invalidValue = true;
        break;
      }
      case UHDM::uhdmunsupported_typespec: {
        bits = 0;
        invalidValue = true;
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
      case UHDM::uhdmarray_var: {
        const UHDM::array_var *var = ((array_var *)typespec);
        variables *regv = var->Variables()->at(0);
        bits += size(regv->Typespec(), invalidValue, inst, pexpr, full);
        ranges = var->Ranges();
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
        } else {
          invalidValue = true;
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
      case uhdmbit_select: {
        bits = 1;
        break;
      }
      case uhdmpart_select: {
        part_select *sel = (part_select *)typespec;
        expr *lexpr = (expr *)sel->Left_range();
        expr *rexpr = (expr *)sel->Right_range();
        int64_t lv =
            getValue(reduceExpr(lexpr, invalidValue, inst, pexpr, muteError));

        int64_t rv =
            getValue(reduceExpr(rexpr, invalidValue, inst, pexpr, muteError));

        if (lv > rv)
          bits = ((lv - rv) + 1);
        else
          bits = ((rv - lv) + 1);
        break;
      }
      default:
        invalidValue = true;
        break;
    }
  }
  if (ranges && !ranges->empty()) {
    if (!full) {
      UHDM::range *last_range = ranges->back();
      expr *lexpr = (expr *)last_range->Left_expr();
      expr *rexpr = (expr *)last_range->Right_expr();
      int64_t lv =
          getValue(reduceExpr(lexpr, invalidValue, inst, pexpr, muteError));

      int64_t rv =
          getValue(reduceExpr(rexpr, invalidValue, inst, pexpr, muteError));

      if (lv > rv)
        bits = bits * (lv - rv + 1);
      else
        bits = bits * (rv - lv + 1);
    } else {
      for (UHDM::range *ran : *ranges) {
        expr *lexpr = (expr *)ran->Left_expr();
        expr *rexpr = (expr *)ran->Right_expr();
        int64_t lv =
            getValue(reduceExpr(lexpr, invalidValue, inst, pexpr, muteError));

        int64_t rv =
            getValue(reduceExpr(rexpr, invalidValue, inst, pexpr, muteError));

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

void resize(expr *resizedExp, int size) {
  bool invalidValue = false;
  ExprEval eval;
  constant *c = (constant *)resizedExp;
  int64_t val = eval.get_value(invalidValue, c);
  if (val == 1) {
    uint64_t mask = NumUtils::getMask(size);
    c->VpiValue("UINT:" + std::to_string(mask));
    c->VpiDecompile(std::to_string(mask));
    c->VpiConstType(vpiUIntConst);
  }
}

expr *ExprEval::reduceCompOp(operation *op, bool &invalidValue, const any *inst,
                             const any *pexpr, bool muteError) {
  expr *result = op;
  Serializer &s = *op->GetSerializer();
  UHDM::VectorOfany &operands = *op->Operands();
  int optype = op->VpiOpType();
  std::string s0;
  std::string s1;
  expr *reduc0 = reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
  expr *reduc1 = reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
  if (invalidValue == true) {
    return result;
  }
  if (reduc0 == nullptr || reduc1 == nullptr) {
    return result;
  }
  int size0 = reduc0->VpiSize();
  int size1 = reduc1->VpiSize();
  if ((reduc0->VpiSize() == -1) && (reduc1->VpiSize() > 1)) {
    resize(reduc0, size1);
  } else if ((reduc1->VpiSize() == -1) && (reduc0->VpiSize() > 1)) {
    resize(reduc1, size0);
  }
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
    int64_t v0 = get_uvalue(invalidValueI, reduc0);
    int64_t v1 = get_uvalue(invalidValueI, reduc1);
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

expr *ExprEval::reduceBitSelect(expr *op, unsigned int index_val,
                                bool &invalidValue, const any *inst,
                                const any *pexpr, bool muteError) {
  Serializer &s = *op->GetSerializer();
  expr *result = nullptr;
  expr *exp = reduceExpr(op, invalidValue, inst, pexpr, muteError);
  if (exp && (exp->UhdmType() == uhdmconstant)) {
    std::string binary;
    constant *cexp = (constant *)exp;
    if (cexp->VpiConstType() == vpiBinaryConst) {
      binary = cexp->VpiValue();
      binary = binary.substr(std::string_view("BIN:").length());
    } else if (cexp->VpiConstType() == vpiHexConst) {
      std::string_view hex = cexp->VpiValue();
      hex.remove_prefix(std::string_view("HEX:").length());
      binary = NumUtils::hexToBin(hex);
    } else {
      int64_t val = get_value(invalidValue, exp);
      binary = NumUtils::toBinary(exp->VpiSize(), val);
    }
    uint64_t wordSize = 1;
    if (typespec *cts = (typespec *)cexp->Typespec()) {
      if (cts->UhdmType() == uhdmint_typespec) {
        int_typespec *icts = (int_typespec *)cts;
        std::string_view value = icts->VpiValue();
        if (value.find("UINT:") == 0) {
          value.remove_prefix(std::string_view("UINT:").length());
          if (NumUtils::parseUint64(value, &wordSize) == nullptr) {
            wordSize = 1;
          }
        } else if (value.find("INT:") == 0) {
          value.remove_prefix(std::string_view("INT:").length());
          if (NumUtils::parseIntLenient(value, &wordSize) == nullptr) {
            wordSize = 1;
          }
        }
      } else if (cts->UhdmType() == uhdminteger_typespec) {
        integer_typespec *icts = (integer_typespec *)cts;
        std::string_view value = icts->VpiValue();
        if (value.find("UINT:") == 0) {
          value.remove_prefix(std::string_view("UINT:").length());
          if (NumUtils::parseUint64(value, &wordSize) == nullptr) {
            wordSize = 1;
          }
        } else if (value.find("INT:") == 0) {
          value.remove_prefix(std::string_view("INT:").length());
          if (NumUtils::parseIntLenient(value, &wordSize) == nullptr) {
            wordSize = 1;
          }
        }
      } else if (cts->UhdmType() == uhdmlogic_typespec) {
        logic_typespec *icts = (logic_typespec *)cts;
        const logic_typespec *elem = icts->Logic_typespec();
        wordSize = size(elem, invalidValue, inst, pexpr, false, muteError);
      }
    }
    if (wordSize == 0) {
      wordSize = 1;
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
          lr = static_cast<unsigned short>(
              get_value(invalidValue, r->Left_expr()));
          rr = static_cast<unsigned short>(
              get_value(invalidValue, r->Right_expr()));
        }
      } else if (tps->UhdmType() == uhdmint_typespec) {
        int_typespec *lts = (int_typespec *)tps;
        VectorOfrange *ranges = lts->Ranges();
        if (ranges) {
          range *r = ranges->at(0);
          bool invalidValue = false;
          lr = static_cast<unsigned short>(
              get_value(invalidValue, r->Left_expr()));
          rr = static_cast<unsigned short>(
              get_value(invalidValue, r->Right_expr()));
        }
      } else if (tps->UhdmType() == uhdmbit_typespec) {
        bit_typespec *lts = (bit_typespec *)tps;
        VectorOfrange *ranges = lts->Ranges();
        if (ranges) {
          range *r = ranges->at(0);
          bool invalidValue = false;
          lr = static_cast<unsigned short>(
              get_value(invalidValue, r->Left_expr()));
          rr = static_cast<unsigned short>(
              get_value(invalidValue, r->Right_expr()));
        }
      }
    }
    c->VpiSize(static_cast<int>(wordSize));
    if (index_val < binary.size()) {
      // TODO: If range does not start at 0
      if (lr >= rr) {
        index_val = static_cast<unsigned int>(binary.size() -
                                              ((index_val + 1) * wordSize));
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
        if (muteError == false && m_muteError == false) {
          s.GetErrorHandler()(ErrorType::UHDM_INTERNAL_ERROR_OUT_OF_BOUND,
                              fullPath, op, nullptr);
        }
        v = "0";
      }
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

int64_t ExprEval::get_value(bool &invalidValue, const UHDM::expr *expr,
                            bool strict) {
  int64_t result = 0;
  int type = 0;
  std::string_view sv;
  if (const UHDM::constant *c = any_cast<const UHDM::constant *>(expr)) {
    type = c->VpiConstType();
    sv = c->VpiValue();
  } else if (const UHDM::variables *c =
                 any_cast<const UHDM::variables *>(expr)) {
    if (c->UhdmType() == uhdmenum_var) {
      type = vpiUIntConst;
      sv = c->VpiValue();
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
          sv = ltrim(sv, '\'');
          sv = ltrim(sv, 's');
          sv = ltrim(sv, 'b');
          sv.remove_prefix(std::string_view("BIN:").length());
          bool invalid = NumUtils::parseBinary(sv, &result) == nullptr;
          if (strict) invalidValue = invalid;
        }
        break;
      }
      case vpiDecConst: {
        sv.remove_prefix(std::string_view("DEC:").length());
        invalidValue = NumUtils::parseInt64(sv, &result) == nullptr;
        break;
      }
      case vpiHexConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim(sv, '\'');
          sv = ltrim(sv, 's');
          sv = ltrim(sv, 'h');
          sv.remove_prefix(std::string_view("HEX:").length());
          invalidValue = NumUtils::parseHex(sv, &result) == nullptr;
        }
        break;
      }
      case vpiOctConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim(sv, '\'');
          sv = ltrim(sv, 's');
          sv = ltrim(sv, 'o');
          sv.remove_prefix(std::string_view("OCT:").length());
          invalidValue = NumUtils::parseOctal(sv, &result) == nullptr;
        }
        break;
      }
      case vpiIntConst: {
        sv.remove_prefix(std::string_view("INT:").length());
        invalidValue = NumUtils::parseInt64(sv, &result) == nullptr;
        break;
      }
      case vpiUIntConst: {
        sv.remove_prefix(std::string_view("UINT:").length());
        invalidValue = NumUtils::parseIntLenient(sv, &result) == nullptr;
        break;
      }
      case vpiScalar: {
        sv.remove_prefix(std::string_view("SCAL:").length());
        invalidValue = NumUtils::parseBinary(sv, &result) == nullptr;
        break;
      }
      case vpiStringConst: {
        sv.remove_prefix(std::string_view("STRING:").length());
        result = 0;
        for (uint32_t i = 0; i < sv.size(); i++) {
          result += (sv[i] << ((sv.size() - (i + 1)) * 8));
        }
        break;
      }
      case vpiRealConst: {
        // Don't do the double precision math, leave it to client tools
        invalidValue = true;
        break;
      }
      default: {
        if (sv.find("UINT:") == 0) {
          sv.remove_prefix(std::string_view("UINT:").length());
          invalidValue = NumUtils::parseIntLenient(sv, &result) == nullptr;
        } else if (sv.find("INT:") == 0) {
          sv.remove_prefix(std::string_view("INT:").length());
          invalidValue = NumUtils::parseInt64(sv, &result) == nullptr;
        } else {
          invalidValue = true;
        }
        break;
      }
    }
  }
  return result;
}

uint64_t ExprEval::get_uvalue(bool &invalidValue, const UHDM::expr *expr,
                              bool strict) {
  uint64_t result = 0;
  int type = 0;
  std::string_view sv;
  if (const UHDM::constant *c = any_cast<const UHDM::constant *>(expr)) {
    type = c->VpiConstType();
    sv = c->VpiValue();
  } else if (const UHDM::variables *c =
                 any_cast<const UHDM::variables *>(expr)) {
    if (c->UhdmType() == uhdmenum_var) {
      type = vpiUIntConst;
      sv = c->VpiValue();
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
          sv = ltrim(sv, '\'');
          sv = ltrim(sv, 's');
          sv = ltrim(sv, 'b');
          sv.remove_prefix(std::string_view("BIN:").length());
          bool invalid = NumUtils::parseBinary(sv, &result) == nullptr;
          if (strict) invalidValue = invalid;
        }
        break;
      }
      case vpiDecConst: {
        sv.remove_prefix(std::string_view("DEC:").length());
        invalidValue = NumUtils::parseUint64(sv, &result) == nullptr;
        break;
      }
      case vpiHexConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim(sv, '\'');
          sv = ltrim(sv, 's');
          sv = ltrim(sv, 'h');
          sv.remove_prefix(std::string_view("HEX:").length());
          invalidValue = NumUtils::parseHex(sv, &result) == nullptr;
        }
        break;
      }
      case vpiOctConst: {
        if (expr->VpiSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim(sv, '\'');
          sv = ltrim(sv, 's');
          sv = ltrim(sv, 'o');
          sv.remove_prefix(std::string_view("OCT:").length());
          invalidValue = NumUtils::parseOctal(sv, &result) == nullptr;
        }
        break;
      }
      case vpiIntConst: {
        sv.remove_prefix(std::string_view("INT:").length());
        invalidValue = NumUtils::parseUint64(sv, &result) == nullptr;
        break;
      }
      case vpiUIntConst: {
        sv.remove_prefix(std::string_view("UINT:").length());
        invalidValue = NumUtils::parseUint64(sv, &result) == nullptr;
        break;
      }
      case vpiScalar: {
        sv.remove_prefix(std::string_view("SCAL:").length());
        invalidValue = NumUtils::parseBinary(sv, &result) == nullptr;
        break;
      }
      case vpiStringConst: {
        sv.remove_prefix(std::string_view("STRING:").length());
        result = 0;
        for (uint32_t i = 0; i < sv.size(); i++) {
          result += (sv[i] << ((sv.size() - (i + 1)) * 8));
        }
        break;
      }
      case vpiRealConst: {
        // Don't do the double precision math, leave it to client tools
        invalidValue = true;
        break;
      }
      default: {
        if (sv.find("UINT:") == 0) {
          sv.remove_prefix(std::string_view("UINT:").length());
          invalidValue = NumUtils::parseUint64(sv, &result) == nullptr;
        } else if (sv.find("INT:") == 0) {
          sv.remove_prefix(std::string_view("INT:").length());
          invalidValue = NumUtils::parseIntLenient(sv, &result) == nullptr;
        } else {
          invalidValue = true;
        }
        break;
      }
    }
  }
  return result;
}

UHDM::task_func *ExprEval::getTaskFunc(std::string_view name,
                                       const any *inst) {
  if (getTaskFuncFunctor) {
    UHDM::task_func *result = getTaskFuncFunctor(name, inst);
    if (result) {
      return result;
    }
  }
  if (inst == nullptr) {
    return nullptr;
  }
  const any *root = inst;
  const any *tmp = inst;
  while (tmp) {
    root = tmp;
    tmp = tmp->VpiParent();
  }
  const design *des = any_cast<design *>(root);
  if (des) m_design = des;
  std::string_view the_name = name;
  const any *the_instance = inst;
  if (m_design && (name.find("::") != std::string::npos)) {
    std::vector<std::string_view> res = tokenizeMulti(name, "::");
    if (res.size() > 1) {
      const std::string_view packName = res[0];
      const std::string_view varName = res[1];
      the_name = varName;
      package *pack = nullptr;
      if (m_design->TopPackages()) {
        for (auto p : *m_design->TopPackages()) {
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
    UHDM::VectorOftask_func *task_funcs = nullptr;
    if (the_instance->UhdmType() == uhdmgen_scope_array) {
    } else if (the_instance->UhdmType() == uhdmdesign) {
      task_funcs = ((design *)the_instance)->Task_funcs();
    } else if (any_cast<instance *>(the_instance)) {
      task_funcs = ((UHDM::instance *)the_instance)->Task_funcs();
    }

    if (task_funcs) {
      for (UHDM::task_func *tf : *task_funcs) {
        if (tf->VpiName() == the_name) {
          return tf;
        }
      }
    }

    the_instance = the_instance->VpiParent();
  }

  return nullptr;
}

any *ExprEval::decodeHierPath(hier_path *path, bool &invalidValue,
                              const any *inst, const any *pexpr,
                              bool returnTypespec, bool muteError) {
  Serializer &s = *path->GetSerializer();
  std::string baseObject;
  if (!path->Path_elems()->empty()) {
    any *firstElem = path->Path_elems()->at(0);
    baseObject = firstElem->VpiName();
  }
  any *object = getObject(baseObject, inst, pexpr, muteError);
  if (object) {
    if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
      object = (any *)passign->Rhs();
    }
  }
  if (object == nullptr) {
    object = getValue(baseObject, inst, pexpr, muteError);
  }
  if (object) {
    // Substitution
    if (param_assign *pass = any_cast<param_assign *>(object)) {
      const any *rhs = pass->Rhs();
      object = reduceExpr((any *)rhs, invalidValue, inst, pexpr, muteError);
    } else if (bit_select *bts = any_cast<bit_select *>(object)) {
      object = reduceExpr((any *)bts, invalidValue, inst, pexpr, muteError);
    } else if (ref_obj *ref = any_cast<ref_obj *>(object)) {
      object = reduceExpr((any *)ref, invalidValue, inst, pexpr, muteError);
    } else if (constant *cons = any_cast<constant *>(object)) {
      ElaboratorListener listener(&s);
      object = UHDM::clone_tree((any *)cons, s, &listener);
      cons = any_cast<constant *>(object);
      if (cons->Typespec() == nullptr)
        cons->Typespec((typespec *)path->Typespec());
    } else if (operation *oper = any_cast<operation *>(object)) {
      if (returnTypespec) {
        object = (typespec *)oper->Typespec();
      }
    }

    std::vector<std::string> the_path;
    for (auto elem : *path->Path_elems()) {
      std::string_view elemName = elem->VpiName();
      elemName = rtrim(elemName, '[');
      the_path.emplace_back(elemName);
      if (elem->UhdmType() == uhdmbit_select) {
        bit_select *select = (bit_select *)elem;
        uint64_t baseIndex = get_value(
            invalidValue, reduceExpr((any *)select->VpiIndex(), invalidValue,
                                     inst, pexpr, muteError));
        the_path.push_back("[" + std::to_string(baseIndex) + "]");
      }
    }

    expr *res =
        (expr *)hierarchicalSelector(the_path, 0, object, invalidValue, inst,
                                     pexpr, returnTypespec, muteError);
    return res;
  }
  return nullptr;
}

any *ExprEval::hierarchicalSelector(std::vector<std::string> &select_path,
                                    unsigned int level, UHDM::any *object,
                                    bool &invalidValue, const any *inst,
                                    const UHDM::any *pexpr, bool returnTypespec,
                                    bool muteError) {
  Serializer &s = (object) ? *object->GetSerializer() : *inst->GetSerializer();
  if (level >= select_path.size()) {
    return (expr *)object;
  }
  std::string elemName = select_path[level];

  if (variables *var = any_cast<variables *>(object)) {
    UHDM_OBJECT_TYPE ttps = var->UhdmType();
    if (ttps == uhdmstruct_var) {
      struct_typespec *stpt =
          (struct_typespec *)((struct_var *)var)->Typespec();
      for (typespec_member *member : *stpt->Members()) {
        if (member->VpiName() == elemName) {
          if (returnTypespec)
            return (expr *)member->Typespec();
          else
            return (expr *)member->Default_value();
        }
      }
    } else if (ttps == uhdmclass_var) {
      class_typespec *stpt = (class_typespec *)((class_var *)var)->Typespec();
      const class_defn *defn = stpt->Class_defn();
      while (defn) {
        if (defn->Variables()) {
          for (variables *member : *defn->Variables()) {
            if (member->VpiName() == elemName) {
              if (returnTypespec)
                return (typespec *)member->Typespec();
              else
                return member;
            }
          }
        }
        const class_defn *tmp = defn;
        defn = nullptr;
        if (const extends *ext = tmp->Extends()) {
          if (const class_typespec *tp = ext->Class_typespec()) {
            defn = tp->Class_defn();
          }
        }
      }

    } else if (ttps == uhdmarray_var) {
      if (returnTypespec) return (typespec *)var->Typespec();
    }
  } else if (typespec *var = any_cast<typespec *>(object)) {
    UHDM_OBJECT_TYPE ttps = var->UhdmType();
    if (ttps == uhdmstruct_typespec) {
      struct_typespec *stpt = (struct_typespec *)(var);
      for (typespec_member *member : *stpt->Members()) {
        if (member->VpiName() == elemName) {
          expr *res = nullptr;
          if (returnTypespec)
            res = (expr *)member->Typespec();
          else
            res = (expr *)member->Default_value();
          if (level == select_path.size() - 1) {
            return res;
          } else {
            any *ex =
                hierarchicalSelector(select_path, level + 1, res, invalidValue,
                                     inst, pexpr, returnTypespec, muteError);
            return ex;
          }
        }
      }
    }
  } else if (io_decl *decl = any_cast<io_decl *>(object)) {
    const any *exp = decl->Expr();
    if (exp) {
      UHDM_OBJECT_TYPE ttps = exp->UhdmType();
      if (ttps == uhdmstruct_var) {
        struct_typespec *stpt =
            (struct_typespec *)((struct_var *)exp)->Typespec();
        for (typespec_member *member : *stpt->Members()) {
          if (member->VpiName() == elemName) {
            if (returnTypespec)
              return (expr *)member->Typespec();
            else
              return (expr *)member->Default_value();
          }
        }
      }
    }
    if (returnTypespec) {
      if (const typespec *tps = decl->Typespec()) {
        UHDM_OBJECT_TYPE ttps = tps->UhdmType();
        if (ttps == uhdmstruct_typespec) {
          struct_typespec *stpt = (struct_typespec *)tps;
          for (typespec_member *member : *stpt->Members()) {
            if (member->VpiName() == elemName) {
              return (expr *)member->Typespec();
            }
          }
        } else if (ttps == uhdmclass_typespec) {
          class_typespec *stpt = (class_typespec *)tps;
          const class_defn *defn = stpt->Class_defn();
          while (defn) {
            if (defn->Variables()) {
              for (variables *member : *defn->Variables()) {
                if (member->VpiName() == elemName) {
                  return (typespec *)member->Typespec();
                }
              }
            }
            const class_defn *tmp = defn;
            defn = nullptr;
            if (const extends *ext = tmp->Extends()) {
              if (const class_typespec *tp = ext->Class_typespec()) {
                defn = tp->Class_defn();
              }
            }
          }
        }
      }
    }
  } else if (nets *var = any_cast<nets *>(object)) {
    UHDM_OBJECT_TYPE ttps = var->UhdmType();
    if (ttps == uhdmstruct_net) {
      struct_typespec *stpt =
          (struct_typespec *)((struct_net *)var)->Typespec();
      for (typespec_member *member : *stpt->Members()) {
        if (member->VpiName() == elemName) {
          if (returnTypespec)
            return (expr *)member->Typespec();
          else
            return (expr *)member->Default_value();
        }
      }
    }
  } else if (constant *cons = any_cast<constant *>(object)) {
    const typespec *ts = cons->Typespec();
    if (ts) {
      UHDM_OBJECT_TYPE ttps = ts->UhdmType();
      if (ttps == uhdmstruct_typespec) {
        struct_typespec *stpt = (struct_typespec *)ts;
        uint64_t from = 0;
        uint64_t width = 0;
        for (typespec_member *member : *stpt->Members()) {
          if (member->VpiName() == elemName) {
            width = size(member, invalidValue, inst, pexpr, true);
            uint64_t iv = get_value(invalidValue, cons);
            uint64_t mask = 0;

            for (uint64_t i = from; i < uint64_t(from + width); i++) {
              mask |= ((uint64_t)1 << i);
            }
            uint64_t res = iv & mask;
            res = res >> (from);
            cons->VpiValue("UINT:" + std::to_string(res));
            cons->VpiSize(static_cast<int>(width));
            return cons;
          } else {
            from += size(member, invalidValue, inst, pexpr, true);
          }
        }
      }
    }
  }

  int selectIndex = -1;
  if (elemName.find('[') != std::string::npos) {
    std::string_view indexName = ltrim(elemName, '[');
    indexName = rtrim(indexName, ']');
    if (NumUtils::parseInt32(indexName, &selectIndex) == nullptr) {
      selectIndex = -1;
    }
    elemName.clear();
    if (operation *oper = any_cast<operation *>(object)) {
      int opType = oper->VpiOpType();
      if (opType == vpiAssignmentPatternOp) {
        UHDM::VectorOfany *operands = oper->Operands();
        int sInd = 0;
        for (auto operand : *operands) {
          if ((selectIndex >= 0) && (sInd == selectIndex)) {
            any *ex = hierarchicalSelector(select_path, level + 1, operand,
                                           invalidValue, inst, pexpr,
                                           returnTypespec, muteError);
            return ex;
          }
          sInd++;
        }
      }
    } else if (typespec *tps = any_cast<typespec *>(object)) {
      if (tps->UhdmType() == uhdmlogic_typespec) {
        logic_typespec *ltps = (logic_typespec *)tps;
        VectorOfrange *ranges = ltps->Ranges();
        if (ranges && (ranges->size() >= 2)) {
          logic_typespec *tmp = s.MakeLogic_typespec();
          VectorOfrange *tmpR = s.MakeRangeVec();
          for (unsigned int i = 1; i < ranges->size(); i++) {
            tmpR->push_back(ranges->at(i));
          }
          tmp->Ranges(tmpR);
          return tmp;
        }
      }
    } else if (constant *c = any_cast<constant *>(object)) {
      expr *ex =
          reduceBitSelect(c, selectIndex, invalidValue, inst, pexpr, muteError);
      return ex;
    }

  } else if (level == 0) {
    any *ex = hierarchicalSelector(select_path, level + 1, object, invalidValue,
                                   inst, pexpr, returnTypespec, muteError);
    return ex;
  }

  if (operation *oper = any_cast<operation *>(object)) {
    int opType = oper->VpiOpType();

    if (opType == vpiAssignmentPatternOp) {
      UHDM::VectorOfany *operands = oper->Operands();
      any *defaultPattern = nullptr;
      int sInd = 0;

      int bIndex = -1;
      if (inst) {
        /*
        any *baseP = nullptr;
        VectorOfany *parameters = nullptr;
        if (inst->UhdmType() == uhdmgen_scope_array) {
        } else if (inst->UhdmType() == uhdmdesign) {
          parameters = ((design *)inst)->Parameters();
        } else if (any_cast<scope *>(inst)) {
          parameters = ((scope *)inst)->Parameters();
        }
        if (parameters) {
          for (auto p : *parameters) {
            if (p->VpiName() == select_path[0]) {
              baseP = p;
              break;
            }
          }
        }
        */
        any *baseP = getObject(select_path[0], inst, pexpr, muteError);
        if (baseP) {
          const typespec *tps = nullptr;
          if (parameter *p = any_cast<parameter *>(baseP)) {
            tps = p->Typespec();
          } else if (operation *p = any_cast<operation *>(baseP)) {
            tps = p->Typespec();
          }

          if (tps) {
            if (tps->UhdmType() == uhdmpacked_array_typespec) {
              packed_array_typespec *tmp = (packed_array_typespec *)tps;
              tps = (typespec *)tmp->Elem_typespec();
            }
            if (tps->UhdmType() == uhdmstruct_typespec) {
              struct_typespec *sts = (struct_typespec *)tps;
              UHDM::VectorOftypespec_member *members = sts->Members();
              if (members) {
                unsigned int i = 0;
                for (UHDM::typespec_member *member : *members) {
                  if (member->VpiName() == elemName) {
                    bIndex = i;
                    break;
                  }
                  i++;
                }
              }
            }
          }
        }
      }
      if (inst) {
        const any *tmpInstance = inst;
        while ((bIndex == -1) && tmpInstance) {
          UHDM::VectorOfparam_assign *param_assigns = nullptr;
          if (tmpInstance->UhdmType() == uhdmgen_scope_array) {
          } else if (tmpInstance->UhdmType() == uhdmdesign) {
            param_assigns = ((design *)tmpInstance)->Param_assigns();
          } else if (any_cast<scope *>(tmpInstance)) {
            param_assigns = ((scope *)tmpInstance)->Param_assigns();
          }
          if (param_assigns) {
            for (param_assign *param : *param_assigns) {
              if (param && param->Lhs()) {
                const std::string_view param_name = param->Lhs()->VpiName();
                if (param_name == select_path[0]) {
                  parameter *p = any_cast<parameter *>((any *)param->Lhs());
                  if (p) {
                    const typespec *tps = p->Typespec();
                    if (tps) {
                      if (tps->UhdmType() == uhdmpacked_array_typespec) {
                        packed_array_typespec *tmp =
                            (packed_array_typespec *)tps;
                        tps = (typespec *)tmp->Elem_typespec();
                      }
                      if (tps->UhdmType() == uhdmstruct_typespec) {
                        struct_typespec *sts = (struct_typespec *)tps;
                        UHDM::VectorOftypespec_member *members = sts->Members();
                        if (members) {
                          unsigned int i = 0;
                          for (UHDM::typespec_member *member : *members) {
                            if (member->VpiName() == elemName) {
                              bIndex = i;
                              break;
                            }
                            i++;
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          tmpInstance = tmpInstance->VpiParent();
        }
      }
      for (auto operand : *operands) {
        UHDM_OBJECT_TYPE operandType = operand->UhdmType();
        if (operandType == uhdmtagged_pattern) {
          tagged_pattern *tpatt = (tagged_pattern *)operand;
          const typespec *tps = tpatt->Typespec();
          if (tps->VpiName() == "default") {
            defaultPattern = (any *)tpatt->Pattern();
          }
          if (!elemName.empty() && (tps->VpiName() == elemName)) {
            const any *patt = tpatt->Pattern();
            UHDM_OBJECT_TYPE pattType = patt->UhdmType();
            if (pattType == uhdmconstant) {
              any *ex = reduceExpr((expr *)patt, invalidValue, inst, pexpr,
                                   muteError);
              if (level < select_path.size()) {
                ex = hierarchicalSelector(select_path, level + 1, ex,
                                          invalidValue, inst, pexpr,
                                          returnTypespec);
              }
              return ex;
            } else if (pattType == uhdmoperation) {
              any *ex = hierarchicalSelector(select_path, level + 1,
                                             (expr *)patt, invalidValue, inst,
                                             pexpr, returnTypespec);
              return ex;
            }
          }
        } else if (operandType == uhdmconstant) {
          if ((bIndex >= 0) && (bIndex == sInd)) {
            any *ex =
                hierarchicalSelector(select_path, level + 1, (expr *)operand,
                                     invalidValue, inst, pexpr, returnTypespec);
            return ex;
          }
        }
        sInd++;
      }
      if (defaultPattern) {
        expr *ex = any_cast<expr *>(defaultPattern);
        if (ex) ex = reduceExpr(ex, invalidValue, inst, pexpr, muteError);
        return ex;
      }
    }
  }
  return nullptr;
}

static std::string hexToBinary(char input[2]) {
  unsigned int x = std::stoul(input, nullptr, 16);
  std::string result = std::bitset<4>(x).to_string();
  return result;
}

expr *ExprEval::reduceExpr(const any *result, bool &invalidValue,
                           const any *inst, const any *pexpr, bool muteError) {
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
          const std::string_view name = ref->VpiName();
          any *tmp = getValue(name, inst, pexpr, muteError);
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
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
              expr *reduc0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
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
                setValueInInstance(operands[0]->VpiName(), operands[0], c,
                                   invalidValue, s, inst, op, muteError);
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
                  setValueInInstance(operands[0]->VpiName(), operands[0], c,
                                     invalidValue, s, inst, op, muteError);
                }
              }
            }
            break;
          }
          case vpiArithLShiftOp:
          case vpiLShiftOp: {
            if (operands.size() == 2) {
              int64_t val0 =
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
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
              expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
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
              expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              int64_t val0 = get_value(invalidValueI, expr0);
              if ((invalidValue == false) && (invalidValueI == false)) {
                int64_t val = -val0;
                uint64_t size = 64;
                if (expr0->UhdmType() == uhdmconstant) {
                  size = expr0->VpiSize();
                }
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(static_cast<int>(size));
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
              expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
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
              expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
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
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              if (operand) {
                uint64_t val = (uint64_t)get_value(invalidValue, operand);
                if (invalidValue) break;
                uint64_t size = 64;
                if (operand->UhdmType() == uhdmconstant) {
                  constant *c = (constant *)operand;
                  size = c->VpiSize();
                  if (const typespec *tps = c->Typespec()) {
                    size = ExprEval::size(tps, invalidValue, inst, pexpr, true,
                                          muteError);
                  }
                  if (size == 1) {
                    val = !val;
                  } else {
                    uint64_t mask = NumUtils::getMask(size);
                    val = ~val;
                    val = val & mask;
                  }
                } else {
                  val = ~val;
                }

                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("UINT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(static_cast<int>(size));
                c->VpiConstType(vpiUIntConst);
                result = c;
              }
            }
            break;
          }
          case vpiNotOp: {
            if (operands.size() == 1) {
              uint64_t val = !((uint64_t)get_value(
                  invalidValue, reduceExpr(operands[0], invalidValue, inst,
                                           pexpr, muteError)));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              if (invalidValue) break;
              for (unsigned int i = 1; i < operands.size(); i++) {
                int64_t oval = get_value(
                    invalidValue, reduceExpr(operands[i], invalidValue, inst,
                                             pexpr, muteError));
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
                                                      inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
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
              expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
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
                  } else if (any_cast<scope *>(inst)) {
                    fullPath = ((scope *)inst)->VpiFullName();
                  }
                  if (muteError == false && m_muteError == false)
                    s.GetErrorHandler()(ErrorType::UHDM_DIVIDE_BY_ZERO,
                                        fullPath, expr1, nullptr);
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiPowerOp: {
            if (operands.size() == 2) {
              expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
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
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
              expr *num_expr =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
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
              if (divideByZero && (!invalidValue)) {
                // Divide by 0
                std::string fullPath;
                if (const gen_scope_array *in =
                        any_cast<gen_scope_array *>(inst)) {
                  fullPath = in->VpiFullName();
                } else if (inst->UhdmType() == uhdmdesign) {
                  fullPath = inst->VpiName();
                } else if (any_cast<scope *>(inst)) {
                  fullPath = ((scope *)inst)->VpiFullName();
                }
                if (muteError == false && m_muteError == false)
                  s.GetErrorHandler()(ErrorType::UHDM_DIVIDE_BY_ZERO, fullPath,
                                      div_expr, nullptr);
              }
            }
            break;
          }
          case vpiConditionOp: {
            if (operands.size() == 3) {
              bool localInvalidValue = false;
              expr *cond =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              int64_t condVal = get_value(invalidValue, cond);
              if (invalidValue) break;
              int64_t val = 0;
              expr *the_val = nullptr;
              if (condVal) {
                the_val = reduceExpr(operands[1], localInvalidValue, inst,
                                     pexpr, muteError);
              } else {
                the_val = reduceExpr(operands[2], localInvalidValue, inst,
                                     pexpr, muteError);
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
                  get_value(invalidValue, reduceExpr(operands[0], invalidValue,
                                                     inst, pexpr, muteError));
              if (invalidValue) break;
              if (n > 1000) n = 1000;  // Must be -1 or something silly
              if (n < 0) n = 0;
              expr *cv = (expr *)(operands[1]);
              if (cv->UhdmType() != uhdmconstant) {
                cv = reduceExpr(cv, invalidValue, inst, pexpr, muteError);
                if (cv->UhdmType() != uhdmconstant) {
                  break;
                }
              }
              UHDM::constant *c = s.MakeConstant();
              int64_t width = cv->VpiSize();
              int consttype = ((UHDM::constant *)cv)->VpiConstType();
              c->VpiConstType(consttype);
              if (consttype == vpiBinaryConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("BIN:").length());
                std::string value;
                if (width > (int)val.size()) {
                  value.append(width - val.size(), '0');
                }
                value += val;
                std::string res;
                for (unsigned int i = 0; i < n; i++) {
                  res += value;
                }
                c->VpiValue("BIN:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiHexConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("HEX:").length());
                std::string res;
                for (unsigned int i = 0; i < n; i++) {
                  res += val;
                }
                c->VpiValue("HEX:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiOctConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("OCT:").length());
                std::string res;
                for (unsigned int i = 0; i < n; i++) {
                  res += val;
                }
                c->VpiValue("OCT:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiStringConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("STRING:").length());
                std::string res;
                for (unsigned int i = 0; i < n; i++) {
                  res += val;
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
                if (expr *tmp = reduceExpr(oper, invalidValue, inst, pexpr,
                                           muteError)) {
                  oper = tmp;
                }
                optype = oper->UhdmType();
              }
              if (optype == uhdmconstant) {
                constant *c2 = (constant *)oper;
                std::string_view sv = c2->VpiValue();
                int size = c2->VpiSize();
                csize += size;
                int type = c2->VpiConstType();
                switch (type) {
                  case vpiBinaryConst: {
                    sv.remove_prefix(std::string_view("BIN:").length());
                    std::string value;
                    if (size > (int)sv.size()) {
                      value.append(size - sv.size(), '0');
                    }
                    if (op->VpiReordered()) {
                      value.append(sv.rbegin(), sv.rend());
                    } else {
                      value.append(sv.begin(), sv.end());
                    }
                    cval += value;
                    break;
                  }
                  case vpiDecConst: {
                    sv.remove_prefix(std::string_view("DEC:").length());
                    int64_t iv = 0;
                    if (NumUtils::parseInt64(sv, &iv) == nullptr) {
                      iv = 0;
                    }
                    std::string bin = NumUtils::toBinary(size, iv);
                    if (op->VpiReordered()) {
                      std::reverse(bin.begin(), bin.end());
                    }
                    cval += bin;
                    break;
                  }
                  case vpiHexConst: {
                    sv.remove_prefix(std::string_view("HEX:").length());
                    std::string tmp = NumUtils::hexToBin(sv);
                    std::string value;
                    if (size > (int)tmp.size()) {
                      value.append(size - tmp.size(), '0');
                    }
                    if (op->VpiReordered()) {
                      std::reverse(tmp.begin(), tmp.end());
                    }
                    value += tmp;
                    cval += value;
                    break;
                  }
                  case vpiOctConst: {
                    sv.remove_prefix(std::string_view("OCT:").length());
                    int64_t iv = 0;
                    if (NumUtils::parseOctal(sv, &iv) == nullptr) {
                      iv = 0;
                    }
                    std::string bin = NumUtils::toBinary(size, iv);
                    if (op->VpiReordered()) {
                      std::reverse(bin.begin(), bin.end());
                    }
                    cval += bin;
                    break;
                  }
                  case vpiIntConst: {
                    if (operands.size() == 1 || (size != 64)) {
                      sv.remove_prefix(std::string_view("INT:").length());
                      int64_t iv = 0;
                      if (NumUtils::parseInt64(sv, &iv) == nullptr) {
                        iv = 0;
                      }
                      std::string bin = NumUtils::toBinary(size, iv);
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
                      sv.remove_prefix(std::string_view("UINT:").length());
                      uint64_t iv = 0;
                      if (NumUtils::parseUint64(sv, &iv) == nullptr) {
                        iv = 0;
                      }
                      std::string bin = NumUtils::toBinary(size, iv);
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
                    sv.remove_prefix(std::string_view("STRING:").length());
                    cval += sv;
                    stringVal = true;
                    break;
                  }
                  default: {
                    if (sv.find("UINT:") == 0) {
                      sv.remove_prefix(std::string_view("UINT:").length());
                      uint64_t iv = 0;
                      if (NumUtils::parseUint64(sv, &iv) == nullptr) {
                        iv = 0;
                      }
                      std::string bin = NumUtils::toBinary(size, iv);
                      if (op->VpiReordered()) {
                        std::reverse(bin.begin(), bin.end());
                      }
                      cval += bin;
                    } else {
                      sv.remove_prefix(std::string_view("IINT:").length());
                      int64_t iv = 0;
                      if (NumUtils::parseInt64(sv, &iv) == nullptr) {
                        iv = 0;
                      }
                      std::string bin = NumUtils::toBinary(size, iv);
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
                  if (muteError == false && m_muteError == false)
                    s.GetErrorHandler()(
                        ErrorType::UHDM_INTERNAL_ERROR_OUT_OF_BOUND, fullPath,
                        op, nullptr);
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
            expr *oper =
                reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
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
                std::string_view val = itps->VpiValue();
                uint64_t cast_to = 0;
                if (val.empty()) {
                  cast_to = 32;
                } else if (val.find("UINT:") == 0) {
                  val.remove_prefix(std::string_view("UINT:").length());
                  if (NumUtils::parseUint64(val, &cast_to) == nullptr) {
                    cast_to = 32;
                  }
                } else {
                  val.remove_prefix(std::string_view("INT:").length());
                  if (NumUtils::parseIntLenient(val, &cast_to) == nullptr) {
                    cast_to = 32;
                  }
                }
                UHDM::constant *c = s.MakeConstant();
                uint64_t mask = ((uint64_t)(1ULL << cast_to)) - 1ULL;
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
    sys_func_call *scall = (sys_func_call *)result;
    const std::string_view name = scall->VpiName();
    if ((name == "$bits") || (name == "$size") || (name == "$high") ||
        (name == "$low") || (name == "$left") || (name == "$right")) {
      uint64_t bits = 0;
      bool found = false;
      for (auto arg : *scall->Tf_call_args()) {
        UHDM::UHDM_OBJECT_TYPE argtype = arg->UhdmType();
        if (argtype == uhdmref_obj) {
          ref_obj *ref = (ref_obj *)arg;
          const std::string_view objname = ref->VpiName();
          any *object = getObject(objname, inst, pexpr, muteError);
          if (object == nullptr) {
            if (inst->UhdmType() == uhdmpackage) {
              std::string name(inst->VpiName());
              name.append("::").append(objname);
              object = getObject(name, inst, pexpr, muteError);
            }
          }
          if (object) {
            if (UHDM::param_assign *passign =
                    any_cast<param_assign *>(object)) {
              object = (any *)passign->Rhs();
            }
          }
          if (object == nullptr) {
            object = (expr *)getValue(objname, inst, pexpr, muteError);
          }
          const typespec *tps = nullptr;
          if (any_cast<array_var *>(object)) {
            // Size the object, not its typespec
          } else if (expr *exp = any_cast<expr *>(object)) {
            tps = exp->Typespec();
          } else if (typespec *tp = any_cast<typespec *>(object)) {
            tps = tp;
          }
          if (tps) {
            bits += size(tps, invalidValue, inst, pexpr, (name != "$size"));
            found = true;
          } else {
            bits += size(object, invalidValue, inst, pexpr, (name != "$size"));
            found = true;
          }
        } else if (argtype == uhdmoperation) {
          operation *oper = (operation *)arg;
          if (oper->VpiOpType() == vpiConcatOp) {
            for (auto op : *oper->Operands()) {
              bits += size(op, invalidValue, inst, pexpr, (name != "$size"));
            }
            found = true;
          }
        } else if (argtype == uhdmhier_path) {
          hier_path *path = (hier_path *)arg;
          auto elems = path->Path_elems();
          if (elems && (elems->size() > 1)) {
            const std::string_view base = elems->at(0)->VpiName();
            const std::string_view suffix = elems->at(1)->VpiName();
            any *var = getObject(base, inst, pexpr, muteError);
            if (var) {
              if (UHDM::param_assign *passign = any_cast<param_assign *>(var)) {
                var = (any *)passign->Rhs();
              }
            }
            if (var) {
              UHDM_OBJECT_TYPE vtype = var->UhdmType();
              if (vtype == uhdmport) {
                port *p = (port *)var;
                if (const typespec *tps = p->Typespec()) {
                  UHDM_OBJECT_TYPE ttps = tps->UhdmType();
                  if (ttps == uhdmstruct_typespec) {
                    struct_typespec *tpss = (struct_typespec *)tps;
                    for (typespec_member *memb : *tpss->Members()) {
                      if (memb->VpiName() == suffix) {
                        const typespec *tps = memb->Typespec();
                        if (tps) {
                          bits += size(tps, invalidValue, inst, pexpr,
                                       (name != "$size"));
                          found = true;
                        }
                        break;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      if (found) {
        UHDM::constant *c = s.MakeConstant();
        c->VpiValue("UINT:" + std::to_string(bits));
        c->VpiDecompile(std::to_string(bits));
        c->VpiSize(64);
        c->VpiConstType(vpiUIntConst);
        result = c;
      }
    } else if (name == "$clog2") {
      bool invalidValue = false;
      for (auto arg : *scall->Tf_call_args()) {
        uint64_t clog2 = 0;
        uint64_t val =
            get_uvalue(invalidValue,
                       reduceExpr(arg, invalidValue, inst, pexpr, muteError));
        if (val) {
          val = val - 1;
          for (; val > 0; clog2 = clog2 + 1) {
            val = val >> 1;
          }
        }
        if (invalidValue == false) {
          UHDM::constant *c = s.MakeConstant();
          c->VpiValue("UINT:" + std::to_string(clog2));
          c->VpiDecompile(std::to_string(clog2));
          c->VpiSize(64);
          c->VpiConstType(vpiUIntConst);
          result = c;
        }
      }
    } else if (name == "$signed" || name == "$unsigned") {
      bool invalidTmpValue = false;
      if (scall->Tf_call_args()) {
        for (auto arg : *scall->Tf_call_args()) {
          expr *val = reduceExpr(arg, invalidTmpValue, inst, pexpr, muteError);
          if (val && (val->UhdmType() == uhdmconstant) &&
              (invalidTmpValue == false)) {
            constant *c = (constant *)val;
            if (c->VpiConstType() == vpiIntConst ||
                c->VpiConstType() == vpiDecConst) {
              int64_t value = get_value(invalidValue, val);
              int64_t size = c->VpiSize();
              if (name == "$signed") {
                return c;
              } else {
                uint64_t res = value;
                if (value >= 0) {
                  return c;
                } else {
                  res = ~value;
                  res = ~res;
                  uint64_t mask = NumUtils::getMask(size);
                  res = res & mask;
                  UHDM::constant *c = s.MakeConstant();
                  c->VpiValue("UINT:" + std::to_string(res));
                  c->VpiDecompile(std::to_string(res));
                  c->VpiSize(static_cast<int>(size));
                  c->VpiConstType(vpiUIntConst);
                  result = c;
                }
              }
            } else if (c->VpiConstType() == vpiUIntConst ||
                       c->VpiConstType() == vpiBinaryConst ||
                       c->VpiConstType() == vpiHexConst ||
                       c->VpiConstType() == vpiOctConst) {
              uint64_t value = get_uvalue(invalidValue, val);
              int64_t size = c->VpiSize();
              if (name == "$signed") {
                int64_t res = value;
                bool negsign = value & (1ULL << (size - 1));
                if (negsign) {
                  res &= ~(1ULL << (size - 1));
                  res = -res;
                }
                UHDM::constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(res));
                c->VpiDecompile(std::to_string(res));
                c->VpiSize(static_cast<int>(size));
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                result = c;
              }
            }
          }
        }
      }
    }
  } else if (objtype == uhdmfunc_call) {
    func_call *scall = (func_call *)result;
    const std::string_view name = scall->VpiName();
    std::vector<any *> *args = scall->Tf_call_args();
    UHDM::task_func *func = getTaskFunc(name, inst);
    function *actual_func = nullptr;
    if (func) {
      actual_func = any_cast<function *>(func);
    }
    if (actual_func == nullptr) {
      if (muteError == false && m_muteError == false) {
        const std::string errMsg(name);
        s.GetErrorHandler()(ErrorType::UHDM_UNDEFINED_USER_FUNCTION, errMsg,
                            scall, nullptr);
      }
      invalidValue = true;
    }
    expr *tmp = evalFunc(actual_func, args, invalidValue, inst, (any *)pexpr,
                         muteError);
    if (tmp && (invalidValue == false)) {
      result = tmp;
    }
  } else if (objtype == uhdmref_obj) {
    ref_obj *ref = (ref_obj *)result;
    const std::string_view name = ref->VpiName();
    any *tmp = getValue(name, inst, pexpr, muteError);
    if (tmp) {
      result = tmp;
    }
    return (expr *)result;
  } else if (objtype == uhdmhier_path) {
    hier_path *path = (hier_path *)result;
    expr *res = (expr *)decodeHierPath(path, invalidValue, inst, pexpr, false);
    return res;
  } else if (objtype == uhdmbit_select) {
    bit_select *sel = (bit_select *)result;
    const std::string_view name = sel->VpiName();
    const expr *index = sel->VpiIndex();
    uint64_t index_val = get_value(
        invalidValue,
        reduceExpr((expr *)index, invalidValue, inst, pexpr, muteError));
    if (invalidValue == false) {
      any *object = getObject(name, inst, pexpr, muteError);
      if (object) {
        if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
          object = (any *)passign->Rhs();
        }
      }
      if (object == nullptr) {
        object = getValue(name, inst, pexpr, muteError);
      }
      if (object) {
        if (expr *tmp = reduceExpr((expr *)object, invalidValue, inst, pexpr,
                                   muteError)) {
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
            expr *exp = reduceExpr(op, invalidValue, inst, pexpr, muteError);
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
            result = reduceBitSelect(op, static_cast<unsigned int>(index_val),
                                     invalidValue, inst, pexpr);
          }
        } else if (otype == uhdmconstant) {
          result = reduceBitSelect((constant *)object,
                                   static_cast<unsigned int>(index_val),
                                   invalidValue, inst, pexpr);
        }
      }
    }
  } else if (objtype == uhdmpart_select) {
    part_select *sel = (part_select *)result;
    any *parent = (any *)sel->VpiParent();
    std::string_view name = parent->VpiName();
    if (name.empty()) {
      if (ref_obj *ref = any_cast<ref_obj *>(parent)) {
        name = ref->VpiDefName();
      }
    }
    any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
        object = (any *)passign->Rhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    if (object && (object->UhdmType() == uhdmconstant)) {
      constant *co = (constant *)object;
      std::string binary;
      if (co->VpiConstType() == vpiBinaryConst) {
        binary = co->VpiValue();
        binary.erase(0, 4);
      } else if (co->VpiConstType() == vpiHexConst) {
        std::string_view val = co->VpiValue();
        val.remove_prefix(4);
        for (char ch : val) {
          char tmp[2] = {ch, '\0'};
          binary += hexToBinary(tmp);
        }
      } else {
        int64_t val = get_value(invalidValue, co);
        if (invalidValue == false) {
          binary = NumUtils::toBinary(co->VpiSize(), val);
        }
      }
      int64_t l = get_value(
          invalidValue,
          reduceExpr(sel->Left_range(), invalidValue, inst, pexpr, muteError));
      int64_t r = get_value(
          invalidValue,
          reduceExpr(sel->Right_range(), invalidValue, inst, pexpr, muteError));
      std::reverse(binary.begin(), binary.end());
      std::string sub;
      if (r > (int64_t)binary.size() || l > (int64_t)binary.size()) {
        sub = "0";
      } else {
        if (l > r)
          sub = binary.substr(r, l - r + 1);
        else
          sub = binary.substr(l, r - l + 1);
      }
      std::reverse(sub.begin(), sub.end());
      UHDM::constant *c = s.MakeConstant();
      c->VpiValue("BIN:" + sub);
      c->VpiDecompile(sub);
      c->VpiSize(static_cast<int>(sub.size()));
      c->VpiConstType(vpiBinaryConst);
      result = c;
    }
  } else if (objtype == uhdmindexed_part_select) {
    indexed_part_select *sel = (indexed_part_select *)result;
    const std::string_view name = sel->VpiParent()->VpiName();
    any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
        object = (any *)passign->Rhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    if (object && (object->UhdmType() == uhdmconstant)) {
      constant *co = (constant *)object;
      int64_t val = get_value(invalidValue, co);
      std::string binary = NumUtils::toBinary(co->VpiSize(), val);
      int64_t base = get_value(
          invalidValue,
          reduceExpr(sel->Base_expr(), invalidValue, inst, pexpr, muteError));
      int64_t offset = get_value(
          invalidValue,
          reduceExpr(sel->Width_expr(), invalidValue, inst, pexpr, muteError));
      std::reverse(binary.begin(), binary.end());
      std::string sub;
      if (sel->VpiIndexedPartSelectType() == vpiPosIndexed) {
        sub = binary.substr(base, base + offset);
      } else {
        sub = binary.substr(base, base - offset);
      }
      std::reverse(sub.begin(), sub.end());
      UHDM::constant *c = s.MakeConstant();
      c->VpiValue("BIN:" + sub);
      c->VpiDecompile(sub);
      c->VpiSize(static_cast<int>(sub.size()));
      c->VpiConstType(vpiBinaryConst);
      result = c;
    }
  } else if (objtype == uhdmvar_select) {
    var_select *sel = (var_select *)result;
    const std::string_view name = sel->VpiName();
    any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (UHDM::param_assign *passign = any_cast<param_assign *>(object)) {
        object = (any *)passign->Rhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    bool selection = false;
    for (auto index : *sel->Exprs()) {
      uint64_t index_val = get_value(
          invalidValue,
          reduceExpr((expr *)index, invalidValue, inst, pexpr, muteError));
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
            expr *exp =
                reduceExpr(object, invalidValue, inst, pexpr, muteError);
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
    expr *tmp = reduceExpr(result, invalidValue, inst, pexpr, muteError);
    if (tmp && !invalidValueTmp) result = tmp;
  }
  return (expr *)result;
}

bool ExprEval::setValueInInstance(std::string_view lhs, any *lhsexp,
                                  expr *rhsexp, bool &invalidValue,
                                  Serializer &s, const any *inst,
                                  const any *scope_exp, bool muteError) {
  bool invalidValueI = false;
  bool invalidValueUI = false;
  bool invalidValueD = false;
  bool opRhs = false;
  std::string_view lhsname = lhs;
  if (lhsname.empty()) {
    lhsname = lhsexp->VpiParent()->VpiName();
  }
  rhsexp = reduceExpr(rhsexp, invalidValue, inst, nullptr, muteError);
  int64_t valI = get_value(invalidValueI, rhsexp);
  uint64_t valUI = get_uvalue(invalidValueUI, rhsexp);
  long double valD = 0;
  if (invalidValueI) {
    valD = get_double(invalidValueD, rhsexp);
  }
  UHDM::VectorOfparam_assign *param_assigns = nullptr;
  if (inst->UhdmType() == uhdmgen_scope_array) {
  } else if (inst->UhdmType() == uhdmdesign) {
    param_assigns = ((design *)inst)->Param_assigns();
    if (param_assigns == nullptr) {
      ((design *)inst)->Param_assigns(s.MakeParam_assignVec());
      param_assigns = ((design *)inst)->Param_assigns();
    }
  } else if (any_cast<scope *>(inst)) {
    param_assigns = ((scope *)inst)->Param_assigns();
    if (param_assigns == nullptr) {
      ((scope *)inst)->Param_assigns(s.MakeParam_assignVec());
      param_assigns = ((scope *)inst)->Param_assigns();
    }
  }
  if (invalidValueI && invalidValueD) {
    if (param_assigns) {
      for (VectorOfparam_assign::iterator itr = param_assigns->begin();
           itr != param_assigns->end(); itr++) {
        if ((*itr)->Lhs()->VpiName() == lhsname) {
          param_assigns->erase(itr);
          break;
        }
      }
      param_assign *pa = s.MakeParam_assign();
      pa->Rhs(rhsexp);
      parameter *param = s.MakeParameter();
      param->VpiName(lhsname);
      pa->Lhs(param);
      param_assigns->push_back(pa);
      if (rhsexp && (rhsexp->UhdmType() == uhdmoperation)) {
        opRhs = true;
      }
    }
  } else if (invalidValueI) {
    if (param_assigns) {
      for (VectorOfparam_assign::iterator itr = param_assigns->begin();
           itr != param_assigns->end(); itr++) {
        if ((*itr)->Lhs()->VpiName() == lhsname) {
          param_assigns->erase(itr);
          break;
        }
      }
      constant *c = s.MakeConstant();
      c->VpiValue("REAL:" + std::to_string((double)valD));
      c->VpiDecompile(std::to_string(valD));
      c->VpiSize(64);
      c->VpiConstType(vpiRealConst);
      param_assign *pa = s.MakeParam_assign();
      pa->Rhs(c);
      parameter *param = s.MakeParameter();
      param->VpiName(lhsname);
      pa->Lhs(param);
      param_assigns->push_back(pa);
    }
  } else {
    if (param_assigns) {
      const any *prevRhs = nullptr;
      for (VectorOfparam_assign::iterator itr = param_assigns->begin();
           itr != param_assigns->end(); itr++) {
        if ((*itr)->Lhs()->VpiName() == lhsname) {
          prevRhs = (*itr)->Rhs();
          param_assigns->erase(itr);
          break;
        }
      }
      constant *c = any_cast<constant *>(rhsexp);
      if (c == nullptr) {
        c = s.MakeConstant();
        c->VpiValue("INT:" + std::to_string(valI));
        c->VpiDecompile(std::to_string(valI));
        c->VpiSize(64);
        c->VpiConstType(vpiIntConst);
      }
      if (lhsexp->UhdmType() == uhdmoperation) {
        operation *op = (operation *)lhsexp;
        if (op->VpiOpType() == vpiConcatOp) {
          std::string rhsbinary = NumUtils::toBinary(c->VpiSize(), valUI);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          VectorOfany *operands = op->Operands();
          uint64_t accumul = 0;
          for (any *oper : *operands) {
            std::string name;
            if (oper->UhdmType() == uhdmindexed_part_select) {
              name = oper->VpiParent()->VpiName();
            } else if (oper->UhdmType() == uhdmpart_select) {
              name = oper->VpiParent()->VpiName();
            } else if (lhsexp->UhdmType() == uhdmbit_select) {
              name = oper->VpiParent()->VpiName();
            } else {
              name = oper->VpiName();
            }
            uint64_t si =
                size(oper, invalidValue, inst, lhsexp, true, muteError);
            std::string part;
            for (uint64_t i = accumul; i < accumul + si; i++) {
              part += rhsbinary[i];
            }
            std::reverse(part.begin(), part.end());
            constant *c = s.MakeConstant();
            c->VpiValue("BIN:" + part);
            c->VpiDecompile(part);
            c->VpiSize(static_cast<int>(part.size()));
            c->VpiConstType(vpiBinaryConst);
            setValueInInstance(name, oper, c, invalidValue, s, inst, lhsexp,
                               muteError);
            accumul = accumul + si;
          }
        }
      } else if (lhsexp->UhdmType() == uhdmindexed_part_select) {
        indexed_part_select *sel = (indexed_part_select *)lhsexp;
        const std::string_view name = lhsexp->VpiParent()->VpiName();
        any *object = getObject(name, inst, scope_exp, muteError);
        if (object) {
          std::string lhsbinary;
          const typespec *tps = nullptr;
          if (expr *elhs = any_cast<expr *>(object)) {
            tps = elhs->Typespec();
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs && prevRhs->UhdmType() == uhdmconstant) {
            const constant *prev = (constant *)prevRhs;
            if (prev->VpiConstType() == vpiBinaryConst) {
              std::string_view val = prev->VpiValue();
              val.remove_prefix(std::string_view("BIN:").length());
              lhsbinary = val;
            } else {
              lhsbinary = NumUtils::toBinary(static_cast<int>(si),
                                             get_uvalue(invalidValue, prev));
            }
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          uint64_t base = get_uvalue(
              invalidValue, reduceExpr(sel->Base_expr(), invalidValue, inst,
                                       lhsexp, muteError));
          uint64_t offset = get_uvalue(
              invalidValue, reduceExpr(sel->Width_expr(), invalidValue, inst,
                                       lhsexp, muteError));
          std::string rhsbinary = NumUtils::toBinary(c->VpiSize(), valUI);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          if (sel->VpiIndexedPartSelectType() == vpiPosIndexed) {
            int index = 0;
            for (uint64_t i = base; i < base + offset; i++) {
              lhsbinary[i] = rhsbinary[index];
              index++;
            }
          } else {
            int index = 0;
            for (uint64_t i = base; i > base - offset; i--) {
              lhsbinary[i] = rhsbinary[index];
              index++;
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.MakeConstant();
          c->VpiValue("BIN:" + lhsbinary);
          c->VpiDecompile(lhsbinary);
          c->VpiSize(static_cast<int>(lhsbinary.size()));
          c->VpiConstType(vpiBinaryConst);
        }
      } else if (lhsexp->UhdmType() == uhdmpart_select) {
        part_select *sel = (part_select *)lhsexp;
        const std::string_view name = lhsexp->VpiParent()->VpiName();
        any *object = getObject(name, inst, scope_exp, muteError);
        if (object) {
          std::string lhsbinary;
          const typespec *tps = nullptr;
          if (expr *elhs = any_cast<expr *>(object)) {
            tps = elhs->Typespec();
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs && prevRhs->UhdmType() == uhdmconstant) {
            const constant *prev = (constant *)prevRhs;
            if (prev->VpiConstType() == vpiBinaryConst) {
              std::string_view val = prev->VpiValue();
              val.remove_prefix(std::string_view("BIN:").length());
              lhsbinary = val;
            } else {
              lhsbinary = NumUtils::toBinary(static_cast<int>(si),
                                             get_uvalue(invalidValue, prev));
            }
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          uint64_t left = get_uvalue(
              invalidValue, reduceExpr(sel->Left_range(), invalidValue, inst,
                                       lhsexp, muteError));
          uint64_t right = get_uvalue(
              invalidValue, reduceExpr(sel->Right_range(), invalidValue, inst,
                                       lhsexp, muteError));
          std::string rhsbinary = NumUtils::toBinary(c->VpiSize(), valUI);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          if (left > right) {
            int index = 0;
            for (uint64_t i = right; i <= left; i++) {
              lhsbinary[i] = rhsbinary[index];
              index++;
            }
          } else {
            int index = 0;
            for (uint64_t i = left; i <= right; i++) {
              lhsbinary[i] = rhsbinary[index];
              index++;
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.MakeConstant();
          c->VpiValue("BIN:" + lhsbinary);
          c->VpiDecompile(lhsbinary);
          c->VpiSize(static_cast<int>(lhsbinary.size()));
          c->VpiConstType(vpiBinaryConst);
        }
      } else if (lhsexp->UhdmType() == uhdmbit_select) {
        bit_select *sel = (bit_select *)lhsexp;
        const std::string_view name = lhsexp->VpiParent()->VpiName();
        any *object = getObject(name, inst, scope_exp, muteError);
        if (object) {
          std::string lhsbinary;
          const typespec *tps = nullptr;
          if (expr *elhs = any_cast<expr *>(object)) {
            tps = elhs->Typespec();
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs && prevRhs->UhdmType() == uhdmconstant) {
            const constant *prev = (constant *)prevRhs;
            if (prev->VpiConstType() == vpiBinaryConst) {
              std::string_view val = prev->VpiValue();
              val.remove_prefix(std::string_view("BIN:").length());
              lhsbinary = val;
            } else {
              lhsbinary = NumUtils::toBinary(static_cast<int>(si),
                                             get_uvalue(invalidValue, prev));
            }
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          uint64_t index =
              get_uvalue(invalidValue, reduceExpr(sel->VpiIndex(), invalidValue,
                                                  inst, lhsexp, muteError));
          lhsbinary[index] = std::to_string(valUI)[0];
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.MakeConstant();
          c->VpiValue("BIN:" + lhsbinary);
          c->VpiDecompile(lhsbinary);
          c->VpiSize(static_cast<int>(lhsbinary.size()));
          c->VpiConstType(vpiBinaryConst);
        }
      }
      param_assign *pa = s.MakeParam_assign();
      pa->Rhs(c);
      parameter *param = s.MakeParameter();
      param->VpiName(lhsname);
      pa->Lhs(param);
      param_assigns->push_back(pa);
    }
  }
  if (invalidValueI && invalidValueD && (!opRhs)) invalidValue = true;
  return invalidValue;
}

void ExprEval::evalStmt(std::string_view funcName, Scopes &scopes,
                        bool &invalidValue, bool &continue_flag,
                        bool &break_flag, bool &return_flag, const any *inst,
                        const any *stmt, std::set<std::string> &local_vars,
                        bool muteError) {
  if (invalidValue) {
    return;
  }
  Serializer &s = *inst->GetSerializer();
  UHDM_OBJECT_TYPE stt = stmt->UhdmType();
  switch (stt) {
    case uhdmcase_stmt: {
      case_stmt *st = (case_stmt *)stmt;
      expr *cond = (expr *)st->VpiCondition();
      int64_t val = get_value(
          invalidValue,
          reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError));
      for (case_item *item : *st->Case_items()) {
        if (VectorOfany *exprs = item->VpiExprs()) {
          bool done = false;
          for (any *exp : *exprs) {
            int64_t vexp = get_value(
                invalidValue, reduceExpr(exp, invalidValue, scopes.back(),
                                         nullptr, muteError));
            if (val == vexp) {
              evalStmt(funcName, scopes, invalidValue, continue_flag,
                       break_flag, return_flag, scopes.back(), item->Stmt(),
                       local_vars, muteError);
              done = true;
              break;
            }
          }
          if (done) break;
        }
      }
      break;
    }
    case uhdmif_else: {
      if_else *st = (if_else *)stmt;
      expr *cond = (expr *)st->VpiCondition();
      int64_t val = get_value(
          invalidValue,
          reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError));
      if (val > 0) {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->VpiStmt(), local_vars,
                 muteError);
      } else {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->VpiElseStmt(), local_vars,
                 muteError);
      }
      break;
    }
    case uhdmif_stmt: {
      if_stmt *st = (if_stmt *)stmt;
      expr *cond = (expr *)st->VpiCondition();
      int64_t val = get_value(
          invalidValue,
          reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError));
      if (val > 0) {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->VpiStmt(), local_vars,
                 muteError);
      }
      break;
    }
    case uhdmbegin: {
      begin *st = (begin *)stmt;
      if (st->Variables()) {
        for (auto var : *st->Variables()) {
          local_vars.insert(std::string(var->VpiName()));
        }
      }
      if (st->Stmts()) {
        for (auto bst : *st->Stmts()) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), bst, local_vars, muteError);
          if (continue_flag || break_flag || return_flag) {
            return;
          }
        }
      }
      break;
    }
    case uhdmnamed_begin: {
      named_begin *st = (named_begin *)stmt;
      if (st->Variables()) {
        for (auto var : *st->Variables()) {
          local_vars.insert(std::string(var->VpiName()));
        }
      }
      if (st->Stmts()) {
        for (auto bst : *st->Stmts()) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), bst, local_vars, muteError);
          if (continue_flag || break_flag || return_flag) {
            return;
          }
        }
      }
      break;
    }
    case uhdmassignment: {
      assignment *st = (assignment *)stmt;
      const std::string_view lhs = st->Lhs()->VpiName();
      expr *lhsexp = (expr *)st->Lhs();
      expr *rhs = (expr *)st->Rhs();
      expr *rhsexp =
          reduceExpr(rhs, invalidValue, scopes.back(), nullptr, muteError);
      invalidValue = setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s,
                                        inst, stmt, muteError);
      break;
    }
    case uhdmassign_stmt: {
      assign_stmt *st = (assign_stmt *)stmt;
      const std::string_view lhs = st->Lhs()->VpiName();
      expr *lhsexp = (expr *)st->Lhs();
      expr *rhs = (expr *)st->Rhs();
      expr *rhsexp =
          reduceExpr(rhs, invalidValue, scopes.back(), nullptr, muteError);
      invalidValue = setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s,
                                        inst, stmt, muteError);
      break;
    }
    case uhdmrepeat: {
      repeat *st = (repeat *)stmt;
      const expr *cond = st->VpiCondition();
      expr *rcond =
          reduceExpr((expr *)cond, invalidValue, scopes.back(), nullptr);
      int64_t val = get_value(
          invalidValue,
          reduceExpr(rcond, invalidValue, scopes.back(), nullptr, muteError));
      if (invalidValue == false) {
        for (int i = 0; i < val; i++) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->VpiStmt(), local_vars,
                   muteError);
        }
      }
      break;
    }
    case uhdmfor_stmt: {
      for_stmt *st = (for_stmt *)stmt;
      if (const any *stmt = st->VpiForInitStmt()) {
        if (stmt->UhdmType() == uhdmassign_stmt) {
          assign_stmt *assign = (assign_stmt *)stmt;
          local_vars.insert(std::string(assign->Lhs()->VpiName()));
        }
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->VpiForInitStmt(), local_vars,
                 muteError);
      }
      if (st->VpiForInitStmts()) {
        for (auto s : *st->VpiForInitStmts()) {
          if (s->UhdmType() == uhdmassign_stmt) {
            assign_stmt *assign = (assign_stmt *)s;
            local_vars.insert(std::string(assign->Lhs()->VpiName()));
          }
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), s, local_vars, muteError);
        }
      }
      while (1) {
        expr *cond = (expr *)st->VpiCondition();
        if (cond) {
          int64_t val = get_value(invalidValue,
                                  reduceExpr(cond, invalidValue, scopes.back(),
                                             nullptr, muteError));
          if (val == 0) {
            break;
          }
          if (invalidValue) break;
        }
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->VpiStmt(), local_vars,
                 muteError);
        if (invalidValue) break;
        if (continue_flag) {
          continue_flag = false;
          continue;
        }
        if (break_flag) {
          break_flag = false;
          break;
        }
        if (return_flag) {
          break;
        }
        if (st->VpiForIncStmt()) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->VpiForIncStmt(), local_vars,
                   muteError);
        }
        if (invalidValue) break;
        if (st->VpiForIncStmts()) {
          for (auto s : *st->VpiForIncStmts()) {
            evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                     return_flag, scopes.back(), s, local_vars, muteError);
          }
        }
        if (invalidValue) break;
      }
      break;
    }
    case uhdmreturn_stmt: {
      return_stmt *st = (return_stmt *)stmt;
      expr *cond = (expr *)st->VpiCondition();
      if (cond) {
        expr *rhsexp =
            reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError);
        ref_obj *lhsexp = s.MakeRef_obj();
        lhsexp->VpiName(funcName);
        invalidValue = setValueInInstance(
            funcName, lhsexp, rhsexp, invalidValue, s, inst, stmt, muteError);
        return_flag = true;
      }
      break;
    }
    case uhdmwhile_stmt: {
      while_stmt *st = (while_stmt *)stmt;
      expr *cond = (expr *)st->VpiCondition();
      if (cond) {
        while (1) {
          int64_t val = get_value(invalidValue,
                                  reduceExpr(cond, invalidValue, scopes.back(),
                                             nullptr, muteError));
          if (invalidValue) break;
          if (val == 0) {
            break;
          }
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->VpiStmt(), local_vars,
                   muteError);
          if (invalidValue) break;
          if (continue_flag) {
            continue_flag = false;
            continue;
          }
          if (break_flag) {
            break_flag = false;
            break;
          }
          if (return_flag) {
            break;
          }
        }
      }
      break;
    }
    case uhdmdo_while: {
      do_while *st = (do_while *)stmt;
      expr *cond = (expr *)st->VpiCondition();
      if (cond) {
        while (1) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->VpiStmt(), local_vars,
                   muteError);
          if (invalidValue) break;
          if (continue_flag) {
            continue_flag = false;
            continue;
          }
          if (break_flag) {
            break_flag = false;
            break;
          }
          if (return_flag) {
            break;
          }
          int64_t val = get_value(invalidValue,
                                  reduceExpr(cond, invalidValue, scopes.back(),
                                             nullptr, muteError));
          if (invalidValue) break;
          if (val == 0) {
            break;
          }
        }
      }
      break;
    }
    case uhdmcontinue_stmt: {
      continue_flag = true;
      break;
    }
    case uhdmbreak_stmt: {
      break_flag = true;
      break;
    }
    case uhdmoperation: {
      operation *op = (operation *)stmt;
      // ++, -- ops
      reduceExpr(op, invalidValue, scopes.back(), nullptr, muteError);
      break;
    }
    default: {
      invalidValue = true;
      if (muteError == false && m_muteError == false) {
        const std::string errMsg(inst->VpiName());
        s.GetErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg, stmt,
                            nullptr);
      }
      break;
    }
  }
}

expr *ExprEval::evalFunc(UHDM::function *func, std::vector<any *> *args,
                         bool &invalidValue, const any *inst, any *pexpr,
                         bool muteError) {
  if (func == nullptr) {
    invalidValue = true;
    return nullptr;
  }
  Serializer &s = *func->GetSerializer();
  const std::string_view name = func->VpiName();
  // set internal scope stack
  Scopes scopes;
  module_inst *scope = s.MakeModule_inst();
  scope->VpiParent((any *)inst);
  if (const instance *pack = func->Instance()) {
    scope->Task_funcs(pack->Task_funcs());
    scope->Parameters(pack->Parameters());
  }
  UHDM::VectorOfparam_assign *param_assigns = nullptr;
  if (inst->UhdmType() == uhdmgen_scope_array) {
  } else if (inst->UhdmType() == uhdmdesign) {
    param_assigns = ((design *)inst)->Param_assigns();
  } else if (any_cast<UHDM::scope *>(inst)) {
    param_assigns = ((UHDM::scope *)inst)->Param_assigns();
  }
  std::set<std::string> variables;
  if (param_assigns) {
    scope->Param_assigns(s.MakeParam_assignVec());
    for (auto p : *param_assigns) {
      ElaboratorListener listener(&s, false, muteError);
      any *pp = UHDM::clone_tree(p, s, &listener);
      scope->Param_assigns()->push_back((param_assign *)pp);
      variables.insert(std::string(p->Lhs()->VpiName()));
    }
  }
  // set args
  if (func->Io_decls()) {
    unsigned int index = 0;
    for (auto io : *func->Io_decls()) {
      if (args && (index < args->size())) {
        const std::string_view ioname = io->VpiName();
        variables.insert(std::string(ioname));
        expr *ioexp = (expr *)args->at(index);
        expr *exparg = reduceExpr(ioexp, invalidValue, scope, pexpr, muteError);
        if (exparg) {
          exparg->Typespec((typespec *)io->Typespec());
          invalidValue = setValueInInstance(ioname, io, exparg, invalidValue, s,
                                            scope, func, muteError);
        }
      }
      index++;
    }
  }
  if (func->Variables()) {
    for (auto var : *func->Variables()) {
      variables.insert(std::string(var->VpiName()));
    }
  }
  variables.insert(std::string(name));
  scopes.push_back(scope);
  if (const UHDM::any *the_stmt = func->Stmt()) {
    UHDM_OBJECT_TYPE stt = the_stmt->UhdmType();
    bool return_flag = false;
    switch (stt) {
      case uhdmbegin: {
        UHDM::begin *st = (UHDM::begin *)the_stmt;
        bool continue_flag = false;
        bool break_flag = false;
        for (auto stmt : *st->Stmts()) {
          evalStmt(name, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scope, stmt, variables, muteError);
          if (return_flag) break;
          if (continue_flag || break_flag) {
            if (muteError == false && m_muteError == false) {
              const std::string errMsg(inst->VpiName());
              s.GetErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg,
                                  stmt, nullptr);
            }
          }
        }
        break;
      }
      case uhdmnamed_begin: {
        UHDM::named_begin *st = (UHDM::named_begin *)the_stmt;
        bool continue_flag = false;
        bool break_flag = false;
        for (auto stmt : *st->Stmts()) {
          evalStmt(name, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scope, stmt, variables, muteError);
          if (return_flag) break;
          if (continue_flag || break_flag) {
            if (muteError == false && m_muteError == false) {
              const std::string errMsg(inst->VpiName());
              s.GetErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg,
                                  stmt, nullptr);
            }
          }
        }
        break;
      }
      default: {
        bool continue_flag = false;
        bool break_flag = false;
        evalStmt(name, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scope, the_stmt, variables, muteError);
        if (continue_flag || break_flag) {
          if (muteError == false && m_muteError == false) {
            const std::string errMsg(inst->VpiName());
            s.GetErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg,
                                the_stmt, nullptr);
          }
        }
        break;
      }
    }
  }
  // return value
  if (scope->Param_assigns()) {
    for (auto p : *scope->Param_assigns()) {
      const std::string n(p->Lhs()->VpiName());
      if ((!n.empty()) && (variables.find(n) == variables.end())) {
        invalidValue = true;
        return nullptr;
      }
    }
    for (auto p : *scope->Param_assigns()) {
      if (p->Lhs()->VpiName() == name) {
        const typespec *tps = nullptr;
        if (func->Return()) tps = func->Return()->Typespec();
        if (tps && (tps->UhdmType() == uhdmlogic_typespec)) {
          uint64_t s = size(tps, invalidValue, inst, pexpr, true, true);
          if (p->Rhs() && (p->Rhs()->UhdmType() == uhdmconstant)) {
            constant *c = (constant *)p->Rhs();
            if (c->VpiConstType() == vpiBinaryConst) {
              std::string_view val = c->VpiValue();
              val.remove_prefix(std::string_view("BIN:").length());
              if (val.size() > s) {
                val.remove_prefix(val.size() - s);
                c->VpiValue(std::string("BIN:").append(val));
                c->VpiDecompile(val);
              }
            } else {
              uint64_t mask = NumUtils::getMask(s);
              int64_t v = get_value(invalidValue, c);
              v = v & mask;
              c->VpiValue("UINT:" + std::to_string(v));
              c->VpiDecompile(std::to_string(v));
              c->VpiConstType(vpiUIntConst);
            }
            c->VpiSize(static_cast<int>(s));
          }
        }
        return (expr *)p->Rhs();
      }
    }
  }
  invalidValue = true;
  return nullptr;
}

namespace UHDM {
std::string vPrint(UHDM::any *handle) {
  if (handle == nullptr) {
    // std::cout << "NULL HANDLE\n";
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
    // std::cout << "NULL HANDLE\n";
    return "NULL HANDLE";
  }
  ExprEval eval;
  Serializer *s = handle->GetSerializer();
  std::stringstream out;
  eval.prettyPrint(*s, handle, 0, out);
  return out.str();
}
