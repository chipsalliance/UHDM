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
static void propagateParamAssign(param_assign* pass, const any* target) {
  UHDM_OBJECT_TYPE targetType = target->UhdmType();
  Serializer& s = *pass->GetSerializer();
  switch (targetType) {
    case uhdmclass_defn: {
      class_defn* defn = (class_defn*)target;
      const any* lhs = pass->Lhs();
      const std::string_view name = lhs->VpiName();
      if (VectorOfany* params = defn->Parameters()) {
        for (any* param : *params) {
          if (param->VpiName() == name) {
            VectorOfparam_assign* passigns = defn->Param_assigns();
            if (passigns == nullptr) {
              defn->Param_assigns(s.MakeParam_assignVec());
              passigns = defn->Param_assigns();
            }
            param_assign* pa = s.MakeParam_assign();
            pa->VpiParent(defn);
            pa->Lhs(param);
            pa->Rhs((any*)pass->Rhs());
            passigns->push_back(pa);
          }
        }
      }
      if (const UHDM::extends* extends = defn->Extends()) {
        propagateParamAssign(pass, extends->Class_typespec());
      }
      if (const auto vars = defn->Variables()) {
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
      const std::string_view name = lhs->VpiName();
      if (VectorOfany* params = defn->Parameters()) {
        for (any* param : *params) {
          if (param->VpiName() == name) {
            VectorOfparam_assign* passigns = defn->Param_assigns();
            if (passigns == nullptr) {
              defn->Param_assigns(s.MakeParam_assignVec());
              passigns = defn->Param_assigns();
            }
            param_assign* pa = s.MakeParam_assign();
            pa->VpiParent(defn);
            pa->Lhs(param);
            pa->Rhs((any*)pass->Rhs());
            passigns->push_back(pa);
          }
        }
      }
      if (const class_defn* def = defn->Class_defn()) {
        propagateParamAssign(pass, def);
      }
      break;
    }
    default:
      break;
  }
}

void ElaboratorListener::enterVariables(const variables* object,
                                        vpiHandle handle) {
  Serializer* s = ((variables*)object)->GetSerializer();
  if (object->UhdmType() == uhdmclass_var) {
    if (!inHierarchy_)
      return;  // Only do class var propagation while in elaboration
    const class_var* cv = (class_var*)object;
    class_var* const rw_cv = (class_var*)cv;
    if (typespec* ctps = (typespec*)cv->Typespec()) {
      ctps = ctps->DeepClone(s, this, rw_cv);
      rw_cv->Typespec(ctps);
      if (class_typespec* cctps = any_cast<class_typespec*>(ctps)) {
        if (VectorOfparam_assign* params = cctps->Param_assigns()) {
          for (param_assign* pass : *params) {
            propagateParamAssign(pass, cctps->Class_defn());
          }
        }
      }
    }
  }
}

void ElaboratorListener::enterAny(const any* object, vpiHandle handle) {
  if (const variables* const var = any_cast<const variables*>(object)) {
    enterVariables(var, handle);
  }
}

void ElaboratorListener::leaveDesign(const design* object, vpiHandle handle) {
  const_cast<design*>(object)->VpiElaborated(true);
}

static std::string_view ltrim_until(std::string_view str, char c) {
  auto it = str.find(c);
  if (it != std::string_view::npos) str.remove_prefix(it + 1);
  return str;
}

void ElaboratorListener::enterModule_inst(const module_inst* object,
                                          vpiHandle handle) {
  bool topLevelModule = object->VpiTopModule();
  const std::string_view instName = object->VpiName();
  const std::string_view defName = object->VpiDefName();
  bool flatModule =
      instName.empty() && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (debug_)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    flatComponentMap_.emplace(object->VpiDefName(), object);
  } else {
    // Hierachical module list (elaborated)
    inHierarchy_ = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->Nets()) {
      for (net* net : *object->Nets()) {
        netMap.emplace(net->VpiName(), net);
      }
    }

    if (object->Variables()) {
      for (variables* var : *object->Variables()) {
        netMap.emplace(var->VpiName(), var);
        if (var->UhdmType() == uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          enum_typespec* etps = (enum_typespec*)evar->Typespec();
          for (auto c : *etps->Enum_consts()) {
            netMap.emplace(c->VpiName(), c);
          }
        }
      }
    }

    if (object->Interfaces()) {
      for (interface_inst* inter : *object->Interfaces()) {
        netMap.emplace(inter->VpiName(), inter);
      }
    }
    if (object->Interface_arrays()) {
      for (interface_array* inter : *object->Interface_arrays()) {
        if (VectorOfinstance* instances = inter->Instances()) {
          for (instance* interf : *instances) {
            netMap.emplace(interf->VpiName(), interf);
          }
        }
      }
    }

    if (object->Ports()) {
      for (port* port : *object->Ports()) {
        if (const any* low = port->Low_conn()) {
          if (const ref_obj *ref = any_cast<const ref_obj*>(low)) {
            if (const any* actual = ref->Actual_group()) {
              if (actual->UhdmType() == uhdmmodport) {
                // If the interface of the modport is not yet in the map
                netMap.emplace(port->VpiName(), actual);
              }
            }
          }
        }
      }
    }
    if (object->Array_nets()) {
      for (array_net* net : *object->Array_nets()) {
        netMap.emplace(net->VpiName(), net);
      }
    }

    if (object->Named_events()) {
      for (named_event* var : *object->Named_events()) {
        netMap.emplace(var->VpiName(), var);
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (muteErrors_ == true) {
      // In final hier_path binding we need the formal parameter, not the actual
      if (object->Param_assigns()) {
        for (param_assign* passign : *object->Param_assigns()) {
          paramMap.emplace(passign->Lhs()->VpiName(), passign->Rhs());
        }
      }
    }
    if (object->Parameters()) {
      for (any* param : *object->Parameters()) {
        ComponentMap::iterator itr = paramMap.find(param->VpiName());
        if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
          paramMap.erase(itr);
        }
        paramMap.emplace(param->VpiName(), param);
      }
    }
    if (object->Def_params()) {
      for (def_param* param : *object->Def_params()) {
        paramMap.emplace(param->VpiName(), param);
      }
    }

    if (object->Typespecs()) {
      for (typespec* tps : *object->Typespecs()) {
        if (tps->UhdmType() == uhdmenum_typespec) {
          enum_typespec* etps = (enum_typespec*)tps;
          for (auto c : *etps->Enum_consts()) {
            paramMap.emplace(c->VpiName(), c);
          }
        }
      }
    }
    if (object->Ports()) {
      for (ports* port : *object->Ports()) {
        if (const any* low = port->Low_conn()) {
          if (const ref_obj *r = any_cast<const ref_obj*>(low)) {
            if (const any* actual = r->Actual_group()) {
              if (actual->UhdmType() == uhdminterface_inst) {
                netMap.emplace(port->VpiName(), actual);
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
        funcMap.emplace(var->VpiName(), var);
      }
    }

    ComponentMap modMap;

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module_inst* defMod = (module_inst*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  paramMap.emplace(econst->VpiName(), econst);
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
          netMap.emplace(gsa->VpiName(), gs);
        }
      }
    }

    // Module itself
    std::string_view modName = ltrim_until(object->VpiName(), '@');
    modMap.emplace(modName, object);

    if (object->Modules()) {
      for (module_inst* mod : *object->Modules()) {
        modMap.emplace(mod->VpiName(), mod);
      }
    }

    if (object->Module_arrays()) {
      for (module_array* mod : *object->Module_arrays()) {
        modMap.emplace(mod->VpiName(), mod);
      }
    }

    if (const clocking_block* block = object->Default_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }

    if (const clocking_block* block = object->Global_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }

    if (object->Clocking_blocks()) {
      for (clocking_block* block : *object->Clocking_blocks()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    // Push instance context on the stack
    instStack_.emplace_back(object, netMap, paramMap, funcMap, modMap);
  }
  if (muteErrors_ == false) {
    elabModule_inst(object, handle);
  }
}

void ElaboratorListener::elabModule_inst(const module_inst* object,
                                         vpiHandle handle) {
  module_inst* inst = const_cast<module_inst*>(object);
  bool topLevelModule = object->VpiTopModule();
  const std::string_view instName = object->VpiName();
  const std::string_view defName = object->VpiDefName();
  bool flatModule =
      instName.empty() && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (debug_)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    flatComponentMap_.emplace(object->VpiDefName(), object);
  } else {
    // Do not elab modules used in hier_path base, that creates a loop
    if (inCallstackOfType(uhdmhier_path)) {
      return;
    }
    if (!clone_) return;
    // Hierachical module list (elaborated)
    inHierarchy_ = true;
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    // Check if Module instance has a definition
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      if (comp->VpiType() != vpiModule) return;
      module_inst* defMod = (module_inst*)comp;
<MODULE_ELABORATOR_LISTENER>
    }
  }
}

void ElaboratorListener::leaveModule_inst(const module_inst* object,
                                          vpiHandle handle) {
  bindScheduledTaskFunc();
  if (inHierarchy_ && !instStack_.empty() &&
      (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
    if (instStack_.empty()) {
      inHierarchy_ = false;
    }
  }
}

void ElaboratorListener::enterPackage(const package* object, vpiHandle handle) {
  ComponentMap netMap;

  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      netMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      netMap.emplace(var->VpiName(), var);
      if (var->UhdmType() == uhdmenum_var) {
        enum_var* evar = (enum_var*)var;
        enum_typespec* etps = (enum_typespec*)evar->Typespec();
        for (auto c : *etps->Enum_consts()) {
          netMap.emplace(c->VpiName(), c);
        }
      }
    }
  }

  if (object->Named_events()) {
    for (named_event* var : *object->Named_events()) {
      netMap.emplace(var->VpiName(), var);
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->Parameters()) {
    for (any* param : *object->Parameters()) {
      paramMap.emplace(param->VpiName(), param);
    }
  }

  // Collect func and task declaration
  ComponentMap funcMap;
  ComponentMap modMap;
  // Push instance context on the stack
  instStack_.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leavePackage(const package* object, vpiHandle handle) {
  if (clone_) {
    if (auto vec = object->Task_funcs()) {
      auto clone_vec = serializer_->MakeTask_funcVec();
      ((package*)object)->Task_funcs(clone_vec);
      for (auto obj : *vec) {
        enterTask_func(obj, nullptr);
        auto* tf = obj->DeepClone(serializer_, this, (package*)object);
        ComponentMap& funcMap =
            std::get<3>(instStack_.at(instStack_.size() - 2));
        auto it = funcMap.find(tf->VpiName());
        if (it != funcMap.end()) funcMap.erase(it);
        funcMap.emplace(tf->VpiName(), tf);
        leaveTask_func(obj, nullptr);
        tf->VpiParent((package*)object);
        clone_vec->push_back(tf);
      }
    }
  }
  bindScheduledTaskFunc();
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterClass_defn(const class_defn* object,
                                         vpiHandle handle) {
  ComponentMap varMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;

  const class_defn* defn = object;
  while (defn != nullptr) {
    // Collect instance elaborated nets
    if (defn->Variables()) {
      for (variables* var : *defn->Variables()) {
        varMap.emplace(var->VpiName(), var);
        if (var->UhdmType() == uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          enum_typespec* etps = (enum_typespec*)evar->Typespec();
          for (auto c : *etps->Enum_consts()) {
            varMap.emplace(c->VpiName(), c);
          }
        }
      }
    }

    if (defn->Named_events()) {
      for (named_event* var : *defn->Named_events()) {
        varMap.emplace(var->VpiName(), var);
      }
    }

    // Collect instance parameters, defparams
    if (defn->Parameters()) {
      for (any* param : *defn->Parameters()) {
        paramMap.emplace(param->VpiName(), param);
      }
    }

    // Collect func and task declaration
    if (defn->Task_funcs()) {
      for (task_func* tf : *defn->Task_funcs()) {
        if (funcMap.find(tf->VpiName()) == funcMap.end()) {
          // Bind to overriden function in sub-class
          funcMap.emplace(tf->VpiName(), tf);
        }
      }
    }

    const class_defn* base_defn = nullptr;
    if (const extends* ext = defn->Extends()) {
      if (const class_typespec* ctps = ext->Class_typespec()) {
        base_defn = ctps->Class_defn();
      }
    }
    defn = base_defn;
  }

  // Push class defn context on the stack
  // Class context is going to be pushed in case of:
  //   - imbricated classes
  //   - inheriting classes (Through the extends relation)
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
  if (muteErrors_ == false) {
    elabClass_defn(object, handle);
  }
}

void ElaboratorListener::elabClass_defn(const class_defn* object,
                                        vpiHandle handle) {
  if (!clone_) return;
  class_defn* cl = (class_defn*)object;
<CLASS_ELABORATOR_LISTENER>
}

void ElaboratorListener::bindScheduledTaskFunc() {
  for (auto& call_prefix : scheduledTfCallBinding_) {
    tf_call* call = call_prefix.first;
    const class_var* prefix = call_prefix.second;
    if (call->UhdmType() == uhdmfunc_call) {
      if (function* f =
              any_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == uhdmtask_call) {
      if (task* f = any_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((task_call*)call)->Task(f);
      }
    } else if (call->UhdmType() == uhdmmethod_func_call) {
      if (function* f =
              any_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == uhdmmethod_task_call) {
      if (task* f = any_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_task_call*)call)->Task(f);
      }
    }
  }
  scheduledTfCallBinding_.clear();
}

void ElaboratorListener::leaveClass_defn(const class_defn* object,
                                         vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterInterface_inst(const interface_inst* object,
                                             vpiHandle handle) {
  const std::string_view instName = object->VpiName();
  const std::string_view defName = object->VpiDefName();
  bool flatModule =
      instName.empty() && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is an interface in a hierachy tree
  if (debug_)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    flatComponentMap_.emplace(object->VpiDefName(), object);
  } else {
    // Hierachical module list (elaborated)
    inHierarchy_ = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->Nets()) {
      for (net* net : *object->Nets()) {
        netMap.emplace(net->VpiName(), net);
      }
    }
    if (object->Array_nets()) {
      for (array_net* net : *object->Array_nets()) {
        netMap.emplace(net->VpiName(), net);
      }
    }

    if (object->Variables()) {
      for (variables* var : *object->Variables()) {
        netMap.emplace(var->VpiName(), var);
        if (var->UhdmType() == uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          enum_typespec* etps = (enum_typespec*)evar->Typespec();
          for (auto c : *etps->Enum_consts()) {
            netMap.emplace(c->VpiName(), c);
          }
        }
      }
    }

    if (object->Interfaces()) {
      for (interface_inst* inter : *object->Interfaces()) {
        netMap.emplace(inter->VpiName(), inter);
      }
    }
    if (object->Interface_arrays()) {
      for (interface_array* inter : *object->Interface_arrays()) {
        for (instance* interf : *inter->Instances())
          netMap.emplace(interf->VpiName(), interf);
      }
    }
    if (object->Named_events()) {
      for (named_event* var : *object->Named_events()) {
        netMap.emplace(var->VpiName(), var);
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (object->Param_assigns()) {
      for (param_assign* passign : *object->Param_assigns()) {
        paramMap.insert(ComponentMap::value_type(passign->Lhs()->VpiName(),
                                                 passign->Rhs()));
      }
    }
    if (object->Parameters()) {
      for (any* param : *object->Parameters()) {
        ComponentMap::iterator itr = paramMap.find(param->VpiName());
        if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
          paramMap.erase(itr);
        }
        paramMap.emplace(param->VpiName(), param);
      }
    }

    if (object->Ports()) {
      for (ports* port : *object->Ports()) {
        if (const any* low = port->Low_conn()) {
          if (const ref_obj* r = any_cast<const ref_obj*>(low)) {
            if (const any* actual = r->Actual_group()) {
              if (actual->UhdmType() == uhdminterface_inst) {
                netMap.emplace(port->VpiName(), actual);
              } else if (actual->UhdmType() == uhdmmodport) {
                // If the interface of the modport is not yet in the map
                netMap.emplace(port->VpiName(), actual);
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
        funcMap.emplace(var->VpiName(), var);
      }
    }

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module_inst* defMod = (module_inst*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  paramMap.emplace(econst->VpiName(), econst);
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
          netMap.emplace(gsa->VpiName(), gs);
        }
      }
    }
    ComponentMap modMap;

    if (const clocking_block* block = object->Default_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }

    if (const clocking_block* block = object->Global_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }

    if (object->Clocking_blocks()) {
      for (clocking_block* block : *object->Clocking_blocks()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    // Push instance context on the stack
    instStack_.emplace_back(object, netMap, paramMap, funcMap, modMap);

    // Check if Module instance has a definition
    if (itrDef != flatComponentMap_.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiInterface: {
          //  interface* defMod = (interface*)comp;
          if (clone_) {
            // Don't activate yet  <INTERFACE//regexp
            // trap//_ELABORATOR_LISTENER> We need to enter/leave modports and
            // perform binding so not to loose the binding performed loosely
            // during Surelog elab
          }
          break;
        }
        default:
          break;
      }
    }
  }
}

void ElaboratorListener::leaveInterface_inst(const interface_inst* object,
                                             vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

// Hardcoded implementations

any* ElaboratorListener::bindNet(std::string_view name) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& netMap = std::get<1>(*i);
    ComponentMap::const_iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (any*)netItr->second;
    }
  }
  return nullptr;
}

// Bind to a net or parameter in the current instance
any* ElaboratorListener::bindAny(std::string_view name) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& netMap = std::get<1>(*i);
    ComponentMap::const_iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (any*)netItr->second;
    }

    const ComponentMap& paramMap = std::get<2>(*i);
    ComponentMap::const_iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      return (any*)paramItr->second;
    }

    const ComponentMap& modMap = std::get<4>(*i);
    ComponentMap::const_iterator modItr = modMap.find(name);
    if (modItr != modMap.end()) {
      return (any*)modItr->second;
    }
  }
  return nullptr;
}

// Bind to a param in the current instance
any* ElaboratorListener::bindParam(std::string_view name) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& paramMap = std::get<2>(*i);
    ComponentMap::const_iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      return (any*)paramItr->second;
    }
  }
  return nullptr;
}

