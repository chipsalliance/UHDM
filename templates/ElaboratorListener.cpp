// -*- c++ -*-

/*

 Copyright 2019-2020 Alain Dargelas

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
 * File:   ElaboratorListener.h
 * Author: alaindargelas
 *
 * Created on May 6, 2020, 10:03 PM
 */

#include <uhdm/ElaboratorListener.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

#include <iostream>

namespace UHDM {

void ElaboratorListener::leaveDesign(const design* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  design* root = (design*)object;
  root->VpiElaborated(true);
}

void ElaboratorListener::enterModule(const module* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  module* inst = (module*)object;
  bool topLevelModule = object->VpiTopModule();
  const std::string& instName = object->VpiName();
  const std::string& defName = object->VpiDefName();
  bool flatModule =
      (instName == "") && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (debug_)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    flatComponentMap_.insert(std::make_pair(object->VpiDefName(), object));
  } else {
    // Hierachical module list (elaborated)
    inHierarchy_ = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->Nets()) {
      for (net* net : *object->Nets()) {
        netMap.insert(std::make_pair(net->VpiName(), net));
      }
    }
    if (object->Ports()) {
      for (port* port : *object->Ports()) {
        if (const any* low = port->Low_conn()) {
          if (low->UhdmType() == uhdmref_obj) {
            ref_obj* ref = (ref_obj*) low;
            if (const any* actual = ref->Actual_group()) {
              if (actual->UhdmType() == uhdmmodport) {
                netMap.insert(std::make_pair(port->VpiName(), actual));
              }
            }
          }
        }
      }
    }
    if (object->Array_nets()) {
      for (array_net* net : *object->Array_nets()) {
        netMap.insert(std::make_pair(net->VpiName(), net));
      }
    }
    if (object->Interfaces()) {
      for (interface* inter : *object->Interfaces()) {
        netMap.insert(std::make_pair(inter->VpiName(), inter));
      }
    }

