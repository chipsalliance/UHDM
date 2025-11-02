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
#include <uhdm/Utils.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

#include <iostream>

namespace uhdm {
static void propagateParamAssign(ParamAssign* pass, const Any* target) {
  UhdmType targetType = target->getUhdmType();
  Serializer& s = *pass->getSerializer();
  switch (targetType) {
    case UhdmType::ClassDefn: {
      ClassDefn* defn = (ClassDefn*)target;
      const Any* lhs = pass->getLhs();
      const std::string_view name = lhs->getName();
      if (AnyCollection* params = defn->getParameters()) {
        for (Any* param : *params) {
          if (param->getName() == name) {
            ParamAssignCollection* passigns = defn->getParamAssigns();
            if (passigns == nullptr) {
              defn->setParamAssigns(s.makeCollection<ParamAssign>());
              passigns = defn->getParamAssigns();
            }
            ParamAssign* pa = s.make<ParamAssign>();
            pa->setParent(defn);
            pa->setLhs(param);
            pa->setRhs((Any*)pass->getRhs());
            passigns->push_back(pa);
          }
        }
      }
      if (const Extends* ext = defn->getExtends()) {
        if (const RefTypespec* rt = ext->getClassTypespec()) {
          propagateParamAssign(pass, rt->getActual<ClassTypespec>());
        }
      }
      if (const auto vars = defn->getVariables()) {
        for (auto var : *vars) {
          propagateParamAssign(pass, var);
        }
      }
      break;
    }
    case UhdmType::Variable: {
      Variable* var = (Variable*)target;
      if (const RefTypespec* const rt = var->getTypespec()) {
        if (const ClassTypespec* const ct = rt->getActual<ClassTypespec>()) {
          propagateParamAssign(pass, ct);
        }
      }
      break;
    }
    case UhdmType::ClassTypespec: {
      ClassTypespec* defn = (ClassTypespec*)target;
      const Any* lhs = pass->getLhs();
      const std::string_view name = lhs->getName();
      if (AnyCollection* params = defn->getParameters()) {
        for (Any* param : *params) {
          if (param->getName() == name) {
            ParamAssignCollection* passigns = defn->getParamAssigns();
            if (passigns == nullptr) {
              defn->setParamAssigns(s.makeCollection<ParamAssign>());
              passigns = defn->getParamAssigns();
            }
            ParamAssign* pa = s.make<ParamAssign>();
            pa->setParent(defn);
            pa->setLhs(param);
            pa->setRhs((Any*)pass->getRhs());
            passigns->push_back(pa);
          }
        }
      }
      if (const ClassDefn* def = defn->getClassDefn()) {
        propagateParamAssign(pass, def);
      }
      break;
    }
    default:
      break;
  }
}

void ElaboratorListener::enterVariable(const Variable* object,
                                       vpiHandle handle) {
  if (!m_inHierarchy)
    return;  // Only do class var propagation while in elaboration

  if (const RefTypespec* const rt = object->getTypespec()) {
    if (getActual<ClassTypespec>(object) != nullptr) {
      Variable* const var = const_cast<Variable*>(object);
      RefTypespec* ctps = rt->deepClone(var, m_context);
      var->setTypespec(ctps);
      if (const ClassTypespec* cctps = ctps->getActual<ClassTypespec>()) {
        if (ParamAssignCollection* params = cctps->getParamAssigns()) {
          for (ParamAssign* pass : *params) {
            propagateParamAssign(pass, cctps->getClassDefn());
          }
        }
      }
    }
  }
}

void ElaboratorListener::leaveDesign(const Design* object, vpiHandle handle) {
  const_cast<Design*>(object)->setElaborated(true);
}

static std::string_view ltrim_until(std::string_view str, char c) {
  auto it = str.find(c);
  if (it != std::string_view::npos) str.remove_prefix(it + 1);
  return str;
}

void ElaboratorListener::enterModule(const Module* object, vpiHandle handle) {
  bool topLevelModule = object->getTopModule();
  const std::string_view instName = object->getName();
  const std::string_view defName = object->getDefName();
  bool flatModule =
      instName.empty() && ((object->getParent() == 0) ||
                           ((object->getParent() != 0) &&
                            (object->getParent()->getVpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (m_debug)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    m_flatComponentMap.emplace(object->getDefName(), object);
  } else {
    // Hierachical module list (elaborated)
    m_inHierarchy = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->getNets()) {
      for (Net* net : *object->getNets()) {
        if (!net->getName().empty()) {
          netMap.emplace(net->getName(), net);
        }
      }
    }

    if (object->getVariables()) {
      for (Variable* var : *object->getVariables()) {
        if (!var->getName().empty()) {
          netMap.emplace(var->getName(), var);
        }
        if (const RefTypespec* rt = var->getTypespec()) {
          if (const EnumTypespec* etps = rt->getActual<EnumTypespec>()) {
            for (auto c : *etps->getEnumConsts()) {
              if (!c->getName().empty()) {
                netMap.emplace(c->getName(), c);
              }
            }
          }
        }
      }
    }

    if (object->getInterfaces()) {
      for (Interface* inter : *object->getInterfaces()) {
        if (!inter->getName().empty()) {
          netMap.emplace(inter->getName(), inter);
        }
      }
    }
    if (object->getInterfaceArrays()) {
      for (InterfaceArray* inter : *object->getInterfaceArrays()) {
        if (InstanceCollection* instances = inter->getInstances()) {
          for (Instance* interf : *instances) {
            if (!interf->getName().empty()) {
              netMap.emplace(interf->getName(), interf);
            }
          }
        }
      }
    }

    if (object->getPorts()) {
      for (Port* port : *object->getPorts()) {
        if (const RefObj* low = port->getLowConn<RefObj>()) {
          if (const Modport* actual = low->getActual<Modport>()) {
            // If the interface of the modport is not yet in the map
            if (!port->getName().empty()) {
              netMap.emplace(port->getName(), actual);
            }
          }
        }
      }
    }

    if (object->getNamedEvents()) {
      for (NamedEvent* var : *object->getNamedEvents()) {
        if (!var->getName().empty()) {
          netMap.emplace(var->getName(), var);
        }
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (m_muteErrors == true) {
      // In final hier_path binding we need the formal parameter, not the actual
      if (object->getParamAssigns()) {
        for (ParamAssign* passign : *object->getParamAssigns()) {
          if (!passign->getLhs()->getName().empty()) {
            paramMap.emplace(passign->getLhs()->getName(), passign->getRhs());
          }
        }
      }
    }
    if (object->getParameters()) {
      for (Any* param : *object->getParameters()) {
        ComponentMap::iterator itr = paramMap.find(param->getName());
        if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
          paramMap.erase(itr);
        }
        if (!param->getName().empty()) {
          paramMap.emplace(param->getName(), param);
        }
      }
    }
    if (object->getDefParams()) {
      for (DefParam* param : *object->getDefParams()) {
        if (!param->getName().empty()) {
          paramMap.emplace(param->getName(), param);
        }
      }
    }

    if (object->getTypespecs()) {
      for (Typespec* tps : *object->getTypespecs()) {
        if (tps->getUhdmType() == UhdmType::EnumTypespec) {
          EnumTypespec* etps = (EnumTypespec*)tps;
          for (auto c : *etps->getEnumConsts()) {
            if (!c->getName().empty()) {
              paramMap.emplace(c->getName(), c);
            }
          }
        }
      }
    }
    if (object->getPorts()) {
      for (Ports* port : *object->getPorts()) {
        if (const RefObj* low = port->getLowConn<RefObj>()) {
          if (const Interface* actual = low->getActual<Interface>()) {
            if (!port->getName().empty()) {
              netMap.emplace(port->getName(), actual);
            }
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (object->getTaskFuncs()) {
      for (TaskFunc* var : *object->getTaskFuncs()) {
        if (!var->getName().empty()) {
          funcMap.emplace(var->getName(), var);
        }
      }
    }

    ComponentMap modMap;

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = m_flatComponentMap.find(defName);
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->getVpiType();
      switch (compType) {
        case vpiModule: {
          Module* defMod = (Module*)comp;
          if (defMod->getTypespecs()) {
            for (Typespec* tps : *defMod->getTypespecs()) {
              if (tps->getUhdmType() == UhdmType::EnumTypespec) {
                EnumTypespec* etps = (EnumTypespec*)tps;
                for (EnumConst* econst : *etps->getEnumConsts()) {
                  if (!econst->getName().empty()) {
                    paramMap.emplace(econst->getName(), econst);
                  }
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (object->getGenScopeArrays()) {
      for (GenScopeArray* gsa : *object->getGenScopeArrays()) {
        if (!gsa->getName().empty()) {
          for (GenScope* gs : *gsa->getGenScopes()) {
            netMap.emplace(gsa->getName(), gs);
          }
        }
      }
    }

    // Module itself
    std::string_view modName = ltrim_until(object->getName(), '@');
    if (!modName.empty()) {
      modMap.emplace(modName, object);  // instance
    }
    modName = ltrim_until(object->getDefName(), '@');
    if (!modName.empty()) {
      modMap.emplace(modName, object);  // definition
    }

    if (object->getModules()) {
      for (Module* mod : *object->getModules()) {
        if (!mod->getName().empty()) {
          modMap.emplace(mod->getName(), mod);
        }
      }
    }

    if (object->getModuleArrays()) {
      for (ModuleArray* mod : *object->getModuleArrays()) {
        if (!mod->getName().empty()) {
          modMap.emplace(mod->getName(), mod);
        }
      }
    }

    if (const ClockingBlock* block = object->getDefaultClocking()) {
      if (!block->getName().empty()) {
        modMap.emplace(block->getName(), block);
      }
    }

    if (const ClockingBlock* block = object->getGlobalClocking()) {
      if (!block->getName().empty()) {
        modMap.emplace(block->getName(), block);
      }
    }

    if (object->getClockingBlocks()) {
      for (ClockingBlock* block : *object->getClockingBlocks()) {
        if (!block->getName().empty()) {
          modMap.emplace(block->getName(), block);
        }
      }
    }

    // Push instance context on the stack
    m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
  }
  if (m_muteErrors == false) {
    elabModule(object, handle);
  }
}

void ElaboratorListener::elabModule(const Module* object, vpiHandle handle) {
  Module* inst = const_cast<Module*>(object);
  bool topLevelModule = object->getTopModule();
  const std::string_view instName = object->getName();
  const std::string_view defName = object->getDefName();
  bool flatModule =
      instName.empty() && ((object->getParent() == 0) ||
                           ((object->getParent() != 0) &&
                            (object->getParent()->getVpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (m_debug)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    m_flatComponentMap.emplace(object->getDefName(), object);
  } else {
    // Do not elab modules used in hier_path base, that creates a loop
    if (inCallstackOfType(UhdmType::HierPath)) {
      return;
    }
    if (!m_clone) return;
    // Hierachical module list (elaborated)
    m_inHierarchy = true;
    ComponentMap::iterator itrDef = m_flatComponentMap.find(defName);
    // Check if Module instance has a definition
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = (*itrDef).second;
      if (comp->getVpiType() != vpiModule) return;
      Module* defMod = (Module*)comp;
<MODULE_ELABORATOR_LISTENER>
    }
  }
}

void ElaboratorListener::leaveModule(const Module* object, vpiHandle handle) {
  bindScheduledTaskFunc();
  if (m_inHierarchy && !m_instStack.empty() &&
      (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
    if (m_instStack.empty()) {
      m_inHierarchy = false;
    }
  }
}

void ElaboratorListener::enterPackage(const Package* object, vpiHandle handle) {
  ComponentMap netMap;
  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        netMap.emplace(var->getName(), var);
      }
      if (const RefTypespec* rt = var->getTypespec()) {
        if (const EnumTypespec* etps = rt->getActual<EnumTypespec>()) {
          for (auto c : *etps->getEnumConsts()) {
            if (!c->getName().empty()) {
              netMap.emplace(c->getName(), c);
            }
          }
        }
      }
    }
  }

  if (object->getNamedEvents()) {
    for (NamedEvent* var : *object->getNamedEvents()) {
      if (!var->getName().empty()) {
        netMap.emplace(var->getName(), var);
      }
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->getParameters()) {
    for (Any* param : *object->getParameters()) {
      if (!param->getName().empty()) {
        paramMap.emplace(param->getName(), param);
      }
    }
  }

  // Collect func and task declaration
  ComponentMap funcMap;
  ComponentMap modMap;
  // Push instance context on the stack
  m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leavePackage(const Package* object, vpiHandle handle) {
  if (m_clone) {
    if (auto vec = object->getTaskFuncs()) {
      auto clone_vec = m_serializer->makeCollection<TaskFunc>();
      ((Package*)object)->setTaskFuncs(clone_vec);
      for (auto obj : *vec) {
        enterTaskFunc(obj, nullptr);
        auto* tf = obj->deepClone((Package*)object, m_context);
        if (!tf->getName().empty()) {
          ComponentMap& funcMap =
              std::get<3>(m_instStack.at(m_instStack.size() - 2));
          auto it = funcMap.find(tf->getName());
          if (it != funcMap.end()) funcMap.erase(it);
          funcMap.emplace(tf->getName(), tf);
        }
        leaveTaskFunc(obj, nullptr);
        tf->setParent((Package*)object);
        clone_vec->push_back(tf);
      }
    }
  }
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterClassDefn(const ClassDefn* object,
                                        vpiHandle handle) {
  ComponentMap varMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;

  const ClassDefn* defn = object;
  while (defn != nullptr) {
    // Collect instance elaborated nets
    if (defn->getVariables()) {
      for (Variable* var : *defn->getVariables()) {
        if (!var->getName().empty()) {
          varMap.emplace(var->getName(), var);
        }
        if (const RefTypespec* rt = var->getTypespec()) {
          if (const EnumTypespec* etps = rt->getActual<EnumTypespec>()) {
            for (auto c : *etps->getEnumConsts()) {
              if (!c->getName().empty()) {
                varMap.emplace(c->getName(), c);
              }
            }
          }
        }
      }
    }

    if (defn->getNamedEvents()) {
      for (NamedEvent* var : *defn->getNamedEvents()) {
        if (!var->getName().empty()) {
          varMap.emplace(var->getName(), var);
        }
      }
    }

    // Collect instance parameters, defparams
    if (defn->getParameters()) {
      for (Any* param : *defn->getParameters()) {
        if (!param->getName().empty()) {
          paramMap.emplace(param->getName(), param);
        }
      }
    }

    // Collect func and task declaration
    if (defn->getMethods()) {
      for (TaskFunc* tf : *defn->getMethods()) {
        if (funcMap.find(tf->getName()) == funcMap.end()) {
          if (!tf->getName().empty()) {
            // Bind to overriden function in sub-class
            funcMap.emplace(tf->getName(), tf);
          }
        }
      }
    }

    const ClassDefn* base_defn = nullptr;
    if (const Extends* ext = defn->getExtends()) {
      if (const RefTypespec* rt = ext->getClassTypespec()) {
        if (const ClassTypespec* ctps = rt->getActual<ClassTypespec>()) {
          base_defn = ctps->getClassDefn();
        }
      }
    }
    defn = base_defn;
  }

  // Push class defn context on the stack
  // Class context is going to be pushed in case of:
  //   - imbricated classes
  //   - inheriting classes (Through the extends relation)
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
  if (m_muteErrors == false) {
    elabClassDefn(object, handle);
  }
}

void ElaboratorListener::elabClassDefn(const ClassDefn* object,
                                       vpiHandle handle) {
  if (!m_clone) return;
  ClassDefn* cl = (ClassDefn*)object;
<CLASS_ELABORATOR_LISTENER>
}

void ElaboratorListener::bindScheduledTaskFunc() {
  for (auto& call_prefix : m_scheduledTfCallBinding) {
    TFCall* call = call_prefix.first;
    const Variable* prefix = call_prefix.second;
    call->setTaskFunc(bindTaskFunc(call->getName(), prefix));
  }
  m_scheduledTfCallBinding.clear();
}

void ElaboratorListener::leaveClassDefn(const ClassDefn* object,
                                        vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterInterface(const Interface* object,
                                        vpiHandle handle) {
  const std::string_view instName = object->getName();
  const std::string_view defName = object->getDefName();
  bool flatModule =
      instName.empty() && ((object->getParent() == 0) ||
                           ((object->getParent() != 0) &&
                            (object->getParent()->getVpiType() != vpiModule)));
  // false when it is an interface in a hierachy tree
  if (m_debug)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    m_flatComponentMap.emplace(object->getDefName(), object);
  } else {
    // Hierachical module list (elaborated)
    m_inHierarchy = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->getNets()) {
      for (Net* net : *object->getNets()) {
        if (!net->getName().empty()) {
          netMap.emplace(net->getName(), net);
        }
      }
    }
    if (object->getVariables()) {
      for (Variable* var : *object->getVariables()) {
        if (!var->getName().empty()) {
          netMap.emplace(var->getName(), var);
        }
        if (const RefTypespec* rt = var->getTypespec()) {
          if (const EnumTypespec* etps = rt->getActual<EnumTypespec>()) {
            for (auto c : *etps->getEnumConsts()) {
              if (!c->getName().empty()) {
                netMap.emplace(c->getName(), c);
              }
            }
          }
        }
      }
    }

    if (object->getInterfaces()) {
      for (Interface* inter : *object->getInterfaces()) {
        if (!inter->getName().empty()) {
          netMap.emplace(inter->getName(), inter);
        }
      }
    }
    if (object->getInterfaceArrays()) {
      for (InterfaceArray* inter : *object->getInterfaceArrays()) {
        for (Instance* interf : *inter->getInstances())
          if (!interf->getName().empty()) {
            netMap.emplace(interf->getName(), interf);
          }
      }
    }
    if (object->getNamedEvents()) {
      for (NamedEvent* var : *object->getNamedEvents()) {
        if (!var->getName().empty()) {
          netMap.emplace(var->getName(), var);
        }
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (object->getParamAssigns()) {
      for (ParamAssign* passign : *object->getParamAssigns()) {
        if (!passign->getLhs()->getName().empty()) {
          paramMap.emplace(passign->getLhs()->getName(), passign->getRhs());
        }
      }
    }
    if (object->getParameters()) {
      for (Any* param : *object->getParameters()) {
        if (!param->getName().empty()) {
          ComponentMap::iterator itr = paramMap.find(param->getName());
          if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
            paramMap.erase(itr);
          }
          paramMap.emplace(param->getName(), param);
        }
      }
    }

    if (object->getPorts()) {
      for (Ports* port : *object->getPorts()) {
        if (!port->getName().empty()) {
          if (const RefObj* ro = port->getLowConn<RefObj>()) {
            if (const Any* actual = ro->getActual()) {
              if (actual->getUhdmType() == UhdmType::Interface) {
                netMap.emplace(port->getName(), actual);
              } else if (actual->getUhdmType() == UhdmType::Modport) {
                // If the interface of the modport is not yet in the map
                netMap.emplace(port->getName(), actual);
              }
            }
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (object->getTaskFuncs()) {
      for (TaskFunc* var : *object->getTaskFuncs()) {
        if (!var->getName().empty()) {
          funcMap.emplace(var->getName(), var);
        }
      }
    }

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = m_flatComponentMap.find(defName);
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->getVpiType();
      switch (compType) {
        case vpiModule: {
          Module* defMod = (Module*)comp;
          if (defMod->getTypespecs()) {
            for (Typespec* tps : *defMod->getTypespecs()) {
              if (tps->getUhdmType() == UhdmType::EnumTypespec) {
                EnumTypespec* etps = (EnumTypespec*)tps;
                for (EnumConst* econst : *etps->getEnumConsts()) {
                  if (!econst->getName().empty()) {
                    paramMap.emplace(econst->getName(), econst);
                  }
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (object->getGenScopeArrays()) {
      for (GenScopeArray* gsa : *object->getGenScopeArrays()) {
        if (!gsa->getName().empty()) {
          for (GenScope* gs : *gsa->getGenScopes()) {
            netMap.emplace(gsa->getName(), gs);
          }
        }
      }
    }
    ComponentMap modMap;

    if (const ClockingBlock* block = object->getDefaultClocking()) {
      if (!block->getName().empty()) {
        modMap.emplace(block->getName(), block);
      }
    }

    if (const ClockingBlock* block = object->getGlobalClocking()) {
      if (!block->getName().empty()) {
        modMap.emplace(block->getName(), block);
      }
    }

    if (object->getClockingBlocks()) {
      for (ClockingBlock* block : *object->getClockingBlocks()) {
        if (!block->getName().empty()) {
          modMap.emplace(block->getName(), block);
        }
      }
    }

    // Push instance context on the stack
    m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);

    // Check if Module instance has a definition
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = itrDef->second;
      int32_t compType = comp->getVpiType();
      switch (compType) {
        case vpiInterface: {
          //  interface* defMod = (interface*)comp;
          if (m_clone) {
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

void ElaboratorListener::leaveInterface(const Interface* object,
                                        vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

// Hardcoded implementations

Any* ElaboratorListener::bindNet(std::string_view name) const {
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
    }
    const ComponentMap& netMap = std::get<1>(*i);
    ComponentMap::const_iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      const Any* p = netItr->second;
      if (const RefObj* r = any_cast<RefObj>(p)) {
        p = r->getActual();
      }
      return const_cast<Any*>(p);
    }
  }
  return nullptr;
}

// Bind to a net or parameter in the current instance
Any* ElaboratorListener::bindAny(std::string_view name) const {
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
    }
    const ComponentMap& netMap = std::get<1>(*i);
    ComponentMap::const_iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      const Any* p = netItr->second;
      if (const RefObj* r = any_cast<RefObj>(p)) {
        p = r->getActual();
      }
      return const_cast<Any*>(p);
    }

    const ComponentMap& paramMap = std::get<2>(*i);
    ComponentMap::const_iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      const Any* p = paramItr->second;
      if (const RefObj* r = any_cast<RefObj>(p)) {
        p = r->getActual();
      }
      return const_cast<Any*>(p);
    }

    const ComponentMap& modMap = std::get<4>(*i);
    ComponentMap::const_iterator modItr = modMap.find(name);
    if (modItr != modMap.end()) {
      const Any* p = modItr->second;
      if (const RefObj* r = any_cast<RefObj>(p)) {
        p = r->getActual();
      }
      return const_cast<Any*>(p);
    }
  }
  return nullptr;
}

// Bind to a param in the current instance
Any* ElaboratorListener::bindParam(std::string_view name) const {
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
    }
    const ComponentMap& paramMap = std::get<2>(*i);
    ComponentMap::const_iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      const Any* p = paramItr->second;
      if (const RefObj* r = any_cast<RefObj>(p)) {
        p = r->getActual();
      }
      return const_cast<Any*>(p);
    }
  }
  return nullptr;
}

// Bind to a function or task in the current scope
TaskFunc* ElaboratorListener::bindTaskFunc(std::string_view name,
                                           const Variable* prefix) const {
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
    }
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      const Any* p = funcItr->second;
      if (const RefObj* r = any_cast<RefObj>(p)) {
        p = r->getActual();
      }
      return const_cast<TaskFunc*>(any_cast<TaskFunc>(p));
    }
  }
  if (prefix) {
    if (const RefTypespec* rt = prefix->getTypespec()) {
      if (const ClassTypespec* tps = rt->getActual<ClassTypespec>()) {
        const ClassDefn* defn = tps->getClassDefn();
        while (defn) {
          if (defn->getMethods()) {
            for (TaskFunc* tf : *defn->getMethods()) {
              if (tf->getName() == name) return tf;
            }
          }

          const ClassDefn* base_defn = nullptr;
          if (const Extends* ext = defn->getExtends()) {
            if (const RefTypespec* ctps_rt = ext->getClassTypespec()) {
              if (const ClassTypespec* ctps =
                      ctps_rt->getActual<ClassTypespec>()) {
                base_defn = ctps->getClassDefn();
              }
            }
          }
          defn = base_defn;
        }
      }
    }
  }
  return nullptr;
}

bool ElaboratorListener::isFunctionCall(std::string_view name,
                                        const Expr* prefix) const {
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->getUhdmType() == UhdmType::Function);
    }
  }
  if (prefix) {
    if (const RefObj* ref = any_cast<RefObj>(prefix)) {
      if (const Variable* vprefix = ref->getActual<Variable>()) {
        if (const RefTypespec* const rt = vprefix->getTypespec()) {
          if (rt->getActual<ClassTypespec>() != nullptr) {
            if (const Any* func = bindTaskFunc(name, vprefix)) {
              return (func->getUhdmType() == UhdmType::Function);
            }
          }
        }
      }
    }
  }
  return true;
}

bool ElaboratorListener::isTaskCall(std::string_view name,
                                    const Expr* prefix) const {
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->getUhdmType() == UhdmType::Task);
    }
  }
  if (prefix) {
    if (const RefObj* ref = any_cast<RefObj>(prefix)) {
      if (const Variable* vprefix = ref->getActual<Variable>()) {
        if (const RefTypespec* const rt = vprefix->getTypespec()) {
          if (rt->getActual<ClassTypespec>() != nullptr) {
            if (const Any* task = bindTaskFunc(name, vprefix)) {
              return (task->getUhdmType() == UhdmType::Task);
            }
          }
        }
      }
    }
  }
  return true;
}

void ElaboratorListener::enterTaskFunc(const TaskFunc* object,
                                       vpiHandle handle) {
  // Collect instance elaborated nets
  ComponentMap varMap;
  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        varMap.emplace(var->getName(), var);
      }
    }
  }
  if (object->getIODecls()) {
    for (IODecl* decl : *object->getIODecls()) {
      if (!decl->getName().empty()) {
        varMap.emplace(decl->getName(), decl);
      }
    }
  }
  if (!object->getName().empty()) {
    varMap.emplace(object->getName(), object->getReturn());
  }

  if (const Any* parent = object->getParent()) {
    if (parent->getUhdmType() == UhdmType::ClassDefn) {
      const ClassDefn* defn = (const ClassDefn*)parent;
      while (defn) {
        if (defn->getVariables()) {
          for (Any* var : *defn->getVariables()) {
            if (!var->getName().empty()) {
              varMap.emplace(var->getName(), var);
            }
          }
        }

        const ClassDefn* base_defn = nullptr;
        if (const Extends* ext = defn->getExtends()) {
          if (const RefTypespec* rt = ext->getClassTypespec()) {
            if (const ClassTypespec* ctps = rt->getActual<ClassTypespec>()) {
              base_defn = ctps->getClassDefn();
            }
          }
        }
        defn = base_defn;
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveTaskFunc(const TaskFunc* object,
                                       vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterForStmt(const ForStmt* object, vpiHandle handle) {
  ComponentMap varMap;
  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        varMap.emplace(var->getName(), var);
      }
    }
  }
  if (object->getForInitStmts()) {
    for (Any* stmt : *object->getForInitStmts()) {
      if (stmt->getUhdmType() == UhdmType::Assignment) {
        Assignment* astmt = (Assignment*)stmt;
        const Any* lhs = astmt->getLhs();
        if (!lhs->getName().empty()) {
          varMap.emplace(lhs->getName(), lhs);
        }
      }
    }
  }
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveForStmt(const ForStmt* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterForeachStmt(const ForeachStmt* object,
                                          vpiHandle handle) {
  ComponentMap varMap;
  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        varMap.emplace(var->getName(), var);
      }
    }
  }
  if (object->getLoopVars()) {
    for (Any* var : *object->getLoopVars()) {
      if (!var->getName().empty()) {
        varMap.emplace(var->getName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveForeachStmt(const ForeachStmt* object,
                                          vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterBegin(const Begin* object, vpiHandle handle) {
  ComponentMap varMap;
  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        varMap.emplace(var->getName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveBegin(const Begin* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterForkStmt(const ForkStmt* object,
                                       vpiHandle handle) {
  ComponentMap varMap;
  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        varMap.emplace(var->getName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}
void ElaboratorListener::leaveForkStmt(const ForkStmt* object,
                                       vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterFunction(const Function* object,
                                       vpiHandle handle) {
  enterTaskFunc(object, handle);
}

void ElaboratorListener::leaveFunction(const Function* object,
                                       vpiHandle handle) {
  leaveTaskFunc(object, handle);
}

void ElaboratorListener::enterTask(const Task* object, vpiHandle handle) {
  enterTaskFunc(object, handle);
}

void ElaboratorListener::leaveTask(const Task* object, vpiHandle handle) {
  leaveTaskFunc(object, handle);
}

void ElaboratorListener::enterGenScope(const GenScope* object,
                                       vpiHandle handle) {
  // Collect instance elaborated nets

  ComponentMap netMap;
  if (object->getNets()) {
    for (Net* net : *object->getNets()) {
      if (!net->getName().empty()) {
        netMap.emplace(net->getName(), net);
      }
    }
  }

  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        netMap.emplace(var->getName(), var);
      }
      if (const RefTypespec* rt = var->getTypespec()) {
        if (const EnumTypespec* etps = rt->getTypespec<EnumTypespec>()) {
          for (auto c : *etps->getEnumConsts()) {
            if (!c->getName().empty()) {
              netMap.emplace(c->getName(), c);
            }
          }
        }
      }
    }
  }

  if (object->getInterfaces()) {
    for (Interface* inter : *object->getInterfaces()) {
      if (!inter->getName().empty()) {
        netMap.emplace(inter->getName(), inter);
      }
    }
  }
  if (object->getInterfaceArrays()) {
    for (InterfaceArray* inter : *object->getInterfaceArrays()) {
      if (InstanceCollection* instances = inter->getInstances()) {
        for (Instance* interf : *instances) {
          if (!interf->getName().empty()) {
            netMap.emplace(interf->getName(), interf);
          }
        }
      }
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->getParameters()) {
    for (Any* param : *object->getParameters()) {
      if (!param->getName().empty()) {
        paramMap.emplace(param->getName(), param);
      }
    }
  }
  if (object->getDefParams()) {
    for (DefParam* param : *object->getDefParams()) {
      if (!param->getName().empty()) {
        paramMap.emplace(param->getName(), param);
      }
    }
  }

  ComponentMap funcMap;
  ComponentMap modMap;

  if (object->getModules()) {
    for (Module* mod : *object->getModules()) {
      if (!mod->getName().empty()) {
        modMap.emplace(mod->getName(), mod);
      }
    }
  }

  if (object->getModuleArrays()) {
    for (ModuleArray* mod : *object->getModuleArrays()) {
      if (!mod->getName().empty()) {
        modMap.emplace(mod->getName(), mod);
      }
    }
  }

  // Collect gen_scope
  if (object->getGenScopeArrays()) {
    for (GenScopeArray* gsa : *object->getGenScopeArrays()) {
      if (!gsa->getName().empty()) {
        for (GenScope* gs : *gsa->getGenScopes()) {
          modMap.emplace(gsa->getName(), gs);
        }
      }
    }
  }
  m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveGenScope(const GenScope* object,
                                       vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::pushVar(Any* var) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  if (!var->getName().empty()) {
    netMap.emplace(var->getName(), var);
  }
  m_instStack.emplace_back(var, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::popVar(Any* var) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == var)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::enterMethodFuncCall(const MethodFuncCall* object,
                                             vpiHandle handle) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  if (object->getArguments()) {
    for (auto arg : *object->getArguments()) {
      if (!arg->getName().empty()) {
        netMap.emplace(arg->getName(), arg);
      }
    }
  }
  m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void ElaboratorListener::leaveMethodFuncCall(const MethodFuncCall* object,
                                             vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void ElaboratorListener::leaveRefObj(const RefObj* object, vpiHandle handle) {
  if (Any* res = bindAny(object->getName())) {
    ((RefObj*)object)->setActual(res);
  }
}

void ElaboratorListener::leaveBitSelect(const BitSelect* object,
                                        vpiHandle handle) {
  leaveRefObj(object, handle);
}

void ElaboratorListener::leaveIndexedPartSelect(const IndexedPartSelect* object,
                                                vpiHandle handle) {
  leaveRefObj(object, handle);
}

void ElaboratorListener::leavePartSelect(const PartSelect* object,
                                         vpiHandle handle) {
  leaveRefObj(object, handle);
}

void ElaboratorListener::leaveVarSelect(const VarSelect* object,
                                        vpiHandle handle) {
  leaveRefObj(object, handle);
}

}  // namespace uhdm