// Bind to a function or task in the current scope
any* ElaboratorListener::bindTaskFunc(std::string_view name,
                                      const class_var* prefix) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (any*)funcItr->second;
    }
  }
  if (prefix) {
    const typespec* tps = prefix->Typespec();
    if (tps && tps->UhdmType() == uhdmclass_typespec) {
      const class_defn* defn = ((const class_typespec*)tps)->Class_defn();
      while (defn) {
        if (defn->Task_funcs()) {
          for (task_func* tf : *defn->Task_funcs()) {
            if (tf->VpiName() == name) return tf;
          }
        }

        const class_defn* base_defn = nullptr;
        if (const extends* ext = defn->Extends()) {
          if (const class_typespec* ctps = ext->Class_typespec()) {
            base_defn = ctps->Class_defn();
          }
        }
        defn = base_defn;
      }
    }
  }
  return nullptr;
}

bool ElaboratorListener::isFunctionCall(std::string_view name,
                                        const expr* prefix) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->UhdmType() == uhdmfunction);
    }
  }
  if (prefix) {
    if (const ref_obj* ref = any_cast<const ref_obj*>(prefix)) {
      const class_var* vprefix =
          any_cast<const class_var*>(ref->Actual_group());
      if (const any* func = bindTaskFunc(name, vprefix)) {
        return (func->UhdmType() == uhdmfunction);
      }
    }
  }
  return true;
}

