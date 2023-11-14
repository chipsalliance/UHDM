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
#include <string.h>
#include <uhdm/ElaboratorListener.h>
#include <uhdm/ExprEval.h>
#include <uhdm/NumUtils.h>
#include <uhdm/Serializer.h>
#include <uhdm/UhdmAdjuster.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

#include <stack>

namespace UHDM {

const any* UhdmAdjuster::resize(const any* object, int32_t maxsize,
                                bool is_overall_unsigned) {
  if (object == nullptr) {
    return nullptr;
  }
  any* result = (any*)object;
  UHDM_OBJECT_TYPE type = result->UhdmType();
  if (type == UHDM_OBJECT_TYPE::uhdmconstant) {
    constant* c = (constant*)result;
    if (c->VpiSize() < maxsize) {
      ElaboratorContext elaboratorContext(serializer_);
      c = (constant*)clone_tree(c, &elaboratorContext);
      int32_t constType = c->VpiConstType();
      bool is_signed = false;
      if (const ref_typespec* rt = c->Typespec()) {
        if (const int_typespec* itps = rt->Actual_typespec<int_typespec>()) {
          if (itps->VpiSigned()) {
            is_signed = true;
          }
        }
      }
      if (constType == vpiBinaryConst) {
        std::string value(c->VpiValue());
        if (is_signed && (!is_overall_unsigned)) {
          value.insert(4, (maxsize - c->VpiSize()), '1');
        } else {
          value.insert(4, (maxsize - c->VpiSize()), '0');
        }
        c->VpiValue(value);
      }
      c->VpiSize(maxsize);
      result = c;
    }
  } else if (type == UHDM_OBJECT_TYPE::uhdmref_obj) {
    ref_obj* ref = (ref_obj*)result;
    const any* actual = ref->Actual_group();
    return resize(actual, maxsize, is_overall_unsigned);
  } else if (type == UHDM_OBJECT_TYPE::uhdmlogic_net) {
    const any* parent = result->VpiParent();
    if (parent && (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst)) {
      module_inst* mod = (module_inst*)parent;
      if (mod->Cont_assigns()) {
        for (cont_assign* cass : *mod->Cont_assigns()) {
          if (cass->Lhs()->VpiName() == result->VpiName()) {
            return resize(cass->Rhs(), maxsize, is_overall_unsigned);
          }
        }
      }
      if (mod->Param_assigns()) {
        for (param_assign* cass : *mod->Param_assigns()) {
          if (cass->Lhs()->VpiName() == result->VpiName()) {
            return resize(cass->Rhs(), maxsize, is_overall_unsigned);
          }
        }
      }
    }
  }
  return result;
}

void UhdmAdjuster::leaveCase_stmt(const case_stmt* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  // Make all expressions match the largest expression size per LRM
  int32_t maxsize = 0;
  updateParentWithReducedExpression(object->VpiCondition(), object);
  bool is_overall_unsigned = false;
  {
    // Find maxsize and is any expression is unsigned
    std::stack<const any*> expressions;
    const expr* cond = object->VpiCondition();
    expressions.push(cond);
    for (case_item* citem : *object->Case_items()) {
      if (citem->VpiExprs()) {
        for (any* exp : *citem->VpiExprs()) {
          expressions.push(exp);
        }
      }
    }
    while (expressions.size()) {
      const any* exp = expressions.top();
      expressions.pop();
      if (exp == nullptr) {
        continue;
      }
      UHDM_OBJECT_TYPE type = exp->UhdmType();
      if (type == UHDM_OBJECT_TYPE::uhdmconstant) {
        constant* ccond = (constant*)exp;
        maxsize = std::max(ccond->VpiSize(), maxsize);
        bool is_signed = false;
        if (const ref_typespec* rt = ccond->Typespec()) {
          if (const int_typespec* itps = rt->Actual_typespec<int_typespec>()) {
            if (itps->VpiSigned()) {
              is_signed = true;
            }
          }
        }
        if (is_signed == false) {
          is_overall_unsigned = true;
        }
      } else if (type == UHDM_OBJECT_TYPE::uhdmref_obj) {
        ref_obj* ref = (ref_obj*)exp;
        const any* actual = ref->Actual_group();
        expressions.push((const expr*)actual);
      } else if (type == UHDM_OBJECT_TYPE::uhdmlogic_net) {
        const any* parent = exp->VpiParent();
        if (parent && (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst)) {
          module_inst* mod = (module_inst*)parent;
          if (mod->Cont_assigns()) {
            for (cont_assign* cass : *mod->Cont_assigns()) {
              if (cass->Lhs()->VpiName() == exp->VpiName()) {
                expressions.push((const expr*)cass->Rhs());
              }
            }
          }
          if (mod->Param_assigns()) {
            for (param_assign* cass : *mod->Param_assigns()) {
              if (cass->Lhs()->VpiName() == exp->VpiName()) {
                expressions.push((const expr*)cass->Rhs());
              }
            }
          }
        }
      }
    }
  }

  {
    // Resize in place
    case_stmt* mut_object = (case_stmt*)object;
    if (expr* newValue = (expr*)resize(object->VpiCondition(), maxsize,
                                       is_overall_unsigned)) {
      if (newValue->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
        mut_object->VpiCondition(newValue);
      }
    }
    for (case_item* citem : *object->Case_items()) {
      if (citem->VpiExprs()) {
        for (uint32_t i = 0; i < citem->VpiExprs()->size(); i++) {
          if (any* newValue = (any*)resize(citem->VpiExprs()->at(i), maxsize,
                                           is_overall_unsigned)) {
            if (newValue->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
              citem->VpiExprs()->at(i) = newValue;
            }
          }
        }
      }
    }
  }
}

void UhdmAdjuster::enterModule_inst(const module_inst* object,
                                    vpiHandle handle) {
  currentInstance_ = object;
}

void UhdmAdjuster::leaveModule_inst(const module_inst* object,
                                    vpiHandle handle) {
  currentInstance_ = nullptr;
}

void UhdmAdjuster::enterPackage(const package* object,
                                    vpiHandle handle) {
  currentInstance_ = object;
}

void UhdmAdjuster::leavePackage(const package* object,
                                    vpiHandle handle) {
  currentInstance_ = nullptr;
}

void UhdmAdjuster::enterGen_scope(const gen_scope* object, vpiHandle handle) {
  currentInstance_ = object;
}

void UhdmAdjuster::leaveGen_scope(const gen_scope* object, vpiHandle handle) {
  currentInstance_ = nullptr;
}

void UhdmAdjuster::leaveConstant(const constant* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (object->VpiSize() == -1) {
    const any* parent = object->VpiParent();
    int32_t size = object->VpiSize();
    bool invalidValue = false;
    ExprEval eval;
    ElaboratorContext elaboratorContext(serializer_);
    if (parent) {
      if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
        operation* op = (operation*)parent;
        int32_t indexSelf = 0;
        int32_t i = 0;
        for (any* oper : *op->Operands()) {
          if (oper != object) {
            int32_t tmp = static_cast<int32_t>(eval.size(
                oper, invalidValue, currentInstance_, op, true, true));
            if (!invalidValue) {
              size = tmp;
            }
          } else {
            indexSelf = i;
          }
          i++;
        }
        if (size != object->VpiSize()) {
          constant* newc = (constant*)clone_tree(object, &elaboratorContext);
          newc->VpiSize(size);
          int64_t val = eval.get_value(invalidValue, object);
          if (val == 1) {
            uint64_t mask = NumUtils::getMask(size);
            newc->VpiValue("UINT:" + std::to_string(mask));
            newc->VpiDecompile(std::to_string(mask));
            newc->VpiConstType(vpiUIntConst);
          }
          op->Operands()->at(indexSelf) = newc;
        }
      } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmcont_assign) {
        cont_assign* assign = (cont_assign*)parent;
        const any* lhs = assign->Lhs();
        if (lhs->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path) {
          hier_path* path = (hier_path*)lhs;
          any* last = path->Path_elems()->back();
          if (last->UhdmType() == UHDM_OBJECT_TYPE::uhdmref_obj) {
            ref_obj* ref = (ref_obj*)last;
            if (const any* actual = ref->Actual_group()) {
              if (actual->UhdmType() == uhdmtypespec_member) {
                typespec_member* member = (typespec_member*)actual;
                if (const ref_typespec* rt = member->Typespec()) {
                  if (const typespec* tps = rt->Actual_typespec()) {
                    uint64_t tmp =
                        eval.size(tps, invalidValue, currentInstance_, assign,
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
        if (size != object->VpiSize()) {
          constant* newc = (constant*)clone_tree(object, &elaboratorContext);
          newc->VpiSize(size);
          int64_t val = eval.get_value(invalidValue, object);
          if (val == 1) {
            uint64_t mask = NumUtils::getMask(size);
            newc->VpiValue("UINT:" + std::to_string(mask));
            newc->VpiDecompile(std::to_string(mask));
            newc->VpiConstType(vpiUIntConst);
          }
          assign->Rhs(newc);
        }
      }
    }
  }
}

void UhdmAdjuster::updateParentWithReducedExpression(const any* object, const any* parent) {
  bool invalidValue = false;
  ExprEval eval(true);
  eval.reduceExceptions({vpiAssignmentPatternOp, vpiMultiAssignmentPatternOp,
                         vpiConcatOp, vpiMultiConcatOp, vpiBitNegOp});
  expr* tmp =
      eval.reduceExpr(object, invalidValue, currentInstance_, parent, true);
  if (invalidValue) return;
  if (tmp == nullptr) return;
  if (tmp->UhdmType() == UHDM_OBJECT_TYPE::uhdmconstant) {
    tmp->VpiFile(object->VpiFile());
    tmp->VpiLineNo(object->VpiLineNo());
    tmp->VpiColumnNo(object->VpiColumnNo());
    tmp->VpiEndLineNo(object->VpiEndLineNo());
    tmp->VpiEndColumnNo(object->VpiEndColumnNo());
  }
  if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
    operation* poper = (operation*)parent;
    VectorOfany* operands = poper->Operands();
    if (operands) {
      uint64_t index = 0;
      for (any* oper : *operands) {
        if (oper == object) {
          operands->at(index) = tmp;
          break;
        }
        index++;
      }
    }
  } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmcont_assign) {
    cont_assign* assign = (cont_assign*)parent;
    if (assign->Lhs() == object) return;
    if (tmp) {
      assign->Rhs(tmp);
    }
  } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmindexed_part_select) {
    indexed_part_select* pselect = (indexed_part_select*) parent;
    if (pselect->Base_expr() == object) {
      pselect->Base_expr(tmp);
    }
    if (pselect->Width_expr() == object) {
      pselect->Width_expr(tmp);
    }
  } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmpart_select) {
    part_select* pselect = (part_select*) parent;
    if (pselect->Left_range() == object) {
      pselect->Left_range(tmp);
    }
    if (pselect->Right_range() == object) {
      pselect->Right_range(tmp);
    }
  } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_select) {
    bit_select* pselect = (bit_select*) parent;
    if (pselect->VpiIndex() == object) {
      pselect->VpiIndex(tmp);
    }
  } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmreturn_stmt) {
    return_stmt* stmt = (return_stmt*)parent;
    stmt->VpiCondition(tmp);
  } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmcase_stmt) {
    case_stmt* stmt = (case_stmt*)parent;
    stmt->VpiCondition(tmp);
  } else if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmcase_item) {
    case_item* poper = (case_item*)parent;
    VectorOfany* operands = poper->VpiExprs();
    if (operands) {
      uint64_t index = 0;
      for (any* oper : *operands) {
        if (oper == object) {
          operands->at(index) = tmp;
          break;
        }
        index++;
      }
    }
  }
}

