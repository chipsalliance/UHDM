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

#include <uhdm/Elaborator.h>
#include <uhdm/ExprEval.h>
#include <uhdm/NumUtils.h>
#include <uhdm/Serializer.h>
#include <uhdm/UhdmVisitor.h>
#include <uhdm/Utils.h>
#include <uhdm/uhdm.h>

#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>

namespace uhdm {
class DetectRefObj final : public UhdmVisitor {
 public:
  explicit DetectRefObj() {}
  ~DetectRefObj() override = default;

  void visitRefObj(const RefObj *object) final {
    hasRefObj = true;
    requestAbort();
  }
  void visitBitSelect(const BitSelect *object) final {
    hasRefObj = true;
    requestAbort();
  }
  void visitIndexedPartSelect(const IndexedPartSelect *object) final {
    hasRefObj = true;
    requestAbort();
  }
  void visitPartSelect(const PartSelect *object) final {
    hasRefObj = true;
    requestAbort();
  }
  void visitVarSelect(const VarSelect *object) final {
    hasRefObj = true;
    requestAbort();
  }
  void visitHierPath(const HierPath *object) final {
    hasRefObj = true;
    requestAbort();
  }
  bool refObjDetected() const { return hasRefObj; }

 private:
  bool hasRefObj = false;
};

bool ExprEval::isFullySpecified(const Typespec *tps) {
  if (tps == nullptr) {
    return true;
  }
  DetectRefObj detector;
  detector.visit(tps);
  if (detector.refObjDetected()) {
    return false;
  }
  return true;
}

std::string ExprEval::toBinary(const Constant *c) {
  std::string result;
  if (c == nullptr) return result;
  int32_t type = c->getConstType();
  std::string_view sv = c->getValue();
  switch (type) {
    case vpiBinaryConst: {
      sv.remove_prefix(std::string_view("BIN:").length());
      result = sv;
      if (c->getSize() >= 0) {
        if (result.size() < (uint32_t)c->getSize()) {
          uint32_t rsize = result.size();
          for (uint32_t i = 0; i < (uint32_t)c->getSize() - rsize; i++) {
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
      result = NumUtils::toBinary(c->getSize(), res);
      break;
    }
    case vpiHexConst: {
      sv.remove_prefix(std::string_view("HEX:").length());
      result = NumUtils::hexToBin(sv);
      if (c->getSize() >= 0) {
        if (result.size() < (uint32_t)c->getSize()) {
          uint32_t rsize = result.size();
          for (uint32_t i = 0; i < (uint32_t)c->getSize() - rsize; i++) {
            result = "0" + result;
          }
        }
      }
      break;
    }
    case vpiOctConst: {
      sv.remove_prefix(std::string_view("OCT:").length());
      result = NumUtils::hexToBin(sv);
      if (c->getSize() >= 0) {
        if (result.size() < (uint32_t)c->getSize()) {
          uint32_t rsize = result.size();
          for (uint32_t i = 0; i < (uint32_t)c->getSize() - rsize; i++) {
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
      result = NumUtils::toBinary(c->getSize(), res);
      break;
    }
    case vpiUIntConst: {
      sv.remove_prefix(std::string_view("UINT:").length());
      uint64_t res = 0;
      if (NumUtils::parseUint64(sv, &res) == nullptr) {
        res = 0;
      }
      result = NumUtils::toBinary(c->getSize(), res);
      break;
    }
    case vpiScalar: {
      sv.remove_prefix(std::string_view("SCAL:").length());
      uint64_t res = 0;
      if (NumUtils::parseBinary(sv, &res) == nullptr) {
        res = 0;
      }
      result = NumUtils::toBinary(c->getSize(), res);
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
      result = NumUtils::toBinary(c->getSize(), res);
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
        result = NumUtils::toBinary(c->getSize(), res);
      } else {
        sv.remove_prefix(std::string_view("INT:").length());
        uint64_t res = 0;
        if (NumUtils::parseIntLenient(sv, &res) == nullptr) {
          res = 0;
        }
        result = NumUtils::toBinary(c->getSize(), res);
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

Any *ExprEval::getValue(std::string_view name, const Any *inst,
                        const Any *pexpr, bool muteError,
                        const Any *checkLoop) {
  Any *result = nullptr;
  if ((inst == nullptr) && (pexpr == nullptr)) {
    return nullptr;
  }
  Serializer *tmps = nullptr;
  if (inst)
    tmps = inst->getSerializer();
  else
    tmps = pexpr->getSerializer();
  Serializer &s = *tmps;
  const Any *root = inst;
  const Any *tmp = inst;
  while (tmp) {
    root = tmp;
    tmp = tmp->getParent();
  }
  const Design *des = any_cast<Design>(root);
  if (des) m_design = des;
  std::string_view the_name = name;
  const Any *the_instance = inst;
  if (m_design && (name.find("::") != std::string::npos)) {
    std::vector<std::string_view> res = tokenizeMulti(name, "::");
    if (res.size() > 1) {
      const std::string_view packName = res[0];
      const std::string_view varName = res[1];
      the_name = varName;
      Package *pack = nullptr;
      if (m_design->getTopPackages()) {
        for (auto p : *m_design->getTopPackages()) {
          if (p->getName() == packName) {
            pack = p;
            break;
          }
        }
      }
      the_instance = pack;
    }
  }

  while (the_instance) {
    ParamAssignCollection *ParamAssigns = nullptr;
    TypespecCollection *Typespecs = nullptr;
    if (the_instance->getUhdmType() == UhdmType::GenScopeArray) {
    } else if (the_instance->getUhdmType() == UhdmType::Design) {
      ParamAssigns = ((Design *)the_instance)->getParamAssigns();
      Typespecs = ((Design *)the_instance)->getTypespecs();
    } else if (const Scope *spe = any_cast<Scope>(the_instance)) {
      ParamAssigns = spe->getParamAssigns();
      Typespecs = spe->getTypespecs();
    }
    if (ParamAssigns) {
      for (auto p : *ParamAssigns) {
        if (p->getLhs() && (p->getLhs()->getName() == the_name)) {
          result = (Any *)p->getRhs();
          break;
        }
      }
    }
    if ((result == nullptr) && (Typespecs != nullptr)) {
      for (auto p : *Typespecs) {
        if (p->getUhdmType() == UhdmType::EnumTypespec) {
          EnumTypespec *e = (EnumTypespec *)p;
          for (auto c : *e->getEnumConsts()) {
            if (c->getName() == the_name) {
              Constant *cc = s.make<Constant>();
              cc->setValue(c->getValue());
              cc->setSize(c->getSize());
              result = cc;
              break;
            }
          }
        }
      }
    }
    if (result && (result->getUhdmType() == UhdmType::Operation)) {
      Operation *op = (Operation *)result;
      if (const RefTypespec *rt = op->getTypespec()) {
        ExprEval eval;
        if (Expr *res = eval.flattenPatternAssignments(s, rt->getActual(),
                                                       (Expr *)result)) {
          if (res->getUhdmType() == UhdmType::Operation) {
            ((Operation *)result)
                ->setOperands(((Operation *)res)->getOperands());
          }
        }
      }
    }
    if (result) break;

    the_instance = the_instance->getParent();
  }

  if (result) {
    UhdmType resultType = result->getUhdmType();
    if (resultType == UhdmType::Constant) {
    } else if (resultType == UhdmType::RefObj) {
      if (result->getName() != name) {
        if (Any *rval = getValue(result->getName(), inst, pexpr, muteError)) {
          result = rval;
        }
      }
    } else if ((resultType == UhdmType::Operation) ||
               (resultType == UhdmType::HierPath) ||
               (resultType == UhdmType::BitSelect) ||
               (resultType == UhdmType::SysFuncCall)) {
      bool invalidValue = false;
      if (checkLoop && (result == checkLoop)) {
        return nullptr;
      }
      if (Any *rval =
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

Any *ExprEval::getObject(std::string_view name, const Any *inst,
                         const Any *pexpr, bool muteError) {
  Any *result = nullptr;
  while (pexpr) {
    if (const Scope *spe = any_cast<Scope>(pexpr)) {
      if (spe->getVariables()) {
        for (auto o : *spe->getVariables()) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (const TaskFunc *s = any_cast<TaskFunc>(pexpr)) {
      if (s->getIODecls()) {
        for (auto o : *s->getIODecls()) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && s->getParamAssigns()) {
        for (auto o : *s->getParamAssigns()) {
          const std::string_view pname = o->getLhs()->getName();
          if (pname == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (pexpr->getUhdmType() == UhdmType::ForeachStmt) {
      ForeachStmt *for_stmt = (ForeachStmt *)pexpr;
      if (AnyCollection *loopvars = for_stmt->getLoopVars()) {
        for (auto var : *loopvars) {
          if (var->getName() == name) {
            result = var;
            break;
          }
        }
      }
    }
    if (pexpr->getUhdmType() == UhdmType::ClassDefn) {
      const ClassDefn *defn = (ClassDefn *)pexpr;
      while (defn) {
        if (defn->getVariables()) {
          for (Variable *member : *defn->getVariables()) {
            if (member->getName() == name) {
              result = member;
              break;
            }
          }
        }
        if (result) break;

        const ClassDefn *base_defn = nullptr;
        if (const Extends *ext = defn->getExtends()) {
          if (const RefTypespec *rt = ext->getClassTypespec()) {
            if (const ClassTypespec *tp = rt->getActual<ClassTypespec>()) {
              base_defn = tp->getClassDefn();
            }
          }
        }
        defn = base_defn;
      }
    }
    if (result) break;
    pexpr = pexpr->getParent();
  }
  if (result == nullptr) {
    while (inst) {
      ParamAssignCollection *ParamAssigns = nullptr;
      VariableCollection *Variables = nullptr;
      NetCollection *nets = nullptr;
      TypespecCollection *Typespecs = nullptr;
      ScopeCollection *scopes = nullptr;
      if (inst->getUhdmType() == UhdmType::GenScopeArray) {
      } else if (inst->getUhdmType() == UhdmType::Design) {
        ParamAssigns = ((Design *)inst)->getParamAssigns();
        Typespecs = ((Design *)inst)->getTypespecs();
      } else if (const Scope *spe = any_cast<Scope>(inst)) {
        ParamAssigns = spe->getParamAssigns();
        Variables = spe->getVariables();
        Typespecs = spe->getTypespecs();
        scopes = spe->getInternalScopes();
        if (const Instance *in = any_cast<Instance>(inst)) {
          nets = in->getNets();
        }
      }
      if ((result == nullptr) && nets) {
        for (auto o : *nets) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && Variables) {
        for (auto o : *Variables) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && ParamAssigns) {
        for (auto o : *ParamAssigns) {
          const std::string_view pname = o->getLhs()->getName();
          if (pname == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && Typespecs) {
        for (auto o : *Typespecs) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && scopes) {
        for (auto o : *scopes) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) ||
          (result && (result->getUhdmType() != UhdmType::Constant) &&
           (result->getUhdmType() != UhdmType::ParamAssign))) {
        if (Any *tmpresult = getValue(name, inst, pexpr, muteError)) {
          result = tmpresult;
        }
      }
      if (result) break;
      if (inst) {
        if (inst->getUhdmType() == UhdmType::Module) {
          break;
        } else {
          inst = inst->getParent();
        }
      }
    }
  }

  if (result && (result->getUhdmType() == UhdmType::RefObj)) {
    RefObj *ref = (RefObj *)result;
    const std::string_view refname = ref->getName();
    if (refname != name) result = getObject(refname, inst, pexpr, muteError);
    if (result) {
      if (ParamAssign *passign = any_cast<ParamAssign>(result)) {
        result = passign->getRhs();
      }
    }
  }
  if ((result == nullptr) && getObjectFunctor) {
    return getObjectFunctor(name, inst, pexpr);
  }
  return result;
}

long double ExprEval::get_double(bool &invalidValue, const Expr *Expr) {
  long double result = 0;
  if (const Constant *c = any_cast<Constant>(Expr)) {
    int32_t type = c->getConstType();
    std::string_view sv = c->getValue();
    switch (type) {
      case vpiRealConst: {
        sv.remove_prefix(std::string_view("REAL:").length());
        invalidValue = NumUtils::parseLongDouble(sv, &result) == nullptr;
        break;
      }
      default: {
        result = static_cast<long double>(get_value(invalidValue, Expr));
        break;
      }
    }
  } else {
    invalidValue = true;
  }
  return result;
}

uint64_t ExprEval::getValue(const Expr *Expr) {
  uint64_t result = 0;
  if (Expr && Expr->getUhdmType() == UhdmType::Constant) {
    Constant *c = (Constant *)Expr;
    std::string_view sv = c->getValue();
    int32_t type = c->getConstType();
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

void ExprEval::recursiveFlattening(Serializer &s, AnyCollection *flattened,
                                   const AnyCollection *ordered,
                                   std::vector<const Typespec *> fieldTypes) {
  // Flattening
  int32_t index = 0;
  for (Any *op : *ordered) {
    if (op->getUhdmType() == UhdmType::TaggedPattern) {
      TaggedPattern *tp = (TaggedPattern *)op;
      const Typespec *ttp = nullptr;
      if (const RefTypespec *rt = tp->getTypespec()) {
        ttp = rt->getActual();
      }
      UhdmType ttpt = ttp->getUhdmType();
      switch (ttpt) {
        case UhdmType::IntTypespec: {
          flattened->emplace_back(tp->getPattern());
          break;
        }
        case UhdmType::IntegerTypespec: {
          flattened->emplace_back(tp->getPattern());
          break;
        }
        case UhdmType::StringTypespec: {
          Any *sop = (Any *)tp->getPattern();
          UhdmType sopt = sop->getUhdmType();
          if (sopt == UhdmType::Operation) {
            AnyCollection *operands = ((Operation *)sop)->getOperands();
            for (auto op1 : *operands) {
              bool substituted = false;
              if (op1->getUhdmType() == UhdmType::TaggedPattern) {
                TaggedPattern *tp1 = (TaggedPattern *)op1;
                const Typespec *ttp1 = nullptr;
                if (const RefTypespec *rt = tp1->getTypespec()) {
                  ttp1 = rt->getActual();
                }
                UhdmType ttpt1 = ttp1->getUhdmType();
                if (ttpt1 == UhdmType::StringTypespec) {
                  if (ttp1->getName() == "default") {
                    const Any *patt = tp1->getPattern();
                    const Typespec *mold = fieldTypes[index];
                    Operation *subst = s.make<Operation>();
                    AnyCollection *sops = s.makeCollection<Any>();
                    subst->setOperands(sops);
                    subst->setOpType(vpiConcatOp);
                    flattened->emplace_back(subst);
                    UhdmType moldtype = mold->getUhdmType();
                    if (moldtype == UhdmType::StructTypespec) {
                      StructTypespec *molds = (StructTypespec *)mold;
                      for (auto mem : *molds->getMembers()) {
                        if (mem) sops->emplace_back((Any *)patt);
                      }
                    } else if (moldtype == UhdmType::LogicTypespec) {
                      LogicTypespec *molds = (LogicTypespec *)mold;
                      RangeCollection *ranges = molds->getRanges();
                      if (!ranges->empty()) {
                        Range *r = ranges->front();
                        uint64_t from = getValue(r->getLeftExpr());
                        uint64_t to = getValue(r->getRightExpr());
                        if (from > to) {
                          std::swap(from, to);
                        }
                        for (uint64_t i = from; i <= to; i++) {
                          sops->emplace_back((Any *)patt);
                        }
                        // TODO: Multidimension
                      }
                    }
                    substituted = true;
                    break;
                  }
                }
              } else if (op1->getUhdmType() == UhdmType::Operation) {
                // recursiveFlattening(s, flattened,
                // ((Operation*)op1)->getOperands(), fieldTypes);
                // substituted = true;
              }
              if (!substituted) {
                flattened->emplace_back(sop);
                break;
              }
            }
          } else {
            flattened->emplace_back(sop);
          }
          break;
        }
        default:
          flattened->emplace_back(op);
          break;
      }
    } else {
      flattened->emplace_back(op);
    }
    index++;
  }
}

Expr *ExprEval::flattenPatternAssignments(Serializer &s, const Typespec *tps,
                                          Expr *exp) {
  Expr *result = exp;
  if ((!exp) || (!tps)) {
    return result;
  }
  // Reordering
  if (exp->getUhdmType() == UhdmType::Operation) {
    Operation *op = (Operation *)exp;
    if (op->getOpType() == vpiConditionOp) {
      AnyCollection *ops = op->getOperands();
      ops->at(1) = flattenPatternAssignments(s, tps, (Expr *)ops->at(1));
      ops->at(2) = flattenPatternAssignments(s, tps, (Expr *)ops->at(2));
      return result;
    }
    if (op->getOpType() != vpiAssignmentPatternOp) {
      return result;
    }
    if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
      ArrayTypespec *atps = (ArrayTypespec *)tps;
      if (const RefTypespec *rt = atps->getElemTypespec()) {
        tps = rt->getActual();
      }
    }
    if (tps == nullptr) {
      return result;
    }
    if (tps->getUhdmType() != UhdmType::StructTypespec) {
      if (const RefTypespec *rt = op->getTypespec()) {
        tps = rt->getActual();
      }
    }
    if (tps == nullptr) {
      return result;
    }
    if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
      ArrayTypespec *atps = (ArrayTypespec *)tps;
      if (const RefTypespec *rt = atps->getElemTypespec()) {
        tps = rt->getActual();
      }
    }
    if (tps->getUhdmType() != UhdmType::StructTypespec) {
      return result;
    }
    if (op->getFlattened()) {
      return result;
    }
    StructTypespec *stps = (StructTypespec *)tps;
    std::vector<std::string_view> fieldNames;
    std::vector<const Typespec *> fieldTypes;
    for (TypespecMember *memb : *stps->getMembers()) {
      if (const RefTypespec *rt = memb->getTypespec()) {
        fieldNames.emplace_back(memb->getName());
        fieldTypes.emplace_back(rt->getActual());
      }
    }
    AnyCollection *orig = op->getOperands();
    if (orig->size() == 1) {
      for (auto oper : *orig) {
        if (oper->getUhdmType() == UhdmType::Operation) {
          Operation *opi = (Operation *)oper;
          if (opi->getOpType() == vpiAssignmentPatternOp) {
            op = opi;
            orig = op->getOperands();
            break;
          }
        }
      }
    }
    AnyCollection *ordered = s.makeCollection<Any>();
    std::vector<Any *> tmp(fieldNames.size());
    Any *defaultOp = nullptr;
    int32_t index = 0;
    bool flatten = false;
    for (auto oper : *orig) {
      if (oper->getUhdmType() == UhdmType::TaggedPattern) {
        TaggedPattern *tp = (TaggedPattern *)oper;
        const Typespec *ttp = nullptr;
        if (const RefTypespec *rt = tp->getTypespec()) {
          ttp = rt->getActual();
        }
        const std::string_view tname = ttp->getName();
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
            if (ttp->getUhdmType() == fieldTypes[i]->getUhdmType()) {
              tmp[i] = oper;
              found = true;
              break;
            }
          }
        }
        if (found == false) {
          if (!m_muteError) {
            const std::string errMsg(tname);
            s.getErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY, errMsg,
                                exp, nullptr);
          }
          return result;
        }
      } else if (oper->getUhdmType() == UhdmType::Operation) {
        return result;
      } else {
        if (index < (int32_t)tmp.size()) {
          tmp[index] = oper;
        } else {
          if (!m_muteError) {
            s.getErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY,
                                "Out of bound!", exp, nullptr);
          }
        }
      }
      index++;
    }
    index = 0;
    Elaborator elaborator(&s, false, m_muteError);
    for (auto opi : tmp) {
      if (defaultOp && (opi == nullptr)) {
        opi = elaborator.clone<>(defaultOp, defaultOp->getParent());
      }
      if (opi == nullptr) {
        if (!m_muteError) {
          const std::string errMsg(fieldNames[index]);
          s.getErrorHandler()(ErrorType::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN,
                              errMsg, exp, nullptr);
        }
        return result;
      }
      if (opi->getUhdmType() == UhdmType::TaggedPattern) {
        TaggedPattern *tp = (TaggedPattern *)opi;
        const Any *patt = tp->getPattern();
        if (patt->getUhdmType() == UhdmType::Constant) {
          Constant *c = (Constant *)patt;
          if (c->getSize() == -1) {
            bool invalidValue = false;
            uint64_t uval = get_uvalue(invalidValue, c);
            if (uval == 1) {
              uint64_t size = ExprEval::size(fieldTypes[index], invalidValue,
                                             nullptr, exp, true, true);
              uint64_t mask = NumUtils::getMask(size);
              uval = mask;
              c->setValue("UINT:" + std::to_string(uval));
              c->setDecompile(std::to_string(uval));
              c->setConstType(vpiUIntConst);
              c->setSize(static_cast<int32_t>(size));
            } else if (uval == 0) {
              uint64_t size = ExprEval::size(fieldTypes[index], invalidValue,
                                             nullptr, exp, true, true);
              c->setValue("UINT:" + std::to_string(uval));
              c->setDecompile(std::to_string(uval));
              c->setConstType(vpiUIntConst);
              c->setSize(static_cast<int32_t>(size));
            }
          }
        } else if (patt->getUhdmType() == UhdmType::Operation) {
          Operation *patt_op = (Operation *)patt;
          if (patt_op->getOpType() == vpiAssignmentPatternOp) {
            opi = flattenPatternAssignments(s, fieldTypes[index], patt_op);
          }
        }
      }
      ordered->emplace_back(opi);
      index++;
    }
    Operation *opres = elaborator.clone<>(op, op->getParent());
    opres->setOperands(ordered);
    if (flatten) {
      opres->setFlattened(true);
    }
    // Flattening
    AnyCollection *flattened = s.makeCollection<Any>();
    recursiveFlattening(s, flattened, ordered, fieldTypes);
    for (auto o : *flattened) o->setParent(opres);
    opres->setOperands(flattened);
    result = opres;
  }
  return result;
}

uint64_t ExprEval::size(const Any *ts, bool &invalidValue, const Any *inst,
                        const Any *pexpr, bool full, bool muteError) {
  if (ts == nullptr) return 0;
  uint64_t bits = 0;
  RangeCollection *ranges = nullptr;
  UhdmType ttps = ts->getUhdmType();
  if (ttps == UhdmType::RefTypespec) {
    RefTypespec *rtps = (RefTypespec *)ts;
    ts = rtps->getActual();
    ttps = ts->getUhdmType();
  }
  switch (ttps) {
    case UhdmType::HierPath: {
      ts = decodeHierPath((HierPath *)ts, invalidValue, inst, nullptr, true);
      if (ts)
        bits = size(ts, invalidValue, inst, pexpr, full);
      else
        invalidValue = true;
      break;
    }
    case UhdmType::ArrayTypespec: {
      ArrayTypespec *lts = (ArrayTypespec *)ts;
      ranges = lts->getRanges();
      if (!full) {
        bits = 1;
      } else if (const RefTypespec *rt = lts->getElemTypespec()) {
        bits = size(rt->getActual(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UhdmType::ShortRealTypespec: {
      bits = 32;
      break;
    }
    case UhdmType::RealTypespec: {
      bits = 32;
      break;
    }
    case UhdmType::ByteTypespec: {
      bits = 8;
      break;
    }
    case UhdmType::ShortIntTypespec: {
      bits = 16;
      break;
    }
    case UhdmType::IntTypespec: {
      IntTypespec *its = (IntTypespec *)ts;
      bits = 32;
      ranges = its->getRanges();
      if (ranges) {
        bits = 1;
      }
      break;
    }
    case UhdmType::LongIntTypespec: {
      bits = 64;
      break;
    }
    case UhdmType::IntegerTypespec: {
      IntegerTypespec *itps = (IntegerTypespec *)ts;
      std::string_view val = itps->getValue();
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
    case UhdmType::BitTypespec: {
      bits = 1;
      BitTypespec *lts = (BitTypespec *)ts;
      ranges = lts->getRanges();
      break;
    }
    case UhdmType::LogicTypespec: {
      bits = 1;
      LogicTypespec *lts = (LogicTypespec *)ts;
      ranges = lts->getRanges();
      break;
    }
    case UhdmType::StringTypespec: {
      bits = 0;
      invalidValue = true;
      break;
    }
    case UhdmType::UnsupportedTypespec: {
      bits = 0;
      invalidValue = true;
      break;
    }
    case UhdmType::Net: {
      bits = 1;
      if (const LogicTypespec *const lt =
              uhdm::getTypespec<LogicTypespec>(ts)) {
        bool tmpInvalidValue = false;
        uint64_t tmpS = size(lt, tmpInvalidValue, inst, pexpr, full);
        if (!tmpInvalidValue) {
          bits = tmpS;
        }
      } else if (const StructTypespec *const st =
                     uhdm::getTypespec<StructTypespec>(ts)) {
        bits += size(st, invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UhdmType::Variable: {
      if (const RefTypespec *const rt =
              static_cast<const Variable *>(ts)->getTypespec()) {
        if (const LogicTypespec *const lt = rt->getActual<LogicTypespec>()) {
          bits = 1;
          bool tmpInvalidValue = false;
          uint64_t tmpS = size(lt, tmpInvalidValue, inst, pexpr, full);
          if (tmpInvalidValue == false) {
            bits = tmpS;
          }
        } else if (const BitTypespec *const bt = rt->getActual<BitTypespec>()) {
          bits = size(bt, invalidValue, inst, pexpr, full);
          ranges = bt->getRanges();
        } else if (rt->getActual<ByteTypespec>() != nullptr) {
          bits = 8;
        } else if (const StructTypespec *const st =
                       rt->getActual<StructTypespec>()) {
          bits += size(st, invalidValue, inst, pexpr, full);
        } else if (const ArrayTypespec *const at =
                       rt->getActual<ArrayTypespec>()) {
          bits += size(at, invalidValue, inst, pexpr, full);
          ranges = at->getRanges();
        } else if (const EnumTypespec *const et =
                       rt->getActual<EnumTypespec>()) {
          bits = size(et, invalidValue, inst, pexpr, full);
        }
      }
      break;
    }
    case UhdmType::StructTypespec: {
      StructTypespec *sts = (StructTypespec *)ts;
      if (TypespecMemberCollection *members = sts->getMembers()) {
        for (TypespecMember *member : *members) {
          if (const RefTypespec *rt = member->getTypespec()) {
            bits += size(rt->getActual(), invalidValue, inst, pexpr, full);
          }
        }
      }
      break;
    }
    case UhdmType::EnumTypespec: {
      if (const RefTypespec *rt =
              ((const EnumTypespec *)ts)->getBaseTypespec()) {
        bits = size(rt->getActual(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UhdmType::UnionTypespec: {
      UnionTypespec *sts = (UnionTypespec *)ts;
      if (TypespecMemberCollection *members = sts->getMembers()) {
        for (TypespecMember *member : *members) {
          if (const RefTypespec *rt = member->getTypespec()) {
            uint64_t max =
                size(rt->getActual(), invalidValue, inst, pexpr, full);
            if (max > bits) bits = max;
          }
        }
      }
      break;
    }
    case UhdmType::Constant: {
      Constant *c = (Constant *)ts;
      bits = c->getSize();
      break;
    }
    case UhdmType::EnumConst: {
      EnumConst *c = (EnumConst *)ts;
      bits = c->getSize();
      break;
    }
    case UhdmType::RefObj: {
      RefObj *ref = (RefObj *)ts;
      const Any *act = ref->getActual();
      if (act == nullptr) {
        std::string_view name = ref->getName();
        act = getObject(name, inst, pexpr, muteError);
      }
      if (act) {
        bits = size(act, invalidValue, inst, pexpr, full);
      } else {
        invalidValue = true;
      }
      break;
    }
    case UhdmType::Operation: {
      Operation *tsop = (Operation *)ts;
      if (tsop->getOpType() == vpiConcatOp) {
        if (auto ops = tsop->getOperands()) {
          for (auto op : *ops) {
            bits += size(op, invalidValue, inst, pexpr, full);
          }
        }
      }
      break;
    }
    case UhdmType::TypespecMember: {
      if (const RefTypespec *rt = ((const TypespecMember *)ts)->getTypespec()) {
        bits += size(rt->getActual(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UhdmType::IODecl: {
      if (const RefTypespec *rt = ((const IODecl *)ts)->getTypespec()) {
        bits += size(rt->getActual(), invalidValue, inst, pexpr, full);
      }
      break;
    }
    case UhdmType::BitSelect: {
      bits = 1;
      break;
    }
    case UhdmType::PartSelect: {
      const PartSelect *sel = (PartSelect *)ts;
      const Expr *lexpr = sel->getLeftExpr();
      const Expr *rexpr = sel->getRightExpr();
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
      const Range *last_range = ranges->back();
      const Expr *lexpr = last_range->getLeftExpr();
      const Expr *rexpr = last_range->getRightExpr();
      int64_t lv =
          getValue(reduceExpr(lexpr, invalidValue, inst, pexpr, muteError));

      int64_t rv =
          getValue(reduceExpr(rexpr, invalidValue, inst, pexpr, muteError));

      if (lv > rv)
        bits = bits * (lv - rv + 1);
      else
        bits = bits * (rv - lv + 1);
    } else {
      for (const Range *ran : *ranges) {
        const Expr *lexpr = ran->getLeftExpr();
        const Expr *rexpr = ran->getRightExpr();
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

uint64_t ExprEval::size(const vpiHandle Typespec, bool &invalidValue,
                        const vpiHandle inst, const vpiHandle pexpr, bool full,
                        bool muteError) {
  const Any *vpiHandle_Typespec = (Any *)((uhdm_handle *)Typespec)->object;
  const Any *vpiHandle_inst =
      !inst ? nullptr : (Any *)((uhdm_handle *)inst)->object;
  const Any *vpiHandle_pexpr =
      !pexpr ? nullptr : (Any *)((uhdm_handle *)pexpr)->object;
  return size(vpiHandle_Typespec, invalidValue, vpiHandle_inst, vpiHandle_pexpr,
              full, muteError);
}

static bool getStringVal(std::string &result, Expr *val) {
  if (const Constant *hs0 = any_cast<Constant>(val)) {
    if (s_vpi_value *sval = String2VpiValue(hs0->getValue())) {
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

void resize(Expr *resizedExp, int32_t size) {
  bool invalidValue = false;
  ExprEval eval;
  Constant *c = (Constant *)resizedExp;
  int64_t val = eval.get_value(invalidValue, c);
  if (val == 1) {
    uint64_t mask = NumUtils::getMask(size);
    c->setValue("UINT:" + std::to_string(mask));
    c->setDecompile(std::to_string(mask));
    c->setConstType(vpiUIntConst);
  }
}

Expr *ExprEval::reduceCompOp(Operation *op, bool &invalidValue, const Any *inst,
                             const Any *pexpr, bool muteError) {
  Expr *result = op;
  Serializer &s = *op->getSerializer();
  AnyCollection &operands = *op->getOperands();
  int32_t optype = op->getOpType();
  std::string s0;
  std::string s1;
  Expr *reduc0 = reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
  Expr *reduc1 = reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
  if (invalidValue == true) {
    return result;
  }
  if (reduc0 == nullptr || reduc1 == nullptr) {
    return result;
  }
  int32_t size0 = reduc0->getSize();
  int32_t size1 = reduc1->getSize();
  if ((reduc0->getSize() == -1) && (reduc1->getSize() > 1)) {
    resize(reduc0, size1);
  } else if ((reduc1->getSize() == -1) && (reduc0->getSize() > 1)) {
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
    Constant *c = s.make<Constant>();
    c->setValue("BIN:" + std::to_string(val));
    c->setDecompile(std::to_string(val));
    c->setSize(1);
    c->setConstType(vpiBinaryConst);
    result = c;
  }
  return result;
}

uint64_t ExprEval::getWordSize(const Expr *exp, const Any *inst,
                               const Any *pexpr) {
  uint64_t wordSize = 1;
  bool invalidValue = false;
  bool muteError = true;
  if (exp == nullptr) {
    return wordSize;
  }
  if (const RefTypespec *ctsrt = exp->getTypespec()) {
    if (const Typespec *cts = ctsrt->getActual()) {
      if (cts->getUhdmType() == UhdmType::ArrayTypespec) {
        ArrayTypespec *atps = (ArrayTypespec *)cts;
        if (const RefTypespec *etsro = atps->getElemTypespec()) {
          cts = etsro->getActual();
        }
      }
      if (cts->getUhdmType() == UhdmType::LongIntTypespec) {
        wordSize = 64;
      } else if (cts->getUhdmType() == UhdmType::ShortIntTypespec) {
        wordSize = 16;
      } else if (cts->getUhdmType() == UhdmType::ByteTypespec) {
        wordSize = 8;
      } else if (cts->getUhdmType() == UhdmType::IntTypespec) {
        IntTypespec *icts = (IntTypespec *)cts;
        std::string_view value = icts->getValue();
        if (exp->getSize() > 32)
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
      } else if (cts->getUhdmType() == UhdmType::IntegerTypespec) {
        IntegerTypespec *icts = (IntegerTypespec *)cts;
        std::string_view value = icts->getValue();
        if (exp->getSize() > 32)
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
      } else if (cts->getUhdmType() == UhdmType::LogicTypespec) {
        LogicTypespec *icts = (LogicTypespec *)cts;
        if (const RefTypespec *rt = icts->getElemTypespec()) {
          wordSize = size(rt->getActual(), invalidValue, inst, pexpr, false,
                          muteError);
        }
      } else if (cts->getUhdmType() == UhdmType::BitTypespec) {
        BitTypespec *icts = (BitTypespec *)cts;
        wordSize = 1;
        if (RangeCollection *ranges = icts->getRanges()) {
          if (icts->getRanges()->size() > 1) {
            Range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            uint16_t lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getLeftExpr(), invalidValue,
                                              inst, pexpr, muteError)));
            uint16_t rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getRightExpr(), invalidValue,
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

Expr *ExprEval::reduceBitSelect(Expr *op, uint32_t index_val,
                                bool &invalidValue, const Any *inst,
                                const Any *pexpr, bool muteError) {
  Serializer &s = *op->getSerializer();
  Expr *result = nullptr;
  Expr *exp = reduceExpr(op, invalidValue, inst, pexpr, muteError);
  if (exp && (exp->getUhdmType() == UhdmType::Constant)) {
    Constant *cexp = (Constant *)exp;
    std::string binary = toBinary(cexp);
    uint64_t wordSize = getWordSize(cexp, inst, pexpr);
    Constant *c = s.make<Constant>();
    uint16_t lr = 0;
    uint16_t rr = 0;
    if (const RefTypespec *rt = exp->getTypespec()) {
      if (const Typespec *tps = rt->getActual()) {
        if (tps->getUhdmType() == UhdmType::LogicTypespec) {
          LogicTypespec *lts = (LogicTypespec *)tps;
          if (RangeCollection *ranges = lts->getRanges()) {
            Range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getLeftExpr(), invalidValue,
                                              inst, pexpr, muteError)));
            rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getRightExpr(), invalidValue,
                                              inst, pexpr, muteError)));
          }
        } else if (tps->getUhdmType() == UhdmType::IntTypespec) {
          IntTypespec *lts = (IntTypespec *)tps;
          if (RangeCollection *ranges = lts->getRanges()) {
            Range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getLeftExpr(), invalidValue,
                                              inst, pexpr, muteError)));
            rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getRightExpr(), invalidValue,
                                              inst, pexpr, muteError)));
          }
        } else if (tps->getUhdmType() == UhdmType::BitTypespec) {
          BitTypespec *lts = (BitTypespec *)tps;
          if (RangeCollection *ranges = lts->getRanges()) {
            Range *r = ranges->at(ranges->size() - 1);
            bool invalid = false;
            lr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getLeftExpr(), invalidValue,
                                              inst, pexpr, muteError)));
            rr = static_cast<uint16_t>(
                get_value(invalid, reduceExpr(r->getRightExpr(), invalidValue,
                                              inst, pexpr, muteError)));
          }
        }
      }
    }
    c->setSize(static_cast<int32_t>(wordSize));
    if (index_val < binary.size()) {
      // TODO: If Range does not start at 0
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
        if (const GenScopeArray *in = any_cast<GenScopeArray>(inst)) {
          fullPath = in->getFullName();
        } else if (inst && inst->getUhdmType() == UhdmType::Design) {
          fullPath = inst->getName();
        } else if (const Scope *spe = any_cast<Scope>(inst)) {
          fullPath = spe->getFullName();
        }
        if (muteError == false && m_muteError == false) {
          s.getErrorHandler()(ErrorType::UHDM_INTERNAL_ERROR_OUT_OF_BOUND,
                              fullPath, op, nullptr);
        }
        v = "0";
      }
      c->setValue("BIN:" + v);
      c->setDecompile(std::to_string(wordSize) + "'b" + v);
      c->setConstType(vpiBinaryConst);
    } else {
      c->setValue("BIN:0");
      c->setDecompile("1'b0");
      c->setConstType(vpiBinaryConst);
    }
    c->setFile(op->getFile());
    c->setStartLine(op->getStartLine());
    c->setStartColumn(op->getStartColumn());
    c->setEndLine(op->getEndLine());
    c->setEndColumn(op->getStartColumn() + 1);
    result = c;
  }
  return result;
}

int64_t ExprEval::get_value(bool &invalidValue, const Expr *Expr, bool strict) {
  int64_t result = 0;
  int32_t type = 0;
  std::string_view sv;
  if (const Constant *c = any_cast<Constant>(Expr)) {
    type = c->getConstType();
    sv = c->getValue();
  } else if (const Variable *v = any_cast<Variable>(Expr)) {
    if (uhdm::getTypespec<EnumTypespec>(v) != nullptr) {
      type = vpiUIntConst;
      sv = v->getValue();
    }
  } else {
    invalidValue = true;
  }
  if (!invalidValue) {
    switch (type) {
      case vpiBinaryConst: {
        if (Expr->getSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim_until(sv, '\'');
          sv = ltrim_until(sv, 's');
          sv = ltrim_until(sv, 'b');
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
        if (Expr->getSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim_until(sv, '\'');
          sv = ltrim_until(sv, 's');
          sv = ltrim_until(sv, 'h');
          sv.remove_prefix(std::string_view("HEX:").length());
          invalidValue = NumUtils::parseHex(sv, &result) == nullptr;
        }
        break;
      }
      case vpiOctConst: {
        if (Expr->getSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim_until(sv, '\'');
          sv = ltrim_until(sv, 's');
          sv = ltrim_until(sv, 'o');
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

uint64_t ExprEval::get_uvalue(bool &invalidValue, const Expr *expr,
                              bool strict) {
  uint64_t result = 0;
  int32_t type = 0;
  std::string_view sv;
  if (const Constant *c = any_cast<Constant>(expr)) {
    type = c->getConstType();
    sv = c->getValue();
  } else if (const Variable *v = any_cast<Variable>(expr)) {
    if (uhdm::getTypespec<EnumTypespec>(v) != nullptr) {
      type = vpiUIntConst;
      sv = v->getValue();
    }
  }
  if (sv.empty()) {
    invalidValue = true;
    return result;
  }
  if (!invalidValue) {
    switch (type) {
      case vpiBinaryConst: {
        if (expr->getSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim_until(sv, '\'');
          sv = ltrim_until(sv, 's');
          sv = ltrim_until(sv, 'b');
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
        if (expr->getSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim_until(sv, '\'');
          sv = ltrim_until(sv, 's');
          sv = ltrim_until(sv, 'h');
          sv.remove_prefix(std::string_view("HEX:").length());
          invalidValue = NumUtils::parseHex(sv, &result) == nullptr;
        }
        break;
      }
      case vpiOctConst: {
        if (expr->getSize() > 64) {
          invalidValue = true;
        } else {
          sv = ltrim_until(sv, '\'');
          sv = ltrim_until(sv, 's');
          sv = ltrim_until(sv, 'o');
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

TaskFunc *ExprEval::getTaskFunc(std::string_view name, const Any *inst) {
  if (getTaskFuncFunctor) {
    if (TaskFunc *result = getTaskFuncFunctor(name, inst)) {
      return result;
    }
  }
  if (inst == nullptr) {
    return nullptr;
  }
  const Any *root = inst;
  const Any *tmp = inst;
  while (tmp) {
    root = tmp;
    tmp = tmp->getParent();
  }
  const Design *des = any_cast<Design>(root);
  if (des) m_design = des;
  std::string_view the_name = name;
  const Any *the_instance = inst;
  if (m_design && (name.find("::") != std::string::npos)) {
    std::vector<std::string_view> res = tokenizeMulti(name, "::");
    if (res.size() > 1) {
      const std::string_view packName = res[0];
      const std::string_view varName = res[1];
      the_name = varName;
      Package *pack = nullptr;
      if (m_design->getTopPackages()) {
        for (auto p : *m_design->getTopPackages()) {
          if (p->getName() == packName) {
            pack = p;
            break;
          }
        }
      }
      the_instance = pack;
    }
  }
  while (the_instance) {
    TaskFuncCollection *task_funcs = nullptr;
    if (the_instance->getUhdmType() == UhdmType::GenScopeArray) {
    } else if (the_instance->getUhdmType() == UhdmType::Design) {
      task_funcs = ((Design *)the_instance)->getTaskFuncs();
    } else if (const Instance *inst = any_cast<Instance>(the_instance)) {
      task_funcs = inst->getTaskFuncs();
    }

    if (task_funcs) {
      for (TaskFunc *tf : *task_funcs) {
        if (tf->getName() == the_name) {
          return tf;
        }
      }
    }

    the_instance = the_instance->getParent();
  }

  return nullptr;
}

Any *ExprEval::decodeHierPath(HierPath *path, bool &invalidValue,
                              const Any *inst, const Any *pexpr,
                              bool returnTypespec, bool muteError) {
  Serializer &s = *path->getSerializer();
  std::string baseObject;
  if (!path->getPathElems()->empty()) {
    Any *firstElem = path->getPathElems()->at(0);
    baseObject = firstElem->getName();
  }
  Any *object = getObject(baseObject, inst, pexpr, muteError);
  if (object) {
    if (ParamAssign *passign = any_cast<ParamAssign>(object)) {
      object = passign->getRhs();
    }
  }
  if (object == nullptr) {
    object = getValue(baseObject, inst, pexpr, muteError);
  }
  if (object) {
    // Substitution
    if (ParamAssign *pass = any_cast<ParamAssign>(object)) {
      const Any *rhs = pass->getRhs();
      object = reduceExpr(rhs, invalidValue, inst, pexpr, muteError);
    } else if (BitSelect *bts = any_cast<BitSelect>(object)) {
      object = reduceExpr(bts, invalidValue, inst, pexpr, muteError);
    } else if (RefObj *ref = any_cast<RefObj>(object)) {
      object = reduceExpr(ref, invalidValue, inst, pexpr, muteError);
    } else if (Constant *cons = any_cast<Constant>(object)) {
      Elaborator elaborator(&s);
      object = elaborator.clone<>(cons, nullptr);
      cons = any_cast<Constant>(object);
      if (cons->getTypespec() == nullptr) {
        RefTypespec *rt = elaborator.clone<>(path->getTypespec(), cons);
        cons->setTypespec(rt);
      }
    } else if (Operation *oper = any_cast<Operation>(object)) {
      if (returnTypespec) {
        if (RefTypespec *rt = oper->getTypespec()) {
          object = rt->getActual();
        }
      }
    }

    std::vector<std::string> the_path;
    for (auto elem : *path->getPathElems()) {
      std::string_view elemName = elem->getName();
      elemName = rtrim_until(elemName, '[');
      the_path.emplace_back(elemName);
      if (elem->getUhdmType() == UhdmType::BitSelect) {
        BitSelect *select = (BitSelect *)elem;
        uint64_t baseIndex = get_value(
            invalidValue, reduceExpr((Any *)select->getIndex(), invalidValue,
                                     inst, pexpr, muteError));
        the_path.emplace_back("[" + std::to_string(baseIndex) + "]");
      }
    }

    return (Expr *)hierarchicalSelector(the_path, 0, object, invalidValue, inst,
                                        pexpr, returnTypespec, muteError);
  }
  return nullptr;
}

Any *ExprEval::hierarchicalSelector(std::vector<std::string> &select_path,
                                    uint32_t level, Any *object,
                                    bool &invalidValue, const Any *inst,
                                    const Any *pexpr, bool returnTypespec,
                                    bool muteError) {
  if (object == nullptr) return nullptr;
  Serializer &s = (object) ? *object->getSerializer() : *inst->getSerializer();
  if (level >= select_path.size()) {
    if (returnTypespec) {
      if (Typespec *tp = any_cast<Typespec>(object)) {
        return tp;
      } else if (Expr *ep = any_cast<Expr>(object)) {
        if (RefTypespec *rt = ep->getTypespec()) {
          return rt->getActual();
        }
      } else if (IODecl *id = any_cast<IODecl>(object)) {
        if (RefTypespec *rt = id->getTypespec()) {
          return rt->getActual();
        }
      }
      return nullptr;
    }
    return (Expr *)object;
  }
  std::string elemName = select_path[level];
  bool lastElem = (level == select_path.size() - 1);
  if (Variable *var = any_cast<Variable>(object)) {
    if (const RefTypespec *rt = var->getTypespec()) {
      if (const StructTypespec *stpt = rt->getActual<StructTypespec>()) {
        for (TypespecMember *member : *stpt->getMembers()) {
          if (member->getName() == elemName) {
            if (returnTypespec) {
              if (RefTypespec *mrt = member->getTypespec()) {
                Any *res = mrt->getActual();
                if (lastElem) {
                  return res;
                } else {
                  return hierarchicalSelector(select_path, level + 1, res,
                                              invalidValue, inst, pexpr,
                                              returnTypespec, muteError);
                }
              }
            } else {
              return member->getDefaultValue();
            }
          }
        }
      } else if (const ClassTypespec *ctps = rt->getActual<ClassTypespec>()) {
        const ClassDefn *defn = ctps->getClassDefn();
        while (defn) {
          if (defn->getVariables()) {
            for (Variable *member : *defn->getVariables()) {
              if (member->getName() == elemName) {
                if (returnTypespec) {
                  if (RefTypespec *mrt = member->getTypespec()) {
                    return mrt->getActual();
                  }
                } else {
                  return member;
                }
              }
            }
          }
          const ClassDefn *base_defn = nullptr;
          if (const Extends *ext = defn->getExtends()) {
            if (const RefTypespec *rt = ext->getClassTypespec()) {
              if (const ClassTypespec *tp = rt->getActual<ClassTypespec>()) {
                base_defn = tp->getClassDefn();
              }
            }
          }
          defn = base_defn;
        }
      } else if (returnTypespec) {
        if (RefTypespec *rt = var->getTypespec()) {
          if (ArrayTypespec *const at = rt->getActual<ArrayTypespec>()) {
            if (lastElem) {
              return at;
            } else {
              return hierarchicalSelector(select_path, level + 1, at,
                                          invalidValue, inst, pexpr,
                                          returnTypespec, muteError);
            }
          }
        }
      }
    }
  } else if (StructTypespec *stpt = any_cast<StructTypespec>(object)) {
    for (TypespecMember *member : *stpt->getMembers()) {
      if (member->getName() == elemName) {
        Any *res = nullptr;
        if (returnTypespec) {
          if (RefTypespec *mrt = member->getTypespec()) {
            Any *res = mrt->getActual();
            if (lastElem) {
              return res;
            } else {
              return hierarchicalSelector(select_path, level + 1, res,
                                          invalidValue, inst, pexpr,
                                          returnTypespec, muteError);
            }
          }
        } else {
          res = member->getDefaultValue();
        }
        if (lastElem) {
          return res;
        } else {
          return hierarchicalSelector(select_path, level + 1, res, invalidValue,
                                      inst, pexpr, returnTypespec, muteError);
        }
      }
    }
  } else if (IODecl *decl = any_cast<IODecl>(object)) {
    if (const Variable *exp = decl->getExpr<Variable>()) {
      if (const RefTypespec *const rt = exp->getTypespec()) {
        if (const StructTypespec *stpt = rt->getActual<StructTypespec>()) {
          for (TypespecMember *member : *stpt->getMembers()) {
            if (member->getName() == elemName) {
              if (returnTypespec) {
                if (RefTypespec *mrt = member->getTypespec()) {
                  Any *res = mrt->getActual();
                  if (lastElem) {
                    return res;
                  } else {
                    return hierarchicalSelector(select_path, level + 1, res,
                                                invalidValue, inst, pexpr,
                                                returnTypespec, muteError);
                  }
                }
              } else {
                return member->getDefaultValue();
              }
            }
          }
        }
      }
    }
    if (returnTypespec) {
      if (const RefTypespec *rt = decl->getTypespec()) {
        if (const Typespec *tps = rt->getActual()) {
          UhdmType ttps = tps->getUhdmType();
          if (ttps == UhdmType::StructTypespec) {
            StructTypespec *stpt = (StructTypespec *)tps;
            for (TypespecMember *member : *stpt->getMembers()) {
              if (member->getName() == elemName) {
                if (RefTypespec *mrt = member->getTypespec()) {
                  Any *res = mrt->getActual();
                  if (lastElem) {
                    return res;
                  } else {
                    return hierarchicalSelector(select_path, level + 1, res,
                                                invalidValue, inst, pexpr,
                                                returnTypespec, muteError);
                  }
                }
              }
            }
          } else if (ttps == UhdmType::ClassTypespec) {
            ClassTypespec *stpt = (ClassTypespec *)tps;
            const ClassDefn *defn = stpt->getClassDefn();
            while (defn) {
              if (defn->getVariables()) {
                for (Variable *member : *defn->getVariables()) {
                  if (member->getName() == elemName) {
                    if (RefTypespec *mrt = member->getTypespec()) {
                      return mrt->getActual();
                    }
                  }
                }
              }
              const ClassDefn *base_defn = nullptr;
              if (const Extends *ext = defn->getExtends()) {
                if (const RefTypespec *rt = ext->getClassTypespec()) {
                  if (const ClassTypespec *tp =
                          rt->getActual<ClassTypespec>()) {
                    base_defn = tp->getClassDefn();
                  }
                }
              }
              defn = base_defn;
            }
          }
        }
      }
    }
  } else if (Net *nt = any_cast<Net>(object)) {
    TypespecMemberCollection *members = nullptr;
    if (const StructTypespec *sts = uhdm::getTypespec<StructTypespec>(nt)) {
      members = sts->getMembers();
    } else if (const UnionTypespec *uts =
                   uhdm::getTypespec<UnionTypespec>(nt)) {
      members = uts->getMembers();
    }
    if (members) {
      for (TypespecMember *member : *members) {
        if (member->getName() == elemName) {
          if (returnTypespec) {
            if (RefTypespec *mrt = member->getTypespec()) {
              Any *res = mrt->getActual();
              if (lastElem) {
                return res;
              } else {
                return hierarchicalSelector(select_path, level + 1, res,
                                            invalidValue, inst, pexpr,
                                            returnTypespec, muteError);
              }
            }
          } else {
            return member->getDefaultValue();
          }
        }
      }
    }
  } else if (Constant *cons = any_cast<Constant>(object)) {
    if (RefTypespec *rt = cons->getTypespec()) {
      if (const Typespec *ts = rt->getActual()) {
        UhdmType ttps = ts->getUhdmType();
        if (ttps == UhdmType::StructTypespec) {
          StructTypespec *stpt = (StructTypespec *)ts;
          uint64_t from = 0;
          uint64_t width = 0;
          for (TypespecMember *member : *stpt->getMembers()) {
            if (member->getName() == elemName) {
              width = size(member, invalidValue, inst, pexpr, true);
              if (cons->getSize() <= 64) {
                uint64_t iv = get_value(invalidValue, cons);
                uint64_t mask = 0;

                for (uint64_t i = from; i < uint64_t(from + width); i++) {
                  mask |= ((uint64_t)1 << i);
                }
                uint64_t res = iv & mask;
                res = res >> (from);
                cons->setValue("UINT:" + std::to_string(res));
                cons->setSize(static_cast<int32_t>(width));
                cons->setConstType(vpiUIntConst);
                return cons;
              } else {
                std::string_view val = cons->getValue();
                int32_t ctype = cons->getConstType();
                if (ctype == vpiHexConst) {
                  std::string_view vval =
                      val.substr(strlen("HEX:"), std::string::npos);
                  std::string bin = NumUtils::hexToBin(vval);
                  std::string res = bin.substr(from, width);
                  cons->setValue("BIN:" + res);
                  cons->setSize(static_cast<int32_t>(width));
                  cons->setConstType(vpiBinaryConst);
                  return cons;
                } else if (ctype == vpiBinaryConst) {
                  std::string_view bin =
                      val.substr(strlen("BIN:"), std::string::npos);
                  std::string_view res = bin.substr(from, width);
                  cons->setValue("BIN:" + std::string(res));
                  cons->setSize(static_cast<int32_t>(width));
                  cons->setConstType(vpiBinaryConst);
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
    std::string_view indexName = ltrim_until(elemName, '[');
    indexName = rtrim_until(indexName, ']');
    if (NumUtils::parseInt32(indexName, &selectIndex) == nullptr) {
      selectIndex = -1;
    }
    elemName.clear();
    if (const Operation *oper = any_cast<Operation>(object)) {
      int32_t opType = oper->getOpType();
      if (opType == vpiAssignmentPatternOp) {
        AnyCollection *operands = oper->getOperands();
        int32_t sInd = 0;
        for (auto operand : *operands) {
          if ((selectIndex >= 0) && (sInd == selectIndex)) {
            return hierarchicalSelector(select_path, level + 1, operand,
                                        invalidValue, inst, pexpr,
                                        returnTypespec, muteError);
          }
          sInd++;
        }
      }
    } else if (const LogicTypespec *ltps = any_cast<LogicTypespec>(object)) {
      RangeCollection *ranges = ltps->getRanges();
      if (ranges && (ranges->size() >= 2)) {
        LogicTypespec *tmp = s.make<LogicTypespec>();
        RangeCollection *tmpR = s.makeCollection<Range>();
        for (uint32_t i = 1; i < ranges->size(); i++) {
          tmpR->emplace_back(ranges->at(i));
        }
        tmp->setRanges(tmpR);
        return tmp;
      }
    } else if (const ArrayTypespec *ltps = any_cast<ArrayTypespec>(object)) {
      if (const RefTypespec *rt = ltps->getElemTypespec()) {
        return (Typespec *)rt->getActual();
      }
    } else if (Constant *c = any_cast<Constant>(object)) {
      if (Expr *tmp = reduceBitSelect(c, selectIndex, invalidValue, inst, pexpr,
                                      muteError)) {
        if (returnTypespec) {
          if (RefTypespec *rt = tmp->getTypespec()) {
            return rt->getActual();
          }
          return nullptr;
        }
        return tmp;
      }
      return object;
    }
  } else if (level == 0) {
    return hierarchicalSelector(select_path, level + 1, object, invalidValue,
                                inst, pexpr, returnTypespec, muteError);
  }

  if (const Operation *oper = any_cast<Operation>(object)) {
    int32_t opType = oper->getOpType();

    if (opType == vpiAssignmentPatternOp) {
      AnyCollection *operands = oper->getOperands();
      Any *defaultPattern = nullptr;
      int32_t sInd = 0;

      int32_t bIndex = -1;
      if (inst) {
        /*
        Any *baseP = nullptr;
        AnyCollection *parameters = nullptr;
        if (inst->getUhdmType() == UhdmType::GenScopeArray) {
        } else if (inst->getUhdmType() == UhdmType::Design) {
          parameters = ((Design *)inst)->Parameters();
        } else if (any_cast<scope *>(inst)) {
          parameters = ((scope *)inst)->Parameters();
        }
        if (parameters) {
          for (auto p : *parameters) {
            if (p->getName() == select_path[0]) {
              baseP = p;
              break;
            }
          }
        }
        */
        if (Any *baseP = getObject(select_path[0], inst, pexpr, muteError)) {
          const Typespec *tps = nullptr;
          if (Parameter *p = any_cast<Parameter>(baseP)) {
            if (const RefTypespec *rt = p->getTypespec()) {
              tps = rt->getActual();
            }
          } else if (Operation *op = any_cast<Operation>(baseP)) {
            if (const RefTypespec *rt = op->getTypespec()) {
              tps = rt->getActual();
            }
          }

          if (tps && (tps->getUhdmType() == UhdmType::ArrayTypespec)) {
            ArrayTypespec *tmp = (ArrayTypespec *)tps;
            if (const RefTypespec *rt = tmp->getElemTypespec()) {
              tps = rt->getActual();
            }
          }
          if (tps && (tps->getUhdmType() == UhdmType::StructTypespec)) {
            StructTypespec *sts = (StructTypespec *)tps;
            if (TypespecMemberCollection *members = sts->getMembers()) {
              uint32_t i = 0;
              for (TypespecMember *member : *members) {
                if (member->getName() == elemName) {
                  bIndex = i;
                  break;
                }
                i++;
              }
            }
          }
        }
      }
      if (inst) {
        const Any *tmpInstance = inst;
        while ((bIndex == -1) && tmpInstance) {
          ParamAssignCollection *ParamAssigns = nullptr;
          if (tmpInstance->getUhdmType() == UhdmType::GenScopeArray) {
          } else if (tmpInstance->getUhdmType() == UhdmType::Design) {
            ParamAssigns = ((Design *)tmpInstance)->getParamAssigns();
          } else if (const Scope *spe = any_cast<Scope>(tmpInstance)) {
            ParamAssigns = spe->getParamAssigns();
          }
          if (ParamAssigns) {
            for (ParamAssign *param : *ParamAssigns) {
              if (param && param->getLhs()) {
                const std::string_view param_name = param->getLhs()->getName();
                if (param_name == select_path[0]) {
                  if (const Parameter *p =
                          any_cast<Parameter>(param->getLhs())) {
                    if (const RefTypespec *rt = p->getTypespec()) {
                      if (const Typespec *tps = rt->getActual()) {
                        if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
                          if (const RefTypespec *ert =
                                  ((ArrayTypespec *)tps)->getElemTypespec()) {
                            tps = ert->getActual();
                          }
                        }
                        if (tps &&
                            (tps->getUhdmType() == UhdmType::StructTypespec)) {
                          StructTypespec *sts = (StructTypespec *)tps;
                          if (TypespecMemberCollection *members =
                                  sts->getMembers()) {
                            uint32_t i = 0;
                            for (TypespecMember *member : *members) {
                              if (member->getName() == elemName) {
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
          tmpInstance = tmpInstance->getParent();
        }
      }
      for (auto operand : *operands) {
        UhdmType operandType = operand->getUhdmType();
        if (operandType == UhdmType::TaggedPattern) {
          TaggedPattern *tpatt = (TaggedPattern *)operand;
          const Typespec *tps = nullptr;
          if (const RefTypespec *rt = tpatt->getTypespec()) {
            tps = rt->getActual();
          }
          if (tps->getName() == "default") {
            defaultPattern = (Any *)tpatt->getPattern();
          }
          if (!elemName.empty() && (tps->getName() == elemName)) {
            const Any *patt = tpatt->getPattern();
            UhdmType pattType = patt->getUhdmType();
            if (pattType == UhdmType::Constant) {
              Any *ex = reduceExpr((Expr *)patt, invalidValue, inst, pexpr,
                                   muteError);
              if (level < select_path.size()) {
                ex = hierarchicalSelector(select_path, level + 1, ex,
                                          invalidValue, inst, pexpr,
                                          returnTypespec);
              }
              if (returnTypespec) {
                if (Typespec *tp = any_cast<Typespec>(ex)) {
                  return tp;
                } else if (Expr *ep = any_cast<Expr>(ex)) {
                  if (RefTypespec *rt = ep->getTypespec()) {
                    return rt->getActual();
                  }
                } else if (IODecl *id = any_cast<IODecl>(ex)) {
                  if (RefTypespec *rt = id->getTypespec()) {
                    return rt->getActual();
                  }
                } else if (Typespec *tp = any_cast<Typespec>(object)) {
                  return tp;
                } else if (Expr *ep = any_cast<Expr>(object)) {
                  if (RefTypespec *rt = ep->getTypespec()) {
                    return rt->getActual();
                  }
                } else if (IODecl *id = any_cast<IODecl>(object)) {
                  if (RefTypespec *rt = id->getTypespec()) {
                    return rt->getActual();
                  }
                }
                return nullptr;
              }
              return ex;
            } else if (pattType == UhdmType::Operation) {
              return hierarchicalSelector(select_path, level + 1, (Expr *)patt,
                                          invalidValue, inst, pexpr,
                                          returnTypespec);
            }
          }
        } else if (operandType == UhdmType::Constant) {
          if ((bIndex >= 0) && (bIndex == sInd)) {
            return hierarchicalSelector(select_path, level + 1, (Expr *)operand,
                                        invalidValue, inst, pexpr,
                                        returnTypespec);
          }
        }
        sInd++;
      }
      if (defaultPattern) {
        if (Expr *ex = any_cast<Expr>(defaultPattern)) {
          ex = reduceExpr(ex, invalidValue, inst, pexpr, muteError);
          if (returnTypespec) {
            if (Typespec *tp = any_cast<Typespec>(ex)) {
              return tp;
            } else if (Expr *ep = any_cast<Expr>(ex)) {
              if (RefTypespec *rt = ep->getTypespec()) {
                return rt->getActual();
              }
            } else if (IODecl *id = any_cast<IODecl>(ex)) {
              if (RefTypespec *rt = id->getTypespec()) {
                return rt->getActual();
              }
            } else if (Typespec *tp = any_cast<Typespec>(object)) {
              return tp;
            } else if (Expr *ep = any_cast<Expr>(object)) {
              if (RefTypespec *rt = ep->getTypespec()) {
                return rt->getActual();
              }
            } else if (IODecl *id = any_cast<IODecl>(object)) {
              if (RefTypespec *rt = id->getTypespec()) {
                return rt->getActual();
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

Expr *ExprEval::reduceExpr(const Any *result, bool &invalidValue,
                           const Any *inst, const Any *pexpr, bool muteError) {
  if (!result) return nullptr;
  Serializer &s = *result->getSerializer();
  UhdmType objtype = result->getUhdmType();
  if (objtype == UhdmType::Operation) {
    Operation *op = (Operation *)result;
    for (auto t : m_skipOperationTypes) {
      if (op->getOpType() == t) {
        return (Expr *)result;
      }
    }
    bool constantOperands = true;
    if (AnyCollection *oprns = op->getOperands()) {
      AnyCollection &operands = *oprns;
      for (auto oper : operands) {
        UhdmType optype = oper->getUhdmType();
        if (optype == UhdmType::RefObj) {
          RefObj *ref = (RefObj *)oper;
          const std::string_view name = ref->getName();
          if ((name == "default") && ref->getStructMember()) continue;
          if (getValue(name, inst, pexpr, muteError, result) == nullptr) {
            constantOperands = false;
            break;
          }
        } else if (optype == UhdmType::Operation) {
        } else if (optype == UhdmType::SysFuncCall) {
        } else if (optype == UhdmType::FuncCall) {
        } else if (optype == UhdmType::BitSelect) {
        } else if (optype == UhdmType::HierPath) {
        } else if (optype == UhdmType::VarSelect) {
        } else if (optype != UhdmType::Constant) {
          constantOperands = false;
          break;
        } else if (optype == UhdmType::Variable) {
          if (uhdm::getTypespec<EnumTypespec>(oper) != nullptr) {
            constantOperands = false;
            break;
          }
        }
      }
      if (constantOperands) {
        int32_t optype = op->getOpType();
        switch (optype) {
          case vpiArithRShiftOp:
          case vpiRShiftOp: {
            if (operands.size() == 2) {
              Expr *arg0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              if (arg0 && arg0->getUhdmType() == UhdmType::Constant) {
                Constant *c = (Constant *)arg0;
                if (c->getSize() == -1) invalidValue = true;
              }
              int64_t val0 = get_value(invalidValue, arg0);
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) >> ((uint64_t)val1);
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val));
              c->setDecompile(std::to_string(val));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Expr *reduc0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val = get_value(invalidValueI, reduc0);
              if ((invalidValue == false) && (invalidValueI == false)) {
                if (op->getOpType() == vpiPostIncOp ||
                    op->getOpType() == vpiPreIncOp) {
                  val++;
                } else {
                  val--;
                }
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(64);
                c->setConstType(vpiIntConst);
                result = c;
                std::map<std::string, const Typespec *> local_vars;
                setValueInInstance(operands[0]->getName(), operands[0], c,
                                   invalidValue, s, inst, op, local_vars, 0,
                                   muteError);
              } else {
                invalidValueD = false;
                long double val = get_double(invalidValueD, reduc0);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  if (op->getOpType() == vpiPostIncOp ||
                      op->getOpType() == vpiPreIncOp) {
                    val++;
                  } else {
                    val--;
                  }
                  Constant *c = s.make<Constant>();
                  c->setValue("REAL:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiRealConst);
                  result = c;
                  std::map<std::string, const Typespec *> local_vars;
                  setValueInInstance(operands[0]->getName(), operands[0], c,
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
              Expr *arg0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              if (arg0 && arg0->getUhdmType() == UhdmType::Constant) {
                Constant *c = (Constant *)arg0;
                if (c->getSize() == -1) invalidValue = true;
              }
              int64_t val0 = get_value(invalidValue, arg0);
              int64_t val1 =
                  get_value(invalidValue, reduceExpr(operands[1], invalidValue,
                                                     inst, pexpr, muteError));
              if (invalidValue) break;
              uint64_t val = ((uint64_t)val0) << ((uint64_t)val1);
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val));
              c->setDecompile(std::to_string(val));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiAddOp:
          case vpiPlusOp: {
            if (operands.size() == 2) {
              Expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              Expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
              bool unsignedOperation = true;
              for (auto exp : {expr0, expr1}) {
                if (exp) {
                  if (exp->getUhdmType() == UhdmType::Constant) {
                    Constant *c = (Constant *)exp;
                    if (c->getConstType() == vpiIntConst ||
                        c->getConstType() == vpiStringConst ||
                        c->getConstType() == vpiRealConst ||
                        c->getConstType() == vpiDecConst) {
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
                  Constant *c = s.make<Constant>();
                  c->setValue("UINT:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize((expr0->getSize() > expr1->getSize())
                                 ? expr0->getSize()
                                 : expr1->getSize());
                  c->setConstType(vpiUIntConst);
                  result = c;
                }
              } else {
                int64_t val0 = get_value(invalidValueI, expr0);
                int64_t val1 = get_value(invalidValueI, expr1);
                if ((invalidValue == false) && (invalidValueI == false)) {
                  int64_t val = val0 + val1;
                  Constant *c = s.make<Constant>();
                  c->setValue("INT:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize((expr0->getSize() > expr1->getSize())
                                 ? expr0->getSize()
                                 : expr1->getSize());
                  c->setConstType(vpiIntConst);
                  result = c;
                } else {
                  invalidValueD = false;
                  long double val0 = get_double(invalidValueD, expr0);
                  long double val1 = get_double(invalidValueD, expr1);
                  if ((invalidValue == false) && (invalidValueD == false)) {
                    long double val = val0 + val1;
                    Constant *c = s.make<Constant>();
                    c->setValue("REAL:" + std::to_string(val));
                    c->setDecompile(std::to_string(val));
                    c->setSize((expr0->getSize() > expr1->getSize())
                                   ? expr0->getSize()
                                   : expr1->getSize());
                    c->setConstType(vpiRealConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val));
              c->setDecompile(std::to_string(val));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val));
              c->setDecompile(std::to_string(val));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val));
              c->setDecompile(std::to_string(val));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val));
              c->setDecompile(std::to_string(val));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiMinusOp: {
            if (operands.size() == 1) {
              bool invalidValueI = false;
              bool invalidValueD = false;
              Expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              int64_t val0 = get_value(invalidValueI, expr0);
              if ((invalidValue == false) && (invalidValueI == false)) {
                int64_t val = -val0;
                uint64_t size = 64;
                if (expr0->getUhdmType() == UhdmType::Constant) {
                  size = expr0->getSize();
                }
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(static_cast<int32_t>(size));
                c->setConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = -val0;
                  Constant *c = s.make<Constant>();
                  c->setValue("REAL:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiRealConst);
                  result = c;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiSubOp: {
            if (operands.size() == 2) {
              Expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              Expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              if ((invalidValue == false) && (invalidValueI == false)) {
                int64_t val = val0 - val1;
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(64);
                c->setConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = val0 - val1;
                  Constant *c = s.make<Constant>();
                  c->setValue("REAL:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiRealConst);
                  result = c;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiMultOp: {
            if (operands.size() == 2) {
              Expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              Expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              if ((invalidValue == false) && (invalidValueI == false)) {
                int64_t val = val0 * val1;
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(64);
                c->setConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = val0 * val1;
                  Constant *c = s.make<Constant>();
                  c->setValue("REAL:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiRealConst);
                  result = c;
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiBitNegOp: {
            if (operands.size() == 1) {
              Expr *operand =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              if (operand) {
                uint64_t val = (uint64_t)get_value(invalidValue, operand);
                if (invalidValue) break;
                uint64_t size = 64;
                if (operand->getUhdmType() == UhdmType::Constant) {
                  Constant *c = (Constant *)operand;
                  size = c->getSize();
                  if (const RefTypespec *rt = c->getTypespec()) {
                    if (const Typespec *tps = rt->getActual()) {
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

                Constant *c = s.make<Constant>();
                c->setValue("UINT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(static_cast<int32_t>(size));
                c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val));
              c->setDecompile(std::to_string(val));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
                  Constant *c = s.make<Constant>();
                  c->setValue("UINT:1");
                  c->setDecompile(std::to_string(1));
                  c->setSize(64);
                  c->setConstType(vpiUIntConst);
                  result = c;
                  break;
                }
              }
            }
            break;
          }
          case vpiUnaryAndOp: {
            if (operands.size() == 1) {
              Constant *cst = (Constant *)(reduceExpr(operands[0], invalidValue,
                                                      inst, pexpr, muteError));
              uint64_t val = get_value(invalidValue, cst);
              if (invalidValue) break;
              uint64_t res = val & 1;
              for (int32_t i = 1; i < cst->getSize(); i++) {
                res = res & ((val & (1ULL << i)) >> i);
              }
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(res));
              c->setDecompile(std::to_string(res));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(res));
              c->setDecompile(std::to_string(res));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(res));
              c->setDecompile(std::to_string(res));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(res));
              c->setDecompile(std::to_string(res));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(res));
              c->setDecompile(std::to_string(res));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(res));
              c->setDecompile(std::to_string(res));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
              result = c;
            }
            break;
          }
          case vpiModOp: {
            if (operands.size() == 2) {
              Expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              Expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              int64_t val = 0;
              if (val1 && (invalidValue == false) && (invalidValueI == false)) {
                val = val0 % val1;
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(64);
                c->setConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if (val1 && (invalidValue == false) &&
                    (invalidValueD == false)) {
                  long double val = 0;
                  val = std::fmod(val0, val1);
                  Constant *c = s.make<Constant>();
                  c->setValue("REAL:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiRealConst);
                  result = c;
                }
                if ((val1 == 0) && (invalidValue == false) &&
                    (invalidValueD == false)) {
                  // Divide by 0
                  std::string fullPath;
                  if (const GenScopeArray *in = any_cast<GenScopeArray>(inst)) {
                    fullPath = in->getFullName();
                  } else if (inst && inst->getUhdmType() == UhdmType::Design) {
                    fullPath = inst->getName();
                  } else if (const Scope *spe = any_cast<Scope>(inst)) {
                    fullPath = spe->getFullName();
                  }
                  if (muteError == false && m_muteError == false)
                    s.getErrorHandler()(ErrorType::UHDM_DIVIDE_BY_ZERO,
                                        fullPath, expr1, nullptr);
                }
              }
              if (invalidValueI && invalidValueD) invalidValue = true;
            }
            break;
          }
          case vpiPowerOp: {
            if (operands.size() == 2) {
              Expr *expr0 =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              Expr *expr1 =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t val0 = get_value(invalidValueI, expr0);
              int64_t val1 = get_value(invalidValueI, expr1);
              int64_t val = 0;
              if ((invalidValue == false) && (invalidValueI == false)) {
                val = static_cast<int64_t>(std::pow<int64_t>(val0, val1));
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(64);
                c->setConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double val0 = get_double(invalidValueD, expr0);
                long double val1 = get_double(invalidValueD, expr1);
                if ((invalidValue == false) && (invalidValueD == false)) {
                  long double val = 0;
                  val = pow(val0, val1);
                  Constant *c = s.make<Constant>();
                  c->setValue("REAL:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiRealConst);
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
              Expr *div_expr =
                  reduceExpr(operands[1], invalidValue, inst, pexpr, muteError);
              Expr *num_expr =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              bool invalidValueI = false;
              bool invalidValueD = false;
              int64_t divisor = get_value(invalidValueI, div_expr);
              int64_t num = get_value(invalidValueI, num_expr);
              if (divisor && (invalidValue == false) &&
                  (invalidValueI == false)) {
                divideByZero = false;
                int64_t val = num / divisor;
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(val));
                c->setDecompile(std::to_string(val));
                c->setSize(64);
                c->setConstType(vpiIntConst);
                result = c;
              } else {
                invalidValueD = false;
                long double divisor = get_double(invalidValueD, div_expr);
                long double num = get_double(invalidValueD, num_expr);
                if (divisor && (invalidValue == false) &&
                    (invalidValueD == false)) {
                  divideByZero = false;
                  long double val = num / divisor;
                  Constant *c = s.make<Constant>();
                  c->setValue("REAL:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiRealConst);
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
                if (const GenScopeArray *in = any_cast<GenScopeArray>(inst)) {
                  fullPath = in->getFullName();
                } else if (inst && inst->getUhdmType() == UhdmType::Design) {
                  fullPath = inst->getName();
                } else if (const Scope *spe = any_cast<Scope>(inst)) {
                  fullPath = spe->getFullName();
                }
                if (muteError == false && m_muteError == false)
                  s.getErrorHandler()(ErrorType::UHDM_DIVIDE_BY_ZERO, fullPath,
                                      div_expr, nullptr);
              }
            }
            break;
          }
          case vpiConditionOp: {
            if (operands.size() == 3) {
              bool localInvalidValue = false;
              Expr *cond =
                  reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
              int64_t condVal = get_value(invalidValue, cond);
              if (invalidValue) break;
              int64_t val = 0;
              Expr *the_val = nullptr;
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
                  Constant *c = s.make<Constant>();
                  c->setValue("INT:" + std::to_string(val));
                  c->setDecompile(std::to_string(val));
                  c->setSize(64);
                  c->setConstType(vpiIntConst);
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
              Expr *cv = (Expr *)(operands[1]);
              if (cv->getUhdmType() != UhdmType::Constant) {
                cv = reduceExpr(cv, invalidValue, inst, pexpr, muteError);
                if (cv->getUhdmType() != UhdmType::Constant) {
                  break;
                }
              }
              Constant *c = s.make<Constant>();
              int64_t width = cv->getSize();
              int32_t consttype = ((Constant *)cv)->getConstType();
              c->setConstType(consttype);
              if (consttype == vpiBinaryConst) {
                std::string_view val = cv->getValue();
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
                c->setValue("BIN:" + res);
                c->setDecompile(res);
              } else if (consttype == vpiHexConst) {
                std::string_view val = cv->getValue();
                val.remove_prefix(std::string_view("HEX:").length());
                std::string res;
                for (uint32_t i = 0; i < n; i++) {
                  res += val;
                }
                c->setValue("HEX:" + res);
                c->setDecompile(res);
              } else if (consttype == vpiOctConst) {
                std::string_view val = cv->getValue();
                val.remove_prefix(std::string_view("OCT:").length());
                std::string res;
                for (uint32_t i = 0; i < n; i++) {
                  res += val;
                }
                c->setValue("OCT:" + res);
                c->setDecompile(res);
              } else if (consttype == vpiStringConst) {
                std::string_view val = cv->getValue();
                val.remove_prefix(std::string_view("STRING:").length());
                std::string res;
                for (uint32_t i = 0; i < n; i++) {
                  res += val;
                }
                c->setValue("STRING:" + res);
                c->setDecompile(res);
              } else {
                uint64_t val = get_value(invalidValue, cv);
                if (invalidValue) break;
                uint64_t res = 0;
                for (uint32_t i = 0; i < n; i++) {
                  res |= val << (i * width);
                }
                c->setValue("UINT:" + std::to_string(res));
                c->setDecompile(std::to_string(res));
                c->setConstType(vpiUIntConst);
              }
              c->setSize(static_cast<int32_t>(n * width));
              // Word size
              if (width) {
                IntTypespec *ts = s.make<IntTypespec>();
                ts->setValue("UINT:" + std::to_string(width));
                RefTypespec *rt = s.make<RefTypespec>();
                rt->setActual(ts);
                rt->setParent(c);
                c->setTypespec(rt);
              }
              result = c;
            }
            break;
          }
          case vpiConcatOp: {
            Constant *c1 = s.make<Constant>();
            std::string cval;
            int32_t csize = 0;
            bool stringVal = false;
            for (uint32_t i = 0; i < operands.size(); i++) {
              Any *oper = operands[i];
              UhdmType optype = oper->getUhdmType();
              int32_t operType = 0;
              if (optype == UhdmType::Operation) {
                Operation *o = (Operation *)oper;
                operType = o->getOpType();
              }
              if ((optype != UhdmType::Constant) && (operType != vpiConcatOp) &&
                  (operType != vpiMultiAssignmentPatternOp) &&
                  (operType != vpiAssignmentPatternOp)) {
                if (Expr *tmp = reduceExpr(oper, invalidValue, inst, pexpr,
                                           muteError)) {
                  oper = tmp;
                }
                optype = oper->getUhdmType();
              }
              if (optype == UhdmType::Constant) {
                Constant *c2 = (Constant *)oper;
                std::string_view sv = c2->getValue();
                int32_t size = c2->getSize();
                csize += size;
                int32_t type = c2->getConstType();
                switch (type) {
                  case vpiBinaryConst: {
                    sv.remove_prefix(std::string_view("BIN:").length());
                    std::string value;
                    if (size > (int32_t)sv.size()) {
                      value.append(size - sv.size(), '0');
                    }
                    if (op->getReordered()) {
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
                    if (op->getReordered()) {
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
                    if (op->getReordered()) {
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
                    if (op->getReordered()) {
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
                      if (op->getReordered()) {
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
                      if (op->getReordered()) {
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
                      if (op->getReordered()) {
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
                      if (op->getReordered()) {
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
                c1->setValue("STRING:" + cval);
                c1->setSize(static_cast<int32_t>(cval.size() * 8));
                c1->setConstType(vpiStringConst);
              } else {
                if (op->getReordered()) {
                  std::reverse(cval.begin(), cval.end());
                }
                if (cval.size() > UHDM_MAX_BIT_WIDTH) {
                  std::string fullPath;
                  if (const GenScopeArray *in = any_cast<GenScopeArray>(inst)) {
                    fullPath = in->getFullName();
                  } else if (inst && inst->getUhdmType() == UhdmType::Design) {
                    fullPath = inst->getName();
                  } else if (const Scope *spe = any_cast<Scope>(inst)) {
                    fullPath = spe->getFullName();
                  }
                  if (muteError == false && m_muteError == false)
                    s.getErrorHandler()(
                        ErrorType::UHDM_INTERNAL_ERROR_OUT_OF_BOUND, fullPath,
                        op, nullptr);
                  cval = "0";
                }
                c1->setValue("BIN:" + cval);
                c1->setSize(csize);
                c1->setConstType(vpiBinaryConst);
              }
              result = c1;
            }
            break;
          }
          case vpiCastOp: {
            Expr *oper =
                reduceExpr(operands[0], invalidValue, inst, pexpr, muteError);
            uint64_t val0 = get_value(invalidValue, oper);
            if (invalidValue) break;
            const Typespec *tps = nullptr;
            if (const RefTypespec *rt = op->getTypespec()) {
              tps = rt->getActual();
            }
            if (tps == nullptr) break;
            UhdmType ttps = tps->getUhdmType();
            if (ttps == UhdmType::IntTypespec) {
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string((int32_t)val0));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
              result = c;
            } else if (ttps == UhdmType::LongIntTypespec) {
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string((int64_t)val0));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
              result = c;
            } else if (ttps == UhdmType::ShortIntTypespec) {
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string((int16_t)val0));
              c->setSize(16);
              c->setConstType(vpiUIntConst);
              result = c;
            } else if (ttps == UhdmType::IntegerTypespec) {
              IntegerTypespec *itps = (IntegerTypespec *)tps;
              std::string_view val = itps->getValue();
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
              Constant *c = s.make<Constant>();
              uint64_t mask = ((uint64_t)(1ULL << cast_to)) - 1ULL;
              uint64_t res = val0 & mask;
              c->setValue("UINT:" + std::to_string(res));
              c->setSize(static_cast<int32_t>(cast_to));
              c->setConstType(vpiUIntConst);
              result = c;
            } else if (ttps == UhdmType::EnumTypespec) {
              // TODO: Should check the value is in Range of the enum and
              // issue error if not
              Constant *c = s.make<Constant>();
              c->setValue("UINT:" + std::to_string(val0));
              c->setSize(64);
              c->setConstType(vpiUIntConst);
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
    return (Expr *)result;
  } else if (objtype == UhdmType::Constant) {
    return (Expr *)result;
  } else if (objtype == UhdmType::SysFuncCall) {
    SysFuncCall *scall = (SysFuncCall *)result;
    const std::string_view name = scall->getName();
    if ((name == "$bits") || (name == "$size") || (name == "$high") ||
        (name == "$low") || (name == "$left") || (name == "$right")) {
      uint64_t bits = 0;
      bool found = false;
      for (auto arg : *scall->getArguments()) {
        UhdmType argtype = arg->getUhdmType();
        if (argtype == UhdmType::RefObj) {
          RefObj *ref = (RefObj *)arg;
          const std::string_view objname = ref->getName();
          Any *object = getObject(objname, inst, pexpr, muteError);
          if (object == nullptr) {
            if (inst && inst->getUhdmType() == UhdmType::Package) {
              std::string name(inst->getName());
              name.append("::").append(objname);
              object = getObject(name, inst, pexpr, muteError);
            }
          }
          if (object) {
            if (ParamAssign *passign = any_cast<ParamAssign>(object)) {
              object = passign->getRhs();
            }
          }
          if (object == nullptr) {
            object = getValue(objname, inst, pexpr, muteError);
          }
          const Typespec *tps = nullptr;
          if (Expr *exp = any_cast<Expr>(object)) {
            if (RefTypespec *rt = exp->getTypespec()) {
              tps = rt->getActual();
            }
          } else if (Typespec *tp = any_cast<Typespec>(object)) {
            tps = tp;
          }

          if ((tps != nullptr) &&
              (tps->getUhdmType() == UhdmType::ArrayTypespec)) {
            // Size the object, not its Typespec
            tps = nullptr;
          }

          if ((name == "$high") || (name == "$low") || (name == "$left") ||
              (name == "$right")) {
            RangeCollection *ranges = nullptr;
            if (tps) {
              switch (tps->getUhdmType()) {
                case UhdmType::BitTypespec: {
                  BitTypespec *bts = (BitTypespec *)tps;
                  ranges = bts->getRanges();
                  break;
                }
                case UhdmType::IntTypespec: {
                  IntTypespec *bts = (IntTypespec *)tps;
                  ranges = bts->getRanges();
                  break;
                }
                case UhdmType::LogicTypespec: {
                  LogicTypespec *bts = (LogicTypespec *)tps;
                  ranges = bts->getRanges();
                  break;
                }
                case UhdmType::ArrayTypespec: {
                  ArrayTypespec *bts = (ArrayTypespec *)tps;
                  ranges = bts->getRanges();
                  break;
                }
                default:
                  break;
              }
            }
            if (ranges) {
              Range *r = ranges->at(0);
              Expr *lr = r->getLeftExpr();
              Expr *rr = r->getRightExpr();
              bool invalidValue = false;
              lr = reduceExpr(lr, invalidValue, inst, pexpr, muteError);
              ExprEval eval;
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
        } else if (argtype == UhdmType::Operation) {
          Operation *oper = (Operation *)arg;
          if (oper->getOpType() == vpiConcatOp) {
            for (auto op : *oper->getOperands()) {
              bits += size(op, invalidValue, inst, pexpr, (name != "$size"));
            }
            found = true;
          }
        } else if (argtype == UhdmType::HierPath) {
          HierPath *path = (HierPath *)arg;
          auto elems = path->getPathElems();
          if (elems && (elems->size() > 1)) {
            const std::string_view base = elems->at(0)->getName();
            const std::string_view suffix = elems->at(1)->getName();
            Any *var = getObject(base, inst, pexpr, muteError);
            if (var) {
              if (ParamAssign *passign = any_cast<ParamAssign>(var)) {
                var = passign->getRhs();
              }
            }
            if (const Port *p = any_cast<Port>(var)) {
              if (const RefTypespec *prt = p->getTypespec()) {
                if (const StructTypespec *tpss =
                        prt->getActual<StructTypespec>()) {
                  for (TypespecMember *memb : *tpss->getMembers()) {
                    if (memb->getName() == suffix) {
                      if (const RefTypespec *rom = memb->getTypespec()) {
                        bits += size(rom->getActual(), invalidValue, inst,
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
        Constant *c = s.make<Constant>();
        c->setValue("UINT:" + std::to_string(bits));
        c->setDecompile(std::to_string(bits));
        c->setSize(64);
        c->setConstType(vpiUIntConst);
        result = c;
      }
    } else if (name == "$clog2") {
      bool invalidValue = false;
      for (auto arg : *scall->getArguments()) {
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
          Constant *c = s.make<Constant>();
          c->setValue("UINT:" + std::to_string(clog2));
          c->setDecompile(std::to_string(clog2));
          c->setSize(64);
          c->setConstType(vpiUIntConst);
          result = c;
        }
      }
    } else if (name == "$signed" || name == "$unsigned") {
      if (scall->getArguments()) {
        const Typespec *optps = nullptr;
        if (const RefTypespec *rt = scall->getTypespec()) {
          optps = rt->getActual();
        }
        for (auto arg : *scall->getArguments()) {
          bool invalidTmpValue = false;
          Expr *val = reduceExpr(arg, invalidTmpValue, inst, pexpr, muteError);
          if (val && (val->getUhdmType() == UhdmType::Constant) &&
              !invalidTmpValue) {
            Constant *c = (Constant *)val;
            if (c->getConstType() == vpiIntConst ||
                c->getConstType() == vpiDecConst) {
              int64_t value = get_value(invalidValue, val);
              int64_t size = c->getSize();
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
                  Constant *c = s.make<Constant>();
                  c->setValue("UINT:" + std::to_string(res));
                  c->setDecompile(std::to_string(res));
                  c->setSize(static_cast<int32_t>(size));
                  c->setConstType(vpiUIntConst);
                  result = c;
                }
              }
            } else if (c->getConstType() == vpiUIntConst ||
                       c->getConstType() == vpiBinaryConst ||
                       c->getConstType() == vpiHexConst ||
                       c->getConstType() == vpiOctConst) {
              uint64_t value = get_uvalue(invalidValue, val);
              int64_t size = c->getSize();
              if (name == "$signed") {
                int64_t res = value;
                bool negsign = value & (1ULL << (size - 1));
                if (optps) {
                  uint32_t bits =
                      ExprEval::size(optps, invalidValue, inst, pexpr, false);
                  bool is_signed = false;
                  if (optps->getUhdmType() == UhdmType::LogicTypespec) {
                    LogicTypespec *ltps = (LogicTypespec *)optps;
                    is_signed = ltps->getSigned();
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
                Constant *c = s.make<Constant>();
                c->setValue("INT:" + std::to_string(res));
                c->setDecompile(std::to_string(res));
                c->setSize(static_cast<int32_t>(size));
                c->setConstType(vpiIntConst);
                result = c;
              } else {
                result = c;
              }
            }
          }
        }
      }
    }
  } else if (objtype == UhdmType::FuncCall) {
    FuncCall *scall = (FuncCall *)result;
    const std::string_view name = scall->getName();
    std::vector<Any *> *args = scall->getArguments();
    Function *actual_func = nullptr;
    if (TaskFunc *func = getTaskFunc(name, inst)) {
      actual_func = any_cast<Function>(func);
    }
    if (actual_func == nullptr) {
      if (muteError == false && m_muteError == false) {
        const std::string errMsg(name);
        s.getErrorHandler()(ErrorType::UHDM_UNDEFINED_USER_FUNCTION, errMsg,
                            scall, nullptr);
      }
      invalidValue = true;
    }
    if (Expr *tmp = evalFunc(actual_func, args, invalidValue, inst,
                             (Any *)pexpr, muteError)) {
      if (!invalidValue) result = tmp;
    }
  } else if (objtype == UhdmType::RefObj) {
    RefObj *ref = (RefObj *)result;
    const std::string_view name = ref->getName();
    if (Any *tmp = getValue(name, inst, pexpr, muteError)) {
      result = tmp;
    }
    return (Expr *)result;
  } else if (objtype == UhdmType::HierPath) {
    HierPath *path = (HierPath *)result;
    return (Expr *)decodeHierPath(path, invalidValue, inst, pexpr, false);
  } else if (objtype == UhdmType::BitSelect) {
    BitSelect *sel = (BitSelect *)result;
    const std::string_view name = sel->getName();
    const Expr *index = sel->getIndex();
    uint64_t index_val = get_value(
        invalidValue,
        reduceExpr((Expr *)index, invalidValue, inst, pexpr, muteError));
    if (invalidValue == false) {
      Any *object = getObject(name, inst, pexpr, muteError);
      if (object) {
        if (ParamAssign *passign = any_cast<ParamAssign>(object)) {
          object = (Any *)passign->getRhs();
        }
      }
      if (object == nullptr) {
        object = getValue(name, inst, pexpr, muteError);
      }
      if (object && (object != result)) {
        if (Expr *tmp = reduceExpr((Expr *)object, invalidValue, inst, pexpr,
                                   muteError)) {
          object = tmp;
        }
        UhdmType otype = object->getUhdmType();
        if (otype == UhdmType::Variable) {
          // PackedArrayVar *array = (PackedArrayVar *)object;
          // AnyCollection *elems = array->getElements();
          // if (elems && index_val < elems->size()) {
          //   Any *elem = elems->at(index_val);
          //   if (elem->getUhdmType() == UhdmType::EnumVar ||
          //       elem->getUhdmType() == UhdmType::StructVar ||
          //       elem->getUhdmType() == UhdmType::UnionVar ||
          //       elem->getUhdmType() == UhdmType::LogicVar) {
          //   } else {
          //     result = elems->at(index_val);
          //   }
          // }
        } else if (otype == UhdmType::ArrayExpr) {
          ArrayExpr *array = (ArrayExpr *)object;
          ExprCollection *elems = array->getExprs();
          if (index_val < elems->size()) {
            result = elems->at(index_val);
          }
        } else if (otype == UhdmType::Operation) {
          Operation *op = (Operation *)object;
          int32_t opType = op->getOpType();
          if (opType == vpiAssignmentPatternOp) {
            AnyCollection *ops = op->getOperands();
            if (ops && (index_val < ops->size())) {
              result = ops->at(index_val);
              if (result->getUhdmType() == UhdmType::Operation) {
                if (const RefTypespec *oprt = op->getTypespec()) {
                  if (const ArrayTypespec *atps =
                          oprt->getActual<ArrayTypespec>()) {
                    if (const RefTypespec *ert = atps->getElemTypespec()) {
                      if (const Typespec *ertts = ert->getActual()) {
                        Elaborator elaborator(&s, false, muteError);
                        RefTypespec *celrt =
                            elaborator.clone<>(ert, const_cast<Any *>(result));
                        celrt->setActual(const_cast<Typespec *>(ertts));
                        ((Operation *)result)->setTypespec(celrt);
                      }
                    }
                  }
                }
              }
            } else if (ops) {
              bool defaultTaggedPattern = false;
              for (auto op : *ops) {
                if (op->getUhdmType() == UhdmType::TaggedPattern) {
                  TaggedPattern *tp = (TaggedPattern *)op;
                  if (const RefTypespec *rt = tp->getTypespec()) {
                    if (const Typespec *tps = rt->getActual()) {
                      if (tps->getName() == "default") {
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
            AnyCollection *ops = op->getOperands();
            if (ops && (index_val < ops->size())) {
              result = ops->at(index_val);
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConditionOp) {
            Expr *exp = reduceExpr(op, invalidValue, inst, pexpr, muteError);
            UhdmType otype = exp->getUhdmType();
            if (otype == UhdmType::Operation) {
              Operation *op = (Operation *)exp;
              int32_t opType = op->getOpType();
              if (opType == vpiAssignmentPatternOp) {
                AnyCollection *ops = op->getOperands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                } else {
                  invalidValue = true;
                }
              } else if (opType == vpiConcatOp) {
                AnyCollection *ops = op->getOperands();
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
        } else if (otype == UhdmType::Constant) {
          result = reduceBitSelect((Constant *)object,
                                   static_cast<uint32_t>(index_val),
                                   invalidValue, inst, pexpr);
        }
      }
    }
  } else if (objtype == UhdmType::PartSelect) {
    PartSelect *sel = (PartSelect *)result;
    std::string_view name = sel->getName();
    if (name.empty()) name = sel->getDefName();
    Any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (ParamAssign *passign = any_cast<ParamAssign>(object)) {
        object = passign->getRhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    if (object && (object->getUhdmType() == UhdmType::Constant)) {
      Constant *co = (Constant *)object;
      std::string binary = toBinary(co);
      int64_t l = get_value(
          invalidValue,
          reduceExpr(sel->getLeftExpr(), invalidValue, inst, pexpr, muteError));
      int64_t r =
          get_value(invalidValue, reduceExpr(sel->getRightExpr(), invalidValue,
                                             inst, pexpr, muteError));
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
      Constant *c = s.make<Constant>();
      c->setValue("BIN:" + sub);
      c->setDecompile(sub);
      c->setSize(static_cast<int32_t>(sub.size()));
      c->setConstType(vpiBinaryConst);
      result = c;
    }
  } else if (objtype == UhdmType::IndexedPartSelect) {
    IndexedPartSelect *sel = (IndexedPartSelect *)result;
    std::string_view name = sel->getName();
    if (name.empty()) name = sel->getDefName();
    Any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (ParamAssign *passign = any_cast<ParamAssign>(object)) {
        object = (Any *)passign->getRhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    if (object && (object->getUhdmType() == UhdmType::Constant)) {
      Constant *co = (Constant *)object;
      std::string binary = toBinary(co);
      int64_t base = get_value(
          invalidValue,
          reduceExpr(sel->getBaseExpr(), invalidValue, inst, pexpr, muteError));
      int64_t offset =
          get_value(invalidValue, reduceExpr(sel->getWidthExpr(), invalidValue,
                                             inst, pexpr, muteError));
      std::reverse(binary.begin(), binary.end());
      std::string sub;
      if (sel->getIndexedPartSelectType() == vpiPosIndexed) {
        if ((uint32_t)(base + offset) <= binary.size())
          sub = binary.substr(base, offset);
      } else {
        if ((uint32_t)base < binary.size())
          sub = binary.substr(base - offset, offset);
      }
      std::reverse(sub.begin(), sub.end());
      Constant *c = s.make<Constant>();
      c->setValue("BIN:" + sub);
      c->setDecompile(sub);
      c->setSize(static_cast<int32_t>(sub.size()));
      c->setConstType(vpiBinaryConst);
      result = c;
    }
  } else if (objtype == UhdmType::VarSelect) {
    VarSelect *sel = (VarSelect *)result;
    const std::string_view name = sel->getName();
    Any *object = getObject(name, inst, pexpr, muteError);
    if (object) {
      if (ParamAssign *passign = any_cast<ParamAssign>(object)) {
        object = passign->getRhs();
      }
    }
    if (object == nullptr) {
      object = getValue(name, inst, pexpr, muteError);
    }
    bool selection = false;
    for (auto index : *sel->getIndexes()) {
      uint64_t index_val = get_value(
          invalidValue,
          reduceExpr((Expr *)index, invalidValue, inst, pexpr, muteError));
      if (object) {
        UhdmType otype = object->getUhdmType();
        if (otype == UhdmType::Operation) {
          Operation *op = (Operation *)object;
          int32_t opType = op->getOpType();
          if (opType == vpiAssignmentPatternOp) {
            AnyCollection *ops = op->getOperands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
              selection = true;
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConcatOp) {
            AnyCollection *ops = op->getOperands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
              selection = true;
            } else {
              invalidValue = true;
            }
          } else if (opType == vpiConditionOp) {
            Expr *exp =
                reduceExpr(object, invalidValue, inst, pexpr, muteError);
            UhdmType otype = exp->getUhdmType();
            if (otype == UhdmType::Operation) {
              Operation *op = (Operation *)exp;
              int32_t opType = op->getOpType();
              if (opType == vpiAssignmentPatternOp) {
                AnyCollection *ops = op->getOperands();
                if (ops && (index_val < ops->size())) {
                  object = ops->at(index_val);
                  selection = true;
                } else {
                  invalidValue = true;
                }
              } else if (opType == vpiConcatOp) {
                AnyCollection *ops = op->getOperands();
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
  if (result && result->getUhdmType() == UhdmType::RefObj) {
    bool invalidValueTmp = false;
    Expr *tmp = reduceExpr(result, invalidValue, inst, pexpr, muteError);
    if (tmp && !invalidValueTmp) result = tmp;
  }
  return (Expr *)result;
}

bool ExprEval::setValueInInstance(
    std::string_view lhs, Any *lhsexp, Expr *rhsexp, bool &invalidValue,
    Serializer &s, const Any *inst, const Any *scope_exp,
    std::map<std::string, const Typespec *> &local_vars, int opType,
    bool muteError) {
  bool invalidValueI = false;
  bool invalidValueUI = false;
  bool invalidValueD = false;
  bool invalidValueB = false;
  bool opRhs = false;
  std::string_view lhsname = lhs;
  if (lhsname.empty()) lhsname = lhsexp->getName();
  rhsexp = reduceExpr(rhsexp, invalidValue, inst, nullptr, muteError);
  int64_t valI = get_value(invalidValueI, rhsexp);
  uint64_t valUI = get_uvalue(invalidValueUI, rhsexp);
  if (rhsexp && (rhsexp->getUhdmType() == UhdmType::Constant)) {
    Constant *t = (Constant *)rhsexp;
    if (t->getConstType() != vpiBinaryConst) {
      invalidValueB = true;
    }
  }
  long double valD = 0;
  if (invalidValueI) {
    valD = get_double(invalidValueD, rhsexp);
  }
  uint64_t wordSize = 1;
  const std::string_view name = lhsexp->getName();
  if (Any *object = getObject(name, inst, scope_exp, muteError)) {
    wordSize = getWordSize(any_cast<Expr>(object), inst, scope_exp);
  }
  ParamAssignCollection *ParamAssigns = nullptr;
  if (inst && inst->getUhdmType() == UhdmType::GenScopeArray) {
  } else if (inst && inst->getUhdmType() == UhdmType::Design) {
    ParamAssigns = ((Design *)inst)->getParamAssigns();
    if (ParamAssigns == nullptr) {
      ((Design *)inst)->setParamAssigns(s.makeCollection<ParamAssign>());
      ParamAssigns = ((Design *)inst)->getParamAssigns();
    }
  } else if (const Scope *spe = any_cast<Scope>(inst)) {
    ParamAssigns = spe->getParamAssigns();
    if (ParamAssigns == nullptr) {
      const_cast<Scope *>(spe)->setParamAssigns(
          s.makeCollection<ParamAssign>());
      ParamAssigns = spe->getParamAssigns();
    }
  }
  if (invalidValueI && invalidValueD) {
    if (ParamAssigns) {
      for (ParamAssignCollection::iterator itr = ParamAssigns->begin();
           itr != ParamAssigns->end(); itr++) {
        if ((*itr)->getLhs()->getName() == lhsname) {
          ParamAssigns->erase(itr);
          break;
        }
      }
      ParamAssign *pa = s.make<ParamAssign>();
      pa->setRhs(rhsexp);
      Parameter *param = s.make<Parameter>();
      param->setName(lhsname);
      pa->setLhs(param);
      ParamAssigns->emplace_back(pa);
      if (rhsexp && ((rhsexp->getUhdmType() == UhdmType::Operation) ||
                     (rhsexp->getUhdmType() == UhdmType::ArrayExpr))) {
        opRhs = true;
      }
    }
  } else if (invalidValueI) {
    if (ParamAssigns) {
      for (ParamAssignCollection::iterator itr = ParamAssigns->begin();
           itr != ParamAssigns->end(); itr++) {
        if ((*itr)->getLhs()->getName() == lhsname) {
          ParamAssigns->erase(itr);
          break;
        }
      }
      Constant *c = s.make<Constant>();
      c->setValue("REAL:" + std::to_string((double)valD));
      c->setDecompile(std::to_string(valD));
      c->setSize(64);
      c->setConstType(vpiRealConst);
      ParamAssign *pa = s.make<ParamAssign>();
      pa->setRhs(c);
      Parameter *param = s.make<Parameter>();
      param->setName(lhsname);
      pa->setLhs(param);
      ParamAssigns->emplace_back(pa);
    }
  } else {
    if (ParamAssigns) {
      const Any *prevRhs = nullptr;
      Constant *c = any_cast<Constant>(rhsexp);
      if (c == nullptr) {
        c = s.make<Constant>();
        c->setValue("INT:" + std::to_string(valI));
        c->setDecompile(std::to_string(valI));
        c->setSize(64);
        c->setConstType(vpiIntConst);
      }
      if (lhsexp->getUhdmType() == UhdmType::Operation) {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin();
             itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
        Operation *op = (Operation *)lhsexp;
        if (op->getOpType() == vpiConcatOp) {
          std::string rhsbinary = toBinary(c);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          AnyCollection *operands = op->getOperands();
          uint64_t accumul = 0;
          for (Any *oper : *operands) {
            const std::string_view name = oper->getName();
            uint64_t si =
                size(oper, invalidValue, inst, lhsexp, true, muteError);
            std::string part;
            for (uint64_t i = accumul; i < accumul + si; i++) {
              part += rhsbinary[i];
            }
            std::reverse(part.begin(), part.end());
            Constant *c = s.make<Constant>();
            c->setValue("BIN:" + part);
            c->setDecompile(part);
            c->setSize(static_cast<int32_t>(part.size()));
            c->setConstType(vpiBinaryConst);
            setValueInInstance(name, oper, c, invalidValue, s, inst, lhsexp,
                               local_vars, vpiConcatOp, muteError);
            accumul = accumul + si;
          }
        }
      } else if (lhsexp->getUhdmType() == UhdmType::IndexedPartSelect) {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin();
             itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
        IndexedPartSelect *sel = (IndexedPartSelect *)lhsexp;
        const std::string_view name = lhsexp->getName();
        if (Any *object = getObject(name, inst, scope_exp, muteError)) {
          std::string lhsbinary;
          const Typespec *tps = nullptr;
          if (const Expr *elhs = any_cast<const Expr *>(object)) {
            if (const RefTypespec *rt = elhs->getTypespec()) {
              tps = rt->getActual();
            }
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs && prevRhs->getUhdmType() == UhdmType::Constant) {
            const Constant *prev = (Constant *)prevRhs;
            lhsbinary = toBinary(prev);
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          uint64_t base = get_uvalue(
              invalidValue, reduceExpr(sel->getBaseExpr(), invalidValue, inst,
                                       lhsexp, muteError));
          uint64_t offset = get_uvalue(
              invalidValue, reduceExpr(sel->getWidthExpr(), invalidValue, inst,
                                       lhsexp, muteError));
          std::string rhsbinary = toBinary(c);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          if (sel->getIndexedPartSelectType() == vpiPosIndexed) {
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
          c = s.make<Constant>();
          c->setValue("BIN:" + lhsbinary);
          c->setDecompile(lhsbinary);
          c->setSize(static_cast<int32_t>(lhsbinary.size()));
          c->setConstType(vpiBinaryConst);
        }
      } else if (lhsexp->getUhdmType() == UhdmType::PartSelect) {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin();
             itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
        PartSelect *sel = (PartSelect *)lhsexp;
        const std::string_view name = lhsexp->getName();
        if (Any *object = getObject(name, inst, scope_exp, muteError)) {
          std::string lhsbinary;
          const Typespec *tps = nullptr;
          if (const Expr *elhs = any_cast<const Expr *>(object)) {
            if (const RefTypespec *rt = elhs->getTypespec()) {
              tps = rt->getActual();
            }
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs && prevRhs->getUhdmType() == UhdmType::Constant) {
            const Constant *prev = (Constant *)prevRhs;
            lhsbinary = toBinary(prev);
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          uint64_t left = get_uvalue(
              invalidValue, reduceExpr(sel->getLeftExpr(), invalidValue, inst,
                                       lhsexp, muteError));
          uint64_t right = get_uvalue(
              invalidValue, reduceExpr(sel->getRightExpr(), invalidValue, inst,
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
          c = s.make<Constant>();
          c->setValue("BIN:" + lhsbinary);
          c->setDecompile(lhsbinary);
          c->setSize(static_cast<int32_t>(lhsbinary.size()));
          c->setConstType(vpiBinaryConst);
        }
      } else if (lhsexp->getUhdmType() == UhdmType::BitSelect) {
        BitSelect *sel = (BitSelect *)lhsexp;
        uint64_t index = get_uvalue(
            invalidValue,
            reduceExpr(sel->getIndex(), invalidValue, inst, lhsexp, muteError));
        const std::string_view name = lhsexp->getName();
        if (Any *object = getObject(name, inst, scope_exp, muteError)) {
          if (object->getUhdmType() == UhdmType::ParamAssign) {
            ParamAssign *param = (ParamAssign *)object;
            if (param->getRhs()->getUhdmType() == UhdmType::ArrayExpr) {
              ArrayExpr *array = (ArrayExpr *)param->getRhs();
              ExprCollection *values = array->getExprs();
              values->resize(index + 1);
              (*values)[index] = rhsexp;
              return false;
            }
          }

          for (ParamAssignCollection::iterator itr = ParamAssigns->begin();
               itr != ParamAssigns->end(); itr++) {
            if ((*itr)->getLhs()->getName() == lhsname) {
              prevRhs = (*itr)->getRhs();
              ParamAssigns->erase(itr);
              break;
            }
          }
          std::string lhsbinary;
          const Typespec *tps = nullptr;
          if (const Expr *elhs = any_cast<const Expr *>(object)) {
            if (const RefTypespec *rt = elhs->getTypespec()) {
              tps = rt->getActual();
            }
          }
          uint64_t si = size(tps, invalidValue, inst, lhsexp, true, muteError);
          if (prevRhs && prevRhs->getUhdmType() == UhdmType::Constant) {
            const Constant *prev = (Constant *)prevRhs;
            if (prev->getConstType() == vpiBinaryConst) {
              std::string_view val = prev->getValue();
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

          int64_t size_rhs = ((Constant *)rhsexp)->getSize();
          if ((wordSize != 1) && (((int64_t)wordSize) < size_rhs))
            size_rhs = wordSize;
          std::string tobinary = NumUtils::toBinary(size_rhs, valUI);
          std::reverse(tobinary.begin(), tobinary.end());
          for (int32_t i = 0; i < size_rhs; i++) {
            if ((((index * size_rhs) + i) < si) &&
                (((index * size_rhs) + i) < lhsbinary.size())) {
              lhsbinary[(index * size_rhs) + i] = tobinary[i];
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.make<Constant>();
          c->setValue("BIN:" + lhsbinary);
          c->setDecompile(lhsbinary);
          c->setSize(static_cast<int32_t>(lhsbinary.size()));
          c->setConstType(vpiBinaryConst);

          RefTypespec *rt = s.make<RefTypespec>();
          rt->setActual(const_cast<Typespec *>(tps));
          rt->setParent(c);
          c->setTypespec(rt);
        } else {
          std::map<std::string, const Typespec *>::iterator itr =
              local_vars.find(std::string(lhs));
          if (itr != local_vars.end()) {
            if (const Typespec *tps = itr->second) {
              if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
                ParamAssign *pa = s.make<ParamAssign>();
                ParamAssigns->emplace_back(pa);
                ArrayExpr *array = s.make<ArrayExpr>();
                ExprCollection *values = s.makeCollection<Expr>();
                values->resize(index + 1);
                (*values)[index] = rhsexp;
                array->setExprs(values);
                pa->setRhs(array);
                Parameter *param = s.make<Parameter>();
                param->setName(lhsname);
                pa->setLhs(param);
                return false;
              }
            }
          }
        }
      } else {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin();
             itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
      }
      if (opType == vpiAddOp) {
        uint64_t prevVal = get_uvalue(invalidValue, (Expr *)prevRhs);
        uint64_t newVal = valUI + prevVal;
        c->setValue("UINT:" + std::to_string(newVal));
        c->setDecompile(std::to_string(newVal));
        c->setConstType(vpiUIntConst);
      } else if (opType == vpiSubOp) {
        int64_t prevVal = get_value(invalidValue, (Expr *)prevRhs);
        int64_t newVal = prevVal - valI;
        c->setValue("INT:" + std::to_string(newVal));
        c->setDecompile(std::to_string(newVal));
        c->setConstType(vpiIntConst);
      } else if (opType == vpiMultOp) {
        int64_t prevVal = get_value(invalidValue, (Expr *)prevRhs);
        int64_t newVal = prevVal * valI;
        c->setValue("INT:" + std::to_string(newVal));
        c->setDecompile(std::to_string(newVal));
        c->setConstType(vpiIntConst);
      } else if (opType == vpiDivOp) {
        int64_t prevVal = get_value(invalidValue, (Expr *)prevRhs);
        int64_t newVal = prevVal / valI;
        c->setValue("INT:" + std::to_string(newVal));
        c->setDecompile(std::to_string(newVal));
        c->setConstType(vpiIntConst);
      }
      if ((c->getSize() == -1) && (c->getConstType() == vpiBinaryConst)) {
        bool tmpInvalidValue = false;
        uint64_t size = ExprEval::size(lhsexp, tmpInvalidValue, inst, scope_exp,
                                       true, true);
        if (tmpInvalidValue) {
          std::map<std::string, const Typespec *>::iterator itr =
              local_vars.find(std::string(lhs));
          if (itr != local_vars.end()) {
            if (const Typespec *tps = itr->second) {
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
          c->setValue("BIN:" + bval);
          c->setDecompile(bval);
          c->setSize(size);
        }
      }
      ParamAssign *pa = s.make<ParamAssign>();
      pa->setRhs(c);
      Parameter *param = s.make<Parameter>();
      param->setName(lhsname);
      pa->setLhs(param);
      ParamAssigns->emplace_back(pa);
    }
  }
  if (invalidValueI && invalidValueD && invalidValueB && (!opRhs)) {
    invalidValue = true;
  }
  return invalidValue;
}

void ExprEval::evalStmt(std::string_view funcName, Scopes &scopes,
                        bool &invalidValue, bool &continue_flag,
                        bool &break_flag, bool &return_flag, const Any *inst,
                        const Any *stmt,
                        std::map<std::string, const Typespec *> &local_vars,
                        bool muteError) {
  if (invalidValue) {
    return;
  }
  Serializer &s = *inst->getSerializer();
  UhdmType stt = stmt->getUhdmType();
  switch (stt) {
    case UhdmType::CaseStmt: {
      CaseStmt *st = (CaseStmt *)stmt;
      Expr *cond = (Expr *)st->getCondition();
      int64_t val = get_value(
          invalidValue,
          reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError));
      for (CaseItem *item : *st->getCaseItems()) {
        if (AnyCollection *exprs = item->getExprs()) {
          bool done = false;
          for (Any *exp : *exprs) {
            int64_t vexp = get_value(
                invalidValue, reduceExpr(exp, invalidValue, scopes.back(),
                                         nullptr, muteError));
            if (val == vexp) {
              evalStmt(funcName, scopes, invalidValue, continue_flag,
                       break_flag, return_flag, scopes.back(), item->getStmt(),
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
    case UhdmType::IfElse: {
      IfElse *st = (IfElse *)stmt;
      Expr *cond = (Expr *)st->getCondition();
      int64_t val = get_value(
          invalidValue,
          reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError));
      if (val > 0) {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->getStmt(), local_vars,
                 muteError);
      } else {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->getElseStmt(), local_vars,
                 muteError);
      }
      break;
    }
    case UhdmType::IfStmt: {
      IfStmt *st = (IfStmt *)stmt;
      Expr *cond = (Expr *)st->getCondition();
      int64_t val = get_value(
          invalidValue,
          reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError));
      if (val > 0) {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->getStmt(), local_vars,
                 muteError);
      }
      break;
    }
    case UhdmType::Begin: {
      Begin *st = (Begin *)stmt;
      if (st->getVariables()) {
        for (auto var : *st->getVariables()) {
          if (const RefTypespec *rt = var->getTypespec()) {
            local_vars.emplace(var->getName(), rt->getActual());
          }
        }
      }
      if (st->getStmts()) {
        for (auto bst : *st->getStmts()) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), bst, local_vars, muteError);
          if (continue_flag || break_flag || return_flag) {
            return;
          }
        }
      }
      break;
    }
    case UhdmType::Assignment: {
      Assignment *st = (Assignment *)stmt;
      const std::string_view lhs = st->getLhs()->getName();
      Expr *lhsexp = st->getLhs();
      const Expr *rhs = st->getRhs<Expr>();
      Expr *rhsexp =
          reduceExpr(rhs, invalidValue, scopes.back(), nullptr, muteError);
      invalidValue =
          setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s, inst, stmt,
                             local_vars, st->getOpType(), muteError);
      break;
    }
    case UhdmType::AssignStmt: {
      AssignStmt *st = (AssignStmt *)stmt;
      const std::string_view lhs = st->getLhs()->getName();
      Expr *lhsexp = st->getLhs();
      const Expr *rhs = st->getRhs();
      Expr *rhsexp =
          reduceExpr(rhs, invalidValue, scopes.back(), nullptr, muteError);
      invalidValue = setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s,
                                        inst, stmt, local_vars, 0, muteError);
      break;
    }
    case UhdmType::Repeat: {
      Repeat *st = (Repeat *)stmt;
      const Expr *cond = st->getCondition();
      Expr *rcond =
          reduceExpr((Expr *)cond, invalidValue, scopes.back(), nullptr);
      int64_t val = get_value(
          invalidValue,
          reduceExpr(rcond, invalidValue, scopes.back(), nullptr, muteError));
      if (invalidValue == false) {
        for (int32_t i = 0; i < val; i++) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->getStmt(), local_vars,
                   muteError);
        }
      }
      break;
    }
    case UhdmType::ForStmt: {
      ForStmt *st = (ForStmt *)stmt;
      if (const Any *stmt = st->getForInitStmt()) {
        if (stmt->getUhdmType() == UhdmType::Assignment) {
          Assignment *assign = (Assignment *)stmt;
          if (const RefTypespec *rt = assign->getLhs()->getTypespec()) {
            local_vars.emplace(assign->getLhs()->getName(), rt->getActual());
          }
        }
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                 return_flag, scopes.back(), st->getForInitStmt(), local_vars,
                 muteError);
      }
      if (st->getForInitStmts()) {
        for (auto s : *st->getForInitStmts()) {
          if (s->getUhdmType() == UhdmType::Assignment) {
            Assignment *assign = (Assignment *)s;
            if (const RefTypespec *rt = assign->getLhs()->getTypespec()) {
              local_vars.emplace(assign->getLhs()->getName(), rt->getActual());
            }
          }
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), s, local_vars, muteError);
        }
      }
      while (1) {
        Expr *cond = (Expr *)st->getCondition();
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
                 return_flag, scopes.back(), st->getStmt(), local_vars,
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
        if (st->getForIncStmt()) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->getForIncStmt(), local_vars,
                   muteError);
        }
        if (invalidValue) break;
        if (st->getForIncStmts()) {
          for (auto s : *st->getForIncStmts()) {
            evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                     return_flag, scopes.back(), s, local_vars, muteError);
          }
        }
        if (invalidValue) break;
      }
      break;
    }
    case UhdmType::ReturnStmt: {
      ReturnStmt *st = (ReturnStmt *)stmt;
      if (const Expr *cond = st->getCondition()) {
        Expr *rhsexp =
            reduceExpr(cond, invalidValue, scopes.back(), nullptr, muteError);
        RefObj *lhsexp = s.make<RefObj>();
        lhsexp->setName(funcName);
        invalidValue =
            setValueInInstance(funcName, lhsexp, rhsexp, invalidValue, s, inst,
                               stmt, local_vars, 0, muteError);
        return_flag = true;
      }
      break;
    }
    case UhdmType::WhileStmt: {
      WhileStmt *st = (WhileStmt *)stmt;
      if (const Expr *cond = st->getCondition()) {
        while (1) {
          int64_t val = get_value(invalidValue,
                                  reduceExpr(cond, invalidValue, scopes.back(),
                                             nullptr, muteError));
          if (invalidValue) break;
          if (val == 0) {
            break;
          }
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->getStmt(), local_vars,
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
    case UhdmType::DoWhile: {
      DoWhile *st = (DoWhile *)stmt;
      if (const Expr *cond = st->getCondition()) {
        while (1) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, scopes.back(), st->getStmt(), local_vars,
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
    case UhdmType::ContinueStmt: {
      continue_flag = true;
      break;
    }
    case UhdmType::BreakStmt: {
      break_flag = true;
      break;
    }
    case UhdmType::Operation: {
      Operation *op = (Operation *)stmt;
      // ++, -- ops
      reduceExpr(op, invalidValue, scopes.back(), nullptr, muteError);
      break;
    }
    default: {
      invalidValue = true;
      if (muteError == false && m_muteError == false) {
        const std::string errMsg(inst->getName());
        s.getErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg, stmt,
                            nullptr);
      }
      break;
    }
  }
}

Expr *ExprEval::evalFunc(Function *func, std::vector<Any *> *args,
                         bool &invalidValue, const Any *inst, Any *pexpr,
                         bool muteError) {
  if (func == nullptr) {
    invalidValue = true;
    return nullptr;
  }
  Serializer &s = *func->getSerializer();
  const std::string_view name = func->getName();
  // set internal scope stack
  Scopes scopes;
  Module *modinst = s.make<Module>();
  modinst->setParent((Any *)inst);
  if (const Instance *pack = func->getInstance()) {
    modinst->setTaskFuncs(pack->getTaskFuncs());
    modinst->setParameters(pack->getParameters());
  }
  ParamAssignCollection *ParamAssigns = nullptr;
  if (inst && inst->getUhdmType() == UhdmType::GenScopeArray) {
  } else if (inst && inst->getUhdmType() == UhdmType::Design) {
    ParamAssigns = ((Design *)inst)->getParamAssigns();
  } else if (const Scope *spe = any_cast<Scope>(inst)) {
    ParamAssigns = spe->getParamAssigns();
  }
  std::map<std::string, const Typespec *> vars;
  if (ParamAssigns) {
    modinst->setParamAssigns(s.makeCollection<ParamAssign>());
    for (auto p : *ParamAssigns) {
      Elaborator elaborator(&s, false, muteError);
      ParamAssign *pp = elaborator.clone<>(p, nullptr);
      modinst->getParamAssigns()->emplace_back(pp);
      const Typespec *tps = nullptr;
      if (const Expr *lhs = any_cast<const Expr *>(p->getLhs())) {
        if (const RefTypespec *rt = lhs->getTypespec()) {
          tps = rt->getActual();
        }
      }
      vars.emplace(std::string(p->getLhs()->getName()), tps);
    }
  }
  // set args
  if (func->getIODecls()) {
    uint32_t index = 0;
    for (auto io : *func->getIODecls()) {
      if (args && (index < args->size())) {
        const std::string_view ioname = io->getName();
        if (io->getTypespec() == nullptr) {
          RefTypespec *rt = s.make<RefTypespec>();
          rt->setParent(io);
          io->setTypespec(rt);
        }
        if (io->getTypespec()->getActual() == nullptr) {
          io->getTypespec()->setActual(s.make<LogicTypespec>());
        }
        Typespec *tps = io->getTypespec()->getActual();
        vars.emplace(ioname, tps);
        Expr *ioexp = (Expr *)args->at(index);
        if (Expr *exparg =
                reduceExpr(ioexp, invalidValue, modinst, pexpr, muteError)) {
          if (exparg->getTypespec() == nullptr) {
            RefTypespec *crt = s.make<RefTypespec>();
            crt->setParent(exparg);
            exparg->setTypespec(crt);
          }
          exparg->getTypespec()->setActual(tps);
          std::map<std::string, const Typespec *> local_vars;
          invalidValue =
              setValueInInstance(ioname, io, exparg, invalidValue, s, modinst,
                                 func, local_vars, 0, muteError);
        }
      }
      index++;
    }
  }
  if (func->getVariables()) {
    for (auto var : *func->getVariables()) {
      if (const RefTypespec *rt = var->getTypespec()) {
        vars.emplace(var->getName(), rt->getActual());
      }
    }
  }
  Typespec *funcReturnTypespec = nullptr;
  if (RefTypespec *rt = func->getReturn()) {
    funcReturnTypespec = rt->getActual();
  }
  if (funcReturnTypespec == nullptr) {
    funcReturnTypespec = s.make<LogicTypespec>();
  }
  Variable *var = s.make<Variable>();
  var->setName(name);
  RefTypespec *frtrt = s.make<RefTypespec>();
  frtrt->setParent(var);
  frtrt->setActual(funcReturnTypespec);
  var->setTypespec(frtrt);
  modinst->getVariables(true)->emplace_back(var);
  vars.emplace(name, funcReturnTypespec);
  scopes.emplace_back(modinst);
  if (const Any *the_stmt = func->getStmt()) {
    UhdmType stt = the_stmt->getUhdmType();
    bool return_flag = false;
    switch (stt) {
      case UhdmType::Begin: {
        Begin *st = (Begin *)the_stmt;
        bool continue_flag = false;
        bool break_flag = false;
        for (auto stmt : *st->getStmts()) {
          evalStmt(name, scopes, invalidValue, continue_flag, break_flag,
                   return_flag, modinst, stmt, vars, muteError);
          if (return_flag) break;
          if (continue_flag || break_flag) {
            if (muteError == false && m_muteError == false) {
              const std::string errMsg(inst->getName());
              s.getErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg,
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
            const std::string errMsg(inst->getName());
            s.getErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg,
                                the_stmt, nullptr);
          }
        }
        break;
      }
    }
  }
  // return value
  if (modinst->getParamAssigns()) {
    for (auto p : *modinst->getParamAssigns()) {
      const std::string n(p->getLhs()->getName());
      if ((!n.empty()) && (vars.find(n) == vars.end())) {
        invalidValue = true;
        return nullptr;
      }
    }
    for (auto p : *modinst->getParamAssigns()) {
      if (p->getLhs()->getName() == name) {
        if (p->getRhs() && (p->getRhs()->getUhdmType() == UhdmType::Constant)) {
          Constant *c = (Constant *)p->getRhs();
          std::string_view val = c->getValue();
          if ((val.find("X") != std::string::npos) ||
              (val.find("x") != std::string::npos)) {
            invalidValue = true;
            return nullptr;
          }
        }
        const Typespec *tps = nullptr;
        if (const RefTypespec *rt = func->getReturn()) {
          tps = rt->getActual();
        }
        if (tps && (tps->getUhdmType() == UhdmType::LogicTypespec)) {
          LogicTypespec *ltps = (LogicTypespec *)tps;
          uint64_t si = size(tps, invalidValue, inst, pexpr, true, true);
          if (p->getRhs() &&
              (p->getRhs()->getUhdmType() == UhdmType::Constant)) {
            Constant *c = (Constant *)p->getRhs();
            Elaborator elaborator(&s, false, muteError);
            c = elaborator.clone<>(c, nullptr);
            if (c->getConstType() == vpiBinaryConst) {
              std::string_view val = c->getValue();
              val.remove_prefix(std::string_view("BIN:").length());
              if (val.size() > si) {
                val.remove_prefix(val.size() - si);
                c->setValue(std::string("BIN:").append(val));
                c->setDecompile(val);
              } else if (ltps->getSigned()) {
                if (val == "1") {
                  c->setValue("INT:-1");
                  c->setDecompile("-1");
                  c->setConstType(vpiIntConst);
                }
              }
            } else {
              uint64_t mask = NumUtils::getMask(si);
              int64_t v = get_value(invalidValue, c);
              v = v & mask;
              c->setValue("UINT:" + std::to_string(v));
              c->setDecompile(std::to_string(v));
              c->setConstType(vpiUIntConst);
            }
            c->setSize(static_cast<int32_t>(si));
            return c;
          }
        }
        return (Expr *)p->getRhs();
      }
    }
  }
  invalidValue = true;
  return nullptr;
}

std::string ExprEval::prettyPrint(const Any *handle) {
  if (handle == nullptr) {
    // std::cout << "NULL HANDLE\n";
    return "NULL HANDLE";
  }
  return uhdm::prettyPrint(handle, 0);
}

std::string vPrint(Any *handle) {
  if (handle == nullptr) {
    // std::cout << "NULL HANDLE\n";
    return "NULL HANDLE";
  }
  std::stringstream out;
  uhdm::prettyPrint(out, handle, 0);
  const std::string s = out.str();
  std::cout << s << "\n";
  return s;
}
}  // namespace uhdm