bool ElaboratorListener::isTaskCall(std::string_view name,
                                    const expr* prefix) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->UhdmType() == uhdmtask);
    }
  }
  if (prefix) {
    if (const ref_obj* ref = any_cast<const ref_obj*>(prefix)) {
      const class_var* vprefix =
          any_cast<const class_var*>(ref->Actual_group());
      if (const any* task = bindTaskFunc(name, vprefix)) {
        return (task->UhdmType() == uhdmtask);
      }
    }
  }
  return true;
}

void ElaboratorListener::enterTask_func(const task_func* object,
                                        vpiHandle handle) {
  // Collect instance elaborated nets
  ComponentMap varMap;
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Io_decls()) {
    for (io_decl* decl : *object->Io_decls()) {
      varMap.emplace(decl->VpiName(), decl);
    }
  }
  varMap.emplace(object->VpiName(), object->Return());

  if (const any* parent = object->VpiParent()) {
    if (parent->UhdmType() == uhdmclass_defn) {
      const class_defn* defn = (const class_defn*)parent;
      while (defn) {
        if (defn->Variables()) {
          for (any* var : *defn->Variables()) {
            varMap.emplace(var->VpiName(), var);
          }
        }

        const class_defn* base_defn = nullptr;
        if (const extends* ext = defn->Extends()) {
          if (const class_typespec* ctps = ext->Class_typespec()) {
            base_defn = ctps->Class_defn();
          }
        }
        defn = base_defn;
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveTask_func(const task_func* object,
                                        vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterFor_stmt(const for_stmt* object,
                                       vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->VpiForInitStmts()) {
    for (any* stmt : *object->VpiForInitStmts()) {
      if (stmt->UhdmType() == uhdmassign_stmt) {
        assign_stmt* astmt = (assign_stmt*)stmt;
        const any* lhs = astmt->Lhs();
        if (lhs->UhdmType() != uhdmref_var) {
          varMap.emplace(lhs->VpiName(), lhs);
        }
      }
    }
  }
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveFor_stmt(const for_stmt* object,
                                       vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterForeach_stmt(const foreach_stmt* object,
                                           vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->VpiLoopVars()) {
    for (any* var : *object->VpiLoopVars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveForeach_stmt(const foreach_stmt* object,
                                           vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterBegin(const begin* object, vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveBegin(const begin* object, vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterNamed_begin(const named_begin* object,
                                          vpiHandle handle) {
  ComponentMap varMap;
  if (!instStack_.empty()) {
    ComponentMap& modMap = std::get<4>(instStack_.back());
    modMap.emplace(object->VpiName(), object);
  }
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
}
void ElaboratorListener::leaveNamed_begin(const named_begin* object,
                                          vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterFork_stmt(const fork_stmt* object,
                                        vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
}
void ElaboratorListener::leaveFork_stmt(const fork_stmt* object,
                                        vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterNamed_fork(const named_fork* object,
                                         vpiHandle handle) {
  ComponentMap varMap;
  if (!instStack_.empty()) {
    ComponentMap& modMap = std::get<4>(instStack_.back());
    modMap.emplace(object->VpiName(), object);
  }
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveNamed_fork(const named_fork* object,
                                         vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterFunction(const function* object,
                                       vpiHandle handle) {
  enterTask_func(object, handle);
}

void ElaboratorListener::leaveFunction(const function* object,
                                       vpiHandle handle) {
  leaveTask_func(object, handle);
}

void ElaboratorListener::enterTask(const task* object, vpiHandle handle) {
  enterTask_func(object, handle);
}

void ElaboratorListener::leaveTask(const task* object, vpiHandle handle) {
  leaveTask_func(object, handle);
}

void ElaboratorListener::enterGen_scope(const gen_scope* object,
                                        vpiHandle handle) {
  // Collect instance elaborated nets

  ComponentMap netMap;
  if (object->Nets()) {
    for (net* net : *object->Nets()) {
      netMap.emplace(net->VpiName(), net);
    }
  }
  if (object->Array_nets()) {
    for (array_net* net : *object->Array_nets()) {
      netMap.emplace(net->VpiName(), net);
    }
  }

  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      netMap.emplace(var->VpiName(), var);
      if (var->UhdmType() == uhdmenum_var) {
        enum_var* evar = (enum_var*)var;
        enum_typespec* etps = (enum_typespec*)evar->Typespec();
        for (auto c : *etps->Enum_consts()) {
          netMap.emplace(c->VpiName(), c);
        }
      }
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->Parameters()) {
    for (any* param : *object->Parameters()) {
      paramMap.emplace(param->VpiName(), param);
    }
  }
  if (object->Def_params()) {
    for (def_param* param : *object->Def_params()) {
      paramMap.emplace(param->VpiName(), param);
    }
  }

  ComponentMap funcMap;
  ComponentMap modMap;

  // Collect gen_scope
  if (object->Gen_scope_arrays()) {
    for (gen_scope_array* gsa : *object->Gen_scope_arrays()) {
      for (gen_scope* gs : *gsa->Gen_scopes()) {
        modMap.emplace(gsa->VpiName(), gs);
      }
    }
  }
  instStack_.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveGen_scope(const gen_scope* object,
                                        vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::pushVar(any* var) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  netMap.emplace(var->VpiName(), var);
  instStack_.emplace_back(var, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::popVar(any* var) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == var)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::enterMethod_func_call(const method_func_call* object,
                                               vpiHandle handle) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  if (object->Tf_call_args()) {
    for (auto arg : *object->Tf_call_args()) {
      netMap.emplace(arg->VpiName(), arg);
    }
  }
  instStack_.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveMethod_func_call(const method_func_call* object,
                                               vpiHandle handle) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == object)) {
    instStack_.pop_back();
  }
}

void ElaboratorListener::leaveRef_obj(const ref_obj* object, vpiHandle handle) {
  if (!object->Actual_group())
    ((ref_obj*)object)->Actual_group(bindAny(object->VpiName()));
}

void ElaboratorListener::leaveBit_select(const bit_select* object,
                                         vpiHandle handle) {
  leaveRef_obj(object, handle);
}

void ElaboratorListener::leaveIndexed_part_select(
    const indexed_part_select* object, vpiHandle handle) {
  leaveRef_obj(object, handle);
}

void ElaboratorListener::leavePart_select(const part_select* object,
                                          vpiHandle handle) {
  leaveRef_obj(object, handle);
}

void ElaboratorListener::leaveVar_select(const var_select* object,
                                         vpiHandle handle) {
  leaveRef_obj(object, handle);
}

}  // namespace UHDM