void UhdmAdjuster::leaveFunc_call(const func_call* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  const std::string_view name = object->VpiName();
  if (name.find("::") != std::string::npos) {
    ExprEval eval;
    std::vector<std::string_view> res = eval.tokenizeMulti(name, "::");
    const std::string_view packName = res[0];
    const std::string_view funcName = res[1];
    if (design_->TopPackages()) {
      for (package* pack : *design_->TopPackages()) {
        if (pack->VpiName() == packName) {
          if (pack->Task_funcs()) {
            for (task_func* tf : *pack->Task_funcs()) {
              if (tf->VpiName() == funcName) {
                if (tf->UhdmType() == uhdmfunction)
                  ((func_call*)object)->Function((function*)tf);
              }
            }
          }
          break;
        }
      }
    }
  }
}

void UhdmAdjuster::leaveReturn_stmt(const return_stmt* object, vpiHandle) {
  if (isInUhdmAllIterator()) return;
  updateParentWithReducedExpression(object->VpiCondition(), object);
}

void UhdmAdjuster::leaveCase_item(const case_item* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  if (object->VpiExprs()) {
    for (auto ex : *object->VpiExprs()) {
      updateParentWithReducedExpression(ex, object);
    }
  }
}

void UhdmAdjuster::leaveSys_func_call(const sys_func_call* object,
                                      vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  const any* parent = object->VpiParent();
  if (parent == nullptr) return;
  std::string_view name = object->VpiName();
  if ((name == "$bits") || (name == "$size") || (name == "$high") ||
      (name == "$low") || (name == "$left") || (name == "$right")) {
    updateParentWithReducedExpression(object, parent);
  }
}

void UhdmAdjuster::leaveOperation(const operation* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  const any* parent = object->VpiParent();
  if (parent == nullptr) return;
  updateParentWithReducedExpression(object, parent);
}

}  // namespace UHDM
