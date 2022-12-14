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

const any* UhdmAdjuster::resize(const any* object, int maxsize,
                                bool is_overall_unsigned) {
  if (object == nullptr) {
    return nullptr;
  }
  any* result = (any*)object;
  UHDM_OBJECT_TYPE type = result->UhdmType();
  if (type == uhdmconstant) {
    constant* c = (constant*)result;
    if (c->VpiSize() < maxsize) {
      ElaboratorListener listener(serializer_);
      c = (constant*)UHDM::clone_tree(c, *serializer_, &listener);
      int constType = c->VpiConstType();
      const typespec* tps = c->Typespec();
      bool is_signed = false;
      if (tps) {
        if (tps->UhdmType() == uhdmint_typespec) {
          int_typespec* itps = (int_typespec*)tps;
          if (itps->VpiSigned()) {
            is_signed = true;
          }
        }
      }
      if (constType == vpiBinaryConst) {
        std::string value = c->VpiValue();
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
  } else if (type == uhdmref_obj) {
    ref_obj* ref = (ref_obj*)result;
    const any* actual = ref->Actual_group();
    return resize(actual, maxsize, is_overall_unsigned);
  } else if (type == uhdmlogic_net) {
    const any* parent = result->VpiParent();
    if (parent && (parent->UhdmType() == uhdmmodule_inst)) {
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
  // Make all expressions match the largest expression size per LRM
  int maxsize = 0;
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
      if (type == uhdmconstant) {
        constant* ccond = (constant*)exp;
        maxsize = std::max(ccond->VpiSize(), maxsize);
        const typespec* tps = ccond->Typespec();
        bool is_signed = false;
        if (tps) {
          if (tps->UhdmType() == uhdmint_typespec) {
            int_typespec* itps = (int_typespec*)tps;
            if (itps->VpiSigned()) {
              is_signed = true;
            }
          }
        }
        if (is_signed == false) {
          is_overall_unsigned = true;
        }
      } else if (type == uhdmref_obj) {
        ref_obj* ref = (ref_obj*)exp;
        const any* actual = ref->Actual_group();
        expressions.push((const expr*)actual);
      } else if (type == uhdmlogic_net) {
        const any* parent = exp->VpiParent();
        if (parent && (parent->UhdmType() == uhdmmodule_inst)) {
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
    UHDM::expr* newValue = (UHDM::expr*)resize(object->VpiCondition(), maxsize,
                                               is_overall_unsigned);
    if (newValue && (newValue->UhdmType() == uhdmconstant)) {
      mut_object->VpiCondition(newValue);
    }
    for (case_item* citem : *object->Case_items()) {
      if (citem->VpiExprs()) {
        for (uint32_t i = 0; i < citem->VpiExprs()->size(); i++) {
          any* newValue = (any*)resize(citem->VpiExprs()->at(i), maxsize,
                                       is_overall_unsigned);
          if (newValue && (newValue->UhdmType() == uhdmconstant)) {
            citem->VpiExprs()->at(i) = newValue;
          }
        }
      }
    }
  }
}

void UhdmAdjuster::enterModule_inst(const module_inst* object, vpiHandle handle) {
  const std::string& instName = object->VpiName();
  bool flatModule = (instName.empty()) &&
                    ((object->VpiParent() == 0) ||
                     ((object->VpiParent() != 0) &&
                      (object->VpiParent()->VpiType() != vpiModuleInst)));
  if (!flatModule) elaboratedTree_ = true;
  currentInstance_ = object;
}

void UhdmAdjuster::leaveConstant(const constant* object, vpiHandle handle) {
  if (!elaboratedTree_) return;
  if (object->VpiSize() == -1) {
    const any* parent = object->VpiParent();
    if (parent) {
      if (parent->UhdmType() == uhdmoperation) {
        operation* op = (operation*)parent;
        int size = object->VpiSize();
        int indexSelf = 0;
        int i = 0;
        for (any* oper : *op->Operands()) {
          if (oper != object) {
            ExprEval eval;
            bool invalidValue = false;
            int tmp = static_cast<int>(eval.size(
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
          ElaboratorListener listener(serializer_);
          constant* newc =
              (constant*)UHDM::clone_tree(object, *serializer_, &listener);
          newc->VpiSize(size);
          bool invalidValue = false;
          UHDM::ExprEval eval;
          int64_t val = eval.get_value(invalidValue, object);
          if (val == 1) {
            uint64_t mask = NumUtils::getMask(size);
            newc->VpiValue("UINT:" + std::to_string(mask));
            newc->VpiDecompile(std::to_string(mask));
            newc->VpiConstType(vpiUIntConst);
          }
          op->Operands()->at(indexSelf) = newc;
        }
      }
    }
  }
}

}  // namespace UHDM
