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
#include <uhdm/vpi_visitor.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <locale>
#include <map>
#include <regex>
#include <sstream>
#include <string_view>

namespace UHDM {
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
  void leaveBit_select(const bit_select *object, vpiHandle handle) final {
    hasRef_obj = true;
  }
  void leaveIndexed_part_select(const indexed_part_select *object,
                                vpiHandle handle) final {
    hasRef_obj = true;
  }
  void leavePart_select(const part_select *object, vpiHandle handle) final {
    hasRef_obj = true;
  }
  void leaveVar_select(const var_select *object, vpiHandle handle) final {
    hasRef_obj = true;
  }
  void leaveHier_path(const hier_path *object, vpiHandle handle) final {
    hasRef_obj = true;
  }
  bool refObjDetected() const { return hasRef_obj; }

 private:
  bool hasRef_obj = false;
};

bool ExprEval::isFullySpecified(const typespec *tps) {
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

std::string ExprEval::toBinary(const constant *c) {
  std::string result;
  if (c == nullptr) return result;
  int32_t type = c->VpiConstType();
  std::string_view sv = c->VpiValue();
  switch (type) {
    case vpiBinaryConst: {
      sv.remove_prefix(std::string_view("BIN:").length());
      result = sv;
      if (c->VpiSize() >= 0) {
        if (result.size() < (uint32_t)c->VpiSize()) {
          uint32_t rsize = result.size();
          for (uint32_t i = 0; i < (uint32_t)c->VpiSize() - rsize; i++) {
            result = "0" + result;
          }
        }
      }
      break;
    }
    case vpiDecConst: {
      sv.remove_prefix(std::string_view("DEC:").length());
      uint64_t res = 0;
      if (NumUtils::parseIntLenient(sv, &res) == nullptr) {
        res = 0;
      }
      result = NumUtils::toBinary(c->VpiSize(), res);
      break;
    }
    case vpiHexConst: {
      sv.remove_prefix(std::string_view("HEX:").length());
      result = NumUtils::hexToBin(sv);
      if (c->VpiSize() >= 0) {
        if (result.size() < (uint32_t)c->VpiSize()) {
          uint32_t rsize = result.size();
          for (uint32_t i = 0; i < (uint32_t)c->VpiSize() - rsize; i++) {
            result = "0" + result;
          }
        }
      }
      break;
    }
    case vpiOctConst: {
      sv.remove_prefix(std::string_view("OCT:").length());
      result = NumUtils::hexToBin(sv);
      if (c->VpiSize() >= 0) {
        if (result.size() < (uint32_t)c->VpiSize()) {
          uint32_t rsize = result.size();
          for (uint32_t i = 0; i < (uint32_t)c->VpiSize() - rsize; i++) {
            result = "0" + result;
          }
        }
      }
      break;
    }
    case vpiIntConst: {
      sv.remove_prefix(std::string_view("INT:").length());
      uint64_t res = 0;
      if (NumUtils::parseIntLenient(sv, &res) == nullptr) {
        res = 0;
      }
      result = NumUtils::toBinary(c->VpiSize(), res);
      break;
    }
    case vpiUIntConst: {
      sv.remove_prefix(std::string_view("UINT:").length());
      uint64_t res = 0;
      if (NumUtils::parseUint64(sv, &res) == nullptr) {
        res = 0;
      }
      result = NumUtils::toBinary(c->VpiSize(), res);
      break;
    }
    case vpiScalar: {
      sv.remove_prefix(std::string_view("SCAL:").length());
      uint64_t res = 0;
      if (NumUtils::parseBinary(sv, &res) == nullptr) {
        res = 0;
      }
      result = NumUtils::toBinary(c->VpiSize(), res);
      break;
    }
    case vpiStringConst: {
      sv.remove_prefix(std::string_view("STRING:").length());
      if (sv.size() > 32) {
        return result;
      }
      uint64_t res = 0;
      for (uint32_t i = 0; i < sv.size(); i++) {
        res += (sv[i] << ((sv.size() - (i + 1)) * 8));
      }
      result = NumUtils::toBinary(c->VpiSize(), res);
      break;
    }
    case vpiRealConst: {
      // Don't do the double precision math, leave it to client tools
      break;
    }
    default: {
      if (sv.find("UINT:") == 0) {
        sv.remove_prefix(std::string_view("UINT:").length());
        uint64_t res = 0;
        if (NumUtils::parseUint64(sv, &res) == nullptr) {
          res = 0;
        }
        result = NumUtils::toBinary(c->VpiSize(), res);
      } else {
        sv.remove_prefix(std::string_view("INT:").length());
        uint64_t res = 0;
        if (NumUtils::parseIntLenient(sv, &res) == nullptr) {
          res = 0;
        }
        result = NumUtils::toBinary(c->VpiSize(), res);
      }
      break;
    }
  }
  return result;
}

std::vector<std::string_view> ExprEval::tokenizeMulti(
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
                        const any *pexpr, bool muteError,
                        const any *checkLoop) {
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
    VectorOfparam_assign *param_assigns = nullptr;
    VectorOfany *parameters = nullptr;
    VectorOftypespec *typespecs = nullptr;
    if (the_instance->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
    } else if (the_instance->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
      param_assigns = ((design *)the_instance)->Param_assigns();
      typespecs = ((design *)the_instance)->Typespecs();
    } else if (const scope *spe = any_cast<const scope *>(the_instance)) {
      param_assigns = spe->Param_assigns();
      typespecs = spe->Typespecs();
      parameters = spe->Parameters();
    }
    if (param_assigns) {
      for (auto p : *param_assigns) {
        if (p->Lhs() && (p->Lhs()->VpiName() == the_name)) {
          result = (any *)p->Rhs();
          // A >64-bit parameter resolves via its complex-value pattern
          // operation, which carries no typespec of its own; propagate the
          // declaring parameter's typespec so downstream member/element
          // selection knows the struct geometry.
          if (result && (result->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation)) {
            operation *rop = (operation *)result;
            if (rop->Typespec() == nullptr) {
              if (const parameter *lp =
                      any_cast<const parameter *>(p->Lhs())) {
                if (lp->Typespec() && lp->Typespec()->Actual_typespec()) {
                  ref_typespec *rt = s.MakeRef_typespec();
                  rt->Actual_typespec(
                      (typespec *)lp->Typespec()->Actual_typespec());
                  rt->VpiParent(rop);
                  rop->Typespec(rt);
                }
              }
            }
          }
          break;
        }
      }
    }
    if ((result == nullptr) && parameters) {
      for (auto p : *parameters) {
        if (p->VpiName() == the_name) {
          if (p->UhdmType() == uhdmparameter) {
            result = (any*) p;
            break;
          }
        }
      }
    }
    if ((result == nullptr) && (typespecs != nullptr)) {
      for (auto p : *typespecs) {
        if (p->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
          enum_typespec *e = (enum_typespec *)p;
          for (auto c : *e->Enum_consts()) {
            if (c->VpiName() == the_name) {
              constant *cc = s.MakeConstant();
              const std::string_view v = c->VpiValue();
              cc->VpiValue(v);
              cc->VpiSize(c->VpiSize());
              // enum_const has no VpiConstType; derive it from the value
              // prefix so downstream folds (e.g. the concat reduction's
              // switch on VpiConstType) don't misparse the string — an
              // unset type falls into the UINT:/IINT: default branch,
              // which turns "HEX:6f" into 0.
              if (v.find("HEX:") == 0)
                cc->VpiConstType(vpiHexConst);
              else if (v.find("BIN:") == 0)
                cc->VpiConstType(vpiBinaryConst);
              else if (v.find("OCT:") == 0)
                cc->VpiConstType(vpiOctConst);
              else if (v.find("DEC:") == 0)
                cc->VpiConstType(vpiDecConst);
              else if (v.find("UINT:") == 0)
                cc->VpiConstType(vpiUIntConst);
              else if (v.find("INT:") == 0)
                cc->VpiConstType(vpiIntConst);
              else if (v.find("STRING:") == 0)
                cc->VpiConstType(vpiStringConst);
              result = cc;
              break;
            }
          }
        }
      }
    }
    if (result && (result->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation)) {
      operation *op = (operation *)result;
      if (const ref_typespec *rt = op->Typespec()) {
        ExprEval eval;
        if (expr *res = eval.flattenPatternAssignments(s, rt->Actual_typespec(),
                                                       (expr *)result)) {
          if (res->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
            ((operation *)result)->Operands(((operation *)res)->Operands());
          }
        }
      }
    }
    if (result) break;

    the_instance = the_instance->VpiParent();
  }

  if (result) {
    UHDM_OBJECT_TYPE resultType = result->UhdmType();
    if (resultType == UHDM_OBJECT_TYPE::uhdmconstant) {
    } else if (resultType == UHDM_OBJECT_TYPE::uhdmref_obj) {
      if (result->VpiName() != name) {
        if (any *rval = getValue(result->VpiName(), inst, pexpr, muteError)) {
          result = rval;
        }
      }
    } else if ((resultType == UHDM_OBJECT_TYPE::uhdmoperation) ||
               (resultType == UHDM_OBJECT_TYPE::uhdmhier_path) ||
               (resultType == UHDM_OBJECT_TYPE::uhdmbit_select) ||
               (resultType == UHDM_OBJECT_TYPE::uhdmsys_func_call)) {
      bool invalidValue = false;
      if (checkLoop && (result == checkLoop)) {
        return nullptr;
      }
      if (any *rval =
              reduceExpr(result, invalidValue, inst, pexpr, muteError)) {
        result = rval;
      }
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
    if (const scope *spe = any_cast<const scope *>(pexpr)) {
      if (spe->Variables()) {
        for (auto o : *spe->Variables()) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (const task_func *s = any_cast<const task_func *>(pexpr)) {
      if (s->Io_decls()) {
        for (auto o : *s->Io_decls()) {
          if (o->VpiName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && s->Param_assigns()) {
        for (auto o : *s->Param_assigns()) {
          const std::string_view pname = o->Lhs()->VpiName();
          if (pname == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (pexpr->UhdmType() == UHDM_OBJECT_TYPE::uhdmforeach_stmt) {
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
    if (pexpr->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_defn) {
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
        if (result) break;

        const class_defn *base_defn = nullptr;
        if (const extends *ext = defn->Extends()) {
          if (const ref_typespec *rt = ext->Class_typespec()) {
            if (const class_typespec *tp =
                    rt->Actual_typespec<class_typespec>()) {
              base_defn = tp->Class_defn();
            }
          }
        }
        defn = base_defn;
      }
    }
    if (result) break;
    pexpr = pexpr->VpiParent();
  }
  if (result == nullptr) {
    while (inst) {
      VectorOfparam_assign *param_assigns = nullptr;
      VectorOfvariables *variables = nullptr;
      VectorOfarray_net *array_nets = nullptr;
      VectorOfnet *nets = nullptr;
      VectorOftypespec *typespecs = nullptr;
      VectorOfscope *scopes = nullptr;
      if (inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
      } else if (inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
        param_assigns = ((design *)inst)->Param_assigns();
        typespecs = ((design *)inst)->Typespecs();
      } else if (const scope *spe = any_cast<const scope *>(inst)) {
        param_assigns = spe->Param_assigns();
        variables = spe->Variables();
        typespecs = spe->Typespecs();
        scopes = spe->Scopes();
        if (const instance *in = any_cast<const instance *>(inst)) {
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
          (result && (result->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant) &&
           (result->UhdmType() != UHDM_OBJECT_TYPE::uhdmparam_assign))) {
        if (any *tmpresult = getValue(name, inst, pexpr, muteError)) {
          result = tmpresult;
        }
      }
      if (result) break;
      if (inst) {
        if (inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst) {
          break;
        } else {
          inst = inst->VpiParent();
        }
      }
    }
  }

  if (result && (result->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj)) {
    ref_obj *ref = (ref_obj *)result;
    const std::string_view refname = ref->VpiName();
    if (refname != name) result = getObject(refname, inst, pexpr, muteError);
    if (result) {
      if (param_assign *passign = any_cast<param_assign *>(result)) {
        result = passign->Rhs();
      }
    }
  }
  if ((result == nullptr) && getObjectFunctor) {
    return getObjectFunctor(name, inst, pexpr);
  }
  return result;
}

long double ExprEval::get_double(bool &invalidValue, const expr *expr) {
  long double result = 0;
  if (const constant *c = any_cast<const constant *>(expr)) {
    int32_t type = c->VpiConstType();
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

uint64_t ExprEval::getValue(const expr *expr) {
  uint64_t result = 0;
  if (expr && expr->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
    constant *c = (constant *)expr;
    std::string_view sv = c->VpiValue();
    int32_t type = c->VpiConstType();
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
  int32_t index = 0;
  for (any *op : *ordered) {
    if (op->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
      tagged_pattern *tp = (tagged_pattern *)op;
      const typespec *ttp = nullptr;
      if (const ref_typespec *rt = tp->Typespec()) {
        ttp = rt->Actual_typespec();
      }
      UHDM_OBJECT_TYPE ttpt = ttp->UhdmType();
      switch (ttpt) {
        case UHDM_OBJECT_TYPE::uhdmint_typespec: {
          flattened->push_back(tp->Pattern());
          break;
        }
        case UHDM_OBJECT_TYPE::uhdminteger_typespec: {
          flattened->push_back(tp->Pattern());
          break;
        }
        case UHDM_OBJECT_TYPE::uhdmstring_typespec: {
          any *sop = (any *)tp->Pattern();
          UHDM_OBJECT_TYPE sopt = sop->UhdmType();
          if (sopt == UHDM_OBJECT_TYPE::uhdmoperation) {
            VectorOfany *operands = ((operation *)sop)->Operands();
            for (auto op1 : *operands) {
              bool substituted = false;
              if (op1->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
                tagged_pattern *tp1 = (tagged_pattern *)op1;
                const typespec *ttp1 = nullptr;
                if (const ref_typespec *rt = tp1->Typespec()) {
                  ttp1 = rt->Actual_typespec();
                }
                UHDM_OBJECT_TYPE ttpt1 = ttp1->UhdmType();
                if (ttpt1 == UHDM_OBJECT_TYPE::uhdmstring_typespec) {
                  if (ttp1->VpiName() == "default") {
                    const any *patt = tp1->Pattern();
                    const typespec *mold = fieldTypes[index];
                    operation *subst = s.MakeOperation();
                    VectorOfany *sops = s.MakeAnyVec();
                    subst->Operands(sops);
                    subst->VpiOpType(vpiConcatOp);
                    flattened->push_back(subst);
                    UHDM_OBJECT_TYPE moldtype = mold->UhdmType();
                    if (moldtype == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
                      struct_typespec *molds = (struct_typespec *)mold;
                      for (auto mem : *molds->Members()) {
                        if (mem) sops->push_back((any *)patt);
                      }
                    } else if (moldtype ==
                               UHDM_OBJECT_TYPE::uhdmlogic_typespec) {
                      logic_typespec *molds = (logic_typespec *)mold;
                      VectorOfrange *ranges = molds->Ranges();
                      if (!ranges->empty()) {
                        range *r = ranges->front();
                        uint64_t from = getValue(r->Left_expr());
                        uint64_t to = getValue(r->Right_expr());
                        if (from > to) {
                          std::swap(from, to);
                        }
                        for (uint64_t i = from; i <= to; i++) {
                          sops->push_back((any *)patt);
                        }
                        // TODO: Multidimension
                      }
                    }
                    substituted = true;
                    break;
                  }
                }
              } else if (op1->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
                // recursiveFlattening(s, flattened,
                // ((operation*)op1)->Operands(), fieldTypes);
                // substituted = true;
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
  if (exp->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
    operation *op = (operation *)exp;
    if (op->VpiOpType() == vpiConditionOp) {
      VectorOfany *ops = op->Operands();
      ops->at(1) = flattenPatternAssignments(s, tps, (expr *)ops->at(1));
      ops->at(2) = flattenPatternAssignments(s, tps, (expr *)ops->at(2));
      return result;
    }
    if (op->VpiOpType() != vpiAssignmentPatternOp) {
      return result;
    }
    if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
      array_typespec *atps = (array_typespec *)tps;
      if (const ref_typespec *rt = atps->Elem_typespec()) {
        tps = rt->Actual_typespec();
      }
    }
    if (tps == nullptr) {
      return result;
    }
    if (tps->UhdmType() != UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
      if (const ref_typespec *rt = op->Typespec()) {
        tps = rt->Actual_typespec();
      }
    }
    if (tps == nullptr) {
      return result;
    }
    if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
      array_typespec *atps = (array_typespec *)tps;
      if (const ref_typespec *rt = atps->Elem_typespec()) {
        tps = rt->Actual_typespec();
      }
    }
    if (tps->UhdmType() != UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
      return result;
    }
    if (op->VpiFlattened()) {
      return result;
    }
    struct_typespec *stps = (struct_typespec *)tps;
    std::vector<std::string_view> fieldNames;
    std::vector<const typespec *> fieldTypes;
    for (typespec_member *memb : *stps->Members()) {
      if (const ref_typespec *rt = memb->Typespec()) {
        fieldNames.emplace_back(memb->VpiName());
        fieldTypes.emplace_back(rt->Actual_typespec());
      }
    }
    VectorOfany *orig = op->Operands();
    if (orig->size() == 1) {
      for (auto oper : *orig) {
        if (oper->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
          operation *opi = (operation *)oper;
          if (opi->VpiOpType() == vpiAssignmentPatternOp) {
            op = opi;
            orig = op->Operands();
            break;
          }
        }
      }
    }
    VectorOfany *ordered = s.MakeAnyVec();
    std::vector<any *> tmp(fieldNames.size());
    any *defaultOp = nullptr;
    int32_t index = 0;
    bool flatten = false;
    for (auto oper : *orig) {
      if (oper->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
        tagged_pattern *tp = (tagged_pattern *)oper;
        const typespec *ttp = nullptr;
        if (const ref_typespec *rt = tp->Typespec()) {
          ttp = rt->Actual_typespec();
        }
        const std::string_view tname = ttp->VpiName();
        bool found = false;
        if (tname == "default") {
          defaultOp = oper;
          found = true;
        }
        for (uint32_t i = 0; i < fieldNames.size(); i++) {
          if (tname == fieldNames[i]) {
            tmp[i] = oper;
            found = true;
            break;
          }
        }
        if (found == false) {
          for (uint32_t i = 0; i < fieldTypes.size(); i++) {
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
      } else if (oper->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
        return result;
      } else if (oper->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
        return result;
      } else {
        if (index < (int32_t)tmp.size()) {
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
    ElaboratorContext elaboratorContext(&s, false, m_muteError);
    for (auto opi : tmp) {
      if (defaultOp && (opi == nullptr)) {
        opi = clone_tree((any *)defaultOp, &elaboratorContext);
        if (opi != nullptr) {
          opi->VpiParent(const_cast<any *>(defaultOp->VpiParent()));
        }
      }
      if (opi == nullptr) {
        if (!m_muteError) {
          const std::string errMsg(fieldNames[index]);
          s.GetErrorHandler()(ErrorType::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN,
                              errMsg, exp, nullptr);
        }
        return result;
      }
      if (opi->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
        tagged_pattern *tp = (tagged_pattern *)opi;
        const any *patt = tp->Pattern();
        if (patt->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
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
              c->VpiSize(static_cast<int32_t>(size));
            } else if (uval == 0) {
              uint64_t size = ExprEval::size(fieldTypes[index], invalidValue,
                                             nullptr, exp, true, true);
              c->VpiValue("UINT:" + std::to_string(uval));
              c->VpiDecompile(std::to_string(uval));
              c->VpiConstType(vpiUIntConst);
              c->VpiSize(static_cast<int32_t>(size));
            }
          }
        } else if (patt->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
          operation *patt_op = (operation *)patt;
          if (patt_op->VpiOpType() == vpiAssignmentPatternOp) {
            opi = flattenPatternAssignments(s, fieldTypes[index], patt_op);
          }
        }
      }
      ordered->push_back(opi);
      index++;
    }
    operation *opres = (operation *)clone_tree((any *)op, &elaboratorContext);
    opres->VpiParent(const_cast<any *>(op->VpiParent()));
    ref_typespec* rtps = s.MakeRef_typespec();
    opres->Typespec(rtps);
    rtps->Actual_typespec((typespec*) tps);
    opres->Operands(ordered);
    if (flatten) {
      opres->VpiFlattened(true);
    }
    // Flattening
    VectorOfany *flattened = s.MakeAnyVec();
    recursiveFlattening(s, flattened, ordered, fieldTypes);
    for (auto o : *flattened) o->VpiParent(opres);
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
    case UHDM_OBJECT_TYPE::uhdmconstant: {
      constant *c = (constant *)object;
      out << c->VpiDecompile();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmparameter: {
      parameter *p = (parameter *)object;
      std::string_view val = p->VpiValue();
      val = ltrim(val, ':');
      out << val;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmsys_func_call: {
      sys_func_call *sysFuncCall = (sys_func_call *)object;
      out << sysFuncCall->VpiName() << "(";
      if (sysFuncCall->Tf_call_args()) {
        for (uint32_t i = 0; i < sysFuncCall->Tf_call_args()->size(); i++) {
          prettyPrint(s, sysFuncCall->Tf_call_args()->at(i), 0, out);
          if (i < sysFuncCall->Tf_call_args()->size() - 1) {
            out << ",";
          }
        }
      }
      out << ")";
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmenum_const: {
      enum_const *c = (enum_const *)object;
      std::string_view val = c->VpiValue();
      val = ltrim(val, ':');
      out << val;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      operation *oper = (operation *)object;
      int32_t opType = oper->VpiOpType();
      switch (opType) {
        case vpiMinusOp:
        case vpiPlusOp:
        case vpiNotOp:
        case vpiBitNegOp:
        case vpiUnaryAndOp:
        case vpiUnaryNandOp:
        case vpiUnaryOrOp:
        case vpiUnaryNorOp:
        case vpiUnaryXorOp:
        case vpiUnaryXNorOp:
        case vpiPreIncOp:
        case vpiPreDecOp: {
          static std::unordered_map<int32_t, std::string_view> opToken = {
              {vpiMinusOp, "-"},    {vpiPlusOp, "+"},
              {vpiNotOp, "!"},      {vpiBitNegOp, "~"},
              {vpiUnaryAndOp, "&"}, {vpiUnaryNandOp, "~&"},
              {vpiUnaryOrOp, "|"},  {vpiUnaryNorOp, "~|"},
              {vpiUnaryXorOp, "^"}, {vpiUnaryXNorOp, "~^"},
              {vpiPreIncOp, "++"},  {vpiPreDecOp, "--"},
          };
          std::stringstream out_op0;
          prettyPrint(s, oper->Operands()->at(0), 0, out_op0);
          out << opToken[opType] << out_op0.str();
          break;
        }
        case vpiSubOp:
        case vpiDivOp:
        case vpiModOp:
        case vpiEqOp:
        case vpiNeqOp:
        case vpiCaseEqOp:
        case vpiCaseNeqOp:
        case vpiGtOp:
        case vpiGeOp:
        case vpiLtOp:
        case vpiLeOp:
        case vpiLShiftOp:
        case vpiRShiftOp:
        case vpiAddOp:
        case vpiMultOp:
        case vpiLogAndOp:
        case vpiLogOrOp:
        case vpiBitAndOp:
        case vpiBitOrOp:
        case vpiBitXorOp:
        case vpiBitXNorOp:
        case vpiArithLShiftOp:
        case vpiArithRShiftOp:
        case vpiPowerOp:
        case vpiImplyOp:
        case vpiNonOverlapImplyOp:
        case vpiOverlapImplyOp: {
          static std::unordered_map<int32_t, std::string_view> opToken = {
              {vpiMinusOp, "-"},
              {vpiPlusOp, "+"},
              {vpiNotOp, "!"},
              {vpiBitNegOp, "~"},
              {vpiUnaryAndOp, "&"},
              {vpiUnaryNandOp, "~&"},
              {vpiUnaryOrOp, "|"},
              {vpiUnaryNorOp, "~|"},
              {vpiUnaryXorOp, "^"},
              {vpiUnaryXNorOp, "~^"},
              {vpiSubOp, "-"},
              {vpiDivOp, "/"},
              {vpiModOp, "%"},
              {vpiEqOp, "=="},
              {vpiNeqOp, "!="},
              {vpiCaseEqOp, "==="},
              {vpiCaseNeqOp, "!=="},
              {vpiGtOp, ">"},
              {vpiGeOp, ">="},
              {vpiLtOp, "<"},
              {vpiLeOp, "<="},
              {vpiLShiftOp, "<<"},
              {vpiRShiftOp, ">>"},
              {vpiAddOp, "+"},
              {vpiMultOp, "*"},
              {vpiLogAndOp, "&&"},
              {vpiLogOrOp, "||"},
              {vpiBitAndOp, "&"},
              {vpiBitOrOp, "|"},
              {vpiBitXorOp, "^"},
              {vpiBitXNorOp, "^~"},
              {vpiArithLShiftOp, "<<<"},
              {vpiArithRShiftOp, ">>>"},
              {vpiPowerOp, "**"},
              {vpiImplyOp, "->"},
              {vpiNonOverlapImplyOp, "|=>"},
              {vpiOverlapImplyOp, "|->"},
          };
          std::stringstream out_op0;
          prettyPrint(s, oper->Operands()->at(0), 0, out_op0);
          std::stringstream out_op1;
          prettyPrint(s, oper->Operands()->at(1), 0, out_op1);
          out << out_op0.str() << " " << opToken[opType] << " "
              << out_op1.str();
          break;
        }
        case vpiConditionOp: {
          std::stringstream out_op0;
          prettyPrint(s, oper->Operands()->at(0), 0, out_op0);
          std::stringstream out_op1;
          prettyPrint(s, oper->Operands()->at(1), 0, out_op1);
          std::stringstream out_op2;
          prettyPrint(s, oper->Operands()->at(2), 0, out_op2);
          out << out_op0.str() << " ? " << out_op1.str() << " : "
              << out_op2.str();
          break;
        }
        case vpiConcatOp:
        case vpiAssignmentPatternOp: {
          switch (opType) {
            case vpiConcatOp: {
              out << "{";
              break;
            }
            case vpiAssignmentPatternOp: {
              out << "'{";
              break;
            }
            default: {
              break;
            }
          };
          for (uint32_t i = 0; i < oper->Operands()->size(); i++) {
            prettyPrint(s, oper->Operands()->at(i), 0, out);
            if (i < oper->Operands()->size() - 1) {
              out << ",";
            }
          }
          out << "}";
          break;
        }
        case vpiMultiConcatOp: {
          std::stringstream mult;
          prettyPrint(s, oper->Operands()->at(0), 0, mult);
          std::stringstream op;
          prettyPrint(s, oper->Operands()->at(1), 0, op);
          out << "{" << mult.str() << "{" << op.str() << "}}";
          break;
        }
        case vpiEventOrOp: {
          std::stringstream op[2];
          prettyPrint(s, oper->Operands()->at(0), 0, op[0]);
          prettyPrint(s, oper->Operands()->at(1), 0, op[1]);
          out << op[0].str() << " or " << op[1].str();
          break;
        }
        case vpiInsideOp: {
          prettyPrint(s, oper->Operands()->at(0), 0, out);
          out << " inside {";
          for (uint32_t i = 1; i < oper->Operands()->size(); i++) {
            prettyPrint(s, oper->Operands()->at(i), 0, out);
            if (i < oper->Operands()->size() - 1) {
              out << ",";
            }
          }
          out << "}";
          break;
        }
        case vpiNullOp: {
          break;
        }
          /*
            { vpiListOp, "," },
            { vpiMinTypMaxOp, ":" },
          */
        case vpiPosedgeOp: {
          std::stringstream op;
          prettyPrint(s, oper->Operands()->at(0), 0, op);
          out << "posedge " << op.str();
          break;
        }
        case vpiNegedgeOp: {
          std::stringstream op;
          prettyPrint(s, oper->Operands()->at(0), 0, op);
          out << "negedge " << op.str();
          break;
        }
        case vpiPostIncOp: {
          std::stringstream op;
          prettyPrint(s, oper->Operands()->at(0), 0, op);
          out << op.str() << "++";
          break;
        }
        case vpiPostDecOp: {
          std::stringstream op;
          prettyPrint(s, oper->Operands()->at(0), 0, op);
          out << op.str() << "--";
          break;
        }

          /*
            { vpiAcceptOnOp, "accept_on" },
            { vpiRejectOnOp, "reject_on" },
            { vpiSyncAcceptOnOp, "sync_accept_on" },
            { vpiSyncRejectOnOp, "sync_reject_on" },
            { vpiOverlapFollowedByOp, "overlapped followed_by" },
            { vpiNonOverlapFollowedByOp, "nonoverlapped followed_by" },
            { vpiNexttimeOp, "nexttime" },
            { vpiAlwaysOp, "always" },
            { vpiEventuallyOp, "eventually" },
            { vpiUntilOp, "until" },
            { vpiUntilWithOp, "until_with" },
            { vpiUnaryCycleDelayOp, "##" },
            { vpiCycleDelayOp, "##" },
            { vpiIntersectOp, "intersection" },
            { vpiFirstMatchOp, "first_match" },
            { vpiThroughoutOp, "throughout" },
            { vpiWithinOp, "within" },
            { vpiRepeatOp, "[=]" },
            { vpiConsecutiveRepeatOp, "[*]" },
            { vpiGotoRepeatOp, "[->]" },
            { vpiMatchOp, "match" },
            { vpiCastOp, "type'" },
            { vpiIffOp, "iff" },
            { vpiWildEqOp, "==?" },
            { vpiWildNeqOp, "!=?" },
            { vpiStreamLROp, "{>>}" },
            { vpiStreamRLOp, "{<<}" },
            { vpiMatchedOp, ".matched" },
            { vpiTriggeredOp, ".triggered" },
            { vpiMultiAssignmentPatternOp, "{n{}}" },
            { vpiIfOp, "if" },
            { vpiIfElseOp, "if–else" },
            { vpiCompAndOp, "and" },
            { vpiCompOrOp, "or" },
            { vpiImpliesOp, "implies" },
            { vpiTypeOp, "type" },
            { vpiAssignmentOp, "=" },
          */

        default:
          break;
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmpart_select: {
      part_select *ps = (part_select *)object;
      prettyPrint(s, ps->Left_range(), 0, out);
      out << ":";
      prettyPrint(s, ps->Right_range(), 0, out);
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmindexed_part_select: {
      indexed_part_select *ps = (indexed_part_select *)object;
      prettyPrint(s, ps->Base_expr(), 0, out);
      if (ps->VpiIndexedPartSelectType() == vpiPosIndexed)
        out << "+";
      else
        out << "-";
      out << ":";
      prettyPrint(s, ps->Width_expr(), 0, out);
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmref_obj: {
      out << object->VpiName();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmvar_select: {
      var_select *vs = (var_select *)object;
      out << vs->VpiName();
      for (uint32_t i = 0; i < vs->Exprs()->size(); i++) {
        out << "[";
        prettyPrint(s, vs->Exprs()->at(i), 0, out);
        out << "]";
      }
      break;
    }
    default: {
      break;
    }
  }
}

uint64_t ExprEval::size(const any *ts, bool &invalidValue, const any *inst,
                        const any *pexpr, bool full, bool muteError) {
  if (ts == nullptr) return 0;
  uint64_t bits = 0;
  VectorOfrange *ranges = nullptr;
  UHDM_OBJECT_TYPE ttps = ts->UhdmType();
  if (ttps == uhdmref_typespec) {
    ref_typespec* rtps = (ref_typespec*) ts;
    ts = rtps->Actual_typespec();
    ttps = ts->UhdmType();
  }
  switch (ttps) {
    case UHDM_OBJECT_TYPE::uhdmhier_path: {
      ts = decodeHierPath((hier_path *)ts, invalidValue, inst, nullptr, ReturnType::TYPESPEC);
      if (ts)
        bits = size(ts, invalidValue, inst, pexpr, full);
      else
        invalidValue = true;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmarray_typespec: {
      array_typespec *lts = (array_typespec *)ts;
      ranges = lts->Ranges();
      if (!full) {
        bits = 1;
      } else if (const ref_typespec *rt = lts->Elem_typespec()) {
        bits = size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmshort_real_typespec: {
      bits = 32;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmreal_typespec: {
      bits = 32;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmbyte_typespec: {
      bits = 8;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmshort_int_typespec: {
      bits = 16;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmint_typespec: {
      int_typespec *its = (int_typespec *)ts;
      bits = 32;
      ranges = its->Ranges();
      if (ranges) {
        bits = 1;
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmlong_int_typespec: {
      bits = 64;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdminteger_typespec: {
      integer_typespec *itps = (integer_typespec *)ts;
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
    case UHDM_OBJECT_TYPE::uhdmbit_typespec: {
      bits = 1;
      bit_typespec *lts = (bit_typespec *)ts;
      ranges = lts->Ranges();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmlogic_typespec: {
      bits = 1;
      logic_typespec *lts = (logic_typespec *)ts;
      ranges = lts->Ranges();
      // A packed array whose element is a typedef — `typedef logic[31:0] word;
      // word [1:0]` — carries the element type in Elem_typespec.  The element
      // width must multiply the outer range(s) below; without this the element
      // is treated as 1 bit and e.g. a `word[1:0]` function return is sized 2
      // instead of 2*32=64 (truncating the return value).  Mirrors the
      // array_typespec / packed_array_typespec cases.
      if (full) {
        if (const ref_typespec *rt = lts->Elem_typespec()) {
          bool tmpInvalidValue = false;
          uint64_t tmpS =
              size(rt->Actual_typespec(), tmpInvalidValue, inst, pexpr, full);
          if (!tmpInvalidValue) bits = tmpS;
        }
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmstring_typespec: {
      bits = 0;
      invalidValue = true;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmunsupported_typespec: {
      bits = 0;
      invalidValue = true;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmlogic_net: {
      bits = 1;
      logic_net *lts = (logic_net *)ts;
      if (const ref_typespec *rt = lts->Typespec()) {
        bool tmpInvalidValue = false;
        uint64_t tmpS = size(rt->Actual_typespec(), tmpInvalidValue, inst, pexpr, full);
        if (tmpInvalidValue == false) {
          bits = tmpS;
        }
      }
      ranges = lts->Ranges();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmlogic_var: {
      bits = 1;
      logic_var *lts = (logic_var *)ts;
      if (const ref_typespec *rt = lts->Typespec()) {
        bool tmpInvalidValue = false;
        uint64_t tmpS = size(rt->Actual_typespec(), tmpInvalidValue, inst, pexpr, full);
        if (tmpInvalidValue == false) {
          bits = tmpS;
        }
      }
      ranges = lts->Ranges();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmbit_var: {
      bits = 1;
      bit_var *lts = (bit_var *)ts;
      if (const ref_typespec *rt = lts->Typespec()) {
        bits = size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      ranges = lts->Ranges();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmbyte_var: {
      bits = 8;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmstruct_var: {
      if (const ref_typespec *rt = ((const struct_var *)ts)->Typespec()) {
        bits += size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmarray_var: {
      const array_var *var = (array_var *)ts;
      variables *regv = var->Variables()->at(0);
      if (const ref_typespec *rt = regv->Typespec()) {
        bits += size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      ranges = var->Ranges();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmstruct_net: {
      if (const ref_typespec *rt = ((const struct_net *)ts)->Typespec()) {
        bits += size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmstruct_typespec: {
      struct_typespec *sts = (struct_typespec *)ts;
      if (VectorOftypespec_member *members = sts->Members()) {
        for (typespec_member *member : *members) {
          if (const ref_typespec *rt = member->Typespec()) {
            bits +=
                size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
          }
        }
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmenum_var: {
      if (const ref_typespec *rt = ((const enum_var *)ts)->Typespec()) {
        bits = size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmenum_typespec: {
      if (const ref_typespec *rt =
              ((const enum_typespec *)ts)->Base_typespec()) {
        bits = size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmunion_typespec: {
      union_typespec *sts = (union_typespec *)ts;
      if (VectorOftypespec_member *members = sts->Members()) {
        for (typespec_member *member : *members) {
          if (const ref_typespec *rt = member->Typespec()) {
            uint64_t max =
                size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
            if (max > bits) bits = max;
          }
        }
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmconstant: {
      constant *c = (constant *)ts;
      bits = c->VpiSize();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmenum_const: {
      enum_const *c = (enum_const *)ts;
      bits = c->VpiSize();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmref_obj: {
      ref_obj *ref = (ref_obj *)ts;
      const any* act = ref->Actual_group();
      if (act == nullptr) {
        std::string_view name = ref->VpiName();
        act = getObject(name, inst, pexpr, muteError);
      }
      if (act) {
        bits = size(act, invalidValue, inst, pexpr, full);
      } else {
        invalidValue = true;
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      operation *tsop = (operation *)ts;
      if (tsop->VpiOpType() == vpiConcatOp) {
        if (auto ops = tsop->Operands()) {
          for (auto op : *ops) {
            bits += size(op, invalidValue, inst, pexpr, full);
          }
        }
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmpacked_array_typespec: {
      packed_array_typespec *tmp = (packed_array_typespec *)ts;
      if (const ref_typespec *rt = tmp->Elem_typespec()) {
        bits += size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      ranges = tmp->Ranges();
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmtypespec_member: {
      if (const ref_typespec *rt = ((const typespec_member *)ts)->Typespec()) {
        bits += size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmio_decl: {
      if (const ref_typespec *rt = ((const io_decl *)ts)->Typespec()) {
        bits += size(rt->Actual_typespec(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmbit_select: {
      bits = 1;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmpart_select: {
      const part_select *sel = (part_select *)ts;
      const expr *lexpr = sel->Left_range();
      const expr *rexpr = sel->Right_range();
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

  if (ranges && !ranges->empty()) {
    if (!full) {
      const range *last_range = ranges->back();
      const expr *lexpr = last_range->Left_expr();
      const expr *rexpr = last_range->Right_expr();
      int64_t lv =
          getValue(reduceExpr(lexpr, invalidValue, inst, pexpr, muteError));

      int64_t rv =
          getValue(reduceExpr(rexpr, invalidValue, inst, pexpr, muteError));

      if (lv > rv)
        bits = bits * (lv - rv + 1);
      else
        bits = bits * (rv - lv + 1);
    } else {
      for (const range *ran : *ranges) {
        const expr *lexpr = ran->Left_expr();
        const expr *rexpr = ran->Right_expr();
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

uint64_t ExprEval::size(const vpiHandle typespec, bool &invalidValue,
                        const vpiHandle inst, const vpiHandle pexpr, bool full,
                        bool muteError) {
  const UHDM::any *vpiHandle_typespec =
      (UHDM::any *)((uhdm_handle *)typespec)->object;
  const UHDM::any *vpiHandle_inst =
      !inst ? nullptr : (UHDM::any *)((uhdm_handle *)inst)->object;
  const UHDM::any *vpiHandle_pexpr =
      !pexpr ? nullptr : (UHDM::any *)((uhdm_handle *)pexpr)->object;
  return size(vpiHandle_typespec, invalidValue, vpiHandle_inst, vpiHandle_pexpr,
              full, muteError);
}

static bool getStringVal(std::string &result, expr *val) {
  if (const constant *hs0 = any_cast<const constant *>(val)) {
    if (s_vpi_value *sval = String2VpiValue(hs0->VpiValue())) {
      if (sval->format == vpiStringVal || sval->format == vpiBinStrVal ||
          sval->format == vpiHexStrVal || sval->format == vpiOctStrVal ||
          sval->format == vpiDecStrVal) {
        result = sval->value.str;
        if (sval->value.str) delete[] sval->value.str;
        delete sval;
        return true;
      } else {
        delete sval;
      }
    }
  }
  return false;
}

void resize(expr *resizedExp, int32_t size) {
  ExprEval eval;
  constant* c = any_cast<constant*>(resizedExp);
  if (c && c->VpiDecompile() == "'1") {
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
  VectorOfany &operands = *op->Operands();
  int32_t optype = op->VpiOpType();
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
  int32_t size0 = reduc0->VpiSize();
  int32_t size1 = reduc1->VpiSize();
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
    long double ld0 = get_double(invalidValueD, reduc0);
    long double ld1 = get_double(invalidValueD, reduc1);
    if ((invalidValue == false) && (invalidValueD == false)) {
      switch (optype) {
        case vpiEqOp:
          val = (ld0 == ld1);
          break;
        case vpiNeqOp:
          val = (ld0 != ld1);
          break;
        case vpiGtOp:
          val = (ld0 > ld1);
          break;
        case vpiGeOp:
          val = (ld0 >= ld1);
          break;
        case vpiLtOp:
          val = (ld0 < ld1);
          break;
        case vpiLeOp:
          val = (ld0 <= ld1);
          break;
        default:
          break;
      }
    } else {
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
      }
    }
  }

  if (invalidValueI && invalidValueD && invalidValueS) {
    invalidValue = true;
  } else {
    constant *c = s.MakeConstant();
    c->VpiValue("BIN:" + std::to_string(val));
    c->VpiDecompile(std::to_string(val));
    c->VpiSize(1);
    c->VpiConstType(vpiBinaryConst);
    result = c;
  }
  return result;
}

uint64_t ExprEval::getWordSize(const expr *exp, const any *inst,
                               const any *pexpr) {
  uint64_t wordSize = 1;
  bool invalidValue = false;
  bool muteError = true;
  if (exp == nullptr) {
    return wordSize;
  }
  if (const ref_typespec *ctsrt = exp->Typespec()) {
    if (const typespec *cts = ctsrt->Actual_typespec()) {
      if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
        packed_array_typespec *patps = (packed_array_typespec *)cts;
        if (const ref_typespec *etsro = patps->Elem_typespec()) {
          cts = etsro->Actual_typespec();
        }
      } else if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
        array_typespec *atps = (array_typespec *)cts;
        if (const ref_typespec *etsro = atps->Elem_typespec()) {
          cts = etsro->Actual_typespec();
        }
      }
      if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmlong_int_typespec) {
        wordSize = 64;
      } else if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmshort_int_typespec) {
        wordSize = 16;
      } else if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmbyte_typespec) {
        wordSize = 8;
      } else if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmint_typespec) {
        int_typespec *icts = (int_typespec *)cts;
        std::string_view value = icts->VpiValue();
        if (exp->VpiSize() > 32)
          wordSize = 32;
        else
          wordSize = 1;
        if (value.find("UINT:") == 0) {
          value.remove_prefix(std::string_view("UINT:").length());
          if (NumUtils::parseUint64(value, &wordSize) == nullptr) {
            wordSize = 32;
          }
        } else if (value.find("INT:") == 0) {
          value.remove_prefix(std::string_view("INT:").length());
          if (NumUtils::parseIntLenient(value, &wordSize) == nullptr) {
            wordSize = 32;
          }
        }
      } else if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdminteger_typespec) {
        integer_typespec *icts = (integer_typespec *)cts;
        std::string_view value = icts->VpiValue();
        if (exp->VpiSize() > 32)
          wordSize = 32;
        else
          wordSize = 1;
        if (value.find("UINT:") == 0) {
          value.remove_prefix(std::string_view("UINT:").length());
          if (NumUtils::parseUint64(value, &wordSize) == nullptr) {
            wordSize = 32;
          }
        } else if (value.find("INT:") == 0) {
          value.remove_prefix(std::string_view("INT:").length());
          if (NumUtils::parseIntLenient(value, &wordSize) == nullptr) {
            wordSize = 32;
          }
        }
      } else if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_typespec) {
        logic_typespec *icts = (logic_typespec *)cts;
        if (const ref_typespec *rt = icts->Elem_typespec()) {
          wordSize = size(rt->Actual_typespec(), invalidValue, inst, pexpr,
                          false, muteError);
        }
      } else if (cts->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_typespec) {
        bit_typespec *icts = (bit_typespec *)cts;
        wordSize = 1;
        if (VectorOfrange *ranges = icts->Ranges()) {
          if (icts->Ranges()->size() > 1) {
            range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            uint16_t lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Left_expr(), invalidValue,
                                              inst, pexpr, muteError)));
            uint16_t rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Right_expr(), invalidValue,
                                              inst, pexpr, muteError)));
            wordSize = (lr > rr) ? (lr - rr + 1) : (rr - lr + 1);
          }
        }
      }
    }
  }
  if (wordSize == 0) {
    wordSize = 1;
  }
  return wordSize;
}

expr *ExprEval::reduceBitSelect(expr *op, uint32_t index_val,
                                bool &invalidValue, const any *inst,
                                const any *pexpr, bool muteError) {
  Serializer &s = *op->GetSerializer();
  expr *result = nullptr;
  expr *exp = reduceExpr(op, invalidValue, inst, pexpr, muteError);
  if (exp && (exp->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant)) {
    constant *cexp = (constant *)exp;
    std::string binary = toBinary(cexp);
    uint64_t wordSize = getWordSize(cexp, inst, pexpr);
    constant *c = s.MakeConstant();
    uint16_t lr = 0;
    uint16_t rr = 0;
    if (const ref_typespec *rt = exp->Typespec()) {
      if (const typespec *tps = rt->Actual_typespec()) {
        if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_typespec) {
          logic_typespec *lts = (logic_typespec *)tps;
          if (VectorOfrange *ranges = lts->Ranges()) {
            range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Left_expr(), invalidValue,
                                              inst, pexpr, muteError)));
            rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Right_expr(), invalidValue,
                                              inst, pexpr, muteError)));
          }
        } else if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmint_typespec) {
          int_typespec *lts = (int_typespec *)tps;
          if (VectorOfrange *ranges = lts->Ranges()) {
            range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Left_expr(), invalidValue,
                                              inst, pexpr, muteError)));
            rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Right_expr(), invalidValue,
                                              inst, pexpr, muteError)));
          }
        } else if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_typespec) {
          bit_typespec *lts = (bit_typespec *)tps;
          if (VectorOfrange *ranges = lts->Ranges()) {
            range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Left_expr(), invalidValue,
                                              inst, pexpr, muteError)));
            rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->Right_expr(), invalidValue,
                                              inst, pexpr, muteError)));
          }
        }
      }
    }
    c->VpiSize(static_cast<int32_t>(wordSize));
    if (index_val < binary.size()) {
      // TODO: If range does not start at 0
      if (lr >= rr) {
        index_val =
            static_cast<uint32_t>(binary.size() - ((index_val + 1) * wordSize));
      }
      std::string v;
      for (uint32_t i = 0; i < wordSize; i++) {
        if ((index_val + i) < binary.size()) {
          char bitv = binary[index_val + i];
          v += std::to_string(bitv - '0');
        }
      }
      if (v.size() > UHDM_MAX_BIT_WIDTH) {
        std::string fullPath;
        if (const gen_scope_array *in =
                any_cast<const gen_scope_array *>(inst)) {
          fullPath = in->VpiFullName();
        } else if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
          fullPath = inst->VpiName();
        } else if (const scope *spe = any_cast<const scope *>(inst)) {
          fullPath = spe->VpiFullName();
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

int64_t ExprEval::get_value(bool &invalidValue, const expr *expr, bool strict) {
  int64_t result = 0;
  int32_t type = 0;
  std::string_view sv;
  if (const constant *c = any_cast<const constant *>(expr)) {
    type = c->VpiConstType();
    sv = c->VpiValue();
  } else if (const variables *v = any_cast<const variables *>(expr)) {
    if (v->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var) {
      type = vpiUIntConst;
      sv = v->VpiValue();
    }
  } else if (const parameter *p = any_cast<const parameter *>(expr)) {
    // default type
    sv = p->VpiValue();
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
        if (sv.size() > 32) {
          invalidValue = true;
          break;
        }
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

uint64_t ExprEval::get_uvalue(bool &invalidValue, const expr *expr,
                              bool strict) {
  uint64_t result = 0;
  int32_t type = 0;
  std::string_view sv;
  if (const constant *c = any_cast<const constant *>(expr)) {
    type = c->VpiConstType();
    sv = c->VpiValue();
  } else if (const variables *v = any_cast<const variables *>(expr)) {
    if (v->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var) {
      type = vpiUIntConst;
      sv = v->VpiValue();
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
        if (sv.size() > 64) {
          invalidValue = true;
          break;
        }
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

task_func *ExprEval::getTaskFunc(std::string_view name, const any *inst) {
  if (getTaskFuncFunctor) {
    if (task_func *result = getTaskFuncFunctor(name, inst)) {
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
  const design *des = any_cast<const design *>(root);
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
    VectorOftask_func *task_funcs = nullptr;
    if (the_instance->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
    } else if (the_instance->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
      task_funcs = ((design *)the_instance)->Task_funcs();
    } else if (const instance *inst =
                   any_cast<const instance *>(the_instance)) {
      task_funcs = inst->Task_funcs();
    }

    if (task_funcs) {
      for (task_func *tf : *task_funcs) {
        if (tf->VpiName() == the_name) {
          return tf;
        }
      }
    }

    the_instance = the_instance->VpiParent();
  }

  return nullptr;
}


// Fold an assignment-pattern (or concat) VALUE against its declared
// typespec into a flat binary string, member/element-wise.  Handles nested
// patterns, `'{default: v}` replication, tagged and positional operands,
// packed arrays, structs, and enum leaves.  This is what makes a >64-bit
// struct parameter's pattern value (unreachable by the legacy 64-bit Value
// engine) selectable: fpnew's `Implementation.UnitTypes[opgrp]` ends on a
// `'{default: PARALLEL}` operation that must reduce to a constant for the
// generate-if conditions downstream to evaluate.
static bool foldPatternBits(const any *val, const typespec *ts, ExprEval *ev,
                            Serializer &s, const any *inst, const any *pexpr,
                            bool muteError, std::string &bits, int depth = 0) {
  if (!val || !ts || depth > 16) return false;
  if (val->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern)
    return foldPatternBits(((const tagged_pattern *)val)->Pattern(), ts, ev, s,
                           inst, pexpr, muteError, bits, depth + 1);
  bool inv = false;
  uint64_t w = ev->size(ts, inv, inst, pexpr, true, muteError);
  if (inv || w == 0) {
    if (ts->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
      const enum_typespec *ets = (const enum_typespec *)ts;
      inv = false;
      w = 0;
      if (ets->Base_typespec() && ets->Base_typespec()->Actual_typespec())
        w = ev->size(ets->Base_typespec()->Actual_typespec(), inv, inst, pexpr,
                     true, muteError);
      if (inv || w == 0) w = 32;
    } else {
      return false;
    }
  }
  if (val->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
    const operation *op = (const operation *)val;
    int32_t ot = op->VpiOpType();
    if (ot == vpiAssignmentPatternOp || ot == vpiConcatOp) {
      VectorOfany *ops = op->Operands();
      if (!ops || ops->empty()) return false;
      auto rangeCount = [&](VectorOfrange *rs) -> uint64_t {
        if (!rs || rs->empty()) return 0;
        bool iv = false;
        int64_t l = ev->get_value(
            iv, ev->reduceExpr((any *)rs->at(0)->Left_expr(), iv, inst, pexpr,
                               muteError));
        int64_t r = ev->get_value(
            iv, ev->reduceExpr((any *)rs->at(0)->Right_expr(), iv, inst,
                               pexpr, muteError));
        if (iv) return 0;
        return (uint64_t)(l > r ? l - r + 1 : r - l + 1);
      };
      const typespec *elemTs = nullptr;
      uint64_t n = 0;
      UHDM_OBJECT_TYPE tt = ts->UhdmType();
      if (tt == UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
        auto *t = (const packed_array_typespec *)ts;
        n = rangeCount(t->Ranges());
        if (t->Elem_typespec()) elemTs = t->Elem_typespec()->Actual_typespec();
      } else if (tt == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
        auto *t = (const array_typespec *)ts;
        n = rangeCount(t->Ranges());
        if (t->Elem_typespec()) elemTs = t->Elem_typespec()->Actual_typespec();
      } else if (tt == UHDM_OBJECT_TYPE::uhdmlogic_typespec) {
        auto *t = (const logic_typespec *)ts;
        if (t->Elem_typespec()) {
          n = rangeCount(t->Ranges());
          elemTs = t->Elem_typespec()->Actual_typespec();
          // Typedef-alias duplication: the Elem chain repeats the outer
          // range — descend one more level and correct the total width.
          if (elemTs) {
            if (const logic_typespec *elt2 =
                    any_cast<const logic_typespec *>(elemTs)) {
              if (elt2->Elem_typespec() &&
                  elt2->Elem_typespec()->Actual_typespec() &&
                  elt2->Ranges() && !elt2->Ranges()->empty()) {
                elemTs = elt2->Elem_typespec()->Actual_typespec();
                bool wiv2 = false;
                uint64_t ew3 =
                    ev->size(elemTs, wiv2, inst, pexpr, true, muteError);
                if (!wiv2 && ew3 > 0 && n > 0) w = n * ew3;
              }
            }
          }
        } else if (t->Ranges() && !t->Ranges()->empty()) {
          // Multi-range packed logic (`logic [0:4][31:0]`): elements are
          // the inner dimensions; single-range: 1-bit elements.
          n = rangeCount(t->Ranges());
          logic_typespec *sub = s.MakeLogic_typespec();
          if (t->Ranges()->size() >= 2) {
            VectorOfrange *tr = s.MakeRangeVec();
            for (uint32_t ri = 1; ri < t->Ranges()->size(); ri++)
              tr->push_back(t->Ranges()->at(ri));
            sub->Ranges(tr);
          }
          elemTs = sub;
        }
      } else if (tt == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
        const struct_typespec *st = (const struct_typespec *)ts;
        if (!st->Members()) return false;
        // index operands: tagged by member name, positional otherwise
        const any *defVal = nullptr;
        std::map<std::string_view, const any *> tagged;
        std::vector<const any *> positional;
        for (auto o : *ops) {
          if (o->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
            const tagged_pattern *tp = (const tagged_pattern *)o;
            const typespec *tts =
                tp->Typespec() ? tp->Typespec()->Actual_typespec() : nullptr;
            if (tts && tts->VpiName() == "default")
              defVal = tp->Pattern();
            else if (tts)
              tagged[tts->VpiName()] = tp->Pattern();
            else
              return false;
          } else {
            positional.push_back(o);
          }
        }
        size_t pi = 0;
        for (typespec_member *m : *st->Members()) {
          const typespec *mts =
              m->Typespec() ? m->Typespec()->Actual_typespec() : nullptr;
          const any *mv = nullptr;
          auto it = tagged.find(m->VpiName());
          if (it != tagged.end())
            mv = it->second;
          else if (tagged.empty() && pi < positional.size())
            mv = positional[pi++];
          else if (defVal)
            mv = defVal;
          if (!mv || !mts) return false;
          std::string mb;
          if (!foldPatternBits(mv, mts, ev, s, inst, pexpr, muteError, mb,
                               depth + 1))
            return false;
          bool miv = false;
          uint64_t mw = ev->size(mts, miv, inst, pexpr, true, muteError);
          if (miv || mw == 0) mw = mb.size();
          if (mb.size() > mw)
            mb = mb.substr(mb.size() - mw);
          else
            while (mb.size() < mw) mb.insert(mb.begin(), '0');
          bits += mb;
        }
        return true;
      }
      if (elemTs && n > 0 && (w % n) == 0) {
        uint64_t ew = w / n;
        auto foldElem = [&](const any *ev0, std::string &out) -> bool {
          std::string eb;
          if (!foldPatternBits(ev0, elemTs, ev, s, inst, pexpr, muteError, eb,
                               depth + 1))
            return false;
          if (eb.size() > ew)
            eb = eb.substr(eb.size() - ew);
          else
            while (eb.size() < ew) eb.insert(eb.begin(), '0');
          out += eb;
          return true;
        };
        // `'{default: v}` — replicate the default into every element
        if (ops->size() == 1 &&
            ops->at(0)->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
          const tagged_pattern *tp = (const tagged_pattern *)ops->at(0);
          const typespec *tts =
              tp->Typespec() ? tp->Typespec()->Actual_typespec() : nullptr;
          if (tts && tts->VpiName() == "default") {
            std::string eb;
            if (!foldElem(tp->Pattern(), eb)) return false;
            for (uint64_t i = 0; i < n; i++) bits += eb;
            return true;
          }
        }
        if (ops->size() == n) {
          for (auto o : *ops)
            if (!foldElem(o, bits)) return false;
          return true;
        }
      }
      // Last chance: derive geometry from the VALUE — nops positional
      // elements of w/nops bits each (guards against a mistrusted or
      // missing element typespec).
      if (!ops->empty() && (w % ops->size()) == 0) {
        uint64_t ew2 = w / ops->size();
        logic_typespec *sub2 = s.MakeLogic_typespec();
        VectorOfrange *sr2 = s.MakeRangeVec();
        range *rg2 = s.MakeRange();
        constant *cl2 = s.MakeConstant();
        cl2->VpiValue("INT:" + std::to_string((int64_t)ew2 - 1));
        cl2->VpiSize(64);
        cl2->VpiConstType(vpiIntConst);
        constant *cr2 = s.MakeConstant();
        cr2->VpiValue("INT:0");
        cr2->VpiSize(64);
        cr2->VpiConstType(vpiIntConst);
        rg2->Left_expr(cl2);
        rg2->Right_expr(cr2);
        sr2->push_back(rg2);
        sub2->Ranges(sr2);
        bool allTagged = false;
        for (auto o : *ops)
          if (o->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern)
            allTagged = true;
        if (!allTagged) {
          std::string acc;
          bool ok2 = true;
          for (auto o : *ops) {
            std::string eb;
            if (!foldPatternBits(o, sub2, ev, s, inst, pexpr, muteError, eb,
                                 depth + 1)) {
              ok2 = false;
              break;
            }
            if (eb.size() > ew2)
              eb = eb.substr(eb.size() - ew2);
            else
              while (eb.size() < ew2) eb.insert(eb.begin(), '0');
            acc += eb;
          }
          if (ok2) {
            bits += acc;
            return true;
          }
        }
      }
      return false;
    }
  }
  // Leaf: reduce to a constant and use its bits.
  bool liv = false;
  expr *re = ev->reduceExpr((any *)val, liv, inst, pexpr, muteError);
  if (liv || !re || re->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
    return false;
  constant *rc = (constant *)re;
  std::string b;
  if (rc->VpiSize() == -1) {
    // fill literal '0 / '1
    bool fiv = false;
    uint64_t fv = ev->get_uvalue(fiv, rc);
    if (fiv) return false;
    b = std::string(w, fv ? '1' : '0');
  } else {
    b = ev->toBinary(rc);
  }
  if (b.size() > w)
    b = b.substr(b.size() - w);
  else
    while (b.size() < w) b.insert(b.begin(), '0');
  for (char bc : b)
    if (bc != '0' && bc != '1') return false;
  bits += b;
  return true;
}

// Wrap foldPatternBits into a BIN constant carrying the typespec.
static constant *foldPatternToConstant(const any *val, const typespec *ts,
                                       ExprEval *ev, Serializer &s,
                                       const any *inst, const any *pexpr,
                                       bool muteError) {
  std::string bits;
  if (!foldPatternBits(val, ts, ev, s, inst, pexpr, muteError, bits) ||
      bits.empty())
    return nullptr;
  constant *c = s.MakeConstant();
  c->VpiValue("BIN:" + bits);
  c->VpiDecompile(bits);
  c->VpiSize(static_cast<int32_t>(bits.size()));
  c->VpiConstType(vpiBinaryConst);
  ref_typespec *rt = s.MakeRef_typespec();
  rt->Actual_typespec((typespec *)ts);
  rt->VpiParent(c);
  c->Typespec(rt);
  return c;
}

// Element select `[N]` on a PACKED-ARRAY-typed constant: slice the whole
// element, not bit N.  The constant's VpiSize can be misrecorded (a pattern
// reduce can size operands by a wrong context), so the geometry trusts the
// raw BIN value string; ascending ranges put element `low` at the MSBs.
// Returns nullptr when the shape does not apply (caller falls back to a
// plain bit select).
static constant *reducePackedElemSelect(constant *c, int64_t selectIndex,
                                        ExprEval *ev, Serializer &s,
                                        bool &invalidValue, const any *inst,
                                        const any *pexpr, bool muteError,
                                        const typespec *cts_fallback = nullptr) {
  const typespec *cts =
      c->Typespec() ? c->Typespec()->Actual_typespec() : nullptr;
  if (!cts) cts = cts_fallback;
  const typespec *ets = nullptr;
  VectorOfrange *rgs = nullptr;
  if (cts) {
    if (const packed_array_typespec *pt =
            any_cast<const packed_array_typespec *>(cts)) {
      if (pt->Elem_typespec()) ets = pt->Elem_typespec()->Actual_typespec();
      rgs = pt->Ranges();
    } else if (const logic_typespec *lt =
                   any_cast<const logic_typespec *>(cts)) {
      if (lt->Elem_typespec()) {
        ets = lt->Elem_typespec()->Actual_typespec();
        rgs = lt->Ranges();
      } else if (lt->Ranges() && lt->Ranges()->size() >= 2) {
        // MULTI-RANGE packed array (`logic [0:4][31:0]` — fpnew_pkg's
        // fmt_unsigned_t): no Elem_typespec exists; the element type is the
        // same logic type minus the outermost range.  Without this branch the
        // select fell through to reduceBitSelect and `FmtPipeRegs[fmt]`
        // returned BIT fmt of the 160-bit value — fpnew slices got
        // NumPipeRegs 1,0,0,0,0 from a '{default:1} array, lost their
        // pipeline registers at elaboration, and CVA6's fpu_wrap/ex_stage
        // early_valid never asserted.
        rgs = lt->Ranges();
        ElaboratorContext elemCtx(&s, false, muteError);
        logic_typespec *elt = (logic_typespec *)clone_tree(lt, &elemCtx);
        if (elt->Ranges() && !elt->Ranges()->empty())
          elt->Ranges()->erase(elt->Ranges()->begin());
        ets = elt;
      }
    }
  }
  if (!ets || !rgs || rgs->empty()) return nullptr;
  std::string_view v = c->VpiValue();
  if (v.rfind("BIN:", 0) != 0) return nullptr;
  std::string bits(v.substr(4));
  bool iv2 = false;
  int64_t l = ev->get_value(
      iv2, ev->reduceExpr(rgs->at(0)->Left_expr(), iv2, inst, pexpr, muteError));
  int64_t r = ev->get_value(
      iv2, ev->reduceExpr(rgs->at(0)->Right_expr(), iv2, inst, pexpr, muteError));
  if (iv2) return nullptr;
  int64_t lo = std::min(l, r);
  uint64_t n = (uint64_t)(std::abs(l - r) + 1);
  if (n < 1 || bits.size() % n != 0) return nullptr;
  uint64_t ew = bits.size() / n;
  if (ew < 2) return nullptr;
  if (selectIndex < lo || (uint64_t)(selectIndex - lo) >= n) return nullptr;
  uint64_t pos = (l < r) ? (uint64_t)(selectIndex - lo)
                         : (n - 1 - (uint64_t)(selectIndex - lo));
  std::string ebits = bits.substr(pos * ew, ew);
  for (char bch : ebits)
    if (bch != '0' && bch != '1') return nullptr;
  constant *ec = s.MakeConstant();
  ec->VpiValue("BIN:" + ebits);
  ec->VpiDecompile(ebits);
  ec->VpiSize(static_cast<int32_t>(ebits.size()));
  ec->VpiConstType(vpiBinaryConst);
  ref_typespec *ert = s.MakeRef_typespec();
  ert->Actual_typespec(const_cast<typespec *>(ets));
  ert->VpiParent(ec);
  ec->Typespec(ert);
  return ec;
}

any *ExprEval::decodeHierPath(hier_path *path, bool &invalidValue,
                              const any *inst, const any *pexpr,
                              ReturnType returnType, bool muteError) {
  Serializer &s = *path->GetSerializer();
  std::string baseObject;
  if (!path->Path_elems()->empty()) {
    any *firstElem = path->Path_elems()->at(0);
    baseObject = firstElem->VpiName();
  }
  any *object = getObject(baseObject, inst, pexpr, muteError);
  // A function LOCAL's var declaration shadows its staged frame value the
  // same way it does for bit selects (see the uhdmbit_select read branch):
  // `res.exp_bits` inside fpnew_pkg::super_format resolved the SHARED
  // struct_var node and read the typespec-member Actual_value annotation
  // left by a PREVIOUS instance's evaluation — {11,52} from an all-formats
  // config leaked into every later config's SUPER_FORMAT.  For VALUE reads,
  // prefer the frame's stored whole-value constant (kept in sync member-by-
  // member via spliceMemberIntoWhole); the member slice then reads through
  // its attached struct typespec.
  if (returnType == ReturnType::VALUE && object &&
      any_cast<variables *>(object) != nullptr) {
    if (any *valobj = getValue(baseObject, inst, pexpr, muteError)) {
      if (valobj->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant)
        object = valobj;
    }
  }
  if (object) {
    if (param_assign *passign = any_cast<param_assign *>(object)) {
      const any *passign_lhs = passign->Lhs();
      object = passign->Rhs();
      // Propagate the parameter's typespec to a bare pattern operation: the
      // member walk needs the struct type to attach member typespecs, and
      // without them a trailing `[N]` bit-selects instead of
      // element-selecting (fpnew's Implementation.UnitTypes[opgrp] when the
      // struct is >64 bits and resolves via the complex-value pattern).
      if (object && passign_lhs) {
        if (operation *op0 = any_cast<operation *>(object)) {
          if (op0->Typespec() == nullptr) {
            const parameter *lp = any_cast<const parameter *>(passign_lhs);
            if (lp && lp->Typespec() && lp->Typespec()->Actual_typespec()) {
              ref_typespec *rt0 = s.MakeRef_typespec();
              rt0->Actual_typespec(
                  (typespec *)lp->Typespec()->Actual_typespec());
              rt0->VpiParent(op0);
              op0->Typespec(rt0);
            }
          }
        }
      }
    }
  }
  if (object == nullptr) {
    object = getValue(baseObject, inst, pexpr, muteError);
  }
  if (object) {
    // Substitution
    if (param_assign *pass = any_cast<param_assign *>(object)) {
      const any *rhs = pass->Rhs();
      object = reduceExpr(rhs, invalidValue, inst, pexpr, muteError);
    } else if (bit_select *bts = any_cast<bit_select *>(object)) {
      object = reduceExpr(bts, invalidValue, inst, pexpr, muteError);
    } else if (ref_obj *ref = any_cast<ref_obj *>(object)) {
      object = reduceExpr(ref, invalidValue, inst, pexpr, muteError);
    } else if (constant *cons = any_cast<constant *>(object)) {
      ElaboratorContext elaboratorContext(&s);
      object = clone_tree(cons, &elaboratorContext);
      cons = any_cast<constant *>(object);
      if (cons->Typespec() == nullptr && path->Typespec() != nullptr) {
        ref_typespec *rt =
            (ref_typespec *)clone_tree(path->Typespec(), &elaboratorContext);
        if (rt != nullptr) {
          rt->VpiParent(cons);
          cons->Typespec(rt);
        }
      }
    } else if (operation *oper = any_cast<operation *>(object)) {
      if (returnType == ReturnType::TYPESPEC) {
        if (ref_typespec *rt = oper->Typespec()) {
          object = rt->Actual_typespec();
        }
      }
    }

    std::vector<std::string> the_path;
    for (auto elem : *path->Path_elems()) {
      std::string_view elemName = elem->VpiName();
      elemName = rtrim(elemName, '[');
      the_path.emplace_back(elemName);
      if (elem->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_select) {
        bit_select *select = (bit_select *)elem;
        uint64_t baseIndex = get_value(
            invalidValue, reduceExpr((any *)select->VpiIndex(), invalidValue,
                                     inst, pexpr, muteError));
        the_path.push_back("[" + std::to_string(baseIndex) + "]");
      }
    }

    expr* result = (expr *)hierarchicalSelector(the_path, 0, object, invalidValue, inst,
                                        pexpr, returnType, muteError);
    if (result == nullptr) {
      invalidValue = true;
      result = (expr *)hierarchicalSelector(the_path, 0, object, invalidValue, inst,
                                        pexpr, returnType, muteError);
    } else if (result->UhdmType() == uhdmhier_path) {
      invalidValue = true;
    }
    return result;
  }
  invalidValue = true;
  return nullptr;
}

any *ExprEval::hierarchicalSelector(std::vector<std::string> &select_path,
                                    uint32_t level, any *object,
                                    bool &invalidValue, const any *inst,
                                    const any *pexpr, ReturnType returnType,
                                    bool muteError) {
  if (object == nullptr) return nullptr;
  Serializer &s = (object) ? *object->GetSerializer() : *inst->GetSerializer();
  if (level >= select_path.size()) {
    if (returnType == ReturnType::TYPESPEC) {
      if (typespec *tp = any_cast<typespec>(object)) {
        return tp;
      } else if (expr *ep = any_cast<expr>(object)) {
        if (ref_typespec *rt = ep->Typespec()) {
          return rt->Actual_typespec();
        }
      } else if (io_decl *id = any_cast<io_decl>(object)) {
        if (ref_typespec *rt = id->Typespec()) {
          return rt->Actual_typespec();
        }
      }
      return nullptr;
    }
    // A fully-selected element that is still an assignment-pattern operation
    // (e.g. `'{default: PARALLEL}` picked by `Implementation.UnitTypes[k]`)
    // must reduce to a constant for VALUE consumers; fold it against its
    // typespec when known.
    if (returnType == ReturnType::VALUE) {
      if (operation *fop = any_cast<operation *>(object)) {
        if (fop->VpiOpType() == vpiAssignmentPatternOp && fop->Typespec() &&
            fop->Typespec()->Actual_typespec()) {
          if (constant *fc = foldPatternToConstant(
                  fop, fop->Typespec()->Actual_typespec(), this, s, inst,
                  pexpr, muteError))
            return fc;
        }
      }
    }
    return (expr *)object;
  }
  std::string elemName = select_path[level];
  bool lastElem = (level == select_path.size() - 1);
  if (constant *cobj = any_cast<constant *>(object)) {
    // Member-name selection into a PACKED struct constant (a config value
    // assembled from a struct-returning function reduces to a constant that
    // carries the struct typespec): slice the member's bits — first declared
    // member occupies the MSBs.  Without this, a walk that lands on such a
    // constant returned null and the width evaluation of
    // `[HPDcacheCfg.u.memAddrWidth-1:0]` collapsed to 1.
    const struct_typespec *stps = nullptr;
    if (const ref_typespec *rt = cobj->Typespec())
      stps = rt->Actual_typespec<struct_typespec>();
    if (stps && stps->Members() && !elemName.empty() && elemName[0] != '[') {
      uint64_t total = 0, off = 0, mw = 0;
      const typespec *mts_found = nullptr;
      bool found = false, bad = false;
      for (typespec_member *member : *stps->Members()) {
        const typespec *mts = member->Typespec()
                                  ? member->Typespec()->Actual_typespec()
                                  : nullptr;
        bool iv = false;
        uint64_t w = size(mts, iv, inst, pexpr, true, true);
        if (iv || w == 0) {
          bad = true;
          break;
        }
        if (!found && member->VpiName() == elemName) {
          found = true;
          mw = w;
          mts_found = mts;
        } else if (!found) {
          off += w;
        }
        total += w;
      }
      if (found && !bad) {
        std::string bin = toBinary(cobj);
        if (bin.size() < total)
          bin.insert(bin.begin(), total - bin.size(), '0');
        else if (bin.size() > total)
          bin = bin.substr(bin.size() - total);
        std::string mbits = bin.substr(off, mw);
        // Only a FULLY-DEFINED slice is a usable value.  A parameter stamped
        // with a partially-evaluated struct (x-laden merge from a failed
        // member write) must NOT satisfy the member select — returning the
        // garbage made `CFG.u.sidWidth` fold to a 96-bit x-mix and every
        // dependent port width collapsed (param_nested_struct_field_width).
        for (char bch : mbits)
          if (bch != '0' && bch != '1') {
            found = false;
            break;
          }
        if (mbits.empty()) found = false;
      }
      if (found && !bad) {
        std::string bin = toBinary(cobj);
        if (bin.size() < total)
          bin.insert(bin.begin(), total - bin.size(), '0');
        else if (bin.size() > total)
          bin = bin.substr(bin.size() - total);
        std::string mbits = bin.substr(off, mw);
        constant *c = s.MakeConstant();
        c->VpiValue("BIN:" + mbits);
        c->VpiDecompile(mbits);
        c->VpiSize(static_cast<int32_t>(mbits.size()));
        c->VpiConstType(vpiBinaryConst);
        if (mts_found) {
          ref_typespec *rtc = s.MakeRef_typespec();
          rtc->Actual_typespec(const_cast<typespec *>(mts_found));
          rtc->VpiParent(c);
          c->Typespec(rtc);
        }
        if (lastElem) return c;
        return hierarchicalSelector(select_path, level + 1, c, invalidValue,
                                    inst, pexpr, returnType, muteError);
      }
    }
  }
  if (variables *var = any_cast<variables *>(object)) {
    UHDM_OBJECT_TYPE ttps = var->UhdmType();
    if (ttps == UHDM_OBJECT_TYPE::uhdmstruct_var) {
      if (const ref_typespec *svrt = var->Typespec()) {
        if (const struct_typespec *stpt =
                svrt->Actual_typespec<struct_typespec>()) {
          for (typespec_member *member : *stpt->Members()) {
            if (member->VpiName() == elemName) {
              if (returnType == ReturnType::TYPESPEC) {
                if (ref_typespec *mrt = member->Typespec()) {
                  any *res = mrt->Actual_typespec();
                  if (lastElem) {
                    return res;
                  } else {
                    return hierarchicalSelector(select_path, level + 1, res,
                                                invalidValue, inst, pexpr,
                                                returnType, muteError);
                  }
                }
              } else if (returnType == ReturnType::MEMBER) {
                return member;
              } else {
                any *res = member->Actual_value() ? member->Actual_value()
                                                  : member->Default_value();
                // A member that is itself a struct must keep walking the
                // remaining path elements (`CFG.u.sidWidth`): returning the
                // intermediate member's value here drops the trailing
                // selectors, and the caller then substitutes a whole-struct
                // value where a scalar field was expected.  The
                // struct_typespec branch below already does this.
                if (lastElem || res == nullptr) return res;
                return hierarchicalSelector(select_path, level + 1, res,
                                            invalidValue, inst, pexpr,
                                            returnType, muteError);
              }
            }
          }
        }
      }
    } else if (ttps == UHDM_OBJECT_TYPE::uhdmclass_var) {
      if (ref_typespec *rt = var->Typespec()) {
        if (class_typespec *ctps = rt->Actual_typespec<class_typespec>()) {
          const class_defn *defn = ctps->Class_defn();
          while (defn) {
            if (defn->Variables()) {
              for (variables *member : *defn->Variables()) {
                if (member->VpiName() == elemName) {
                  if (returnType == ReturnType::TYPESPEC) {
                    if (ref_typespec *mrt = member->Typespec()) {
                      return mrt->Actual_typespec();
                    }
                  } else {
                    return member;
                  }
                }
              }
            }
            const class_defn *base_defn = nullptr;
            if (const extends *ext = defn->Extends()) {
              if (const ref_typespec *rt = ext->Class_typespec()) {
                if (const class_typespec *tp =
                        rt->Actual_typespec<class_typespec>()) {
                  base_defn = tp->Class_defn();
                }
              }
            }
            defn = base_defn;
          }
        }
      }
    } else if (ttps == UHDM_OBJECT_TYPE::uhdmarray_var) {
      if (returnType == ReturnType::TYPESPEC) {
        if (ref_typespec *rt = var->Typespec()) {
          any *res = rt->Actual_typespec();
          if (lastElem) {
            return res;
          } else {
            return hierarchicalSelector(select_path, level + 1, res,
                                        invalidValue, inst, pexpr,
                                        returnType, muteError);
          }
        }
      }
    }
  } else if (struct_typespec *stpt = any_cast<struct_typespec>(object)) {
    for (typespec_member *member : *stpt->Members()) {
      if (member->VpiName() == elemName) {
        any *res = nullptr;
        if (returnType == ReturnType::TYPESPEC) {
          if (ref_typespec *mrt = member->Typespec()) {
            any *res = mrt->Actual_typespec();
            if (lastElem) {
              return res;
            } else {
              return hierarchicalSelector(select_path, level + 1, res,
                                          invalidValue, inst, pexpr,
                                          returnType, muteError);
            }
          }
        } else {
          res = member->Actual_value() ? member->Actual_value() : member->Default_value();
        }
        if (lastElem) {
          return res;
        } else {
          return hierarchicalSelector(select_path, level + 1, res, invalidValue,
                                      inst, pexpr, returnType, muteError);
        }
      }
    }
  } else if (io_decl *decl = any_cast<io_decl *>(object)) {
    if (const any *exp = decl->Expr()) {
      UHDM_OBJECT_TYPE ttps = exp->UhdmType();
      if (ttps == UHDM_OBJECT_TYPE::uhdmstruct_var) {
        if (const ref_typespec *rt = ((struct_var *)exp)->Typespec()) {
          if (const struct_typespec *stpt =
                  rt->Actual_typespec<struct_typespec>()) {
            for (typespec_member *member : *stpt->Members()) {
              if (member->VpiName() == elemName) {
                if (returnType == ReturnType::TYPESPEC) {
                  if (ref_typespec *mrt = member->Typespec()) {
                    any *res = mrt->Actual_typespec();
                    if (lastElem) {
                      return res;
                    } else {
                      return hierarchicalSelector(select_path, level + 1, res,
                                                  invalidValue, inst, pexpr,
                                                  returnType, muteError);
                    }
                  }
                } else {
                  any *res = member->Actual_value() ? member->Actual_value()
                                                    : member->Default_value();
                  // Same nested-member walk as the struct_var branch above.
                  if (lastElem || res == nullptr) return res;
                  return hierarchicalSelector(select_path, level + 1, res,
                                              invalidValue, inst, pexpr,
                                              returnType, muteError);
                }
              }
            }
          }
        }
      }
    }
    if (returnType == ReturnType::TYPESPEC) {
      if (const ref_typespec *rt = decl->Typespec()) {
        if (const typespec *tps = rt->Actual_typespec()) {
          UHDM_OBJECT_TYPE ttps = tps->UhdmType();
          if (ttps == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
            struct_typespec *stpt = (struct_typespec *)tps;
            for (typespec_member *member : *stpt->Members()) {
              if (member->VpiName() == elemName) {
                if (ref_typespec *mrt = member->Typespec()) {
                  any *res = mrt->Actual_typespec();
                  if (lastElem) {
                    return res;
                  } else {
                    return hierarchicalSelector(select_path, level + 1, res,
                                                invalidValue, inst, pexpr,
                                                returnType, muteError);
                  }
                }
              }
            }
          } else if (ttps == UHDM_OBJECT_TYPE::uhdmclass_typespec) {
            class_typespec *stpt = (class_typespec *)tps;
            const class_defn *defn = stpt->Class_defn();
            while (defn) {
              if (defn->Variables()) {
                for (variables *member : *defn->Variables()) {
                  if (member->VpiName() == elemName) {
                    if (ref_typespec *mrt = member->Typespec()) {
                      return mrt->Actual_typespec();
                    }
                  }
                }
              }
              const class_defn *base_defn = nullptr;
              if (const extends *ext = defn->Extends()) {
                if (const ref_typespec *rt = ext->Class_typespec()) {
                  if (const class_typespec *tp =
                          rt->Actual_typespec<class_typespec>()) {
                    base_defn = tp->Class_defn();
                  }
                }
              }
              defn = base_defn;
            }
          }
        }
      }
    }
    if (returnType == ReturnType::VALUE) {
      VectorOfparam_assign *param_assigns = nullptr;
      if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
      } else if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
        param_assigns = ((design *)inst)->Param_assigns();
      } else if (const scope *spe = any_cast<const scope *>(inst)) {
        param_assigns = spe->Param_assigns();
      }
      if (param_assigns) {
        for (param_assign *param : *param_assigns) {
          if (param && param->Lhs()) {
            const std::string_view param_name = param->Lhs()->VpiName();
            if (param_name == elemName) {
              const std::string_view param_value_name = param->Rhs()->VpiName();
              any *objectrhs = getObject(param_value_name, inst, pexpr, muteError);
              if (objectrhs == nullptr) {
                objectrhs = getValue(param_value_name, inst, pexpr, muteError);
              }
              return hierarchicalSelector(select_path, level + 1, objectrhs ? objectrhs : param->Rhs(),
                                                invalidValue, inst, pexpr,
                                                returnType, muteError);
            }
          }
        }
      }
    }
  } else if (nets *nt = any_cast<nets *>(object)) {
    UHDM_OBJECT_TYPE ttps = nt->UhdmType();
    if (ttps == UHDM_OBJECT_TYPE::uhdmstruct_net) {
      if (const ref_typespec *rt = ((struct_net *)nt)->Typespec()) {
        VectorOftypespec_member *members = nullptr;
        if (const struct_typespec *sts =
                rt->Actual_typespec<struct_typespec>()) {
          members = sts->Members();
        } else if (const union_typespec *uts =
                       rt->Actual_typespec<union_typespec>()) {
          members = uts->Members();
        }
        if (members) {
          for (typespec_member *member : *members) {
            if (member->VpiName() == elemName) {
              if (returnType == ReturnType::TYPESPEC) {
                if (ref_typespec *mrt = member->Typespec()) {
                  any *res = mrt->Actual_typespec();
                  if (lastElem) {
                    return res;
                  } else {
                    return hierarchicalSelector(select_path, level + 1, res,
                                                invalidValue, inst, pexpr,
                                                returnType, muteError);
                  }
                }
              } else {
                any *res = member->Actual_value() ? member->Actual_value()
                                                  : member->Default_value();
                // Same nested-member walk as the struct_var branch above.
                if (lastElem || res == nullptr) return res;
                return hierarchicalSelector(select_path, level + 1, res,
                                            invalidValue, inst, pexpr,
                                            returnType, muteError);
              }
            }
          }
        }
      }
    }
  } else if (port *tport = any_cast<port *>(object)) {
    any* low_conn = tport->Low_conn();
    if (low_conn) {
      if (low_conn->UhdmType() == uhdmref_obj) {
        ref_obj* ref = (ref_obj*) low_conn;
        any* actual = ref->Actual_group();
        if (actual) {
          if (actual->UhdmType() == uhdmmodport) {
            modport* mport = (modport*) actual;
            if (mport->Io_decls()) {
              for (io_decl* decl : *mport->Io_decls()) {
                if (elemName == decl->VpiName()) {
                  if (returnType == ReturnType::MEMBER) {
                    return decl;
                  } else if (returnType == ReturnType::TYPESPEC) {
                    if (decl->Typespec())
                      return decl->Typespec()->Actual_typespec();
                  } else {
                    return decl->Expr();
                  }
                }
              }
            }
          }
        }
      }
    }  
  } else if (constant *cons = any_cast<constant *>(object)) {
    if (ref_typespec *rt = cons->Typespec()) {
      if (const typespec *ts = rt->Actual_typespec()) {
        UHDM_OBJECT_TYPE ttps = ts->UhdmType();
        if (ttps == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
          struct_typespec *stpt = (struct_typespec *)ts;
          uint64_t from = 0;
          uint64_t width = 0;
          for (typespec_member *member : *stpt->Members()) {
            if (member->VpiName() == elemName) {
              width = size(member, invalidValue, inst, pexpr, true);
              if (cons->VpiSize() <= 64) {
                uint64_t iv = get_value(invalidValue, cons);
                uint64_t mask = 0;

                for (uint64_t i = from; i < uint64_t(from + width); i++) {
                  mask |= ((uint64_t)1 << i);
                }
                uint64_t res = iv & mask;
                res = res >> (from);
                cons->VpiValue("UINT:" + std::to_string(res));
                cons->VpiSize(static_cast<int32_t>(width));
                cons->VpiConstType(vpiUIntConst);
                return cons;
              } else {
                std::string_view val = cons->VpiValue();
                int32_t ctype = cons->VpiConstType();
                if (ctype == vpiHexConst) {
                  std::string_view vval =
                      val.substr(strlen("HEX:"), std::string::npos);
                  std::string bin = NumUtils::hexToBin(vval);
                  std::string res = bin.substr(from, width);
                  cons->VpiValue("BIN:" + res);
                  cons->VpiSize(static_cast<int32_t>(width));
                  cons->VpiConstType(vpiBinaryConst);
                  return cons;
                } else if (ctype == vpiBinaryConst) {
                  std::string_view bin =
                      val.substr(strlen("BIN:"), std::string::npos);
                  std::string_view res;
                  if (bin == "0") 
                    res = bin;
                  else 
                    res = bin.substr(from, width);
                  cons->VpiValue("BIN:" + std::string(res));
                  cons->VpiSize(static_cast<int32_t>(width));
                  cons->VpiConstType(vpiBinaryConst);
                  return cons;
                }
              }
            } else {
              from += size(member, invalidValue, inst, pexpr, true);
            }
          }
        }
      }
    }
  }

  int32_t selectIndex = -1;
  if (elemName.find('[') != std::string::npos) {
    std::string_view indexName = ltrim(elemName, '[');
    indexName = rtrim(indexName, ']');
    if (NumUtils::parseInt32(indexName, &selectIndex) == nullptr) {
      selectIndex = -1;
    }
    elemName.clear();
    if (const operation *oper = any_cast<const operation *>(object)) {
      int32_t opType = oper->VpiOpType();
      if (opType == vpiAssignmentPatternOp) {
        VectorOfany *operands = oper->Operands();
        // Element typespec from the pattern's own (array) typespec — needed
        // so the picked element can itself fold/select downstream.
        const typespec *elemTs = nullptr;
        if (oper->Typespec()) {
          if (const typespec *ots = oper->Typespec()->Actual_typespec()) {
            if (const packed_array_typespec *pt =
                    any_cast<const packed_array_typespec *>(ots)) {
              if (pt->Elem_typespec())
                elemTs = pt->Elem_typespec()->Actual_typespec();
            } else if (const array_typespec *at =
                           any_cast<const array_typespec *>(ots)) {
              if (at->Elem_typespec())
                elemTs = at->Elem_typespec()->Actual_typespec();
            } else if (const logic_typespec *lt =
                           any_cast<const logic_typespec *>(ots)) {
              if (lt->Elem_typespec())
                elemTs = lt->Elem_typespec()->Actual_typespec();
            }
          }
        }
        // Typedef-alias duplication (`typedef fmt_unsigned_t [0:3] t;`):
        // the Elem_typespec chain repeats the OUTER range, so the element
        // type is the chain's OWN Elem_typespec, one level deeper.
        bool dup_stripped = false;
        if (elemTs) {
          if (const logic_typespec *elt =
                  any_cast<const logic_typespec *>(elemTs)) {
            if (elt->Elem_typespec() &&
                elt->Elem_typespec()->Actual_typespec() && elt->Ranges() &&
                !elt->Ranges()->empty()) {
              elemTs = elt->Elem_typespec()->Actual_typespec();
              dup_stripped = true;
            }
          }
        }
        // Guard against Surelog's typedef-alias duplication: the
        // Elem_typespec can point at a typespec that repeats the OUTER
        // geometry.  Validate size(elem) * n == size(whole); if not,
        // synthesize a plain logic typespec of the true element width.
        if (!dup_stripped && elemTs && oper->Typespec() &&
            oper->Typespec()->Actual_typespec()) {
          const typespec *ots0 = oper->Typespec()->Actual_typespec();
          VectorOfrange *org = nullptr;
          if (const packed_array_typespec *pt0 =
                  any_cast<const packed_array_typespec *>(ots0))
            org = pt0->Ranges();
          else if (const array_typespec *at0 =
                       any_cast<const array_typespec *>(ots0))
            org = at0->Ranges();
          else if (const logic_typespec *lt0 =
                       any_cast<const logic_typespec *>(ots0))
            org = lt0->Ranges();
          if (org && !org->empty()) {
            bool giv = false;
            int64_t gl = get_value(
                giv, reduceExpr((any *)org->at(0)->Left_expr(), giv, inst,
                                pexpr, muteError));
            int64_t gr = get_value(
                giv, reduceExpr((any *)org->at(0)->Right_expr(), giv, inst,
                                pexpr, muteError));
            uint64_t gn =
                giv ? 0 : (uint64_t)(gl > gr ? gl - gr + 1 : gr - gl + 1);
            bool wiv = false;
            uint64_t ww = size(ots0, wiv, inst, pexpr, true, muteError);
            bool eiv2 = false;
            uint64_t ew0 = size(elemTs, eiv2, inst, pexpr, true, muteError);
            if (!wiv && gn > 0 && (ww % gn) == 0) {
              uint64_t exp_ew = ww / gn;
              if (eiv2 || ew0 != exp_ew) {
                logic_typespec *sub0 = s.MakeLogic_typespec();
                VectorOfrange *sr = s.MakeRangeVec();
                range *rg = s.MakeRange();
                constant *cl = s.MakeConstant();
                cl->VpiValue("INT:" + std::to_string((int64_t)exp_ew - 1));
                cl->VpiSize(64);
                cl->VpiConstType(vpiIntConst);
                constant *cr = s.MakeConstant();
                cr->VpiValue("INT:0");
                cr->VpiSize(64);
                cr->VpiConstType(vpiIntConst);
                rg->Left_expr(cl);
                rg->Right_expr(cr);
                sr->push_back(rg);
                sub0->Ranges(sr);
                elemTs = sub0;
              }
            }
          }
        }
        auto tagElem = [&](any *picked) -> any * {
          if (!picked || !elemTs) return picked;
          // Tag a CLONE, never the shared node: the picked element belongs
          // to a persisted pattern shared by every consumer, and mutating
          // its typespec re-types unrelated reads
          // (compressed_instr_decoder's CoproInstr constants).  Only an
          // untyped OPERATION needs the tag — it feeds the final
          // pattern-fold (fpnew's `'{default: PARALLEL}` element).
          if (picked->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation &&
              ((operation *)picked)->Typespec() == nullptr) {
            ElaboratorContext elabCtx(&s, false, true);
            if (any *cl = clone_tree(picked, &elabCtx)) {
              operation *pop = (operation *)cl;
              ref_typespec *rt = s.MakeRef_typespec();
              rt->Actual_typespec((typespec *)elemTs);
              rt->VpiParent(pop);
              pop->Typespec(rt);
              return cl;
            }
          }
          return picked;
        };
        // `'{default: v}` — every element is the default value.
        if (operands && operands->size() == 1 &&
            operands->at(0)->UhdmType() ==
                UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
          tagged_pattern *tp0 = (tagged_pattern *)operands->at(0);
          const typespec *tts =
              tp0->Typespec() ? tp0->Typespec()->Actual_typespec() : nullptr;
          if (tts && tts->VpiName() == "default" && selectIndex >= 0) {
            return hierarchicalSelector(select_path, level + 1,
                                        tagElem((any *)tp0->Pattern()),
                                        invalidValue, inst, pexpr, returnType,
                                        muteError);
          }
        }
        int32_t sInd = 0;
        for (auto operand : *operands) {
          if ((selectIndex >= 0) && (sInd == selectIndex)) {
            return hierarchicalSelector(select_path, level + 1,
                                        tagElem(operand), invalidValue, inst,
                                        pexpr, returnType, muteError);
          }
          sInd++;
        }
      }
    } else if (const logic_typespec *ltps =
                   any_cast<const logic_typespec *>(object)) {
      VectorOfrange *ranges = ltps->Ranges();
      if (ranges && (ranges->size() >= 2)) {
        logic_typespec *tmp = s.MakeLogic_typespec();
        VectorOfrange *tmpR = s.MakeRangeVec();
        for (uint32_t i = 1; i < ranges->size(); i++) {
          tmpR->push_back(ranges->at(i));
        }
        tmp->Ranges(tmpR);
        return tmp;
      }
    } else if (const array_typespec *ltps =
                   any_cast<const array_typespec *>(object)) {
      if (const ref_typespec *rt = ltps->Elem_typespec()) {
        return (typespec *)rt->Actual_typespec();
      }
    } else if (const packed_array_typespec *ltps =
                   any_cast<const packed_array_typespec *>(object)) {
      if (const ref_typespec *rt = ltps->Elem_typespec()) {
        return (typespec *)rt->Actual_typespec();
      }
    } else if (constant *c = any_cast<constant *>(object)) {
      // ELEMENT select on a packed-array-typed constant: `[N]` must slice
      // the whole element, not bit N (fpnew_top's
      // `Implementation.UnitTypes[opgrp]` was stamped with single bits
      // tracking the genvar, poisoning paramod uniquification, constant
      // folding and generate elaboration downstream).  A pattern-member
      // constant often carries NO typespec (a >64-bit struct resolves
      // through the complex-value pattern, whose members are bare) — derive
      // the expected type by walking the select path from the base
      // parameter's typespec.
      const typespec *walk_ts = nullptr;
      if ((c->Typespec() == nullptr ||
           c->Typespec()->Actual_typespec() == nullptr) &&
          inst && level >= 1) {
        const typespec *tps = nullptr;
        // The base may be a PACKAGE parameter — resolve through the same
        // functor chain decodeHierPath uses.
        if (any *bobj = getObject(select_path[0], inst, pexpr, muteError)) {
          if (param_assign *bpa = any_cast<param_assign *>(bobj))
            bobj = (any *)bpa->Lhs();
          if (const parameter *bp = any_cast<const parameter *>(bobj))
            if (bp->Typespec()) tps = bp->Typespec()->Actual_typespec();
        }
        std::string_view baseName = select_path[0];
        size_t cpos = baseName.rfind("::");
        if (cpos != std::string_view::npos) baseName.remove_prefix(cpos + 2);
        const any *tmpi = inst;
        while (!tps && tmpi) {
          VectorOfparam_assign *pas = nullptr;
          if (tmpi->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
          } else if (tmpi->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
            pas = ((design *)tmpi)->Param_assigns();
          } else if (const scope *spe = any_cast<const scope *>(tmpi)) {
            pas = spe->Param_assigns();
          }
          if (pas) {
            for (param_assign *pa : *pas) {
              if (!pa->Lhs()) continue;
              std::string_view ln = pa->Lhs()->VpiName();
              size_t lpos = ln.rfind("::");
              if (lpos != std::string_view::npos) ln.remove_prefix(lpos + 2);
              if (ln == baseName || pa->Lhs()->VpiName() == select_path[0]) {
                if (const parameter *p =
                        any_cast<const parameter *>(pa->Lhs()))
                  if (p->Typespec())
                    tps = p->Typespec()->Actual_typespec();
                break;
              }
            }
          }
          tmpi = tmpi->VpiParent();
        }
        const typespec *cur = tps;
        auto unwrapA = [](const typespec *t) -> const typespec * {
          if (t->UhdmType() == UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
            if (const ref_typespec *rt =
                    ((packed_array_typespec *)t)->Elem_typespec())
              return rt->Actual_typespec();
            return nullptr;
          } else if (t->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
            if (const ref_typespec *rt =
                    ((array_typespec *)t)->Elem_typespec())
              return rt->Actual_typespec();
            return nullptr;
          }
          return t;
        };
        for (uint32_t wl = 1; wl < level && cur; wl++) {
          const std::string &tok = select_path[wl];
          if (!tok.empty() && tok[0] == '[') {
            const typespec *nxt = unwrapA(cur);
            cur = (nxt == cur) ? nullptr : nxt;
          } else {
            const struct_typespec *st =
                (cur->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_typespec)
                    ? (const struct_typespec *)cur
                    : nullptr;
            const typespec *next = nullptr;
            if (st && st->Members()) {
              for (typespec_member *member : *st->Members()) {
                if (member->VpiName() == tok) {
                  if (const ref_typespec *mrt = member->Typespec())
                    next = mrt->Actual_typespec();
                  break;
                }
              }
            }
            cur = next;
          }
        }
        walk_ts = cur;
      }
      if (constant *ec = reducePackedElemSelect(c, selectIndex, this, s,
                                                invalidValue, inst, pexpr,
                                                muteError, walk_ts)) {
        if (lastElem) {
          if (returnType == ReturnType::TYPESPEC) {
            if (ref_typespec *rt = ec->Typespec()) return rt->Actual_typespec();
            return nullptr;
          }
          return ec;
        }
        return hierarchicalSelector(select_path, level + 1, ec, invalidValue,
                                    inst, pexpr, returnType, muteError);
      }
      if (expr *tmp = reduceBitSelect(c, selectIndex, invalidValue, inst, pexpr,
                                      muteError)) {
        if (returnType == ReturnType::TYPESPEC) {
          if (ref_typespec *rt = tmp->Typespec()) {
            return rt->Actual_typespec();
          }
          return nullptr;
        }
        return tmp;
      }
      return object;
    }
  } else if (level == 0) {
    return hierarchicalSelector(select_path, level + 1, object, invalidValue,
                                inst, pexpr, returnType, muteError);
  }

  if (const operation *oper = any_cast<const operation *>(object)) {
    int32_t opType = oper->VpiOpType();

    if (opType == vpiAssignmentPatternOp) {
      VectorOfany *operands = oper->Operands();
      any *defaultPattern = nullptr;
      int32_t sInd = 0;

      int32_t bIndex = -1;
      const typespec *bMemberTs = nullptr;
      if (inst) {
        /*
        any *baseP = nullptr;
        VectorOfany *parameters = nullptr;
        if (inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
        } else if (inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
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
        {
          any *baseP = getObject(select_path[0], inst, pexpr, muteError);
          const typespec *tps = nullptr;
          if (parameter *p = any_cast<parameter *>(baseP)) {
            if (const ref_typespec *rt = p->Typespec()) {
              tps = rt->Actual_typespec();
            }
          } else if (operation *op = any_cast<operation *>(baseP)) {
            if (const ref_typespec *rt = op->Typespec()) {
              tps = rt->Actual_typespec();
            }
          } else if (io_decl *decl = any_cast<io_decl *>(baseP)) {
            if (const ref_typespec *rt = decl->Typespec()) {
              tps = rt->Actual_typespec();
            }
          }
          // A struct parameter's VALUE is a bare assignment-pattern operation
          // that carries no typespec (only NAMED operands are tagged with their
          // member type).  The declared struct type is on the PARAMETER
          // declaration in the REAL design component — unreachable from the
          // placeholder `inst` used during expression reduction.  Ask the host
          // (Surelog) to resolve the name to its declared typespec through a
          // dedicated functor; this side-channel does NOT feed value
          // resolution, so instance-specific parameter values are unaffected.
          if (tps == nullptr && getTypespecFunctor) {
            if (any *tsobj = getTypespecFunctor(select_path[0], inst, pexpr)) {
              if (const typespec *t = any_cast<const typespec *>(tsobj)) {
                tps = t;
              }
            }
          }
          if (tps) {
            // `tps` is the DECLARED type of the ROOT base (select_path[0]).
            // Walk the already-consumed path elements (select_path[1..level-1])
            // down to the type that directly contains `elemName`, honoring both
            // struct-member descents (e.g. `CFG.HSK.DLY` where `HSK` is a nested
            // struct) and array-index tokens `[N]` (e.g. `FP_ENCODINGS[fmt]
            // .exp_bits`, where the element type must be unwrapped).  Computing
            // the containing struct lets a POSITIONAL pattern operand be indexed
            // by member order below.
            const typespec *cur = tps;
            auto unwrapArray = [](const typespec *t) -> const typespec * {
              if (t->UhdmType() == UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
                if (const ref_typespec *rt =
                        ((packed_array_typespec *)t)->Elem_typespec())
                  return rt->Actual_typespec();
                return nullptr;
              } else if (t->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
                if (const ref_typespec *rt =
                        ((array_typespec *)t)->Elem_typespec())
                  return rt->Actual_typespec();
                return nullptr;
              }
              return t;
            };
            for (uint32_t lvl = 1; lvl < level && cur; lvl++) {
              const std::string &tok = select_path[lvl];
              if (!tok.empty() && tok[0] == '[') {
                cur = unwrapArray(cur);
              } else {
                const struct_typespec *st =
                    (cur->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_typespec)
                        ? (const struct_typespec *)cur
                        : nullptr;
                const typespec *next = nullptr;
                if (st && st->Members()) {
                  for (typespec_member *member : *st->Members()) {
                    if (member->VpiName() == tok) {
                      if (const ref_typespec *mrt = member->Typespec())
                        next = mrt->Actual_typespec();
                      break;
                    }
                  }
                }
                cur = next;
              }
            }
            // Peel any array dimensions still in front of the member access.
            while (cur &&
                   (cur->UhdmType() ==
                        UHDM_OBJECT_TYPE::uhdmpacked_array_typespec ||
                    cur->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec)) {
              const typespec *nxt = unwrapArray(cur);
              if (nxt == cur) break;
              cur = nxt;
            }
            if (cur &&
                cur->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
              struct_typespec *sts = (struct_typespec *)cur;
              if (sts->Members()) {
                uint32_t i = 0;
                for (typespec_member *member : *sts->Members()) {
                  if (member->VpiName() == elemName) {
                    bIndex = i;
                    if (const ref_typespec *mrt = member->Typespec())
                      bMemberTs = mrt->Actual_typespec();
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
          VectorOfparam_assign *param_assigns = nullptr;
          if (tmpInstance->UhdmType() ==
              UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
          } else if (tmpInstance->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
            param_assigns = ((design *)tmpInstance)->Param_assigns();
          } else if (const scope *spe = any_cast<const scope *>(tmpInstance)) {
            param_assigns = spe->Param_assigns();
          }
          if (param_assigns) {
            for (param_assign *param : *param_assigns) {
              if (param && param->Lhs()) {
                const std::string_view param_name = param->Lhs()->VpiName();
                if (param_name == select_path[0]) {
                  if (const parameter *p =
                          any_cast<const parameter *>(param->Lhs())) {
                    if (const ref_typespec *rt = p->Typespec()) {
                      if (const typespec *tps = rt->Actual_typespec()) {
                        if (tps->UhdmType() ==
                            UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
                          if (const ref_typespec *ert =
                                  ((packed_array_typespec *)tps)
                                      ->Elem_typespec()) {
                            tps = ert->Actual_typespec();
                          }
                        } else if (tps->UhdmType() ==
                                   UHDM_OBJECT_TYPE::uhdmarray_typespec) {
                          if (const ref_typespec *ert =
                                  ((array_typespec *)tps)->Elem_typespec()) {
                            tps = ert->Actual_typespec();
                          }
                        }
                        if (tps && (tps->UhdmType() ==
                                    UHDM_OBJECT_TYPE::uhdmstruct_typespec)) {
                          struct_typespec *sts = (struct_typespec *)tps;
                          if (VectorOftypespec_member *members =
                                  sts->Members()) {
                            uint32_t i = 0;
                            for (typespec_member *member : *members) {
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
          }
          tmpInstance = tmpInstance->VpiParent();
        }
      }
      for (auto operand : *operands) {
        UHDM_OBJECT_TYPE operandType = operand->UhdmType();
        if (operandType == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
          tagged_pattern *tpatt = (tagged_pattern *)operand;
          const typespec *tps = nullptr;
          if (const ref_typespec *rt = tpatt->Typespec()) {
            tps = rt->Actual_typespec();
          }
          if (tps && tps->VpiName() == "default") {
            defaultPattern = (any *)tpatt->Pattern();
          }
          if (tps && !elemName.empty() && (tps->VpiName() == elemName)) {
            const any *patt = tpatt->Pattern();
            UHDM_OBJECT_TYPE pattType = patt->UhdmType();
            if (pattType == UHDM_OBJECT_TYPE::uhdmconstant) {
              any *ex = reduceExpr((expr *)patt, invalidValue, inst, pexpr,
                                   muteError);
              // Attach the member's TRUE typespec to the pattern's folded
              // constant: it usually carries none (and can carry a
              // mis-recorded size), so a following `[N]` bit-selected
              // instead of element-selecting (fpnew's
              // `Implementation.UnitTypes[opgrp]`).
              if (constant *exc = any_cast<constant *>(ex)) {
                if (exc->Typespec() == nullptr) {
                  const struct_typespec *stps2 = nullptr;
                  if (const expr *oe = any_cast<const expr *>(object))
                    if (oe->Typespec())
                      stps2 = any_cast<const struct_typespec *>(
                          oe->Typespec()->Actual_typespec());
                  if (stps2 && stps2->Members()) {
                    for (typespec_member *m2 : *stps2->Members()) {
                      if (m2->VpiName() == elemName && m2->Typespec()) {
                        ref_typespec *rt2 = s.MakeRef_typespec();
                        rt2->Actual_typespec(
                            (typespec *)m2->Typespec()->Actual_typespec());
                        rt2->VpiParent(exc);
                        exc->Typespec(rt2);
                        break;
                      }
                    }
                  }
                }
              }
              if (level < select_path.size()) {
                ex = hierarchicalSelector(select_path, level + 1, ex,
                                          invalidValue, inst, pexpr,
                                          returnType);
              }
              if (returnType == ReturnType::TYPESPEC) {
                if (typespec *tp = any_cast<typespec>(ex)) {
                  return tp;
                } else if (expr *ep = any_cast<expr>(ex)) {
                  if (ref_typespec *rt = ep->Typespec()) {
                    return rt->Actual_typespec();
                  }
                } else if (io_decl *id = any_cast<io_decl>(ex)) {
                  if (ref_typespec *rt = id->Typespec()) {
                    return rt->Actual_typespec();
                  }
                } else if (typespec *tp = any_cast<typespec>(object)) {
                  return tp;
                } else if (expr *ep = any_cast<expr>(object)) {
                  if (ref_typespec *rt = ep->Typespec()) {
                    return rt->Actual_typespec();
                  }
                } else if (io_decl *id = any_cast<io_decl>(object)) {
                  if (ref_typespec *rt = id->Typespec()) {
                    return rt->Actual_typespec();
                  }
                }
                return nullptr;
              }
              return ex;
            } else if (pattType == UHDM_OBJECT_TYPE::uhdmoperation) {
              // Tag the member's pattern with the member typespec (the
              // tagged_pattern's typespec IS the member type) so nested
              // `[N]` selects and final folding know the geometry.
              operation *patt_op = (operation *)patt;
              if (patt_op->Typespec() == nullptr && tps) {
                ref_typespec *rt3 = s.MakeRef_typespec();
                rt3->Actual_typespec((typespec *)tps);
                rt3->VpiParent(patt_op);
                patt_op->Typespec(rt3);
              }
              return hierarchicalSelector(select_path, level + 1, (expr *)patt,
                                          invalidValue, inst, pexpr,
                                          returnType);
            }
          }
        } else if (operandType == UHDM_OBJECT_TYPE::uhdmconstant) {
          if ((bIndex >= 0) && (bIndex == sInd)) {
            // Attach the member's TRUE typespec to the positional constant:
            // pre-folded pattern operands carry none (and can carry a
            // mis-recorded size), so a following `[N]` bit-selected instead
            // of element-selecting.
            if (bMemberTs) {
              constant *oc = (constant *)operand;
              if (oc->Typespec() == nullptr) {
                ref_typespec *ort = s.MakeRef_typespec();
                ort->Actual_typespec((typespec *)bMemberTs);
                ort->VpiParent(oc);
                oc->Typespec(ort);
              }
            }
            return hierarchicalSelector(select_path, level + 1, (expr *)operand,
                                        invalidValue, inst, pexpr,
                                        returnType);
          }
        } else if (operandType == UHDM_OBJECT_TYPE::uhdmoperation) {
          // A positional operand that is itself an assignment-pattern (a nested
          // struct value, e.g. `HSK` in `cfg_t`).  This happens when a struct
          // parameter's value comes from a package localparam whose clone lost
          // the tagged-pattern member names, leaving a purely positional tree.
          if ((bIndex >= 0) && (bIndex == sInd)) {
            // Tag the member's value with its declared typespec so nested
            // `[N]` selects and final pattern folding know the geometry
            // (fpnew's Implementation.UnitTypes[opgrp]).
            if (bMemberTs) {
              operation *oop = (operation *)operand;
              if (oop->Typespec() == nullptr) {
                ref_typespec *ort = s.MakeRef_typespec();
                ort->Actual_typespec((typespec *)bMemberTs);
                ort->VpiParent(oop);
                oop->Typespec(ort);
              }
            }
            return hierarchicalSelector(select_path, level + 1, operand,
                                        invalidValue, inst, pexpr, returnType);
          }
        }
        sInd++;
      }
      if (defaultPattern) {
        if (expr *ex = any_cast<expr *>(defaultPattern)) {
          ex = reduceExpr(ex, invalidValue, inst, pexpr, muteError);
          if (returnType == ReturnType::TYPESPEC) {
            if (typespec *tp = any_cast<typespec>(ex)) {
              return tp;
            } else if (expr *ep = any_cast<expr>(ex)) {
              if (ref_typespec *rt = ep->Typespec()) {
                return rt->Actual_typespec();
              }
            } else if (io_decl *id = any_cast<io_decl>(ex)) {
              if (ref_typespec *rt = id->Typespec()) {
                return rt->Actual_typespec();
              }
            } else if (typespec *tp = any_cast<typespec>(object)) {
              return tp;
            } else if (expr *ep = any_cast<expr>(object)) {
              if (ref_typespec *rt = ep->Typespec()) {
                return rt->Actual_typespec();
              }
            } else if (io_decl *id = any_cast<io_decl>(object)) {
              if (ref_typespec *rt = id->Typespec()) {
                return rt->Actual_typespec();
              }
            }
            return nullptr;
          }
          return ex;
        }
      }
    }
  }
  return nullptr;
}

// Does `e` reference `name` (a ref_obj / select base / nested operand or call
// argument)?  Used by evalFunc to refuse binding a formal to an argument
// expression that still mentions a formal of the SAME name — the classic
// `outer(inner(x))` with both formals called `g`: inner's non-constant result
// `{g[0], g[1]}` bound to outer's `g` makes getValue("g") reduce to itself.
static bool exprRefsName(const any *e, std::string_view name, int depth = 0) {
  if (!e || depth > 64) return false;
  switch (e->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmref_obj:
      return ((const ref_obj *)e)->VpiName() == name;
    case UHDM_OBJECT_TYPE::uhdmbit_select: {
      const bit_select *b = (const bit_select *)e;
      return b->VpiName() == name ||
             exprRefsName(b->VpiIndex(), name, depth + 1);
    }
    case UHDM_OBJECT_TYPE::uhdmpart_select: {
      const part_select *ps = (const part_select *)e;
      return ps->VpiName() == name ||
             exprRefsName(ps->Left_range(), name, depth + 1) ||
             exprRefsName(ps->Right_range(), name, depth + 1);
    }
    case UHDM_OBJECT_TYPE::uhdmindexed_part_select: {
      const indexed_part_select *ps = (const indexed_part_select *)e;
      return ps->VpiName() == name ||
             exprRefsName(ps->Base_expr(), name, depth + 1) ||
             exprRefsName(ps->Width_expr(), name, depth + 1);
    }
    case UHDM_OBJECT_TYPE::uhdmvar_select: {
      const var_select *vs = (const var_select *)e;
      if (vs->VpiName() == name) return true;
      if (vs->Exprs())
        for (auto x : *vs->Exprs())
          if (exprRefsName(x, name, depth + 1)) return true;
      return false;
    }
    case UHDM_OBJECT_TYPE::uhdmoperation: {
      const operation *op = (const operation *)e;
      if (op->Operands())
        for (auto x : *op->Operands())
          if (exprRefsName(x, name, depth + 1)) return true;
      return false;
    }
    case UHDM_OBJECT_TYPE::uhdmfunc_call:
    case UHDM_OBJECT_TYPE::uhdmsys_func_call: {
      const tf_call *c = (const tf_call *)e;
      if (c->Tf_call_args())
        for (auto x : *c->Tf_call_args())
          if (exprRefsName(x, name, depth + 1)) return true;
      return false;
    }
    default:
      return false;
  }
}

expr *ExprEval::reduceExpr(const any *result, bool &invalidValue,
                           const any *inst, const any *pexpr, bool muteError) {
  if (!result) return nullptr;
  // Recursion guard: a self-referential binding (getValue -> reduceExpr ->
  // getValue on the same name) used to recurse until the stack was exhausted
  // (SIGSEGV on OpenTitan aes_sbox_dom).  Legitimate reductions never nest
  // this deep; give up with invalidValue instead of crashing.
  static thread_local int32_t reduceDepth = 0;
  struct DepthGuard {
    int32_t &d;
    explicit DepthGuard(int32_t &dd) : d(dd) { ++d; }
    ~DepthGuard() { --d; }
  } depthGuard(reduceDepth);
  if (reduceDepth > 1024) {
    invalidValue = true;
    return nullptr;
  }
  Serializer &s = *result->GetSerializer();
  UHDM_OBJECT_TYPE objtype = result->UhdmType();
  if (objtype == UHDM_OBJECT_TYPE::uhdmoperation) {
    operation *op = (operation *)result;
    for (auto t : m_skipOperationTypes) {
      if (op->VpiOpType() == t) {
        return (expr *)result;
      }
    }
    bool constantOperands = true;
    if (VectorOfany *oprns = op->Operands()) {
      VectorOfany &operands = *oprns;
      for (auto oper : operands) {
        UHDM_OBJECT_TYPE optype = oper->UhdmType();
        if (optype == UHDM_OBJECT_TYPE::uhdmref_obj) {
          ref_obj *ref = (ref_obj *)oper;
          const std::string_view name = ref->VpiName();
          if (name == "default" && ref->VpiStructMember()) continue;
          if (getValue(name, inst, pexpr, muteError, result) == nullptr) {
            constantOperands = false;
            break;
          }
        } else if (optype == UHDM_OBJECT_TYPE::uhdmoperation) {
        } else if (optype == UHDM_OBJECT_TYPE::uhdmsys_func_call) {
        } else if (optype == UHDM_OBJECT_TYPE::uhdmfunc_call) {
        } else if (optype == UHDM_OBJECT_TYPE::uhdmbit_select) {
        } else if (optype == UHDM_OBJECT_TYPE::uhdmhier_path) {
        } else if (optype == UHDM_OBJECT_TYPE::uhdmvar_select) {
        } else if (optype == UHDM_OBJECT_TYPE::uhdmenum_var) {
        } else if (optype != UHDM_OBJECT_TYPE::uhdmconstant) {
          constantOperands = false;
          break;
        }
      }
      if (constantOperands) {
        int32_t optype = op->VpiOpType();
        switch (optype) {
          case vpiArithRShiftOp:
          case vpiRShiftOp: {
            if (operands.size() == 2) {
              expr *arg0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              if (arg0 && arg0->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
                constant *c = (constant *)arg0;
                if (c->VpiSize() == -1) invalidValue = true;
              }
              int64_t val0 = get_value(invalidValue, arg0);
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) >> ((uint64_t)val1);
              constant *c = s.MakeConstant();
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
                constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(64);
                c->VpiConstType(vpiIntConst);
                result = c;
                std::map<std::string, const typespec *> local_vars;
                setValueInInstance(operands[0]->VpiName(), operands[0], c,
                                   invalidValue, s, inst, op, local_vars, 0,
                                   muteError);
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
                  constant *c = s.MakeConstant();
                  c->VpiValue("REAL:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize(64);
                  c->VpiConstType(vpiRealConst);
                  result = c;
                  std::map<std::string, const typespec *> local_vars;
                  setValueInInstance(operands[0]->VpiName(), operands[0], c,
                                     invalidValue, s, inst, op, local_vars, 0,
                                     muteError);
                }
              }
            }
            break;
          }
          case vpiArithLShiftOp:
          case vpiLShiftOp: {
            if (operands.size() == 2) {
              expr *arg0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              if (arg0 && arg0->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
                constant *c = (constant *)arg0;
                if (c->VpiSize() == -1) invalidValue = true;
              }
              int64_t val0 = get_value(invalidValue, arg0);
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) << ((uint64_t)val1);
              constant *c = s.MakeConstant();
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
                  if (exp->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
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
                  constant *c = s.MakeConstant();
                  c->VpiValue("UINT:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize((expr0->VpiSize() > expr1->VpiSize())
                                 ? expr0->VpiSize()
                                 : expr1->VpiSize());
                  c->VpiConstType(vpiUIntConst);
                  result = c;
                }
              } else {
                int64_t val0 = get_value(invalidValueI, expr0);
                int64_t val1 = get_value(invalidValueI, expr1);
                if ((invalidValue == false) && (invalidValueI == false)) {
                  int64_t val = val0 + val1;
                  constant *c = s.MakeConstant();
                  c->VpiValue("INT:" + std::to_string(val));
                  c->VpiDecompile(std::to_string(val));
                  c->VpiSize((expr0->VpiSize() > expr1->VpiSize())
                                 ? expr0->VpiSize()
                                 : expr1->VpiSize());
                  c->VpiConstType(vpiIntConst);
                  result = c;
                } else {
                  invalidValueD = false;
                  long double val0 = get_double(invalidValueD, expr0);
                  long double val1 = get_double(invalidValueD, expr1);
                  if ((invalidValue == false) && (invalidValueD == false)) {
                    long double val = val0 + val1;
                    constant *c = s.MakeConstant();
                    c->VpiValue("REAL:" + std::to_string(val));
                    c->VpiDecompile(std::to_string(val));
                    c->VpiSize((expr0->VpiSize() > expr1->VpiSize())
                                   ? expr0->VpiSize()
                                   : expr1->VpiSize());
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
              constant *c = s.MakeConstant();
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
              constant *c = s.MakeConstant();
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
              constant *c = s.MakeConstant();
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
              constant *c = s.MakeConstant();
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
                if (expr0->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
                  size = expr0->VpiSize();
                }
                constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(static_cast<int32_t>(size));
                c->VpiConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = -val0;
                  constant *c = s.MakeConstant();
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
                constant *c = s.MakeConstant();
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
                  constant *c = s.MakeConstant();
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
                constant *c = s.MakeConstant();
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
                  constant *c = s.MakeConstant();
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
                if (operand->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
                  constant *c = (constant *)operand;
                  size = c->VpiSize();
                  if (const ref_typespec *rt = c->Typespec()) {
                    if (const typespec *tps = rt->Actual_typespec()) {
                      size = ExprEval::size(tps, invalidValue, inst, pexpr,
                                            true, muteError);
                    }
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

                constant *c = s.MakeConstant();
                c->VpiValue("UINT:" + std::to_string(val));
                c->VpiDecompile(std::to_string(val));
                c->VpiSize(static_cast<int32_t>(size));
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
              constant *c = s.MakeConstant();
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
              for (uint32_t i = 1; i < operands.size(); i++) {
                int64_t oval = get_value(
                    invalidValue, reduceExpr(operands[i], invalidValue, inst,
                                             pexpr, muteError));
                if (invalidValue) break;
                if (oval == val) {
                  constant *c = s.MakeConstant();
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
              for (int32_t i = 1; i < cst->VpiSize(); i++) {
                res = res & ((val & (1ULL << i)) >> i);
              }
              constant *c = s.MakeConstant();
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
              for (uint32_t i = 1; i < 32; i++) {
                res = res & ((val & (1ULL << i)) >> i);
              }
              res = !res;
              constant *c = s.MakeConstant();
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
              for (uint32_t i = 1; i < 32; i++) {
                res = res | ((val & (1ULL << i)) >> i);
              }
              constant *c = s.MakeConstant();
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
              for (uint32_t i = 1; i < 64; i++) {
                res = res | ((val & (1ULL << i)) >> i);
              }
              res = !res;
              constant *c = s.MakeConstant();
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
              for (uint32_t i = 1; i < 64; i++) {
                res = res ^ ((val & (1ULL << i)) >> i);
              }
              constant *c = s.MakeConstant();
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
              for (uint32_t i = 1; i < 64; i++) {
                res = res ^ ((val & (1ULL << i)) >> i);
              }
              res = !res;
              constant *c = s.MakeConstant();
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
                constant *c = s.MakeConstant();
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
                  constant *c = s.MakeConstant();
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
                          any_cast<const gen_scope_array *>(inst)) {
                    fullPath = in->VpiFullName();
                  } else if (inst &&
                             inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
                    fullPath = inst->VpiName();
                  } else if (const scope *spe = any_cast<const scope *>(inst)) {
                    fullPath = spe->VpiFullName();
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
                constant *c = s.MakeConstant();
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
                  constant *c = s.MakeConstant();
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
                constant *c = s.MakeConstant();
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
                  constant *c = s.MakeConstant();
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
                        any_cast<const gen_scope_array *>(inst)) {
                  fullPath = in->VpiFullName();
                } else if (inst &&
                           inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
                  fullPath = inst->VpiName();
                } else if (const scope *spe = any_cast<const scope *>(inst)) {
                  fullPath = spe->VpiFullName();
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
              // An UNBASED UNSIZED fill literal (`'1`, `'0`, `'x`, `'z` —
              // VpiSize() == -1) must keep its fill identity: collapsing it
              // through get_value() yields INT:1, so a context-determined
              // `wide = cond ? … : '1` came out as the VALUE 1 instead of
              // all-ones (CVA6 cva6_ptw `req_port_o.data_be = CVA6Cfg.
              // IS_XLEN32 ? be_gen_32(…) : '1` → 8'h01 instead of 8'hff).
              // Hand the literal back unchanged and let the consumer
              // replicate it to the target width.
              bool the_val_is_fill = false;
              if (the_val && the_val->UhdmType() == uhdmconstant)
                the_val_is_fill = (((constant *)the_val)->VpiSize() == -1);
              if (localInvalidValue == false && !the_val_is_fill) {
                val = get_value(localInvalidValue, the_val);
                if (localInvalidValue == false) {
                  constant *c = s.MakeConstant();
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
              if (cv->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant) {
                cv = reduceExpr(cv, invalidValue, inst, pexpr, muteError);
                if (cv->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant) {
                  break;
                }
              }
              constant *c = s.MakeConstant();
              int64_t width = cv->VpiSize();
              int32_t consttype = ((constant *)cv)->VpiConstType();
              c->VpiConstType(consttype);
              if (consttype == vpiBinaryConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("BIN:").length());
                std::string value;
                if (width > (int32_t)val.size()) {
                  value.append(width - val.size(), '0');
                }
                value += val;
                std::string res;
                for (uint32_t i = 0; i < n; i++) {
                  res += value;
                }
                c->VpiValue("BIN:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiHexConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("HEX:").length());
                std::string res;
                for (uint32_t i = 0; i < n; i++) {
                  res += val;
                }
                c->VpiValue("HEX:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiOctConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("OCT:").length());
                std::string res;
                for (uint32_t i = 0; i < n; i++) {
                  res += val;
                }
                c->VpiValue("OCT:" + res);
                c->VpiDecompile(res);
              } else if (consttype == vpiStringConst) {
                std::string_view val = cv->VpiValue();
                val.remove_prefix(std::string_view("STRING:").length());
                std::string res;
                for (uint32_t i = 0; i < n; i++) {
                  res += val;
                }
                c->VpiValue("STRING:" + res);
                c->VpiDecompile(res);
              } else {
                uint64_t val = get_value(invalidValue, cv);
                if (invalidValue) break;
                uint64_t res = 0;
                for (uint32_t i = 0; i < n; i++) {
                  res |= val << (i * width);
                }
                c->VpiValue("UINT:" + std::to_string(res));
                c->VpiDecompile(std::to_string(res));
                c->VpiConstType(vpiUIntConst);
              }
              c->VpiSize(static_cast<int32_t>(n * width));
              // Word size
              if (width) {
                int_typespec *ts = s.MakeInt_typespec();
                ts->VpiValue("UINT:" + std::to_string(width));
                ref_typespec *rt = s.MakeRef_typespec();
                rt->Actual_typespec(ts);
                rt->VpiParent(c);
                c->Typespec(rt);
              }
              result = c;
            }
            break;
          }
          case vpiConcatOp: {
            constant *c1 = s.MakeConstant();
            std::string cval;
            int32_t csize = 0;
            bool stringVal = false;
            // A packed-array-typed concat (`logic [N-1:0][W-1:0] P = {eN, ...,
            // e0}`) whose operands are unfolded arithmetic (`{Width{1'b1}} -
            // ResetValue`) reduces each element to a DEFAULT-width 64-bit int.
            // The vpiIntConst / vpiUIntConst cases below then hit the
            // `size == 64` guard and abort the whole fold (c1 = nullptr), so the
            // concat is never folded and the parameter value is wrong (OpenTitan
            // prim_count's ResetValues).  Derive the true per-operand element
            // width from the concat's OWN typespec (total width / operand count)
            // so those 64-bit operands are truncated to W bits instead.
            int64_t concatElemW = 0;
            if (const ref_typespec *ort = op->Typespec()) {
              if (const typespec *ots = ort->Actual_typespec()) {
                bool ivw = false;
                int64_t totW = size(ots, ivw, inst, pexpr, true, muteError);
                if (!ivw && totW > 0 && !operands.empty() &&
                    (totW % (int64_t)operands.size() == 0))
                  concatElemW = totW / (int64_t)operands.size();
              }
            }
            for (uint32_t i = 0; i < operands.size(); i++) {
              any *oper = operands[i];
              UHDM_OBJECT_TYPE optype = oper->UhdmType();
              int32_t operType = 0;
              if (optype == UHDM_OBJECT_TYPE::uhdmoperation) {
                operation *o = (operation *)oper;
                operType = o->VpiOpType();
              }
              if ((optype != UHDM_OBJECT_TYPE::uhdmconstant) &&
                  (operType != vpiConcatOp) &&
                  (operType != vpiMultiAssignmentPatternOp) &&
                  (operType != vpiAssignmentPatternOp)) {
                if (expr *tmp = reduceExpr(oper, invalidValue, inst, pexpr,
                                           muteError)) {
                  oper = tmp;
                }
                optype = oper->UhdmType();
              }
              if (optype == UHDM_OBJECT_TYPE::uhdmconstant) {
                constant *c2 = (constant *)oper;
                std::string_view sv = c2->VpiValue();
                int32_t size = c2->VpiSize();
                csize += size;
                int32_t type = c2->VpiConstType();
                switch (type) {
                  case vpiBinaryConst: {
                    sv.remove_prefix(std::string_view("BIN:").length());
                    std::string value;
                    if (size > (int32_t)sv.size()) {
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
                    if (size > (int32_t)tmp.size()) {
                      value.append(size - tmp.size(), '0');
                    } else if (size < (int32_t)tmp.size()) {
                      tmp.erase(0, (int32_t)tmp.size() - size);
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
                    // A default-width 64-bit operand whose real element width
                    // is known from the concat typespec is truncated to that
                    // width instead of aborting the fold.
                    int32_t useSize = size;
                    if (size == 64 && operands.size() != 1 && concatElemW > 0 &&
                        concatElemW < 64) {
                      useSize = (int32_t)concatElemW;
                      csize += useSize - size;
                    }
                    if (operands.size() == 1 || (useSize != 64)) {
                      sv.remove_prefix(std::string_view("INT:").length());
                      int64_t iv = 0;
                      if (NumUtils::parseInt64(sv, &iv) == nullptr) {
                        iv = 0;
                      }
                      std::string bin = NumUtils::toBinary(useSize, iv);
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
                    int32_t useSize = size;
                    if (size == 64 && operands.size() != 1 && concatElemW > 0 &&
                        concatElemW < 64) {
                      useSize = (int32_t)concatElemW;
                      csize += useSize - size;
                    }
                    if (operands.size() == 1 || (useSize != 64)) {
                      sv.remove_prefix(std::string_view("UINT:").length());
                      uint64_t iv = 0;
                      if (NumUtils::parseUint64(sv, &iv) == nullptr) {
                        iv = 0;
                      }
                      std::string bin = NumUtils::toBinary(useSize, iv);
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
                    } else if (sv.find("HEX:") == 0) {
                      // A constant with an unset VpiConstType can still
                      // carry a typed value string (enum_const values are
                      // stored as HEX:); parse by prefix instead of
                      // misreading it through the IINT: fallback below,
                      // which would fold the operand to 0.
                      sv.remove_prefix(std::string_view("HEX:").length());
                      std::string tmp = NumUtils::hexToBin(sv);
                      std::string value;
                      if (size > (int32_t)tmp.size()) {
                        value.append(size - tmp.size(), '0');
                      } else if (size < (int32_t)tmp.size()) {
                        tmp.erase(0, (int32_t)tmp.size() - size);
                      }
                      if (op->VpiReordered()) {
                        std::reverse(tmp.begin(), tmp.end());
                      }
                      value += tmp;
                      cval += value;
                    } else if (sv.find("BIN:") == 0) {
                      sv.remove_prefix(std::string_view("BIN:").length());
                      std::string value;
                      if (size > (int32_t)sv.size()) {
                        value.append(size - sv.size(), '0');
                      }
                      if (op->VpiReordered()) {
                        value.append(sv.rbegin(), sv.rend());
                      } else {
                        value.append(sv.begin(), sv.end());
                      }
                      cval += value;
                    } else if (sv.find("DEC:") == 0) {
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
                    } else if (sv.find("OCT:") == 0) {
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
                    } else if (sv.find("INT:") == 0) {
                      // Signed-int constant value string.  Without this the
                      // IINT: fallback below strips 5 chars from "INT:<v>" and
                      // mis-parses the value to 0 — folding a concat operand
                      // such as `{Width{1'b1}} - ResetValue` (INT:3) to 0 and
                      // corrupting the whole concatenation (prim_count).
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
                c1->VpiSize(static_cast<int32_t>(cval.size() * 8));
                c1->VpiConstType(vpiStringConst);
              } else {
                if (op->VpiReordered()) {
                  std::reverse(cval.begin(), cval.end());
                }
                if (cval.size() > UHDM_MAX_BIT_WIDTH) {
                  std::string fullPath;
                  if (const gen_scope_array *in =
                          any_cast<const gen_scope_array *>(inst)) {
                    fullPath = in->VpiFullName();
                  } else if (inst &&
                             inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
                    fullPath = inst->VpiName();
                  } else if (const scope *spe = any_cast<const scope *>(inst)) {
                    fullPath = spe->VpiFullName();
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
            const typespec *tps = nullptr;
            if (const ref_typespec *rt = op->Typespec()) {
              tps = rt->Actual_typespec();
            }
            if (tps == nullptr) break;
            UHDM_OBJECT_TYPE ttps = tps->UhdmType();
            if (ttps == UHDM_OBJECT_TYPE::uhdmint_typespec) {
              constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string((int32_t)val0));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            } else if (ttps == UHDM_OBJECT_TYPE::uhdmlong_int_typespec) {
              constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string((int64_t)val0));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
            } else if (ttps == UHDM_OBJECT_TYPE::uhdmbit_typespec) {
              constant *c = s.MakeConstant();
              c->VpiValue("BIN:" + std::to_string((int64_t)val0 & 1));
              c->VpiSize(1);
              c->VpiConstType(vpiBinaryConst);
              result = c;
            } else if (ttps == UHDM_OBJECT_TYPE::uhdmshort_int_typespec) {
              constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string((int16_t)val0));
              c->VpiSize(16);
              c->VpiConstType(vpiUIntConst);
              result = c;
            } else if (ttps == UHDM_OBJECT_TYPE::uhdminteger_typespec) {
              integer_typespec *itps = (integer_typespec *)tps;
              std::string_view val = itps->VpiValue();
              uint64_t cast_to = 0;
              if (val.empty()) {
                // A size cast `N'(expr)` whose width N is a constant
                // EXPRESSION (e.g. `$clog2(X)'(...)`) leaves VpiValue empty
                // and stores the width expression in the typespec's Expr().
                // Evaluate it instead of defaulting to the 32-bit `integer`
                // width (which would skip the intended truncation).
                cast_to = 32;
                if (const expr *we = itps->Expr()) {
                  bool wiv = false;
                  uint64_t w = get_value(
                      wiv,
                      reduceExpr((any *)we, wiv, inst, pexpr, muteError));
                  if (!wiv && w > 0) cast_to = w;
                }
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
              // SV LRM §6.24.1: a size cast `N'(expr)` preserves the
              // OPERAND's signedness.  We only treat the operand as
              // signed when an EXPLICIT signed marker is present (a
              // signed typespec on the folded constant) — `vpiIntConst`
              // alone is unreliable because Surelog stores arithmetic
              // results (e.g. `PartEnd - 4` from an unsigned PartEnd)
              // as `vpiIntConst` regardless of operand signedness.
              // `$signed(...)` attaches the signed typespec below so
              // that `8'(4'(signed'(-8'd1)))` correctly sign-extends.
              bool oper_signed = false;
              if (constant *oc = any_cast<constant *>(oper)) {
                if (const ref_typespec *ort = oc->Typespec()) {
                  if (const typespec *ots = ort->Actual_typespec()) {
                    UHDM_OBJECT_TYPE ott = ots->UhdmType();
                    if (ott == UHDM_OBJECT_TYPE::uhdmlogic_typespec)
                      oper_signed = ((const logic_typespec*)ots)->VpiSigned();
                    else if (ott == UHDM_OBJECT_TYPE::uhdmbit_typespec)
                      oper_signed = ((const bit_typespec*)ots)->VpiSigned();
                    else if (ott == UHDM_OBJECT_TYPE::uhdmint_typespec)
                      oper_signed = ((const int_typespec*)ots)->VpiSigned();
                    else if (ott == UHDM_OBJECT_TYPE::uhdminteger_typespec)
                      oper_signed = ((const integer_typespec*)ots)->VpiSigned();
                    else if (ott == UHDM_OBJECT_TYPE::uhdmbyte_typespec)
                      oper_signed = ((const byte_typespec*)ots)->VpiSigned();
                    else if (ott == UHDM_OBJECT_TYPE::uhdmshort_int_typespec)
                      oper_signed = ((const short_int_typespec*)ots)->VpiSigned();
                    else if (ott == UHDM_OBJECT_TYPE::uhdmlong_int_typespec)
                      oper_signed = ((const long_int_typespec*)ots)->VpiSigned();
                  }
                }
              }
              constant *c = s.MakeConstant();
              // `1ULL << 64` is undefined (x86 yields 1, so the mask became
              // 0): a 64-bit-or-wider size cast `64'(X)` folded to ZERO
              // (OpenTitan hmac_core: `localparam bit [63:0] BlockSizeSHA256in64
              // = 64'(BlockSizeSHA256)` and every constant derived from it).
              uint64_t mask = (cast_to >= 64) ? ~0ULL
                                              : ((uint64_t)(1ULL << cast_to)) - 1ULL;
              resize(oper, cast_to);
              val0 = get_value(invalidValue, oper);
              uint64_t res = val0 & mask;
              if (oper_signed) {
                int64_t sres = (int64_t)res;
                if (cast_to < 64) {
                  uint64_t sign = 1ULL << (cast_to - 1);
                  if (res & sign) sres = (int64_t)(res | ~mask);
                }
                c->VpiValue("INT:" + std::to_string(sres));
                c->VpiConstType(vpiIntConst);
                // Propagate the signed marker so a chained outer cast
                // (`8'(4'(...))`) still sees its operand as signed.
                logic_typespec *lt = s.MakeLogic_typespec();
                lt->VpiSigned(true);
                ref_typespec *rt = s.MakeRef_typespec();
                rt->VpiParent(c);
                rt->Actual_typespec(lt);
                c->Typespec(rt);
              } else {
                c->VpiValue("UINT:" + std::to_string(res));
                c->VpiConstType(vpiUIntConst);
              }
              c->VpiSize(static_cast<int32_t>(cast_to));
              result = c;
            } else if (ttps == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
              // TODO: Should check the value is in range of the enum and
              // issue error if not
              constant *c = s.MakeConstant();
              c->VpiValue("UINT:" + std::to_string(val0));
              c->VpiSize(64);
              c->VpiConstType(vpiUIntConst);
              result = c;
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
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmconstant) {
    return (expr *)result;
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmsys_func_call) {
    sys_func_call *scall = (sys_func_call *)result;
    const std::string_view name = scall->VpiName();
    // Per SV LRM §20.7, `$size(arr [, dim])` returns the size of the
    // numbered dimension (1 = outermost).  The old handler routed
    // `$size` through the generic `size()` function which (a) added
    // the dim-index argument's bit-width to the running total, (b)
    // applied the LAST range only (`ranges->back()`) instead of the
    // requested one, and (c) had no `array_net` case at all — so
    // `$size(z)` / `$size(z, N)` on an unpacked array net returned
    // wrong values.  Handle `$size` independently before the generic
    // path.
    if (name == "$size") {
      // Collect all dimensions (outer → inner: unpacked first, packed
      // last) of the first call argument.  `strip_outer` counts
      // bit_select / var_select indices we still need to strip from
      // the front of the dim list.
      struct DimEntry { int64_t left, right; };
      std::vector<DimEntry> alldims;
      int strip_outer = 0;
      bool dim_ok = true;

      std::function<void(const any *)> walk_ts;
      std::function<void(const any *)> walk_obj;

      walk_ts = [&](const any *ts) {
        if (!ts) return;
        UHDM_OBJECT_TYPE tt = ts->UhdmType();
        if (tt == uhdmref_typespec) {
          ts = ((ref_typespec *)ts)->Actual_typespec();
          if (!ts) return;
          tt = ts->UhdmType();
        }
        VectorOfrange *rs = nullptr;
        const any *elem_ts = nullptr;
        if (tt == uhdmlogic_typespec) {
          auto *t = (logic_typespec *)ts;
          rs = t->Ranges();
          if (t->Elem_typespec()) elem_ts = t->Elem_typespec()->Actual_typespec();
        } else if (tt == uhdmbit_typespec) {
          rs = ((bit_typespec *)ts)->Ranges();
        } else if (tt == uhdmint_typespec) {
          rs = ((int_typespec *)ts)->Ranges();
        } else if (tt == uhdmpacked_array_typespec) {
          auto *t = (packed_array_typespec *)ts;
          rs = t->Ranges();
          if (t->Elem_typespec()) elem_ts = t->Elem_typespec()->Actual_typespec();
        } else if (tt == uhdmarray_typespec) {
          auto *t = (array_typespec *)ts;
          rs = t->Ranges();
          if (t->Elem_typespec()) elem_ts = t->Elem_typespec()->Actual_typespec();
        }
        if (rs) {
          for (range *r : *rs) {
            bool inv = false;
            int64_t lv = getValue(reduceExpr(r->Left_expr(), inv, inst, pexpr, muteError));
            int64_t rv = getValue(reduceExpr(r->Right_expr(), inv, inst, pexpr, muteError));
            if (inv) { dim_ok = false; return; }
            alldims.push_back({lv, rv});
          }
        }
        if (elem_ts) walk_ts(elem_ts);
      };

      walk_obj = [&](const any *obj) {
        if (!obj) { dim_ok = false; return; }
        UHDM_OBJECT_TYPE ot = obj->UhdmType();
        if (ot == uhdmarray_net) {
          auto *an = (array_net *)obj;
          if (an->Ranges()) {
            for (range *r : *an->Ranges()) {
              bool inv = false;
              int64_t lv = getValue(reduceExpr(r->Left_expr(), inv, inst, pexpr, muteError));
              int64_t rv = getValue(reduceExpr(r->Right_expr(), inv, inst, pexpr, muteError));
              if (inv) { dim_ok = false; return; }
              alldims.push_back({lv, rv});
            }
          }
          if (an->Nets() && !an->Nets()->empty()) {
            auto *n = (*an->Nets())[0];
            if (n->Typespec()) walk_ts(n->Typespec()->Actual_typespec());
          }
        } else if (auto *lvar = any_cast<logic_var *>(obj)) {
          if (lvar->Typespec()) walk_ts(lvar->Typespec()->Actual_typespec());
        } else if (auto *lnet = any_cast<logic_net *>(obj)) {
          if (lnet->Typespec()) walk_ts(lnet->Typespec()->Actual_typespec());
        } else if (auto *var = any_cast<array_var *>(obj)) {
          if (var->Ranges()) {
            for (range *r : *var->Ranges()) {
              bool inv = false;
              int64_t lv = getValue(reduceExpr(r->Left_expr(), inv, inst, pexpr, muteError));
              int64_t rv = getValue(reduceExpr(r->Right_expr(), inv, inst, pexpr, muteError));
              if (inv) { dim_ok = false; return; }
              alldims.push_back({lv, rv});
            }
          }
          if (var->Variables() && !var->Variables()->empty()) {
            variables *v = (*var->Variables())[0];
            if (v->Typespec()) walk_ts(v->Typespec()->Actual_typespec());
          }
        }
      };

      if (scall->Tf_call_args() && !scall->Tf_call_args()->empty()) {
        any *arg0 = scall->Tf_call_args()->at(0);
        UHDM_OBJECT_TYPE at = arg0->UhdmType();
        // bit_select / var_select strip outer dims by Exprs().size().
        if (at == uhdmbit_select) {
          ++strip_outer;
          auto *bs = (bit_select *)arg0;
          std::string_view bname = bs->VpiName();
          any *obj = getObject(bname, inst, pexpr, muteError);
          walk_obj(obj);
        } else if (at == uhdmvar_select) {
          auto *vs = (var_select *)arg0;
          if (vs->Exprs()) strip_outer = (int)vs->Exprs()->size();
          std::string_view bname = vs->VpiName();
          any *obj = getObject(bname, inst, pexpr, muteError);
          walk_obj(obj);
        } else if (at == uhdmref_obj) {
          auto *ref = (ref_obj *)arg0;
          std::string_view objname = ref->VpiName();
          any *obj = getObject(objname, inst, pexpr, muteError);
          if (obj == nullptr) {
            // Fall back to typespec on the ref itself.
            if (ref->Typespec()) walk_ts(ref->Typespec()->Actual_typespec());
          } else {
            walk_obj(obj);
          }
        } else {
          dim_ok = false;
        }
      } else {
        dim_ok = false;
      }

      // Determine the dim index from the optional 2nd arg.
      int64_t dim_index = 1;
      if (dim_ok && scall->Tf_call_args()->size() >= 2) {
        bool inv = false;
        dim_index = getValue(
            reduceExpr(scall->Tf_call_args()->at(1), inv, inst, pexpr, muteError));
        if (inv) dim_ok = false;
      }

      if (dim_ok) {
        // Apply outer-dim stripping from bit/var-selects.
        while (strip_outer > 0 && !alldims.empty()) {
          alldims.erase(alldims.begin());
          --strip_outer;
        }
        if (dim_index >= 1 && dim_index <= (int64_t)alldims.size()) {
          const DimEntry &d = alldims[dim_index - 1];
          uint64_t dsize = (d.left > d.right)
                               ? (uint64_t)(d.left - d.right) + 1
                               : (uint64_t)(d.right - d.left) + 1;
          constant *c = s.MakeConstant();
          c->VpiValue("UINT:" + std::to_string(dsize));
          c->VpiDecompile(std::to_string(dsize));
          c->VpiSize(64);
          c->VpiConstType(vpiUIntConst);
          return c;
        }
      }
      // Fall through to generic handler below if our walker couldn't
      // resolve the dim — preserves behavior for cases we don't cover
      // (struct fields, function-call args, etc).
    }
    if ((name == "$bits") || (name == "$size") || (name == "$high") ||
        (name == "$low") || (name == "$left") || (name == "$right")) {
      uint64_t bits = 0;
      bool found = false;
      for (auto arg : *scall->Tf_call_args()) {
        UHDM_OBJECT_TYPE argtype = arg->UhdmType();
        if (argtype == UHDM_OBJECT_TYPE::uhdmref_obj) {
          ref_obj *ref = (ref_obj *)arg;
          const std::string_view objname = ref->VpiName();
          any *object = getObject(objname, inst, pexpr, muteError);
          if (object == nullptr) {
            if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmpackage) {
              std::string name(inst->VpiName());
              name.append("::").append(objname);
              object = getObject(name, inst, pexpr, muteError);
            }
          }
          if (object) {
            if (param_assign *passign = any_cast<param_assign *>(object)) {
              object = passign->Rhs();
            }
          }
          if (object == nullptr) {
            object = getValue(objname, inst, pexpr, muteError);
          }
          const typespec *tps = nullptr;
          if (any_cast<array_var *>(object)) {
            // Size the object, not its typespec
          } else if (expr *exp = any_cast<expr *>(object)) {
            if (ref_typespec *rt = exp->Typespec()) {
              tps = rt->Actual_typespec();
            }
          } else if (typespec *tp = any_cast<typespec *>(object)) {
            tps = tp;
          }

          if ((name == "$high") || (name == "$low") || (name == "$left") ||
              (name == "$right")) {
            VectorOfrange *ranges = nullptr;
            if (tps) {
              switch (tps->UhdmType()) {
                case uhdmbit_typespec: {
                  bit_typespec *bts = (bit_typespec *)tps;
                  ranges = bts->Ranges();
                  break;
                }
                case uhdmint_typespec: {
                  int_typespec *bts = (int_typespec *)tps;
                  ranges = bts->Ranges();
                  break;
                }
                case uhdmlogic_typespec: {
                  logic_typespec *bts = (logic_typespec *)tps;
                  ranges = bts->Ranges();
                  break;
                }
                case uhdmarray_typespec: {
                  array_typespec *bts = (array_typespec *)tps;
                  ranges = bts->Ranges();
                  break;
                }
                case uhdmpacked_array_typespec: {
                  packed_array_typespec *bts = (packed_array_typespec *)tps;
                  ranges = bts->Ranges();
                  break;
                }
                default:
                  break;
              }
            }
            if (ranges) {
              range *r = ranges->at(0);
              expr *lr = r->Left_expr();
              expr *rr = r->Right_expr();
              bool invalidValue = false;
              lr = reduceExpr(lr, invalidValue, inst, pexpr, muteError);
              UHDM::ExprEval eval;
              int64_t lrv = eval.get_value(invalidValue, lr);
              rr = reduceExpr(rr, invalidValue, inst, pexpr, muteError);
              int64_t rrv = eval.get_value(invalidValue, rr);
              if (name == "$left") {
                return lr;
              } else if (name == "$right") {
                return rr;
              } else if (name == "$high") {
                if (lrv > rrv) {
                  return lr;
                } else {
                  return rr;
                }
              } else if (name == "$low") {
                if (lrv > rrv) {
                  return rr;
                } else {
                  return lr;
                }
              }
            }
          }

          if (tps) {
            bits += size(tps, invalidValue, inst, pexpr, (name != "$size"));
            found = true;
          } else {
            if (object) {
              bits +=
                  size(object, invalidValue, inst, pexpr, (name != "$size"));
              found = true;
            } else {
              invalidValue = true;
            }
          }
        } else if (argtype == UHDM_OBJECT_TYPE::uhdmoperation) {
          operation *oper = (operation *)arg;
          if (oper->VpiOpType() == vpiConcatOp) {
            for (auto op : *oper->Operands()) {
              bits += size(op, invalidValue, inst, pexpr, (name != "$size"));
            }
            found = true;
          }
        } else if (argtype == UHDM_OBJECT_TYPE::uhdmhier_path) {
          hier_path *path = (hier_path *)arg;
          auto elems = path->Path_elems();
          if (elems && (elems->size() > 1)) {
            const std::string_view base = elems->at(0)->VpiName();
            const std::string_view suffix = elems->at(1)->VpiName();
            any *var = getObject(base, inst, pexpr, muteError);
            if (var) {
              if (param_assign *passign = any_cast<param_assign *>(var)) {
                var = passign->Rhs();
              }
            }
            if (const port *p = any_cast<port>(var)) {
              if (const ref_typespec *prt = p->Typespec()) {
                if (const struct_typespec *tpss =
                        prt->Actual_typespec<struct_typespec>()) {
                  for (typespec_member *memb : *tpss->Members()) {
                    if (memb->VpiName() == suffix) {
                      if (const ref_typespec *rom = memb->Typespec()) {
                        bits += size(rom->Actual_typespec(), invalidValue, inst,
                                     pexpr, (name != "$size"));
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
      if (found) {
        constant *c = s.MakeConstant();
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
          constant *c = s.MakeConstant();
          c->VpiValue("UINT:" + std::to_string(clog2));
          c->VpiDecompile(std::to_string(clog2));
          c->VpiSize(64);
          c->VpiConstType(vpiUIntConst);
          result = c;
        }
      }
    } else if (name == "$signed" || name == "$unsigned") {
      if (scall->Tf_call_args()) {
        const typespec *optps = nullptr;
        if (const ref_typespec *rt = scall->Typespec()) {
          optps = rt->Actual_typespec();
        }
        for (auto arg : *scall->Tf_call_args()) {
          bool invalidTmpValue = false;
          expr *val = reduceExpr(arg, invalidTmpValue, inst, pexpr, muteError);
          if (val && (val->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) &&
              !invalidTmpValue) {
            constant *c = (constant *)val;
            if (c->VpiConstType() == vpiIntConst ||
                c->VpiConstType() == vpiDecConst) {
              int64_t value = get_value(invalidValue, val);
              int64_t size = c->VpiSize();
              if (name == "$signed") {
                // Attach a signed typespec hint so downstream code (in
                // particular the size-cast handler) can distinguish a
                // genuinely-signed value from an incidental vpiIntConst
                // produced by arithmetic over unsigned operands.
                if (!c->Typespec()) {
                  logic_typespec *lt = s.MakeLogic_typespec();
                  lt->VpiSigned(true);
                  ref_typespec *rt = s.MakeRef_typespec();
                  rt->VpiParent(c);
                  rt->Actual_typespec(lt);
                  c->Typespec(rt);
                }
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
                  constant *c = s.MakeConstant();
                  c->VpiValue("UINT:" + std::to_string(res));
                  c->VpiDecompile(std::to_string(res));
                  c->VpiSize(static_cast<int32_t>(size));
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
                if (optps) {
                  uint32_t bits =
                      ExprEval::size(optps, invalidValue, inst, pexpr, false);
                  bool is_signed = false;
                  if (optps->UhdmType() ==
                      UHDM_OBJECT_TYPE::uhdmlogic_typespec) {
                    logic_typespec *ltps = (logic_typespec *)optps;
                    is_signed = ltps->VpiSigned();
                  }
                  if (!is_signed) {
                    if ((size >= 0) && (bits > size)) {
                      for (uint32_t i = (uint32_t)size; i < bits; i++) {
                        res |= 1ULL << i;
                      }
                    }
                  } else {
                    uint32_t half = (2 << (size - 2));
                    if (res >= half) {
                      res = (-(2 << (size - 1))) + res;
                    }
                  }
                } else {
                  if (negsign) {
                    res &= ~(1ULL << (size - 1));
                    res = -res;
                  }
                }
                constant *c = s.MakeConstant();
                c->VpiValue("INT:" + std::to_string(res));
                c->VpiDecompile(std::to_string(res));
                c->VpiSize(static_cast<int32_t>(size));
                c->VpiConstType(vpiIntConst);
                // Attach signed typespec hint (see comment above).
                logic_typespec *lt = s.MakeLogic_typespec();
                lt->VpiSigned(true);
                ref_typespec *rt = s.MakeRef_typespec();
                rt->VpiParent(c);
                rt->Actual_typespec(lt);
                c->Typespec(rt);
                result = c;
              } else {
                result = c;
              }
            }
          }
        }
      }
    }
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmfunc_call) {
    func_call *scall = (func_call *)result;
    const std::string_view name = scall->VpiName();
    std::vector<any *> *args = scall->Tf_call_args();
    function *actual_func = nullptr;
    if (task_func *func = getTaskFunc(name, inst)) {
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
    if (expr *tmp = evalFunc(actual_func, args, invalidValue, inst,
                             (any *)pexpr, muteError)) {
      if (!invalidValue) result = tmp;
    }
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmref_obj) {
    ref_obj *ref = (ref_obj *)result;
    const std::string_view name = ref->VpiName();
    if (any *tmp = getValue(name, inst, pexpr, muteError)) {
      result = tmp;
    } else if (const any *actual = ref->Actual_group()) {
      // Fallback: use the elaborated Actual pointer to resolve the value
      // This handles parameters/localparams in generate scopes that
      // can't be found by name lookup alone
      if (const parameter *param = any_cast<const parameter *>(actual)) {
        std::string_view val = param->VpiValue();
        if (!val.empty()) {
          bool tmpInvalid = false;
          get_value(tmpInvalid, (const expr *)param);
          if (!tmpInvalid) {
            constant *c = s.MakeConstant();
            c->VpiValue(std::string(val));
            c->VpiDecompile(param->VpiDecompile());
            c->VpiSize(param->VpiSize());
            c->VpiConstType(param->VpiConstType());
            result = c;
          }
        }
      }
    }
    return (expr *)result;
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmhier_path) {
    hier_path *path = (hier_path *)result;
    return (expr *)decodeHierPath(path, invalidValue, inst, pexpr, ReturnType::VALUE);
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmstruct_var) {
    // A struct value computed by a constant function keeps its member values
    // as typespec_member Actual_value annotations (evalStmt's hier_path LHS
    // branch writes them member by member).  Assemble the packed constant
    // from those annotations so the struct_var reduces like any other
    // constant.  Without this, a parameter override whose actual is such a
    // value (`child #(.Cfg(Cfg))` with Cfg built by a struct-returning
    // function chain) never gets a usable value: the instance parameter is
    // stamped 0 and every member fold of it reads 0 — CVA6's
    // hpdcache_ctrl_pe read HPDcacheCfg.u.lowLatency as 0 that way.
    const struct_var *sv = (const struct_var *)result;
    std::function<bool(const struct_typespec *, std::string &)> assemble =
        [&](const struct_typespec *stps, std::string &bits) -> bool {
      if (!stps || !stps->Members()) return false;
      for (typespec_member *member : *stps->Members()) {
        const typespec *mts = member->Typespec()
                                  ? member->Typespec()->Actual_typespec()
                                  : nullptr;
        bool tmpInvalid = false;
        uint64_t mw = size(mts, tmpInvalid, inst, pexpr, true, muteError);
        if ((tmpInvalid || mw == 0) && mts &&
            mts->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
          // An ENUM member's size can come back 0 here; its packed width is
          // the base typespec's (default int = 32 when unspecified).
          const enum_typespec *ets = (const enum_typespec *)mts;
          tmpInvalid = false;
          mw = 0;
          if (ets->Base_typespec() && ets->Base_typespec()->Actual_typespec())
            mw = size(ets->Base_typespec()->Actual_typespec(), tmpInvalid,
                      inst, pexpr, true, muteError);
          if (tmpInvalid || mw == 0) {
            tmpInvalid = false;
            mw = 32;
          }
        }
        if (tmpInvalid || mw == 0) {
          return false;
        }
        const any *mv = member->Actual_value();
        if (!mv) mv = member->Default_value();
        if (!mv) {
          return false;
        }
        if (mv->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_var) {
          const struct_typespec *nstps = nullptr;
          if (const ref_typespec *nrt = ((const struct_var *)mv)->Typespec())
            nstps = nrt->Actual_typespec<struct_typespec>();
          if (!nstps && mts &&
              mts->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_typespec)
            nstps = (const struct_typespec *)mts;
          std::string sub;
          if (!assemble(nstps, sub)) return false;
          if (sub.size() > mw)
            sub = sub.substr(sub.size() - mw);
          else
            while (sub.size() < mw) sub.insert(sub.begin(), '0');
          bits += sub;
        } else {
          bool iv = false;
          expr *rme =
              reduceExpr((any *)mv, iv, inst, pexpr, muteError);
          std::string b;
          if (!iv && rme &&
              rme->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
            // Convert through the binary string, not a 64-bit integer —
            // config structs carry members far wider than 64 bits (CVA6's
            // region address/length arrays).
            b = toBinary((constant *)rme);
          } else {
            // A packed-array member value kept as an ASSIGNMENT PATTERN or
            // CONCAT of constants (`NonIdempotentAddrBase = '{...}`):
            // reduce each element and concatenate, first element = MSBs.
            const operation *op = any_cast<const operation *>(mv);
            if (!op && rme) op = any_cast<const operation *>(rme);
            int32_t optype = op ? op->VpiOpType() : 0;
            if (op && op->Operands() && op->Operands()->size() == 1 &&
                (optype == vpiMultiAssignmentPatternOp ||
                 optype == vpiAssignmentPatternOp || optype == vpiCastOp)) {
              // A CAST or pattern already folded to ONE constant narrower
              // than the member (CVA6's NonIdempotentAddrBase carries a
              // 128-bit cast constant for a [15:0][63:0] field): SV
              // assignment widening zero-extends — the generic left-pad
              // below applies it.
              bool eiv = false;
              expr *re = reduceExpr((*op->Operands())[0], eiv, inst, pexpr,
                                    muteError);
              if (eiv || !re ||
                  re->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant) {
                return false;
              }
              b = toBinary((constant *)re);
            } else if (op && op->Operands() && op->Operands()->size() == 2 &&
                (optype == vpiMultiAssignmentPatternOp ||
                 optype == vpiMultiConcatOp)) {
              // Replication (`'{N{value}}` / `{N{value}}`): repeat the
              // element's bits N times.
              bool civ = false;
              int64_t cnt = get_value(
                  civ, reduceExpr((*op->Operands())[0], civ, inst, pexpr,
                                  muteError));
              bool eiv = false;
              expr *re = reduceExpr((*op->Operands())[1], eiv, inst, pexpr,
                                    muteError);
              if (civ || eiv || cnt <= 0 || !re ||
                  re->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant ||
                  (mw % (uint64_t)cnt) != 0) {
                return false;
              }
              uint64_t ew = mw / (uint64_t)cnt;
              std::string eb = toBinary((constant *)re);
              if (eb.size() > ew)
                eb = eb.substr(eb.size() - ew);
              else
                while (eb.size() < ew) eb.insert(eb.begin(), '0');
              for (int64_t r = 0; r < cnt; r++) b += eb;
            } else if (op && op->Operands() && !op->Operands()->empty() &&
                (optype == vpiAssignmentPatternOp || optype == vpiConcatOp)) {
              size_t n = op->Operands()->size();
              if (mw % n) {
                return false;
              }
              uint64_t ew = mw / n;
              bool ok = true;
              for (auto operand : *op->Operands()) {
                bool eiv = false;
                expr *re = reduceExpr(operand, eiv, inst, pexpr, muteError);
                if (eiv || !re ||
                    re->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant) {
                  ok = false;
                  break;
                }
                std::string eb = toBinary((constant *)re);
                if (eb.size() > ew)
                  eb = eb.substr(eb.size() - ew);
                else
                  while (eb.size() < ew) eb.insert(eb.begin(), '0');
                b += eb;
              }
              if (!ok) {
                return false;
              }
            } else {
              return false;
            }
          }
          if (b.size() > mw)
            b = b.substr(b.size() - mw);
          else
            while (b.size() < mw) b.insert(b.begin(), '0');
          for (char bch : b)
            if (bch != '0' && bch != '1') {
              return false;
            }
          bits += b;
        }
      }
      return true;
    };
    const struct_typespec *stps = nullptr;
    if (const ref_typespec *rt = sv->Typespec())
      stps = rt->Actual_typespec<struct_typespec>();
    std::string bits;
    if (assemble(stps, bits) && !bits.empty()) {
      constant *c = s.MakeConstant();
      c->VpiValue("BIN:" + bits);
      c->VpiDecompile(bits);
      c->VpiSize(static_cast<int32_t>(bits.size()));
      c->VpiConstType(vpiBinaryConst);
      ref_typespec *rtc = s.MakeRef_typespec();
      rtc->Actual_typespec(const_cast<struct_typespec *>(stps));
      rtc->VpiParent(c);
      c->Typespec(rtc);
      return c;
    }
    return (expr *)result;
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmbit_select) {
    bit_select *sel = (bit_select *)result;
    const std::string_view name = sel->VpiName();
    const expr *index = sel->VpiIndex();
    uint64_t index_val = get_value(
        invalidValue,
        reduceExpr((expr *)index, invalidValue, inst, pexpr, muteError));
    if (invalidValue == false) {
      any *object = getObject(name, inst, pexpr, muteError);
      // Resolve the base's DECLARED typespec via the host functor.  When the
      // base is a packed-array parameter (`logic [N-1:0][W-1:0] P`), getObject
      // hands back P's already-folded value CONSTANT — which no longer carries
      // the packed-array dimensions — so a later element-select loses the
      // geometry and degrades to a single-bit select (OpenTitan prim_count's
      // `ResetValues[k]` returned 1 bit instead of the W-bit element).  Grab the
      // declared typespec by name and pass it to reducePackedElemSelect below as
      // cts_fallback so it can still find the ranges.
      const typespec *bsel_param_ts = nullptr;
      if (getTypespecFunctor) {
        if (any *tsobj = getTypespecFunctor(name, inst, pexpr))
          bsel_param_ts = any_cast<const typespec *>(tsobj);
      }
      if (object) {
        if (param_assign *passign = any_cast<param_assign *>(object)) {
          object = (any *)passign->Rhs();
        }
      }
      // A function FORMAL (io_decl) shadows its staged argument value in the
      // pexpr task_func chain; the formal carries no value, so resolve the
      // name through the value store instead (fpnew's
      // `cfg[fmt]` inside get_conv_lane_formats).  A function LOCAL's var
      // declaration shadows the same way (`lanefmts[fmt]` inside
      // get_conv_lane_int_formats after `lanefmts = get_conv_lane_formats(...)`
      // — the logic_var won the lookup and the whole RHS went invalid).
      if (object && (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmio_decl ||
                     any_cast<variables *>(object) != nullptr)) {
        if (any *valobj = getValue(name, inst, pexpr, muteError))
          object = valobj;
      }
      if (object == nullptr) {
        object = getValue(name, inst, pexpr, muteError);
      }
      if (object && (object != result)) {
        if (expr *tmp = reduceExpr((expr *)object, invalidValue, inst, pexpr,
                                   muteError)) {
          object = tmp;
        }
        UHDM_OBJECT_TYPE otype = object->UhdmType();
        if (otype == UHDM_OBJECT_TYPE::uhdmpacked_array_var) {
          packed_array_var *array = (packed_array_var *)object;
          VectorOfany *elems = array->Elements();
          if (elems && index_val < elems->size()) {
            any *elem = elems->at(index_val);
            if (elem->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var ||
                elem->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_var ||
                elem->UhdmType() == UHDM_OBJECT_TYPE::uhdmunion_var ||
                elem->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_var) {
            } else {
              result = elems->at(index_val);
            }
          }
        } else if (otype == UHDM_OBJECT_TYPE::uhdmarray_expr) {
          array_expr *array = (array_expr *)object;
          VectorOfexpr *elems = array->Exprs();
          if (index_val < elems->size()) {
            result = elems->at(index_val);
          }
        } else if (otype == UHDM_OBJECT_TYPE::uhdmoperation) {
          operation *op = (operation *)object;
          int32_t opType = op->VpiOpType();
          if (opType == vpiAssignmentPatternOp) {
            VectorOfany *ops = op->Operands();
            if (ops && (index_val < ops->size())) {
              result = ops->at(index_val);
              if (result->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
                if (const ref_typespec *oprt = op->Typespec()) {
                  if (const array_typespec *atps =
                          oprt->Actual_typespec<array_typespec>()) {
                    if (const ref_typespec *ert = atps->Elem_typespec()) {
                      if (const typespec *ertts = ert->Actual_typespec()) {
                        ElaboratorContext elaboratorContext(&s, false,
                                                            muteError);
                        ref_typespec *celrt =
                            (ref_typespec *)clone_tree(ert, &elaboratorContext);
                        celrt->Actual_typespec(const_cast<typespec *>(ertts));
                        celrt->VpiParent((any *)result);
                        ((operation *)result)->Typespec(celrt);
                      }
                    }
                  } else if (const packed_array_typespec *patps =
                                 oprt->Actual_typespec<
                                     packed_array_typespec>()) {
                    if (const ref_typespec *ert = patps->Elem_typespec()) {
                      if (const typespec *ertts = ert->Actual_typespec()) {
                        ElaboratorContext elaboratorContext(&s, false,
                                                            muteError);
                        ref_typespec *celrt =
                            (ref_typespec *)clone_tree(ert, &elaboratorContext);
                        celrt->Actual_typespec(const_cast<typespec *>(ertts));
                        celrt->VpiParent((any *)result);
                        ((operation *)result)->Typespec(celrt);
                      }
                    }
                  }
                }
              }
            } else if (ops) {
              bool defaultTaggedPattern = false;
              for (auto op : *ops) {
                if (op->UhdmType() == UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
                  tagged_pattern *tp = (tagged_pattern *)op;
                  if (const ref_typespec *rt = tp->Typespec()) {
                    if (const typespec *tps = rt->Actual_typespec()) {
                      if (tps->VpiName() == "default") {
                        defaultTaggedPattern = true;
                        break;
                      }
                    }
                  }
                }
              }
              if (!defaultTaggedPattern) invalidValue = true;
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConcatOp) {
            VectorOfany *ops = op->Operands();
            if (ops && (index_val < ops->size())) {
              result = ops->at(index_val);
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConditionOp) {
            expr *exp = reduceExpr(op, invalidValue, inst, pexpr, muteError);
            UHDM_OBJECT_TYPE otype = exp->UhdmType();
            if (otype == UHDM_OBJECT_TYPE::uhdmoperation) {
              operation *op = (operation *)exp;
              int32_t opType = op->VpiOpType();
              if (opType == vpiAssignmentPatternOp) {
                VectorOfany *ops = op->Operands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                } else {
                  invalidValue = true;
                }
              } else if (opType == vpiConcatOp) {
                VectorOfany *ops = op->Operands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                } else {
                  invalidValue = true;
                }
              }
            }
            if (object) result = object;
          } else if (opType == vpiMultiConcatOp) {
            result = reduceBitSelect(op, static_cast<uint32_t>(index_val),
                                     invalidValue, inst, pexpr);
          }
        } else if (otype == UHDM_OBJECT_TYPE::uhdmconstant) {
          // Element select on a packed-array-typed constant first (see
          // reducePackedElemSelect) — bit select only for scalar vectors.
          if (constant *ec = reducePackedElemSelect(
                  (constant *)object, (int64_t)index_val, this, s,
                  invalidValue, inst, pexpr, muteError, bsel_param_ts)) {
            result = ec;
                  } else {
            result = reduceBitSelect((constant *)object,
                                     static_cast<uint32_t>(index_val),
                                     invalidValue, inst, pexpr);
                  }
        } else {
              }
      }
    }
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmpart_select) {
    part_select *sel = (part_select *)result;
    std::string_view name = sel->VpiName();
    if (name.empty()) name = sel->VpiDefName();
    any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (param_assign *passign = any_cast<param_assign *>(object)) {
        object = passign->Rhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    if (object && (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant)) {
      constant *co = (constant *)object;
      std::string binary = toBinary(co);
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
      constant *c = s.MakeConstant();
      c->VpiValue("BIN:" + sub);
      c->VpiDecompile(sub);
      c->VpiSize(static_cast<int32_t>(sub.size()));
      c->VpiConstType(vpiBinaryConst);
      result = c;
    }
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmindexed_part_select) {
    indexed_part_select *sel = (indexed_part_select *)result;
    std::string_view name = sel->VpiName();
    if (name.empty()) name = sel->VpiDefName();
    any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (param_assign *passign = any_cast<param_assign *>(object)) {
        object = (any *)passign->Rhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    if (object && (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant)) {
      constant *co = (constant *)object;
      std::string binary = toBinary(co);
      int64_t base = get_value(
          invalidValue,
          reduceExpr(sel->Base_expr(), invalidValue, inst, pexpr, muteError));
      int64_t offset = get_value(
          invalidValue,
          reduceExpr(sel->Width_expr(), invalidValue, inst, pexpr, muteError));
      std::reverse(binary.begin(), binary.end());
      std::string sub;
      if (sel->VpiIndexedPartSelectType() == vpiPosIndexed) {
        if ((uint32_t)(base + offset) <= binary.size())
          sub = binary.substr(base, offset);
      } else {
        if ((uint32_t)base < binary.size())
          sub = binary.substr(base - offset, offset);
      }
      std::reverse(sub.begin(), sub.end());
      constant *c = s.MakeConstant();
      c->VpiValue("BIN:" + sub);
      c->VpiDecompile(sub);
      c->VpiSize(static_cast<int32_t>(sub.size()));
      c->VpiConstType(vpiBinaryConst);
      result = c;
    }
  } else if (objtype == UHDM_OBJECT_TYPE::uhdmvar_select) {
    var_select *sel = (var_select *)result;
    const std::string_view name = sel->VpiName();
    any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (param_assign *passign = any_cast<param_assign *>(object)) {
        object = passign->Rhs();
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
        if (otype == UHDM_OBJECT_TYPE::uhdmoperation) {
          operation *op = (operation *)object;
          int32_t opType = op->VpiOpType();
          if (opType == vpiAssignmentPatternOp) {
            VectorOfany *ops = op->Operands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
              selection = true;
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConcatOp) {
            VectorOfany *ops = op->Operands();
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
            if (otype == UHDM_OBJECT_TYPE::uhdmoperation) {
              operation *op = (operation *)exp;
              int32_t opType = op->VpiOpType();
              if (opType == vpiAssignmentPatternOp) {
                VectorOfany *ops = op->Operands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                  selection = true;
                } else {
                  invalidValue = true;
                }
              } else if (opType == vpiConcatOp) {
                VectorOfany *ops = op->Operands();
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
  if (result && result->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj) {
    bool invalidValueTmp = false;
    expr *tmp = reduceExpr(result, invalidValue, inst, pexpr, muteError);
    if (tmp && !invalidValueTmp) result = tmp;
  }
  return (expr *)result;
}


// Splice a member write's constant bits into the whole-value constant stored
// for the base variable (param_assigns keyed by base name): keeps `return
// res` and later whole-var reads in sync with `res.member = ...` writes —
// without this fpnew_pkg::super_format's accumulated members never reached
// the returned value and the paramod stamped 0.
static bool spliceMemberIntoWhole(ExprEval *ev, Serializer &s,
                                  const any *inst, hier_path *path,
                                  expr *rhsexp, bool muteError) {
  if (rhsexp == nullptr ||
      rhsexp->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant)
    return false;
  if (path->Path_elems() == nullptr || path->Path_elems()->size() < 2)
    return false;
  const std::string_view base = path->Path_elems()->at(0)->VpiName();
  VectorOfparam_assign *param_assigns = nullptr;
  if (const scope *spe = any_cast<const scope *>(inst))
    param_assigns = spe->Param_assigns();
  if (param_assigns == nullptr) return false;
  param_assign *base_pa = nullptr;
  for (param_assign *pa : *param_assigns)
    if (pa->Lhs() && pa->Lhs()->VpiName() == base) {
      base_pa = pa;
      break;
    }
  if (base_pa == nullptr) return false;
  constant *whole = any_cast<constant *>(base_pa->Rhs());
  if (whole == nullptr) return false;
  const struct_typespec *stps = nullptr;
  if (const ref_typespec *rt = whole->Typespec())
    stps = rt->Actual_typespec<struct_typespec>();
  if (stps == nullptr || stps->Members() == nullptr) return false;
  // Only single-level member paths are spliced here (res.exp_bits).
  if (path->Path_elems()->size() != 2) return false;
  const std::string_view mname = path->Path_elems()->at(1)->VpiName();
  uint64_t total = 0, off = 0, mw = 0;
  bool found = false;
  for (typespec_member *member : *stps->Members()) {
    const typespec *mts =
        member->Typespec() ? member->Typespec()->Actual_typespec() : nullptr;
    bool iv = false;
    uint64_t w = ev->size(mts, iv, inst, nullptr, true, true);
    if (iv || w == 0) return false;
    if (!found && member->VpiName() == mname) {
      found = true;
      mw = w;
    } else if (!found) {
      off += w;
    }
    total += w;
  }
  if (!found || mw == 0) return false;
  std::string bin = ev->toBinary(whole);
  if (bin.size() < total) bin.insert(bin.begin(), total - bin.size(), '0');
  else if (bin.size() > total) bin = bin.substr(bin.size() - total);
  std::string mbits = ev->toBinary(any_cast<constant *>(rhsexp));
  if (mbits.size() < mw) mbits.insert(mbits.begin(), mw - mbits.size(), '0');
  else if (mbits.size() > mw) mbits = mbits.substr(mbits.size() - mw);
  bin.replace(off, mw, mbits);
  constant *nc = s.MakeConstant();
  nc->VpiValue("BIN:" + bin);
  nc->VpiDecompile(bin);
  nc->VpiSize(static_cast<int32_t>(bin.size()));
  nc->VpiConstType(vpiBinaryConst);
  ref_typespec *rtc = s.MakeRef_typespec();
  rtc->Actual_typespec(const_cast<struct_typespec *>(stps));
  rtc->VpiParent(nc);
  nc->Typespec(rtc);
  base_pa->Rhs(nc);
  return true;
}

bool ExprEval::setValueInInstance(
    std::string_view lhs, any *lhsexp, expr *rhsexp, bool &invalidValue,
    Serializer &s, const any *inst, const any *scope_exp,
    std::map<std::string, const typespec *> &local_vars, int opType,
    bool muteError) {
  bool invalidValueI = false;
  bool invalidValueUI = false;
  bool invalidValueD = false;
  bool invalidValueB = false;
  bool opRhs = false;
  std::string_view lhsname = lhs;
  if (lhsname.empty()) lhsname = lhsexp->VpiName();
  rhsexp = reduceExpr(rhsexp, invalidValue, inst, nullptr, muteError);
  int64_t valI = get_value(invalidValueI, rhsexp);
  uint64_t valUI = get_uvalue(invalidValueUI, rhsexp);
  if (rhsexp && (rhsexp->UhdmType() == uhdmconstant)) {
    constant *t = (constant *)rhsexp;
    if (t->VpiConstType() != vpiBinaryConst) {
      invalidValueB = true;
    }
  }
  long double valD = 0;
  if (invalidValueI) {
    valD = get_double(invalidValueD, rhsexp);
  }
  uint64_t wordSize = 1;
  const std::string_view name = lhsexp->VpiName();
  if (any *object = getObject(name, inst, scope_exp, muteError)) {
    wordSize = getWordSize(any_cast<const expr *>(object), inst, scope_exp);
  }
  VectorOfparam_assign *param_assigns = nullptr;
  if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
  } else if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
    param_assigns = ((design *)inst)->Param_assigns();
    if (param_assigns == nullptr) {
      ((design *)inst)->Param_assigns(s.MakeParam_assignVec());
      param_assigns = ((design *)inst)->Param_assigns();
    }
  } else if (const scope *spe = any_cast<const scope *>(inst)) {
    param_assigns = spe->Param_assigns();
    if (param_assigns == nullptr) {
      const_cast<scope *>(spe)->Param_assigns(s.MakeParam_assignVec());
      param_assigns = spe->Param_assigns();
    }
  }
  if (invalidValueI && invalidValueD) {
    if (lhsexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path) {
      hier_path *path = (hier_path *)lhsexp;
      expr *object = (expr *)decodeHierPath(path, invalidValue, inst, lhsexp,
                                            ReturnType::MEMBER);
      if (object) {
        if (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmtypespec_member) {
          typespec_member *tmp = (typespec_member *)object;
          tmp->Actual_value(rhsexp);
          spliceMemberIntoWhole(this, s, inst, path, rhsexp, muteError);
          return false;
        }
      }
    }
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
      if (rhsexp &&
          ((rhsexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) ||
           (rhsexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_expr))) {
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
      constant *c = any_cast<constant *>(rhsexp);
      if (c == nullptr) {
        c = s.MakeConstant();
        c->VpiValue("INT:" + std::to_string(valI));
        c->VpiDecompile(std::to_string(valI));
        c->VpiSize(64);
        c->VpiConstType(vpiIntConst);
      }
      if (lhsexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
        for (VectorOfparam_assign::iterator itr = param_assigns->begin();
             itr != param_assigns->end(); itr++) {
          if ((*itr)->Lhs()->VpiName() == lhsname) {
            prevRhs = (*itr)->Rhs();
            param_assigns->erase(itr);
            break;
          }
        }
        operation *op = (operation *)lhsexp;
        if (op->VpiOpType() == vpiConcatOp) {
          std::string rhsbinary = toBinary(c);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          VectorOfany *operands = op->Operands();
          uint64_t accumul = 0;
          for (any *oper : *operands) {
            const std::string_view name = oper->VpiName();
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
            c->VpiSize(static_cast<int32_t>(part.size()));
            c->VpiConstType(vpiBinaryConst);
            setValueInInstance(name, oper, c, invalidValue, s, inst, lhsexp,
                               local_vars, vpiConcatOp, muteError);
            accumul = accumul + si;
          }
        }
      } else if (lhsexp->UhdmType() ==
                 UHDM_OBJECT_TYPE::uhdmindexed_part_select) {
        for (VectorOfparam_assign::iterator itr = param_assigns->begin();
             itr != param_assigns->end(); itr++) {
          if ((*itr)->Lhs()->VpiName() == lhsname) {
            prevRhs = (*itr)->Rhs();
            param_assigns->erase(itr);
            break;
          }
        }
        indexed_part_select *sel = (indexed_part_select *)lhsexp;
        const std::string_view name = lhsexp->VpiName();
        if (any *object = getObject(name, inst, scope_exp, muteError)) {
          std::string lhsbinary;
          const typespec *tps = nullptr;
          if (const expr *elhs = any_cast<const expr *>(object)) {
            if (const ref_typespec *rt = elhs->Typespec()) {
              tps = rt->Actual_typespec();
            }
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs &&
              prevRhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
            const constant *prev = (constant *)prevRhs;
            lhsbinary = toBinary(prev);
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          // Pad an undersized previous value to the declared width — a
          // caller-frame local with the same name (get_conv_lane_int_formats'
          // 4-bit `res` shadowing get_conv_lane_formats' 5-bit `res`) loads a
          // short string, the top windex write lands outside it and drops
          // silently (fpnew: the FP32 term of every CONV lane's int-format
          // mask, CONV_INT_FORMATS 1101 instead of 1111).
          while (lhsbinary.size() < si) lhsbinary += 'x';

          uint64_t base = get_uvalue(
              invalidValue, reduceExpr(sel->Base_expr(), invalidValue, inst,
                                       lhsexp, muteError));
          uint64_t offset = get_uvalue(
              invalidValue, reduceExpr(sel->Width_expr(), invalidValue, inst,
                                       lhsexp, muteError));
          std::string rhsbinary = toBinary(c);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          if (sel->VpiIndexedPartSelectType() == vpiPosIndexed) {
            int32_t index = 0;
            for (uint64_t i = base; i < base + offset; i++) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          } else {
            int32_t index = 0;
            for (uint64_t i = base; i > base - offset; i--) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.MakeConstant();
          c->VpiValue("BIN:" + lhsbinary);
          c->VpiDecompile(lhsbinary);
          c->VpiSize(static_cast<int32_t>(lhsbinary.size()));
          c->VpiConstType(vpiBinaryConst);
        }
      } else if (lhsexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmpart_select) {
        for (VectorOfparam_assign::iterator itr = param_assigns->begin();
             itr != param_assigns->end(); itr++) {
          if ((*itr)->Lhs()->VpiName() == lhsname) {
            prevRhs = (*itr)->Rhs();
            param_assigns->erase(itr);
            break;
          }
        }
        part_select *sel = (part_select *)lhsexp;
        const std::string_view name = lhsexp->VpiName();
        if (any *object = getObject(name, inst, scope_exp, muteError)) {
          std::string lhsbinary;
          const typespec *tps = nullptr;
          if (const expr *elhs = any_cast<const expr *>(object)) {
            if (const ref_typespec *rt = elhs->Typespec()) {
              tps = rt->Actual_typespec();
            }
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs &&
              prevRhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
            const constant *prev = (constant *)prevRhs;
            lhsbinary = toBinary(prev);
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          // Pad an undersized previous value to the declared width — a
          // caller-frame local with the same name (get_conv_lane_int_formats'
          // 4-bit `res` shadowing get_conv_lane_formats' 5-bit `res`) loads a
          // short string, the top windex write lands outside it and drops
          // silently (fpnew: the FP32 term of every CONV lane's int-format
          // mask, CONV_INT_FORMATS 1101 instead of 1111).
          while (lhsbinary.size() < si) lhsbinary += 'x';

          uint64_t left = get_uvalue(
              invalidValue, reduceExpr(sel->Left_range(), invalidValue, inst,
                                       lhsexp, muteError));
          uint64_t right = get_uvalue(
              invalidValue, reduceExpr(sel->Right_range(), invalidValue, inst,
                                       lhsexp, muteError));
          std::string rhsbinary = toBinary(c);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          if (left > right) {
            int32_t index = 0;
            for (uint64_t i = right; i <= left; i++) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          } else {
            int32_t index = 0;
            for (uint64_t i = left; i <= right; i++) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.MakeConstant();
          c->VpiValue("BIN:" + lhsbinary);
          c->VpiDecompile(lhsbinary);
          c->VpiSize(static_cast<int32_t>(lhsbinary.size()));
          c->VpiConstType(vpiBinaryConst);
        }
      } else if (lhsexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_select) {
        bit_select *sel = (bit_select *)lhsexp;
        uint64_t index = get_uvalue(
            invalidValue,
            reduceExpr(sel->VpiIndex(), invalidValue, inst, lhsexp, muteError));
        const std::string_view name = lhsexp->VpiName();
        if (any *object = getObject(name, inst, scope_exp, muteError)) {
          if (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmparam_assign) {
            param_assign *param = (param_assign *)object;
            if (param->Rhs()->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_expr) {
              array_expr *array = (array_expr *)param->Rhs();
              VectorOfexpr *values = array->Exprs();
              values->resize(index + 1);
              (*values)[index] = rhsexp;
              return false;
            }
          }

          for (VectorOfparam_assign::iterator itr = param_assigns->begin();
               itr != param_assigns->end(); itr++) {
            if ((*itr)->Lhs()->VpiName() == lhsname) {
              prevRhs = (*itr)->Rhs();
              param_assigns->erase(itr);
              break;
            }
          }
          std::string lhsbinary;
          const typespec *tps = nullptr;
          if (const expr *elhs = any_cast<const expr *>(object)) {
            if (const ref_typespec *rt = elhs->Typespec()) {
              tps = rt->Actual_typespec();
            }
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs &&
              prevRhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
            const constant *prev = (constant *)prevRhs;
            if (prev->VpiConstType() == vpiBinaryConst) {
              std::string_view val = prev->VpiValue();
              val.remove_prefix(std::string_view("BIN:").length());
              lhsbinary = val;
            } else {
              lhsbinary = NumUtils::toBinary(static_cast<int32_t>(si),
                                             get_uvalue(invalidValue, prev));
            }
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }

          // Pad an undersized previous value to the declared width — a
          // caller-frame local with the same name (get_conv_lane_int_formats'
          // 4-bit `res` shadowing get_conv_lane_formats' 5-bit `res`) loads a
          // short string, the top windex write lands outside it and drops
          // silently (fpnew: the FP32 term of every CONV lane's int-format
          // mask, CONV_INT_FORMATS 1101 instead of 1111).
          while (lhsbinary.size() < si) lhsbinary += 'x';

          int64_t size_rhs = ((constant *)rhsexp)->VpiSize();
          if ((wordSize != 1) && (((int64_t)wordSize) < size_rhs))
            size_rhs = wordSize;
          // Plain 1-D vector target: each bit-select writes exactly ONE
          // bit, positioned per the declared range (an ascending [0:N]
          // range puts index 0 at the MSB).  Without this, a folded RHS
          // (a UINT from `&&`/compare, VpiSize 64) landed only index 0
          // and spilled past the vector for every other index, and
          // ascending vectors wrote mirrored bit positions (fpnew's
          // `res[fmt] = cfg[fmt] && ...` in get_conv_lane_formats).
          uint64_t windex = index;
          {
            VectorOfrange *wrgs = nullptr;
            if (tps) {
              if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_typespec) {
                auto *lt = (const logic_typespec *)tps;
                if (!lt->Elem_typespec()) wrgs = lt->Ranges();
              } else if (tps->UhdmType() ==
                         UHDM_OBJECT_TYPE::uhdmbit_typespec) {
                wrgs = ((const bit_typespec *)tps)->Ranges();
              } else if (tps->UhdmType() ==
                         UHDM_OBJECT_TYPE::uhdmint_typespec) {
                wrgs = ((const int_typespec *)tps)->Ranges();
              }
            }
            if (wrgs && wrgs->size() == 1) {
              bool rinv = false;
              int64_t wl = get_value(
                  rinv, reduceExpr(wrgs->at(0)->Left_expr(), rinv, inst,
                                   lhsexp, muteError));
              int64_t wr = get_value(
                  rinv, reduceExpr(wrgs->at(0)->Right_expr(), rinv, inst,
                                   lhsexp, muteError));
              if (!rinv) {
                size_rhs = 1;
                int64_t wi = (wl < wr) ? (wr - (int64_t)index)
                                       : ((int64_t)index - wr);
                windex = (wi < 0) ? (uint64_t)si : (uint64_t)wi;
              }
            }
          }
          std::string tobinary = NumUtils::toBinary(size_rhs, valUI);
          std::reverse(tobinary.begin(), tobinary.end());
          for (int32_t i = 0; i < size_rhs; i++) {
            if ((((windex * size_rhs) + i) < si) &&
                (((windex * size_rhs) + i) < lhsbinary.size())) {
              char newc = tobinary[i];
              // Compound assignment (`res[i] |= term`): fold the previous
              // bit in — the plain overwrite made each accumulate-loop
              // iteration clobber the earlier ones (fpnew_pkg::
              // get_conv_lane_int_formats kept only its last iteration).
              char oldc = lhsbinary[(windex * size_rhs) + i];
              if (oldc == '0' || oldc == '1') {
                int ob = (oldc == '1');
                int nb = (newc == '1');
                switch (opType) {
                  case vpiBitOrOp:  newc = (ob | nb) ? '1' : '0'; break;
                  case vpiBitAndOp: newc = (ob & nb) ? '1' : '0'; break;
                  case vpiBitXorOp: newc = (ob ^ nb) ? '1' : '0'; break;
                  default: break;
                }
              }
              lhsbinary[(windex * size_rhs) + i] = newc;
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.MakeConstant();
          c->VpiValue("BIN:" + lhsbinary);
          c->VpiDecompile(lhsbinary);
          c->VpiSize(static_cast<int32_t>(lhsbinary.size()));
          c->VpiConstType(vpiBinaryConst);

          ref_typespec *rt = s.MakeRef_typespec();
          rt->Actual_typespec(const_cast<typespec *>(tps));
          rt->VpiParent(c);
          c->Typespec(rt);
        } else {
          std::map<std::string, const typespec *>::iterator itr =
              local_vars.find(std::string(lhs));
          if (itr != local_vars.end()) {
            if (const typespec *tps = itr->second) {
              if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
                param_assign *pa = s.MakeParam_assign();
                param_assigns->push_back(pa);
                array_expr *array = s.MakeArray_expr();
                VectorOfexpr *values = s.MakeExprVec();
                values->resize(index + 1);
                (*values)[index] = rhsexp;
                array->Exprs(values);
                pa->Rhs(array);
                parameter *param = s.MakeParameter();
                param->VpiName(lhsname);
                pa->Lhs(param);
                return false;
              }
            }
          }
        }
      } else if (lhsexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path) {
        hier_path *path = (hier_path *)lhsexp;
        expr *object = (expr *)decodeHierPath(path, invalidValue, inst, lhsexp,
                                              ReturnType::MEMBER);
        if (object) {
          if (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmtypespec_member) {
            typespec_member *tmp = (typespec_member *)object;
            tmp->Actual_value(rhsexp);
            spliceMemberIntoWhole(this, s, inst, path, rhsexp, muteError);
            return false;
          }
        }
      } else {
        for (VectorOfparam_assign::iterator itr = param_assigns->begin();
             itr != param_assigns->end(); itr++) {
          if ((*itr)->Lhs()->VpiName() == lhsname) {
            prevRhs = (*itr)->Rhs();
            param_assigns->erase(itr);
            break;
          }
        }
      }
      if (opType == vpiAddOp) {
        uint64_t prevVal = get_uvalue(invalidValue, (expr *)prevRhs);
        uint64_t newVal = valUI + prevVal;
        c->VpiValue("UINT:" + std::to_string(newVal));
        c->VpiDecompile(std::to_string(newVal));
        c->VpiConstType(vpiUIntConst);
      } else if (opType == vpiSubOp) {
        int64_t prevVal = get_value(invalidValue, (expr *)prevRhs);
        int64_t newVal = prevVal - valI;
        c->VpiValue("INT:" + std::to_string(newVal));
        c->VpiDecompile(std::to_string(newVal));
        c->VpiConstType(vpiIntConst);
      } else if (opType == vpiMultOp) {
        int64_t prevVal = get_value(invalidValue, (expr *)prevRhs);
        int64_t newVal = prevVal * valI;
        c->VpiValue("INT:" + std::to_string(newVal));
        c->VpiDecompile(std::to_string(newVal));
        c->VpiConstType(vpiIntConst);
      } else if (opType == vpiDivOp) {
        int64_t prevVal = get_value(invalidValue, (expr *)prevRhs);
        int64_t newVal = prevVal / valI;
        c->VpiValue("INT:" + std::to_string(newVal));
        c->VpiDecompile(std::to_string(newVal));
        c->VpiConstType(vpiIntConst);
      }
      if ((c->VpiSize() == -1) && (c->VpiConstType() == vpiBinaryConst)) {
        bool tmpInvalidValue = false;
        uint64_t size = ExprEval::size(lhsexp, tmpInvalidValue, inst, scope_exp,
                                       true, true);
        if (tmpInvalidValue) {
          std::map<std::string, const typespec *>::iterator itr =
              local_vars.find(std::string(lhs));
          if (itr != local_vars.end()) {
            if (const typespec *tps = itr->second) {
              tmpInvalidValue = false;
              size = ExprEval::size(tps, tmpInvalidValue, inst, scope_exp, true,
                                    true);
            }
          }
        }
        if (!tmpInvalidValue) {
          std::string bval;
          if (valUI) {
            for (uint32_t i = 0; i < size; i++) bval += "1";
          } else {
            bval = NumUtils::toBinary(size, valUI);
          }
          c->VpiValue("BIN:" + bval);
          c->VpiDecompile(bval);
          c->VpiSize(size);
        }
      }
      // Attach the variable's declared typespec to the stored constant: a
      // later member select on the whole-value (`res.exp_bits` after
      // `res = '0` in fpnew_pkg::super_format) reaches
      // hierarchicalSelector's constant branch, which needs the struct
      // typespec to slice the member — without it the read returned null,
      // the RHS reduce went invalid, and the function's result stamped 0.
      if (c->Typespec() == nullptr) {
        const typespec *lts = nullptr;
        auto lv_it2 = local_vars.find(std::string(lhsname));
        if (lv_it2 != local_vars.end()) lts = lv_it2->second;
        if (lts == nullptr) {
          if (const expr *le = any_cast<const expr *>(lhsexp)) {
            if (le->Typespec()) lts = le->Typespec()->Actual_typespec();
          }
        }
        if (lts) {
          ref_typespec *rtc = s.MakeRef_typespec();
          rtc->Actual_typespec(const_cast<typespec *>(lts));
          rtc->VpiParent(c);
          c->Typespec(rtc);
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
  if (invalidValueI && invalidValueD && invalidValueB && (!opRhs)) {
    invalidValue = true;
  }
  return invalidValue;
}

void ExprEval::evalBlock_(std::string_view funcName, Scopes &scopes,
                          bool &invalidValue, bool &continue_flag,
                          bool &break_flag, bool &return_flag,
                          VectorOfvariables *variables, VectorOfany *stmts,
                          std::map<std::string, const typespec *> &local_vars,
                          bool muteError) {
  // Variables declared in this block must shadow any same-named
  // outer variable.  `param_assigns` (where setValueInInstance stores
  // values) is keyed by name only — without explicit save/restore the
  // inner block's write would overwrite the outer scope's binding.
  VectorOfparam_assign *param_assigns = nullptr;
  if (const scope *spe = any_cast<const scope *>(scopes.back()))
    param_assigns = spe->Param_assigns();

  std::vector<param_assign *> saved_outer;
  std::vector<std::pair<std::string, const typespec *>> saved_local;
  if (variables && param_assigns) {
    for (auto var : *variables) {
      std::string_view vn = var->VpiName();
      // Save and remove any outer param_assign with the same name.
      for (auto itr = param_assigns->begin(); itr != param_assigns->end();
           ++itr) {
        if ((*itr)->Lhs() && (*itr)->Lhs()->VpiName() == vn) {
          saved_outer.push_back(*itr);
          param_assigns->erase(itr);
          break;
        }
      }
      // Save and remove any outer local_vars entry.
      auto lv_it = local_vars.find(std::string(vn));
      if (lv_it != local_vars.end()) {
        saved_local.emplace_back(lv_it->first, lv_it->second);
        local_vars.erase(lv_it);
      }
      if (const ref_typespec *rt = var->Typespec())
        local_vars.emplace(vn, rt->Actual_typespec());
    }
  }

  if (stmts) {
    for (auto bst : *stmts) {
      evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
               return_flag, scopes.back(), bst, local_vars, muteError);
      if (continue_flag || break_flag || return_flag) break;
    }
  }

  // Restore outer-scope param_assigns and local_vars.
  if (variables && param_assigns) {
    for (auto var : *variables) {
      std::string_view vn = var->VpiName();
      for (auto itr = param_assigns->begin(); itr != param_assigns->end();
           ++itr) {
        if ((*itr)->Lhs() && (*itr)->Lhs()->VpiName() == vn) {
          param_assigns->erase(itr);
          break;
        }
      }
      local_vars.erase(std::string(vn));
    }
    for (auto pa : saved_outer) param_assigns->push_back(pa);
    for (auto &kv : saved_local) local_vars.emplace(kv.first, kv.second);
  }
}

void ExprEval::evalStmt(std::string_view funcName, Scopes &scopes,
                        bool &invalidValue, bool &continue_flag,
                        bool &break_flag, bool &return_flag, const any *inst,
                        const any *stmt,
                        std::map<std::string, const typespec *> &local_vars,
                        bool muteError) {
  if (invalidValue) {
    return;
  }
  Serializer &s = *inst->GetSerializer();
  UHDM_OBJECT_TYPE stt = stmt->UhdmType();
  switch (stt) {
    case UHDM_OBJECT_TYPE::uhdmcase_stmt: {
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
    case UHDM_OBJECT_TYPE::uhdmif_else: {
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
    case UHDM_OBJECT_TYPE::uhdmif_stmt: {
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
    case UHDM_OBJECT_TYPE::uhdmbegin: {
      begin *st = (begin *)stmt;
      evalBlock_(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, st->Variables(), st->Stmts(), local_vars,
                 muteError);
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmnamed_begin: {
      named_begin *st = (named_begin *)stmt;
      evalBlock_(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, st->Variables(), st->Stmts(), local_vars,
                 muteError);
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmassignment: {
      assignment *st = (assignment *)stmt;
      const std::string_view lhs = st->Lhs()->VpiName();
      expr *lhsexp = st->Lhs();
      const expr *rhs = st->Rhs<expr>();
      expr *rhsexp =
          reduceExpr(rhs, invalidValue, scopes.back(), st, muteError);
      invalidValue =
          setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s, inst, stmt,
                             local_vars, st->VpiOpType(), muteError);
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmassign_stmt: {
      assign_stmt *st = (assign_stmt *)stmt;
      const std::string_view lhs = st->Lhs()->VpiName();
      expr *lhsexp = st->Lhs();
      const expr *rhs = st->Rhs();
      expr *rhsexp =
          reduceExpr(rhs, invalidValue, scopes.back(), nullptr, muteError);
      invalidValue = setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s,
                                        inst, stmt, local_vars, 0, muteError);
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmrepeat: {
      repeat *st = (repeat *)stmt;
      const expr *cond = st->VpiCondition();
      expr *rcond =
          reduceExpr((expr *)cond, invalidValue, scopes.back(), nullptr);
      int64_t val = get_value(
          invalidValue,
          reduceExpr(rcond, invalidValue, scopes.back(), nullptr, muteError));
      if (invalidValue == false) {
        for (int32_t i = 0; i < val; i++) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->VpiStmt(), local_vars,
                   muteError);
        }
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmfor_stmt: {
      for_stmt *st = (for_stmt *)stmt;
      if (const any *stmt = st->VpiForInitStmt()) {
        if (stmt->UhdmType() == UHDM_OBJECT_TYPE::uhdmassignment) {
          assignment *assign = (assignment *)stmt;
          if (const ref_typespec *rt = assign->Lhs()->Typespec()) {
            local_vars.emplace(assign->Lhs()->VpiName(), rt->Actual_typespec());
          }
        }
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->VpiForInitStmt(), local_vars,
                 muteError);
      }
      if (st->VpiForInitStmts()) {
        for (auto s : *st->VpiForInitStmts()) {
          if (s->UhdmType() == UHDM_OBJECT_TYPE::uhdmassignment) {
            assignment *assign = (assignment *)s;
            if (const ref_typespec *rt = assign->Lhs()->Typespec()) {
              local_vars.emplace(assign->Lhs()->VpiName(),
                                 rt->Actual_typespec());
            }
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
    case UHDM_OBJECT_TYPE::uhdmreturn_stmt: {
      return_stmt *st = (return_stmt *)stmt;
      if (const expr *cond = st->VpiCondition()) {
        expr *rhsexp =
            reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError);
        ref_obj *lhsexp = s.MakeRef_obj();
        lhsexp->VpiName(funcName);
        invalidValue =
            setValueInInstance(funcName, lhsexp, rhsexp, invalidValue, s, inst,
                               stmt, local_vars, 0, muteError);
        return_flag = true;
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmwhile_stmt: {
      while_stmt *st = (while_stmt *)stmt;
      if (const expr *cond = st->VpiCondition()) {
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
    case UHDM_OBJECT_TYPE::uhdmdo_while: {
      do_while *st = (do_while *)stmt;
      if (const expr *cond = st->VpiCondition()) {
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
    case UHDM_OBJECT_TYPE::uhdmcontinue_stmt: {
      continue_flag = true;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmbreak_stmt: {
      break_flag = true;
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmoperation: {
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

expr *ExprEval::evalFunc(function *func, std::vector<any *> *args,
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
  module_inst *modinst = s.MakeModule_inst();
  modinst->VpiParent((any *)inst);
  if (const instance *pack = func->Instance()) {
    modinst->Task_funcs(pack->Task_funcs());
    modinst->Parameters(pack->Parameters());
  }
  VectorOfparam_assign *param_assigns = nullptr;
  if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope_array) {
  } else if (inst && inst->UhdmType() == UHDM_OBJECT_TYPE::uhdmdesign) {
    param_assigns = ((design *)inst)->Param_assigns();
  } else if (const scope *spe = any_cast<const scope *>(inst)) {
    param_assigns = spe->Param_assigns();
  }
  std::map<std::string, const typespec *> vars;
  if (param_assigns) {
    modinst->Param_assigns(s.MakeParam_assignVec());
    for (auto p : *param_assigns) {
      // Scope setup only: this clone exists to populate the name->typespec
      // map below (from p->Lhs()); the cloned Rhs is never read here.  Cloning
      // a struct-typed param whose Rhs is a const-function call (e.g.
      // `CVA6Cfg = build_config(cva6_cfg)`) can re-enter the function's result
      // typespec whose members carry not-yet-evaluated struct-arg field refs
      // (`CVA6Cfg.NrNonIdempotentRules`), which are unreachable in this
      // detached clone.  Those are internal artifacts, not user errors, so
      // always mute this scope-setup clone (real param errors are reported by
      // the actual parameter elaboration).
      ElaboratorContext elaboratorContext(&s, false, /*muteErrors=*/true);
      any *pp = clone_tree(p, &elaboratorContext);
      modinst->Param_assigns()->push_back((param_assign *)pp);
      const typespec *tps = nullptr;
      if (const expr *lhs = any_cast<const expr *>(p->Lhs())) {
        if (const ref_typespec *rt = lhs->Typespec()) {
          tps = rt->Actual_typespec();
        }
      }
      vars.emplace(std::string(p->Lhs()->VpiName()), tps);
    }
  }
  // set args
  if (func->Io_decls()) {
    uint32_t index = 0;
    for (auto io : *func->Io_decls()) {
      if (args && (index < args->size())) {
        const std::string_view ioname = io->VpiName();
        if (io->Typespec() == nullptr) {
          ref_typespec *rt = s.MakeRef_typespec();
          rt->VpiParent(io);
          io->Typespec(rt);
        }
        if (io->Typespec()->Actual_typespec() == nullptr) {
          io->Typespec()->Actual_typespec(s.MakeLogic_typespec());
        }
        typespec *tps = io->Typespec()->Actual_typespec();
        vars.emplace(ioname, tps);
        expr *ioexp = (expr *)args->at(index);
        if (expr *exparg =
                reduceExpr(ioexp, invalidValue, modinst, pexpr, muteError)) {
          // A NON-constant argument that still mentions one of this
          // function's formals (`outer(inner(x))`, both formals `g`: inner
          // reduces to `{g[0], g[1]}`) cannot be bound by name — the body's
          // `g` would resolve to an expression containing `g`.  Not a
          // compile-time value: stop here.
          // (A BARE ref_obj to a same-named formal is fine: getValue returns
          // the param_assign Rhs without re-reducing it, so no loop — and
          // the historical behaviour, incl. the typespec annotations evalFunc
          // leaves on the body, is kept for that common `f(value)` shape.)
          // Only when the argument IS a nested function call: the leaked
          // names are the nested callee's formals, which have no meaning in
          // this scope.  A select / operation over this caller's own
          // variables (`sbox4_8bit(state_in[0 +: 8])` with state_in bound in
          // the caller) resolves through the scope chain as before.
          if (exparg->UhdmType() != UHDM_OBJECT_TYPE::uhdmconstant &&
              ioexp->UhdmType() == UHDM_OBJECT_TYPE::uhdmfunc_call) {
            const func_call *nested = (const func_call *)ioexp;
            const function *nfunc = nullptr;
            if (task_func *tf = getTaskFunc(nested->VpiName(), inst))
              nfunc = any_cast<const function *>(tf);
            bool selfRef = false;
            if (nfunc && nfunc->Io_decls()) {
              for (auto io2 : *nfunc->Io_decls())
                if (exprRefsName(exparg, io2->VpiName())) { selfRef = true; break; }
            } else {
              for (auto io2 : *func->Io_decls())
                if (exprRefsName(exparg, io2->VpiName())) { selfRef = true; break; }
            }
            if (selfRef) {
              invalidValue = true;
              return nullptr;
            }
          }
          if (exparg->Typespec() == nullptr) {
            ref_typespec *crt = s.MakeRef_typespec();
            crt->VpiParent(exparg);
            exparg->Typespec(crt);
          }
          if (exparg->Typespec())
            exparg->Typespec()->Actual_typespec(tps);
          // Truncate the constant argument to the formal parameter's
          // declared width so the in-function value reflects the
          // truncated (1-bit / 5-bit / etc.) view of the argument.
          // Without this, `function f; input reg signed inp; ...
          // f = inp;` reads `inp` as the full-width argument constant
          // (e.g. 64-bit `1`) instead of the 1-bit signed value
          // (`1` → -1), and a subsequent assignment to a wider return
          // type fails to sign-extend correctly
          // (yosys/tests/verilog/func_typename_ret.sv gold module).
          if (exparg->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant &&
              tps && tps->UhdmType() ==
                     UHDM_OBJECT_TYPE::uhdmlogic_typespec) {
            bool sz_invalid = false;
            uint64_t formal_w = size(tps, sz_invalid, modinst, pexpr,
                                     true, true);
            constant *cexp = (constant *)exparg;
            if (!sz_invalid && formal_w > 0 &&
                cexp->VpiConstType() != vpiBinaryConst &&
                static_cast<uint64_t>(cexp->VpiSize()) > formal_w) {
              bool gv_invalid = false;
              int64_t v = get_value(gv_invalid, cexp);
              if (!gv_invalid) {
                uint64_t mask = NumUtils::getMask(formal_w);
                int64_t truncated = v & mask;
                logic_typespec *ltps2 = (logic_typespec *)tps;
                if (ltps2->VpiSigned() && formal_w < 64) {
                  int64_t msb = (truncated >> (formal_w - 1)) & 1;
                  if (msb) {
                    // Sign-extend so subsequent assignment to a wider
                    // LHS uses the negative value.
                    truncated |= ~mask;
                  }
                }
                if (ltps2->VpiSigned()) {
                  cexp->VpiValue("INT:" + std::to_string(truncated));
                  cexp->VpiConstType(vpiIntConst);
                } else {
                  cexp->VpiValue("UINT:" +
                                 std::to_string(static_cast<uint64_t>(truncated)));
                  cexp->VpiConstType(vpiUIntConst);
                }
                cexp->VpiDecompile(std::to_string(truncated));
                cexp->VpiSize(static_cast<int32_t>(formal_w));
              }
            }
          }
          std::map<std::string, const typespec *> local_vars;
          invalidValue =
              setValueInInstance(ioname, io, exparg, invalidValue, s, modinst,
                                 func, local_vars, 0, muteError);
        }
      }
      index++;
    }
  }
  if (func->Variables()) {
    for (auto var : *func->Variables()) {
      if (const ref_typespec *rt = var->Typespec()) {
        vars.emplace(var->VpiName(), rt->Actual_typespec());
      }
    }
  }
  typespec *funcReturnTypespec = nullptr;
  if (variables *r = func->Return()) {
    if (ref_typespec *rt = r->Typespec()) {
      funcReturnTypespec = rt->Actual_typespec();
    }
  }
  if (funcReturnTypespec == nullptr) {
    funcReturnTypespec = s.MakeLogic_typespec();
  }
  modinst->Variables(s.MakeVariablesVec());
  logic_var *var = s.MakeLogic_var();
  var->VpiName(name);
  ref_typespec *frtrt = s.MakeRef_typespec();
  frtrt->VpiParent(var);
  frtrt->Actual_typespec(funcReturnTypespec);
  var->Typespec(frtrt);
  modinst->Variables()->push_back(var);
  vars.emplace(name, funcReturnTypespec);
  scopes.push_back(modinst);
  if (const any *the_stmt = func->Stmt()) {
    UHDM_OBJECT_TYPE stt = the_stmt->UhdmType();
    bool return_flag = false;
    switch (stt) {
      case UHDM_OBJECT_TYPE::uhdmbegin: {
        begin *st = (begin *)the_stmt;
        bool continue_flag = false;
        bool break_flag = false;
        for (auto stmt : *st->Stmts()) {
          evalStmt(name, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, modinst, stmt, vars, muteError);
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
      case UHDM_OBJECT_TYPE::uhdmnamed_begin: {
        named_begin *st = (named_begin *)the_stmt;
        bool continue_flag = false;
        bool break_flag = false;
        for (auto stmt : *st->Stmts()) {
          evalStmt(name, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, modinst, stmt, vars, muteError);
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
                 return_flag, modinst, the_stmt, vars, muteError);
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
  if (modinst->Param_assigns()) {
    for (auto p : *modinst->Param_assigns()) {
      const std::string n(p->Lhs()->VpiName());
      if ((!n.empty()) && (vars.find(n) == vars.end())) {
        invalidValue = true;
        return nullptr;
      }
    }
    for (auto p : *modinst->Param_assigns()) {
      if (p->Lhs()->VpiName() == name) {
        if (p->Rhs() &&
            (p->Rhs()->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant)) {
          constant *c = (constant *)p->Rhs();
          std::string_view val = c->VpiValue();
          if ((val.find("X") != std::string::npos) ||
              (val.find("x") != std::string::npos)) {
            invalidValue = true;
            return nullptr;
          }
        }
        if (p->Rhs() &&
            (p->Rhs()->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj)) {
          ref_obj* ref = (ref_obj *)p->Rhs(); 
          std::string_view refname = ref->VpiName();
          std::map<std::string, const typespec *>::iterator vitr = vars.find(std::string(refname));
          if (vitr != vars.end()) {
            UHDM::struct_var* structv = s.MakeStruct_var();
            UHDM::ref_typespec* rtps = s.MakeRef_typespec();
            structv->Typespec(rtps);
            rtps->Actual_typespec((typespec*) (*vitr).second);
            return structv;
          }
          for (auto p1 : *modinst->Param_assigns()) {
            if (p1->Lhs()->VpiName() == refname) {
              return (expr *)p1->Rhs();
            }
          }
        }
        const typespec *tps = nullptr;
        if (const variables *r = func->Return()) {
          if (const ref_typespec *rt = r->Typespec()) {
            tps = rt->Actual_typespec();
          }
        }
        if (tps && (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmlogic_typespec)) {
          logic_typespec *ltps = (logic_typespec *)tps;
          uint64_t si = size(tps, invalidValue, inst, pexpr, true, true);
          if (p->Rhs() &&
              (p->Rhs()->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant)) {
            constant *c = (constant *)p->Rhs();
            ElaboratorContext elaboratorContext(&s, false, muteError);
            c = (constant *)clone_tree(c, &elaboratorContext);
            if (c->VpiConstType() == vpiBinaryConst) {
              std::string_view val = c->VpiValue();
              val.remove_prefix(std::string_view("BIN:").length());
              if (val.size() > si) {
                val.remove_prefix(val.size() - si);
                c->VpiValue(std::string("BIN:").append(val));
                c->VpiDecompile(val);
              } else if (ltps->VpiSigned()) {
                if (val == "1") {
                  c->VpiValue("INT:-1");
                  c->VpiDecompile("-1");
                  c->VpiConstType(vpiIntConst);
                }
              }
            } else {
              uint64_t mask = NumUtils::getMask(si);
              int64_t v = get_value(invalidValue, c);
              v = v & mask;
              c->VpiValue("UINT:" + std::to_string(v));
              c->VpiDecompile(std::to_string(v));
              c->VpiConstType(vpiUIntConst);
            }
            c->VpiSize(static_cast<int32_t>(si));
            return c;
          }
        }
        return (expr *)p->Rhs();
      }
    }
  }
  invalidValue = true;
  return nullptr;
}

std::string vPrint(any *handle) {
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

std::string ExprEval::prettyPrint(const any *handle) {
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
}  // namespace UHDM