    if (object->Named_events()) {
      for (named_event* var : *object->Named_events()) {
        netMap.insert(std::make_pair(var->VpiName(), var));
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (object->Param_assigns()) {
      for (param_assign* passign : *object->Param_assigns()) {
        paramMap.insert(
            std::make_pair(passign->Lhs()->VpiName(), passign->Rhs()));
      }
    }
    if (object->Parameters()) {
      for (any* param : *object->Parameters()) {
        ComponentMap::iterator itr = paramMap.find(param->VpiName());
        if (itr != paramMap.end()) {
          if ((*itr).second == nullptr) {
            paramMap.erase(itr);
          }
        }
        paramMap.insert(std::make_pair(param->VpiName(), param));
      }
    }
    if (object->Def_params()) {
      for (def_param* param : *object->Def_params()) {
        paramMap.insert(std::make_pair(param->VpiName(), param));
      }
    }
    if (object->Variables()) {
      for (variables* var : *object->Variables()) {
        paramMap.insert(std::make_pair(var->VpiName(), var));
      }
    }
    if (object->Ports()) {
      for (ports* port : *object->Ports()) {
        if (const any* low = port->Low_conn()) {
          if (low->UhdmType() == uhdmref_obj) {
            ref_obj* r = (ref_obj*)low;
            if (const any* actual = r->Actual_group()) {
              if (actual->UhdmType() == uhdminterface) {
                netMap.insert(std::make_pair(port->VpiName(), actual));
              }
            }
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (object->Task_funcs()) {
      for (task_func* var : *object->Task_funcs()) {
        funcMap.insert(std::make_pair(var->VpiName(), var));
      }
    }

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      int compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module* defMod = (module*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  paramMap.insert(std::make_pair(econst->VpiName(), econst));
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (object->Gen_scope_arrays()) {
      for (gen_scope_array* gsa : *object->Gen_scope_arrays()) {
        for (gen_scope* gs : *gsa->Gen_scopes()) {
          netMap.insert(std::make_pair(gsa->VpiName(), gs));
        }
      }
    }

    // Push instance context on the stack
    instStack_.push_back(
        std::make_pair(object, std::make_tuple(netMap, paramMap, funcMap)));

    // Check if Module instance has a definition
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      int compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module* defMod = (module*)comp;
          if (clone_) {
<MODULE_ELABORATOR_LISTENER>
          }
          break;
        }
        default:
          break;
      }
    }
  }
}

void ElaboratorListener::leaveModule(const module* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  bindScheduledTaskFunc();
  if (inHierarchy_) {
    instStack_.pop_back();
    if (instStack_.empty()) {
      inHierarchy_ = false;
    }
  }
}

void ElaboratorListener::enterPackage(const package* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  ComponentMap netMap;

  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      netMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      netMap.insert(std::make_pair(var->VpiName(), var));
    }
  }

  if (object->Named_events()) {
    for (named_event* var : *object->Named_events()) {
      netMap.insert(std::make_pair(var->VpiName(), var));
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->Parameters()) {
    for (any* param : *object->Parameters()) {
      paramMap.insert(std::make_pair(param->VpiName(), param));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      paramMap.insert(std::make_pair(var->VpiName(), var));
    }
  }

  // Collect func and task declaration
  ComponentMap funcMap;

  // Push instance context on the stack
  instStack_.push_back(
      std::make_pair(object, std::make_tuple(netMap, paramMap, funcMap)));
}

void ElaboratorListener::leavePackage(const package* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  if (clone_) {
    if (auto vec = object->Task_funcs()) {
      auto clone_vec = serializer_->MakeTask_funcVec();
      ((package*)object)->Task_funcs(clone_vec);
      for (auto obj : *vec) {
        enterTask_func(obj, object, nullptr, nullptr);
        auto* tf = obj->DeepClone(serializer_, this, (package*)object);
        ComponentMap& funcMap =
            std::get<2>((instStack_.at(instStack_.size() - 2)).second);
        funcMap.erase(tf->VpiName());
        funcMap.insert(std::make_pair(tf->VpiName(), tf));
        leaveTask_func(obj, object, nullptr, nullptr);
        tf->VpiParent((package*)object);
        clone_vec->push_back(tf);
      }
    }
  }
  bindScheduledTaskFunc();
  instStack_.pop_back();
}

void ElaboratorListener::enterClass_defn(const class_defn* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  if (!visited_.insert(object).second) return;

  class_defn* cl = (class_defn*)object;

  // Collect instance elaborated nets
  ComponentMap varMap;
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }

 if (object->Named_events()) {
    for (named_event* var : *object->Named_events()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->Parameters()) {
    for (any* param : *object->Parameters()) {
      paramMap.insert(std::make_pair(param->VpiName(), param));
    }
  }

  // Collect func and task declaration
  ComponentMap funcMap;
  if (object->Task_funcs()) {
    for (task_func* var : *object->Task_funcs()) {
      funcMap.insert(std::make_pair(var->VpiName(), var));
    }
  }

  // Push class defn context on the stack
  // Class context is going to be pushed in case of:
  //   - imbricated classes
  //   - inheriting classes (Through the extends relation)
  instStack_.push_back(
      std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));
  if (clone_) {
<CLASS_ELABORATOR_LISTENER>
  }
}

void ElaboratorListener::bindScheduledTaskFunc() {
  for (auto& call_prefix : scheduledTfCallBinding_) {
    tf_call* call = call_prefix.first;
    const class_var* prefix = call_prefix.second;
    if (call->UhdmType() == uhdmfunc_call) {
      if (function* f =
              dynamic_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == uhdmtask_call) {
      if (task* f =
              dynamic_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((task_call*)call)->Task(f);
      }
    } else if (call->UhdmType() == uhdmmethod_func_call) {
      if (function* f =
              dynamic_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == uhdmmethod_task_call) {
      if (task* f =
              dynamic_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_task_call*)call)->Task(f);
      }
    }
  }
  scheduledTfCallBinding_.clear();
}

void ElaboratorListener::leaveClass_defn(const class_defn* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  bindScheduledTaskFunc();
  if (!instStack_.empty() && (instStack_.back().first == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterInterface(const interface* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  // interface* inst = (interface*)object;
  const std::string& instName = object->VpiName();
  const std::string& defName = object->VpiDefName();
  bool flatModule =
      (instName == "") && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is an interface in a hierachy tree
  if (debug_)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    flatComponentMap_.insert(std::make_pair(object->VpiDefName(), object));
  } else {
    // Hierachical module list (elaborated)
    inHierarchy_ = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->Nets()) {
      for (net* net : *object->Nets()) {
        netMap.insert(std::make_pair(net->VpiName(), net));
      }
    }
    if (object->Array_nets()) {
      for (array_net* net : *object->Array_nets()) {
        netMap.insert(std::make_pair(net->VpiName(), net));
      }
    }
    if (object->Interfaces()) {
      for (interface* inter : *object->Interfaces()) {
        netMap.insert(std::make_pair(inter->VpiName(), inter));
      }
    }

    if (object->Named_events()) {
      for (named_event* var : *object->Named_events()) {
        netMap.insert(std::make_pair(var->VpiName(), var));
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (object->Param_assigns()) {
      for (param_assign* passign : *object->Param_assigns()) {
        paramMap.insert(
            std::make_pair(passign->Lhs()->VpiName(), passign->Rhs()));
      }
    }
    if (object->Parameters()) {
      for (any* param : *object->Parameters()) {
        ComponentMap::iterator itr = paramMap.find(param->VpiName());
        if (itr != paramMap.end()) {
          if ((*itr).second == nullptr) {
            paramMap.erase(itr);
          }
        }
        paramMap.insert(std::make_pair(param->VpiName(), param));
      }
    }

    if (object->Variables()) {
      for (variables* var : *object->Variables()) {
        paramMap.insert(std::make_pair(var->VpiName(), var));
      }
    }
    if (object->Ports()) {
      for (ports* port : *object->Ports()) {
        if (const any* low = port->Low_conn()) {
          if (low->UhdmType() == uhdmref_obj) {
            ref_obj* r = (ref_obj*)low;
            if (const any* actual = r->Actual_group()) {
              if (actual->UhdmType() == uhdminterface) {
                netMap.insert(std::make_pair(port->VpiName(), actual));
              }
            }
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (object->Task_funcs()) {
      for (task_func* var : *object->Task_funcs()) {
        funcMap.insert(std::make_pair(var->VpiName(), var));
      }
    }

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      int compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module* defMod = (module*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  paramMap.insert(std::make_pair(econst->VpiName(), econst));
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (object->Gen_scope_arrays()) {
      for (gen_scope_array* gsa : *object->Gen_scope_arrays()) {
        for (gen_scope* gs : *gsa->Gen_scopes()) {
          netMap.insert(std::make_pair(gsa->VpiName(), gs));
        }
      }
    }

    // Push instance context on the stack
    instStack_.push_back(
        std::make_pair(object, std::make_tuple(netMap, paramMap, funcMap)));

    // Check if Module instance has a definition
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      int compType = comp->VpiType();
      switch (compType) {
        case vpiInterface: {
          //  interface* defMod = (interface*)comp;
          if (clone_) {
            // Don't activate yet  <INTERFACE//regexp trap//_ELABORATOR_LISTENER>
            // We need to enter/leave modports and perform binding so not to loose the 
            // binding performed loosely during Surelog elab 
          }
          break;
        }
        default:
          break;
      }
    }
  }
}

void ElaboratorListener::leaveInterface(const interface* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  bindScheduledTaskFunc();
  if (!instStack_.empty() && (instStack_.back().first == object)) {
    instStack_.pop_back();
  }
}

// Hardcoded implementations

any* ElaboratorListener::bindNet(const std::string& name) {
  for (InstStack::reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    ComponentMap& netMap = std::get<0>((*i).second);
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (any*)(*netItr).second;
    }
  }
  return nullptr;
}

// Bind to a net or parameter in the current instance
any* ElaboratorListener::bindAny(const std::string& name) {
  for (InstStack::reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    ComponentMap& netMap = std::get<0>((*i).second);
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (any*)(*netItr).second;
    }

    ComponentMap& paramMap = std::get<1>((*i).second);
    ComponentMap::iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      return (any*)(*paramItr).second;
    }
  }
  return nullptr;
}

// Bind to a param in the current instance
any* ElaboratorListener::bindParam(const std::string& name) {
  for (InstStack::reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    ComponentMap& paramMap = std::get<1>((*i).second);
    ComponentMap::iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      return (any*)(*paramItr).second;
    }
  }
  return nullptr;
}

// Bind to a function or task in the current scope
any* ElaboratorListener::bindTaskFunc(const std::string& name,
                                      const class_var* prefix) {
  for (InstStack::reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    ComponentMap& funcMap = std::get<2>((*i).second);
    ComponentMap::iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (any*)(*funcItr).second;
    }
  }
  if (prefix) {
    const typespec* tps = prefix->Typespec();
    if (tps && tps->UhdmType() == uhdmclass_typespec) {
      const class_defn* def = ((class_typespec*)tps)->Class_defn();
      while (def) {
        if (def->Task_funcs()) {
          for (task_func* tf : *def->Task_funcs()) {
            if (tf->VpiName() == name) return tf;
          }
        }
        const UHDM::extends* ext = def->Extends();
        if (ext) {
          const class_typespec* tps = ext->Class_typespec();
          def = tps->Class_defn();
        } else {
          break;
        }
      }
    }
  }
  return nullptr;
}

bool ElaboratorListener::isFunctionCall(const std::string& name,
                                        const expr* prefix) {
  if (instStack_.size()) {
    for (InstStack::reverse_iterator i = instStack_.rbegin();
         i != instStack_.rend(); ++i) {
      ComponentMap& funcMap = std::get<2>((*i).second);
      ComponentMap::iterator funcItr = funcMap.find(name);
      if (funcItr != funcMap.end()) {
        return ((*funcItr).second->UhdmType() == uhdmfunction);
      }
    }
  }
  if (prefix) {
    const ref_obj* ref = any_cast<const ref_obj*>(prefix);
    const class_var* vprefix = nullptr;
    if (ref) vprefix = any_cast<const class_var*>(ref->Actual_group());
    any* func = bindTaskFunc(name, vprefix);
    if (func) {
      if (func->UhdmType() == uhdmfunction) {
        return true;
      } else {
        return false;
      }
    }
  }
  return true;
}

bool ElaboratorListener::isTaskCall(const std::string& name,
                                    const expr* prefix) {
  if (instStack_.size()) {
    for (InstStack::reverse_iterator i = instStack_.rbegin();
         i != instStack_.rend(); ++i) {
      ComponentMap& funcMap = std::get<2>((*i).second);
      ComponentMap::iterator funcItr = funcMap.find(name);
      if (funcItr != funcMap.end()) {
        return ((*funcItr).second->UhdmType() == uhdmtask);
      }
    }
  }
  if (prefix) {
    const ref_obj* ref = any_cast<const ref_obj*>(prefix);
    const class_var* vprefix = nullptr;
    if (ref) vprefix = any_cast<const class_var*>(ref->Actual_group());
    any* func = bindTaskFunc(name, vprefix);
    if (func) {
      if (func->UhdmType() == uhdmtask) {
        return true;
      } else {
        return false;
      }
    }
  }
  return true;
}

static void propagateParamAssign(param_assign* pass, const any* target) {
  UHDM_OBJECT_TYPE targetType = target->UhdmType();
  Serializer& s = *pass->GetSerializer();
  switch (targetType) {
    case uhdmclass_defn: {
      class_defn* defn = (class_defn*)target;
      const any* lhs = pass->Lhs();
      const std::string& name = lhs->VpiName();
      VectorOfany* params = defn->Parameters();
      if (params) {
        for (any* param : *params) {
          if (param->VpiName() == name) {
            VectorOfparam_assign* passigns = defn->Param_assigns();
            if (passigns == nullptr) {
              defn->Param_assigns(s.MakeParam_assignVec());
              passigns = defn->Param_assigns();
            }
            param_assign* pa = s.MakeParam_assign();
            pa->Lhs(param);
            pa->Rhs((any*)pass->Rhs());
            passigns->push_back(pa);
          }
        }
      }
      const UHDM::extends* extends = defn->Extends();
      if (extends) {
        propagateParamAssign(pass, extends->Class_typespec());
      }
      const auto vars = defn->Variables();
      if (vars) {
        for (auto var : *vars) {
          propagateParamAssign(pass, var);
        }
      }
      break;
    }
    case uhdmclass_var: {
      class_var* var = (class_var*)target;
      propagateParamAssign(pass, var->Typespec());
      break;
    }
    case uhdmclass_typespec: {
      class_typespec* defn = (class_typespec*)target;
      const any* lhs = pass->Lhs();
      const std::string& name = lhs->VpiName();
      VectorOfany* params = defn->Parameters();
      if (params) {
        for (any* param : *params) {
          if (param->VpiName() == name) {
            VectorOfparam_assign* passigns = defn->Param_assigns();
            if (passigns == nullptr) {
              defn->Param_assigns(s.MakeParam_assignVec());
              passigns = defn->Param_assigns();
            }
            param_assign* pa = s.MakeParam_assign();
            pa->Lhs(param);
            pa->Rhs((any*)pass->Rhs());
            passigns->push_back(pa);
          }
        }
      }
      const class_defn* def = defn->Class_defn();
      if (def) {
        propagateParamAssign(pass, (class_defn*)def);
      }
      break;
    }
    default:
      break;
  }
}

void ElaboratorListener::enterVariables(const variables* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  Serializer* s = ((variables*)object)->GetSerializer();
  if (object->UhdmType() == uhdmclass_var) {
    if (!inHierarchy_)
      return;  // Only do class var propagation while in elaboration
    const class_var* cv = (class_var*)object;
    class_var* const rw_cv = (class_var*)cv;
    typespec* ctps = (typespec*)cv->Typespec();
    if (ctps) {
      ctps = ctps->DeepClone(s, this, rw_cv);
      rw_cv->Typespec(ctps);
      if (class_typespec* cctps = any_cast<class_typespec*>(ctps)) {
        VectorOfparam_assign* params = cctps->Param_assigns();
        if (params) {
          for (param_assign* pass : *params) {
            propagateParamAssign(pass, cctps->Class_defn());
          }
        }
      }
    }
  }
}

void ElaboratorListener::enterRef_var(const ref_var* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterShort_real_var(const short_real_var* object,
                                             const BaseClass* parent,
                                             vpiHandle handle,
                                             vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterReal_var(const real_var* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterByte_var(const byte_var* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterShort_int_var(const short_int_var* object,
                                            const BaseClass* parent,
                                            vpiHandle handle,
                                            vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterInt_var(const int_var* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterLong_int_var(const long_int_var* object,
                                           const BaseClass* parent,
                                           vpiHandle handle,
                                           vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterInteger_var(const integer_var* object,
                                          const BaseClass* parent,
                                          vpiHandle handle,
                                          vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterTime_var(const time_var* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterArray_var(const array_var* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterPacked_array_var(const packed_array_var* object,
                                               const BaseClass* parent,
                                               vpiHandle handle,
                                               vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterBit_var(const bit_var* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterLogic_var(const logic_var* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterStruct_var(const struct_var* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterUnion_var(const union_var* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterEnum_var(const enum_var* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterString_var(const string_var* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterChandle_var(const chandle_var* object,
                                          const BaseClass* parent,
                                          vpiHandle handle,
                                          vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterVar_bit(const var_bit* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterClass_var(const class_var* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  enterVariables(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterTask_func(const task_func* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  // Collect instance elaborated nets
  ComponentMap varMap;
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->Io_decls()) {
    for (io_decl* decl : *object->Io_decls()) {
      varMap.insert(std::make_pair(decl->VpiName(), decl));
    }
  }
  varMap.insert(std::make_pair(object->VpiName(), object->Return()));

  ComponentMap paramMap;

  ComponentMap funcMap;

  instStack_.push_back(
      std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));
}

void ElaboratorListener::leaveTask_func(const task_func* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  instStack_.pop_back();
}

void ElaboratorListener::enterForeach_stmt(const foreach_stmt* object,
                                           const BaseClass* parent,
                                           vpiHandle handle,
                                           vpiHandle parentHandle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->VpiLoopVars()) {
    for (any* var : *object->VpiLoopVars()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  ComponentMap paramMap;

  ComponentMap funcMap;

  instStack_.push_back(
      std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));
}

void ElaboratorListener::leaveForeach_stmt(const foreach_stmt* object,
                                           const BaseClass* parent,
                                           vpiHandle handle,
                                           vpiHandle parentHandle) {
  instStack_.pop_back();
}

void ElaboratorListener::enterBegin(const begin* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  ComponentMap paramMap;
  ComponentMap funcMap;
  instStack_.push_back(
      std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));
}

void ElaboratorListener::leaveBegin(const begin* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  instStack_.pop_back();
}

void ElaboratorListener::enterNamed_begin(const named_begin* object,
                                          const BaseClass* parent,
                                          vpiHandle handle,
                                          vpiHandle parentHandle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  ComponentMap paramMap;
  ComponentMap funcMap;
  instStack_.push_back(
      std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));
}
void ElaboratorListener::leaveNamed_begin(const named_begin* object,
                                          const BaseClass* parent,
                                          vpiHandle handle,
                                          vpiHandle parentHandle) {
  instStack_.pop_back();
}

void ElaboratorListener::enterFork_stmt(const fork_stmt* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  ComponentMap paramMap;
  ComponentMap funcMap;
  instStack_.push_back(
      std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));
}
void ElaboratorListener::leaveFork_stmt(const fork_stmt* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  instStack_.pop_back();
}

void ElaboratorListener::enterNamed_fork(const named_fork* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.insert(std::make_pair(var->VpiName(), var));
    }
  }
  ComponentMap paramMap;
  ComponentMap funcMap;
  instStack_.push_back(
      std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));
}
void ElaboratorListener::leaveNamed_fork(const named_fork* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  instStack_.pop_back();
}

void ElaboratorListener::enterFunction(const function* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  enterTask_func(object, parent, handle, parentHandle);
}

void ElaboratorListener::leaveFunction(const function* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  leaveTask_func(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterTask(const task* object, const BaseClass* parent,
                                   vpiHandle handle, vpiHandle parentHandle) {
  enterTask_func(object, parent, handle, parentHandle);
}

void ElaboratorListener::leaveTask(const task* object, const BaseClass* parent,
                                   vpiHandle handle, vpiHandle parentHandle) {
  leaveTask_func(object, parent, handle, parentHandle);
}

void ElaboratorListener::enterGen_scope(const gen_scope* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  // Collect instance elaborated nets

  ComponentMap netMap;
  if (object->Nets()) {
    for (net* net : *object->Nets()) {
      netMap.insert(std::make_pair(net->VpiName(), net));
    }
  }
  if (object->Array_nets()) {
    for (array_net* net : *object->Array_nets()) {
      netMap.insert(std::make_pair(net->VpiName(), net));
    }
  }
  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->Parameters()) {
    for (any* param : *object->Parameters()) {
      paramMap.insert(std::make_pair(param->VpiName(), param));
    }
  }
  if (object->Def_params()) {
    for (def_param* param : *object->Def_params()) {
      paramMap.insert(std::make_pair(param->VpiName(), param));
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      paramMap.insert(std::make_pair(var->VpiName(), var));
    }
  }

  ComponentMap funcMap;

  instStack_.push_back(
      std::make_pair(object, std::make_tuple(netMap, paramMap, funcMap)));
}

void ElaboratorListener::leaveGen_scope(const gen_scope* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  instStack_.pop_back();
}

void ElaboratorListener::leaveRef_obj(const ref_obj* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  if (!object->Actual_group())
    ((ref_obj*)object)->Actual_group(bindAny(object->VpiName()));
}

}  // namespace UHDM
