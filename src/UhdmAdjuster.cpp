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
#include <uhdm/ElaboratorListener.h>
#include <uhdm/ExprEval.h>
#include <uhdm/NumUtils.h>
#include <uhdm/Serializer.h>
#include <uhdm/UhdmAdjuster.h>
#include <uhdm/Utils.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

#include <cstring>
#include <stack>

namespace uhdm {

const Any* UhdmAdjuster::resize(const Any* object, int32_t maxsize,
                                bool is_overall_unsigned) {
  if (object == nullptr) {
    return nullptr;
  }
  Any* result = (Any*)object;
  UhdmType type = result->getUhdmType();
  if (type == UhdmType::Constant) {
    Constant* c = (Constant*)result;
    if (c->getSize() < maxsize) {
      ElaboratorContext elaboratorContext(m_serializer);
      c = (Constant*)clone_tree(c, &elaboratorContext);
      int32_t constType = c->getConstType();
      bool is_signed = false;
      if (const RefTypespec* rt = c->getTypespec()) {
        if (const IntTypespec* itps = rt->getActual<IntTypespec>()) {
          if (itps->getSigned()) {
            is_signed = true;
          }
        }
      }
      if (constType == vpiBinaryConst) {
        std::string value(c->getValue());
        if (is_signed && (!is_overall_unsigned)) {
          value.insert(4, (maxsize - c->getSize()), '1');
        } else {
          value.insert(4, (maxsize - c->getSize()), '0');
        }
        c->setValue(value);
      }
      c->setSize(maxsize);
      result = c;
    }
  } else if (type == UhdmType::RefObj) {
    RefObj* ref = (RefObj*)result;
    const Any* actual = ref->getActual();
    return resize(actual, maxsize, is_overall_unsigned);
  } else if (type == UhdmType::Net) {
    if (getTypespec<LogicTypespec>(result) != nullptr) {
      const Any* parent = result->getParent();
      if (parent && (parent->getUhdmType() == UhdmType::Module)) {
        Module* mod = (Module*)parent;
        if (mod->getParamAssigns()) {
          for (ParamAssign* cass : *mod->getParamAssigns()) {
            if (cass->getLhs()->getName() == result->getName()) {
              return resize(cass->getRhs(), maxsize, is_overall_unsigned);
            }
          }
        }
      }
    }
  }
  return result;
}

void UhdmAdjuster::leaveCaseStmt(const CaseStmt* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  // Make all expressions match the largest expression size per LRM
  int32_t maxsize = 0;
  updateParentWithReducedExpression(object->getCondition(), object);
  bool is_overall_unsigned = false;
  {
    // Find maxsize and is Any expression is unsigned
    std::stack<const Any*> expressions;
    const Expr* cond = object->getCondition();
    expressions.push(cond);
    for (CaseItem* citem : *object->getCaseItems()) {
      if (citem->getExprs()) {
        for (Any* exp : *citem->getExprs()) {
          expressions.push(exp);
        }
      }
    }
    while (expressions.size()) {
      const Any* exp = expressions.top();
      expressions.pop();
      if (exp == nullptr) {
        continue;
      }
      UhdmType type = exp->getUhdmType();
      if (type == UhdmType::Constant) {
        Constant* ccond = (Constant*)exp;
        maxsize = std::max(ccond->getSize(), maxsize);
        bool is_signed = false;
        if (const RefTypespec* rt = ccond->getTypespec()) {
          if (const IntTypespec* itps = rt->getActual<IntTypespec>()) {
            if (itps->getSigned()) {
              is_signed = true;
            }
          }
        }
        if (is_signed == false) {
          is_overall_unsigned = true;
        }
      } else if (type == UhdmType::RefObj) {
        RefObj* ref = (RefObj*)exp;
        const Any* actual = ref->getActual();
        expressions.push((const Expr*)actual);
      } else if (type == UhdmType::Net) {
        if (getTypespec<LogicTypespec>(exp) != nullptr) {
          const Any* parent = exp->getParent();
          if (parent && (parent->getUhdmType() == UhdmType::Module)) {
            Module* mod = (Module*)parent;
            if (mod->getContAssigns()) {
              for (ContAssign* cass : *mod->getContAssigns()) {
                if (cass->getLhs()->getName() == exp->getName()) {
                  expressions.push((const Expr*)cass->getRhs());
                }
              }
            }
            if (mod->getParamAssigns()) {
              for (ParamAssign* cass : *mod->getParamAssigns()) {
                if (cass->getLhs()->getName() == exp->getName()) {
                  expressions.push((const Expr*)cass->getRhs());
                }
              }
            }
          }
        }
      }
    }
  }

  {
    // Resize in place
    CaseStmt* mut_object = (CaseStmt*)object;
    if (Expr* newValue = (Expr*)resize(object->getCondition(), maxsize,
                                       is_overall_unsigned)) {
      if (newValue->getUhdmType() == UhdmType::Constant) {
        mut_object->setCondition(newValue);
      }
    }
    for (CaseItem* citem : *object->getCaseItems()) {
      if (citem->getExprs()) {
        for (uint32_t i = 0; i < citem->getExprs()->size(); i++) {
          if (Any* newValue = (Any*)resize(citem->getExprs()->at(i), maxsize,
                                           is_overall_unsigned)) {
            if (newValue->getUhdmType() == UhdmType::Constant) {
              citem->getExprs()->at(i) = newValue;
            }
          }
        }
      }
    }
  }
}

void UhdmAdjuster::enterModule(const Module* object, vpiHandle handle) {
  m_currentInstance = object;
}

void UhdmAdjuster::leaveModule(const Module* object, vpiHandle handle) {
  m_currentInstance = nullptr;
}

void UhdmAdjuster::enterPackage(const Package* object, vpiHandle handle) {
  m_currentInstance = object;
}

void UhdmAdjuster::leavePackage(const Package* object, vpiHandle handle) {
  m_currentInstance = nullptr;
}

void UhdmAdjuster::enterGenScope(const GenScope* object, vpiHandle handle) {
  m_currentInstance = object;
}

void UhdmAdjuster::leaveGenScope(const GenScope* object, vpiHandle handle) {
  m_currentInstance = nullptr;
}

void UhdmAdjuster::leaveConstant(const Constant* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (object->getSize() == -1) {
    const Any* parent = object->getParent();
    int32_t size = object->getSize();
    bool invalidValue = false;
    ExprEval eval;
    ElaboratorContext elaboratorContext(m_serializer);
    if (parent) {
      if (parent->getUhdmType() == UhdmType::Operation) {
        Operation* op = (Operation*)parent;
        int32_t indexSelf = 0;
        int32_t i = 0;
        for (Any* oper : *op->getOperands()) {
          if (oper != object) {
            int32_t tmp = static_cast<int32_t>(eval.size(
                oper, invalidValue, m_currentInstance, op, true, true));
            if (!invalidValue) {
              size = tmp;
            }
          } else {
            indexSelf = i;
          }
          i++;
        }
        if (size != object->getSize()) {
          Constant* newc = (Constant*)clone_tree(object, &elaboratorContext);
          newc->setSize(size);
          int64_t val = eval.get_value(invalidValue, object);
          if (val == 1) {
            uint64_t mask = NumUtils::getMask(size);
            newc->setValue("UINT:" + std::to_string(mask));
            newc->setDecompile(std::to_string(mask));
            newc->setConstType(vpiUIntConst);
          }
          op->getOperands()->at(indexSelf) = newc;
        }
      } else if (parent->getUhdmType() == UhdmType::ContAssign) {
        ContAssign* assign = (ContAssign*)parent;
        const Any* lhs = assign->getLhs();
        if (lhs->getUhdmType() == UhdmType::HierPath) {
          HierPath* path = (HierPath*)lhs;
          Any* last = path->getPathElems()->back();
          if (last->getUhdmType() == UhdmType::RefObj) {
            RefObj* ref = (RefObj*)last;
            if (const Any* actual = ref->getActual()) {
              if (actual->getUhdmType() == UhdmType::TypespecMember) {
                TypespecMember* member = (TypespecMember*)actual;
                if (const RefTypespec* rt = member->getTypespec()) {
                  if (const Typespec* tps = rt->getActual()) {
                    uint64_t tmp =
                        eval.size(tps, invalidValue, m_currentInstance, assign,
                                  true, true);
                    if (!invalidValue) {
                      size = static_cast<int32_t>(tmp);
                    }
                  }
                }
              }
            }
          }
        }
        if (size != object->getSize()) {
          Constant* newc = (Constant*)clone_tree(object, &elaboratorContext);
          newc->setSize(size);
          int64_t val = eval.get_value(invalidValue, object);
          if (val == 1) {
            uint64_t mask = NumUtils::getMask(size);
            newc->setValue("UINT:" + std::to_string(mask));
            newc->setDecompile(std::to_string(mask));
            newc->setConstType(vpiUIntConst);
          }
          assign->setRhs(newc);
        }
      }
    }
  }
}

void UhdmAdjuster::updateParentWithReducedExpression(const Any* object,
                                                     const Any* parent) {
  bool invalidValue = false;
  ExprEval eval(true);
  eval.reduceExceptions({vpiAssignmentPatternOp, vpiMultiAssignmentPatternOp,
                         vpiConcatOp, vpiMultiConcatOp, vpiBitNegOp});
  Expr* tmp =
      eval.reduceExpr(object, invalidValue, m_currentInstance, parent, true);
  if (invalidValue) return;
  if (tmp == nullptr) return;
  if (tmp->getUhdmType() == UhdmType::Constant) {
    tmp->setFile(object->getFile());
    tmp->setStartLine(object->getStartLine());
    tmp->setStartColumn(object->getStartColumn());
    tmp->setEndLine(object->getEndLine());
    tmp->setEndColumn(object->getEndColumn());
  }
  if (parent->getUhdmType() == UhdmType::Operation) {
    Operation* poper = (Operation*)parent;
    if (AnyCollection* operands = poper->getOperands()) {
      uint64_t index = 0;
      for (Any* oper : *operands) {
        if (oper == object) {
          operands->at(index) = tmp;
          break;
        }
        index++;
      }
    }
  } else if (parent->getUhdmType() == UhdmType::ContAssign) {
    ContAssign* assign = (ContAssign*)parent;
    if (assign->getLhs() == object) return;
    if (tmp) assign->setRhs(tmp);
  } else if (parent->getUhdmType() == UhdmType::IndexedPartSelect) {
    IndexedPartSelect* pselect = (IndexedPartSelect*)parent;
    if (pselect->getBaseExpr() == object) {
      pselect->setBaseExpr(tmp);
    }
    if (pselect->getWidthExpr() == object) {
      pselect->setWidthExpr(tmp);
    }
  } else if (parent->getUhdmType() == UhdmType::PartSelect) {
    PartSelect* pselect = (PartSelect*)parent;
    if (pselect->getLeftExpr() == object) {
      pselect->setLeftExpr(tmp);
    }
    if (pselect->getRightExpr() == object) {
      pselect->setRightExpr(tmp);
    }
  } else if (parent->getUhdmType() == UhdmType::BitSelect) {
    BitSelect* pselect = (BitSelect*)parent;
    if (pselect->getIndex() == object) {
      pselect->setIndex(tmp);
    }
  } else if (parent->getUhdmType() == UhdmType::ReturnStmt) {
    ReturnStmt* stmt = (ReturnStmt*)parent;
    stmt->setCondition(tmp);
  } else if (parent->getUhdmType() == UhdmType::CaseStmt) {
    CaseStmt* stmt = (CaseStmt*)parent;
    stmt->setCondition(tmp);
  } else if (parent->getUhdmType() == UhdmType::CaseItem) {
    CaseItem* poper = (CaseItem*)parent;
    if (AnyCollection* operands = poper->getExprs()) {
      uint64_t index = 0;
      for (Any* oper : *operands) {
        if (oper == object) {
          operands->at(index) = tmp;
          break;
        }
        index++;
      }
    }
  }
}

void UhdmAdjuster::leaveFuncCall(const FuncCall* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  const std::string_view name = object->getName();
  if (name.find("::") != std::string::npos) {
    ExprEval eval;
    std::vector<std::string_view> res = eval.tokenizeMulti(name, "::");
    const std::string_view packName = res[0];
    const std::string_view funcName = res[1];
    if (m_design->getTopPackages()) {
      for (Package* pack : *m_design->getTopPackages()) {
        if (pack->getName() == packName) {
          if (pack->getTaskFuncs()) {
            for (TaskFunc* tf : *pack->getTaskFuncs()) {
              if (tf->getName() == funcName) {
                if (tf->getUhdmType() == UhdmType::Function)
                  ((FuncCall*)object)->setTaskFunc(tf);
              }
            }
          }
          break;
        }
      }
    }
  }
}

void UhdmAdjuster::leaveReturnStmt(const ReturnStmt* object, vpiHandle) {
  if (isInUhdmAllIterator()) return;
  updateParentWithReducedExpression(object->getCondition(), object);
}

void UhdmAdjuster::leaveCaseItem(const CaseItem* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (object->getExprs()) {
    for (auto ex : *object->getExprs()) {
      updateParentWithReducedExpression(ex, object);
    }
  }
}

void UhdmAdjuster::leaveSysFuncCall(const SysFuncCall* object,
                                    vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  const Any* parent = object->getParent();
  if (parent == nullptr) return;
  std::string_view name = object->getName();
  if ((name == "$bits") || (name == "$size") || (name == "$high") ||
      (name == "$low") || (name == "$left") || (name == "$right")) {
    updateParentWithReducedExpression(object, parent);
  }
}

void UhdmAdjuster::leaveOperation(const Operation* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  const Any* parent = object->getParent();
  if (parent == nullptr) return;
  updateParentWithReducedExpression(object, parent);
}

}  // namespace uhdm
