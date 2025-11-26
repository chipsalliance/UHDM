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
 * File:   Elaborator.cpp
 * Author: hs
 *
 * Created on November 24, 2025, 10:03 PM
 */

#include <uhdm/Elaborator.h>
#include <uhdm/ExprEval.h>
#include <uhdm/Utils.h>
#include <uhdm/uhdm.h>

#include <iostream>

namespace uhdm {
Elaborator::Elaborator(Serializer* serializer, bool debug /* = false */,
                       bool muteErrors /* = false */)
    : Cloner(serializer), m_debug(debug), m_muteErrors(muteErrors) {}

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
            ParamAssignCollection* const passigns = defn->getParamAssigns(true);
            ParamAssign* pa = s.make<ParamAssign>();
            pa->setParent(defn);
            pa->setLhs(param);
            pa->setRhs((Any*)pass->getRhs());
            passigns->emplace_back(pa);
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
      if (const ClassTypespec* const ct = getTypespec<ClassTypespec>(target)) {
        propagateParamAssign(pass, ct);
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
            ParamAssignCollection* passigns = defn->getParamAssigns(true);
            ParamAssign* pa = s.make<ParamAssign>();
            pa->setParent(defn);
            pa->setLhs(param);
            pa->setRhs((Any*)pass->getRhs());
            passigns->emplace_back(pa);
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

void Elaborator::enterVariable(const Variable* object, vpiHandle handle) {
  if (!m_inHierarchy)
    return;  // Only do class var propagation while in elaboration
  if (const ClassTypespec* const ct = getTypespec<ClassTypespec>(object)) {
    if (const RefTypespec* tps = object->getTypespec()) {
      Variable* const var = const_cast<Variable*>(object);
      RefTypespec* ctps = clone(tps, var);
      var->setTypespec(ctps);
      if (const ClassTypespec* const cct = ctps->getActual<ClassTypespec>()) {
        if (ParamAssignCollection* params = cct->getParamAssigns()) {
          for (ParamAssign* pass : *params) {
            propagateParamAssign(pass, cct->getClassDefn());
          }
        }
      }
    }
  }
}

void Elaborator::enterAny(const Any* object, vpiHandle handle) {
  if (const Variable* const var = any_cast<const Variable*>(object)) {
    enterVariable(var, handle);
  }
}

void Elaborator::leaveDesign(const Design* object, vpiHandle handle) {
  const_cast<Design*>(object)->setElaborated(true);
}

void Elaborator::enterModule(const Module* object, vpiHandle handle) {
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
        if (const EnumTypespec* const etps = getTypespec<EnumTypespec>(var)) {
          for (auto c : *etps->getEnumConsts()) {
            if (!c->getName().empty()) {
              netMap.emplace(c->getName(), c);
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
      // In final HierPath binding we need the formal parameter, not the actual
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
  if (m_muteErrors) return;

  Module* inst = const_cast<Module*>(object);
  // false when it is a module in a hierachy tree
  if (m_debug)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    m_flatComponentMap.emplace(object->getDefName(), object);
  } else {
    // Do not elab modules used in HierPath base, that creates a loop
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
      //<MODULE_ELABORATOR_LISTENER>
    }
  }
}

void Elaborator::leaveModule(const Module* object, vpiHandle handle) {
  bindScheduledTaskFunc();
  if (m_inHierarchy && !m_instStack.empty() &&
      (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
    if (m_instStack.empty()) {
      m_inHierarchy = false;
    }
  }
}

void Elaborator::enterPackage(const Package* object, vpiHandle handle) {
  ComponentMap netMap;
  if (object->getVariables()) {
    for (Variable* var : *object->getVariables()) {
      if (!var->getName().empty()) {
        netMap.emplace(var->getName(), var);
      }
      if (const EnumTypespec* etps = getTypespec<EnumTypespec>(var)) {
        for (auto c : *etps->getEnumConsts()) {
          if (!c->getName().empty()) {
            netMap.emplace(c->getName(), c);
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

void Elaborator::leavePackage(const Package* object, vpiHandle handle) {
  if (m_clone) {
    if (auto vec = object->getTaskFuncs()) {
      auto clone_vec = ((Package*)object)->getTaskFuncs(true);
      for (auto obj : *vec) {
        enterTaskFunc(obj, nullptr);
        auto* tf = clone(obj, (Package*)object);
        if (!tf->getName().empty()) {
          ComponentMap& funcMap =
              std::get<3>(m_instStack.at(m_instStack.size() - 2));
          auto it = funcMap.find(tf->getName());
          if (it != funcMap.end()) funcMap.erase(it);
          funcMap.emplace(tf->getName(), tf);
        }
        leaveTaskFunc(obj, nullptr);
        tf->setParent((Package*)object);
        clone_vec->emplace_back(tf);
      }
    }
  }
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterClassDefn(const ClassDefn* object, vpiHandle handle) {
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
        if (const EnumTypespec* etps = getTypespec<EnumTypespec>(var))
          for (auto c : *etps->getEnumConsts()) {
            if (!c->getName().empty()) {
              varMap.emplace(c->getName(), c);
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
  if (!m_muteErrors && !m_clone) return;
  ClassDefn* cl = (ClassDefn*)object;
  //<CLASS_ELABORATOR_LISTENER>
}

void Elaborator::bindScheduledTaskFunc() {
  for (auto& call_prefix : m_scheduledTfCallBinding) {
    TFCall* call = call_prefix.first;
    const Variable* prefix = call_prefix.second;
    call->setTaskFunc(bindTaskFunc(call->getName(), prefix));
  }
  m_scheduledTfCallBinding.clear();
}

void Elaborator::leaveClassDefn(const ClassDefn* object, vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterInterface(const Interface* object, vpiHandle handle) {
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

void Elaborator::leaveInterface(const Interface* object, vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

// Hardcoded implementations

Any* Elaborator::bindNet(std::string_view name) const {
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
Any* Elaborator::bindAny(std::string_view name) const {
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
Any* Elaborator::bindParam(std::string_view name) const {
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

// Bind to a Function or Task in the current scope
TaskFunc* Elaborator::bindTaskFunc(std::string_view name,
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

bool Elaborator::isFunctionCall(std::string_view name,
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

bool Elaborator::isTaskCall(std::string_view name, const Expr* prefix) const {
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

void Elaborator::enterTaskFunc(const TaskFunc* object, vpiHandle handle) {
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

void Elaborator::leaveTaskFunc(const TaskFunc* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterForStmt(const ForStmt* object, vpiHandle handle) {
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

void Elaborator::leaveForStmt(const ForStmt* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterForeachStmt(const ForeachStmt* object, vpiHandle handle) {
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

void Elaborator::leaveForeachStmt(const ForeachStmt* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterBegin(const Begin* object, vpiHandle handle) {
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

void Elaborator::leaveBegin(const Begin* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterForkStmt(const ForkStmt* object, vpiHandle handle) {
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

void Elaborator::leaveForkStmt(const ForkStmt* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterFunction(const Function* object, vpiHandle handle) {
  enterTaskFunc(object, handle);
}

void Elaborator::leaveFunction(const Function* object, vpiHandle handle) {
  leaveTaskFunc(object, handle);
}

void Elaborator::enterTask(const Task* object, vpiHandle handle) {
  enterTaskFunc(object, handle);
}

void Elaborator::leaveTask(const Task* object, vpiHandle handle) {
  leaveTaskFunc(object, handle);
}

void Elaborator::enterGenScope(const GenScope* object, vpiHandle handle) {
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

void Elaborator::leaveGenScope(const GenScope* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::pushVar(Any* var) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  if (!var->getName().empty()) {
    netMap.emplace(var->getName(), var);
  }
  m_instStack.emplace_back(var, netMap, paramMap, funcMap, modMap);
}

void Elaborator::popVar(Any* var) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == var)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterMethodFuncCall(const MethodFuncCall* object,
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

void Elaborator::leaveMethodFuncCall(const MethodFuncCall* object,
                                     vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::leaveRefObj(const RefObj* object, vpiHandle handle) {
  const Any* actual = (any_cast<RefObj>(object))->getActual();
  const Any* parent = object->getParent();
  // Last call binding happens here.
  // Logic net are the default binding (When no proper binding was found).
  // Hier path binding leaf node is more accurately done in the clone_tree
  // Operation because of variable name scope shadowing issues.
  if ((!actual) ||
      (actual && parent && parent->getUhdmType() != UhdmType::HierPath)) {
    if (Any* res = bindAny(object->getName())) {
      ((RefObj*)object)->setActual(res);
    }
  }
}

void Elaborator::leaveBitSelect(const BitSelect* object, vpiHandle handle) {
  leaveRefObj(object, handle);
}

void Elaborator::leaveIndexedPartSelect(const IndexedPartSelect* object,
                                        vpiHandle handle) {
  leaveRefObj(object, handle);
}

void Elaborator::leavePartSelect(const PartSelect* object, vpiHandle handle) {
  leaveRefObj(object, handle);
}

void Elaborator::leaveVarSelect(const VarSelect* object, vpiHandle handle) {
  leaveRefObj(object, handle);
}

// clang-format off
//<COPY_IMPLEMENTATIONS>
// clang-format on

Any* Elaborator::cloneAny(const Any* source, Any* parent) {
  if (source == nullptr) return nullptr;

  switch (source->getUhdmType()) {
    case UhdmType::Net: {
      if (Any* const target = bindNet(source->getName())) {
        return target;
      }
    } break;
    case UhdmType::Parameter:
    case UhdmType::TypeParameter: {
      if (Any* const target = bindParam(source->getName())) {
        // TODO(HS): BAD HACK!!!
        target->setParent(parent);
        if (Parameter* const targetParameter = any_cast<Parameter>(target)) {
          const uint32_t id = target->getUhdmId();
          *targetParameter = *static_cast<const Parameter*>(source);
          target->setUhdmId(id);
          copy(static_cast<const Parameter*>(source), targetParameter);
        } else if (TypeParameter* const targetTypeParameter =
                       any_cast<TypeParameter>(target)) {
          const uint32_t id = target->getUhdmId();
          *targetTypeParameter = *static_cast<const TypeParameter*>(source);
          target->setUhdmId(id);
          copy(static_cast<const TypeParameter*>(source), targetTypeParameter);
        }
        return target;
      }
    } break;
    case UhdmType::Begin: {
      enterBegin(static_cast<const Begin*>(source), nullptr);
    } break;
    case UhdmType::ForkStmt: {
      enterForkStmt(static_cast<const ForkStmt*>(source), nullptr);
    } break;
    default:
      break;
  }

  Any* target = Cloner::cloneAny(source, parent);

  switch (source->getUhdmType()) {
    case UhdmType::Begin: {
      leaveBegin(static_cast<const Begin*>(source), nullptr);
    } break;
    case UhdmType::ForkStmt: {
      leaveForkStmt(static_cast<const ForkStmt*>(source), nullptr);
    } break;
    default:
      break;
  }

  return target;
}

SysFuncCall* Elaborator::clone(const SysFuncCall* source, Any* parent) {
  SysFuncCall* const target = m_serializer->clone<SysFuncCall>(source);
  target->setParent(parent);
  if (auto obj = source->getUserSystf())
    target->setUserSystf(clone(obj, target));
  if (auto obj = source->getScope()) target->setScope(clone(obj, target));
  if (auto vec = source->getArguments())
    target->setArguments(cloneT(vec, target));
  if (auto obj = source->getTypespec()) target->setTypespec(clone(obj, target));
  return target;
}

SysTaskCall* Elaborator::clone(const SysTaskCall* source, Any* parent) {
  SysTaskCall* const target = m_serializer->clone<SysTaskCall>(source);
  target->setParent(parent);
  if (auto obj = source->getUserSystf())
    target->setUserSystf(clone(obj, target));
  if (auto obj = source->getScope()) target->setScope(clone(obj, target));
  if (auto vec = source->getArguments())
    target->setArguments(cloneT(vec, target));
  if (auto obj = source->getTypespec()) target->setTypespec(clone(obj, target));
  return target;
}

TFCall* Elaborator::clone(const MethodFuncCall* source, Any* parent) {
  const Expr* prefix = source->getPrefix();
  if (prefix) {
    prefix = clone(prefix, const_cast<MethodFuncCall*>(source));
  }
  bool is_function = isFunctionCall(source->getName(), prefix);
  if (is_function) {
    MethodFuncCall* const target = m_serializer->clone<MethodFuncCall>(source);
    target->setParent(parent);
    if (auto obj = source->getPrefix()) target->setPrefix(clone(obj, target));
    const Any* parent = target->getParent();
    const Variable* var = getActual<Variable>(target->getPrefix());
    if (getTypespec<ClassTypespec>(var) != nullptr)
      m_scheduledTfCallBinding.emplace_back(target, var);
    Any* pushedVar = nullptr;
    if (auto vec = source->getArguments()) {
      auto clone_vec = target->getArguments(true);
      for (auto obj : *vec) {
        Any* arg = clone(obj, target);
        // CB callbacks_to_append[$];
        // unique_callbacks_to_append = callbacks_to_append.unique( cb_ )
        // with ( cb_.get_inst_id );
        if (parent->getUhdmType() == UhdmType::HierPath) {
          HierPath* phier = (HierPath*)parent;
          Any* last = phier->getPathElems()->back();
          if (RefObj* last_ref = any_cast<RefObj>(last)) {
            if (const Any* actual = last_ref->getActual()) {
              if (RefObj* refarg = any_cast<RefObj>(arg)) {
                bool override = false;
                if (const Any* act = refarg->getActual()) {
                  if (act->getName() == obj->getName()) {
                    override = true;
                  }
                } else {
                  override = true;
                }
                // if (override) {
                //   if (actual->getUhdmType() == UhdmType::array_var) {
                //     array_var* arr = (array_var*)actual;
                //     if (arr->getVariables() && !arr->getVariables()->empty()) {
                //       Variable* var = arr->getVariables()->front();
                //       if (Variable* varclone = clone(var, obj->getParent())) {
                //         varclone->setName(obj->getName());
                //         actual = varclone;
                //         pushVar(varclone);
                //         pushedVar = varclone;
                //       }
                //     }
                //   }
                //   refarg->setActual((Any*)actual);
                // }
              }
            }
          }
        }
        clone_vec->emplace_back(arg);
      }
    }
    if (auto obj = source->getWith()) target->setWith(clone(obj, target));
    if (pushedVar) popVar(pushedVar);
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  } else {
    MethodTaskCall* const target = m_serializer->make<MethodTaskCall>();
    target->setName(source->getName());
    target->setArguments(source->getArguments());
    target->setParent(parent);
    target->setFile(source->getFile());
    target->setStartLine(source->getStartLine());
    target->setStartColumn(source->getStartColumn());
    target->setEndLine(source->getEndLine());
    target->setEndColumn(source->getEndColumn());
    if (auto obj = source->getPrefix()) target->setPrefix(clone(obj, target));
    const Variable* var = getActual<Variable>(target->getPrefix());
    if (getTypespec<ClassTypespec>(var) != nullptr)
      m_scheduledTfCallBinding.emplace_back(target, var);
    if (auto obj = source->getWith()) target->setWith(clone(obj, target));
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto vec = source->getArguments())
      target->setArguments(cloneT(vec, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  }
}

Constant* Elaborator::clone(const Constant* source, Any* parent) {
  if (uniquifyTypespec() || (source->getSize() == -1)) {
    Constant* const target = m_serializer->clone<Constant>(source);
    target->setParent(parent);
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  } else {
    return const_cast<Constant*>(source);
  }
}

TaggedPattern* Elaborator::clone(const TaggedPattern* source, Any* parent) {
  if (uniquifyTypespec()) {
    TaggedPattern* const target = m_serializer->clone<TaggedPattern>(source);
    target->setParent(parent);
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    if (auto obj = source->getPattern()) target->setPattern(clone(obj, target));
    return target;
  } else {
    return const_cast<TaggedPattern*>(source);
  }
}

TFCall* Elaborator::clone(const MethodTaskCall* source, Any* parent) {
  const Expr* prefix = source->getPrefix();
  if (prefix) {
    prefix = clone(prefix, const_cast<MethodTaskCall*>(source));
  }
  bool is_task = isTaskCall(source->getName(), prefix);
  if (is_task) {
    MethodTaskCall* const target = m_serializer->clone<MethodTaskCall>(source);
    target->setParent(parent);
    if (auto obj = source->getPrefix()) target->setPrefix(clone(obj, target));
    const Variable* var = getActual<Variable>(target->getPrefix());
    if (getTypespec<ClassTypespec>(var) != nullptr)
      m_scheduledTfCallBinding.emplace_back(target, var);
    if (auto obj = source->getWith()) target->setWith(clone(obj, target));
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto vec = source->getArguments())
      target->setArguments(cloneT(vec, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  } else {
    MethodFuncCall* const target = m_serializer->make<MethodFuncCall>();
    target->setName(source->getName());
    target->setArguments(source->getArguments());
    target->setParent(parent);
    target->setFile(source->getFile());
    target->setStartLine(source->getStartLine());
    target->setStartColumn(source->getStartColumn());
    target->setEndLine(source->getEndLine());
    target->setEndColumn(source->getEndColumn());
    if (auto obj = source->getPrefix()) target->setPrefix(clone(obj, target));
    const Variable* var = getActual<Variable>(target->getPrefix());
    if (getTypespec<ClassTypespec>(var) != nullptr)
      m_scheduledTfCallBinding.emplace_back(target, var);
    if (auto obj = source->getWith()) target->setWith(clone(obj, target));
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto vec = source->getArguments())
      target->setArguments(cloneT(vec, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  }
}

TFCall* Elaborator::clone(const FuncCall* source, Any* parent) {
  bool is_function = isFunctionCall(source->getName(), nullptr);
  if (is_function) {
    FuncCall* const target = m_serializer->clone<FuncCall>(source);
    target->setParent(parent);
    m_scheduledTfCallBinding.emplace_back(target, nullptr);
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto vec = source->getArguments())
      target->setArguments(cloneT(vec, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  } else {
    TaskCall* const target = m_serializer->make<TaskCall>();
    target->setName(source->getName());
    target->setArguments(source->getArguments());
    target->setParent(parent);
    target->setFile(source->getFile());
    target->setStartLine(source->getStartLine());
    target->setStartColumn(source->getStartColumn());
    target->setEndLine(source->getEndLine());
    target->setEndColumn(source->getEndColumn());
    m_scheduledTfCallBinding.emplace_back(target, nullptr);
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto vec = source->getArguments())
      target->setArguments(cloneT(vec, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  }
}

TFCall* Elaborator::clone(const TaskCall* source, Any* parent) {
  bool is_task = isTaskCall(source->getName(), nullptr);
  if (is_task) {
    TaskCall* const target = m_serializer->clone<TaskCall>(source);
    target->setParent(parent);
    m_scheduledTfCallBinding.emplace_back(target, nullptr);
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto vec = source->getArguments())
      target->setArguments(cloneT(vec, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  } else {
    FuncCall* const target = m_serializer->make<FuncCall>();
    target->setParent(parent);
    target->setName(source->getName());
    target->setFile(source->getFile());
    target->setStartLine(source->getStartLine());
    target->setStartColumn(source->getStartColumn());
    target->setEndLine(source->getEndLine());
    target->setEndColumn(source->getEndColumn());
    target->setArguments(source->getArguments());
    m_scheduledTfCallBinding.emplace_back(target, nullptr);
    if (auto obj = source->getScope()) target->setScope(clone(obj, target));
    if (auto vec = source->getArguments())
      target->setArguments(cloneT(vec, target));
    if (auto obj = source->getTypespec())
      target->setTypespec(clone(obj, target));
    return target;
  }
}

GenScopeArray* Elaborator::clone(const GenScopeArray* source, Any* parent) {
  GenScopeArray* const target = m_serializer->clone<GenScopeArray>(source);
  target->setParent(parent);
  if (auto obj = source->getGenVar()) target->setGenVar(clone(obj, target));
  if (auto vec = source->getGenScopes()) {
    auto clone_vec = target->getGenScopes(true);
    for (auto obj : *vec) {
      enterGenScope(obj, nullptr);
      clone_vec->emplace_back(clone(obj, target));
      leaveGenScope(obj, nullptr);
    }
  }
  if (auto obj = source->getInstance()) target->setInstance(clone(obj, target));
  return target;
}

Function* Elaborator::clone(const Function* source, Any* parent) {
  Function* const target = m_serializer->clone<Function>(source);
  target->setParent(parent);
  if (auto obj = source->getLeftExpr()) target->setLeftExpr(clone(obj, target));
  if (auto obj = source->getRightExpr())
    target->setRightExpr(clone(obj, target));
  if (auto obj = source->getReturn()) target->setReturn(clone(obj, target));
  if (auto obj = source->getInstance()) target->setInstance((Instance*)obj);
  if (Instance* inst = any_cast<Instance>(parent)) target->setInstance(inst);
  if (auto obj = source->getClassDefn())
    target->setClassDefn(clone(obj, target));
  if (auto vec = source->getIODecls()) target->setIODecls(cloneT(vec, target));
  if (auto vec = source->getVariables())
    target->setVariables(cloneT(vec, target));
  if (auto vec = source->getParameters())
    target->setParameters(cloneT(vec, target));
  if (auto vec = source->getTypespecs())
    target->setTypespecs(cloneT(vec, target));
  enterTaskFunc(target, nullptr);
  if (auto vec = source->getConcurrentAssertions())
    target->setConcurrentAssertions(cloneT(vec, target));
  if (auto vec = source->getPropertyDecls())
    target->setPropertyDecls(cloneT(vec, target));
  if (auto vec = source->getSequenceDecls())
    target->setSequenceDecls(cloneT(vec, target));
  if (auto vec = source->getNamedEvents())
    target->setNamedEvents(cloneT(vec, target));
  if (auto vec = source->getNamedEventArrays())
    target->setNamedEventArrays(cloneT(vec, target));
  if (auto vec = source->getParamAssigns())
    target->setParamAssigns(cloneT(vec, target));
  if (auto vec = source->getLetDecls())
    target->setLetDecls(cloneT(vec, target));
  if (auto vec = source->getAttributes())
    target->setAttributes(cloneT(vec, target));
  if (auto vec = source->getInstanceItems())
    target->setInstanceItems(cloneT(vec, target));
  if (auto obj = source->getStmt()) target->setStmt(clone(obj, target));
  leaveTaskFunc(target, nullptr);
  return target;
}

Task* Elaborator::clone(const Task* source, Any* parent) {
  Task* const target = m_serializer->clone<Task>(source);
  target->setParent(parent);
  if (auto obj = source->getLeftExpr()) target->setLeftExpr(clone(obj, target));
  if (auto obj = source->getRightExpr())
    target->setRightExpr(clone(obj, target));
  if (auto obj = source->getReturn()) target->setReturn(clone(obj, target));
  if (auto obj = source->getInstance()) target->setInstance((Instance*)obj);
  if (Instance* inst = any_cast<Instance>(parent)) target->setInstance(inst);
  if (auto obj = source->getClassDefn())
    target->setClassDefn(clone(obj, target));
  if (auto vec = source->getIODecls()) target->setIODecls(cloneT(vec, target));
  if (auto vec = source->getVariables())
    target->setVariables(cloneT(vec, target));
  if (auto vec = source->getTypespecs())
    target->setTypespecs(cloneT(vec, target));
  enterTaskFunc(target, nullptr);
  if (auto vec = source->getConcurrentAssertions())
    target->setConcurrentAssertions(cloneT(vec, target));
  if (auto vec = source->getPropertyDecls())
    target->setPropertyDecls(cloneT(vec, target));
  if (auto vec = source->getSequenceDecls())
    target->setSequenceDecls(cloneT(vec, target));
  if (auto vec = source->getNamedEvents())
    target->setNamedEvents(cloneT(vec, target));
  if (auto vec = source->getNamedEventArrays())
    target->setNamedEventArrays(cloneT(vec, target));
  if (auto vec = source->getParamAssigns())
    target->setParamAssigns(cloneT(vec, target));
  if (auto vec = source->getLetDecls())
    target->setLetDecls(cloneT(vec, target));
  if (auto vec = source->getAttributes())
    target->setAttributes(cloneT(vec, target));
  if (auto vec = source->getParameters())
    target->setParameters(cloneT(vec, target));
  if (auto vec = source->getInstanceItems())
    target->setInstanceItems(cloneT(vec, target));
  if (auto obj = source->getStmt()) target->setStmt(clone(obj, target));
  leaveTaskFunc(target, nullptr);
  return target;
}

ContAssign* Elaborator::clone(const ContAssign* source, Any* parent) {
  ContAssign* const target = m_serializer->clone<ContAssign>(source);
  target->setParent(parent);
  if (auto obj = source->getDelay()) target->setDelay(clone(obj, target));
  Expr* lhs = nullptr;
  if (auto obj = source->getLhs()) {
    lhs = clone(obj, target);
    if (lhs->getUhdmType() == UhdmType::HierPath) {
      HierPath* path = (HierPath*)lhs;
      Any* last = path->getPathElems()->back();
      if (RefObj* ro = any_cast<RefObj>(last)) {
        if (Net* n = ro->getActual<Net>()) {
          // The net parent has to be the same as a current scope
          if (n->getParent() == parent) lhs = n;
        }
      }
    }
    target->setLhs(lhs);
  }
  if (auto obj = source->getRhs()) {
    Expr* rhs = clone(obj, target);
    if (rhs->getUhdmType() == UhdmType::HierPath) {
      HierPath* path = (HierPath*)rhs;
      Any* last = path->getPathElems()->back();
      if (RefObj* ro = any_cast<RefObj>(last)) {
        if (Constant* c = ro->getActual<Constant>()) {
          // The constant parrent's parent has to be the same as a current scope
          if (c->getParent()->getParent() == parent) rhs = c;
        }
      }
    }
    target->setRhs(rhs);
    if (RefObj* ro = any_cast<RefObj>(lhs)) {
      if (Variable* const var = ro->getActual<Variable>()) {
        if (StructTypespec* const st = getTypespec<StructTypespec>(var)) {
          ExprEval eval(m_muteErrors);
          if (Expr* res =
                  eval.flattenPatternAssignments(*m_serializer, st, rhs)) {
            if (res->getUhdmType() == UhdmType::Operation) {
              ((Operation*)rhs)->setOperands(((Operation*)res)->getOperands());
            }
          }
        }
      }
    }
  }
  if (auto vec = source->getBits()) target->setBits(cloneT(vec, target));
  return target;
}

Any* Elaborator::bindClassTypespec(ClassTypespec* ctps, Any* current,
                                   std::string_view name, bool& found) {
  Any* previous = nullptr;
  const ClassDefn* defn = ctps->getClassDefn();
  while (defn) {
    if (defn->getVariables()) {
      for (Variable* var : *defn->getVariables()) {
        if (var->getName() == name) {
          if (RefObj* ro = any_cast<RefObj>(current)) {
            ro->setActual(var);
          }
          previous = var;
          found = true;
          break;
        }
      }
    }
    if (defn->getNamedEvents()) {
      for (NamedEvent* event : *defn->getNamedEvents()) {
        if (event->getName() == name) {
          if (RefObj* ro = any_cast<RefObj>(current)) {
            ro->setActual(event);
          }
          previous = event;
          found = true;
          break;
        }
      }
    }
    if (defn->getMethods()) {
      for (TaskFunc* tf : *defn->getMethods()) {
        if (tf->getName() == name) {
          if (RefObj* ro = any_cast<RefObj>(current)) {
            ro->setActual(tf);
          } else if ((current->getUhdmType() == UhdmType::MethodFuncCall) ||
                     (current->getUhdmType() == UhdmType::MethodTaskCall)) {
            any_cast<TFCall>(current)->setTaskFunc(tf);
          }
          previous = tf;
          found = true;
          break;
        }
      }
    }
    if (found) break;

    const ClassDefn* base_defn = nullptr;
    if (const Extends* ext = defn->getExtends()) {
      if (const RefTypespec* rt = ext->getClassTypespec()) {
        if (const ClassTypespec* tp = rt->getActual<ClassTypespec>()) {
          base_defn = tp->getClassDefn();
        }
      }
    }
    defn = base_defn;
  }
  return previous;
}

HierPath* Elaborator::clone(const HierPath* source, Any* parent) {
  HierPath* const target = m_serializer->clone<HierPath>(source);
  target->setParent(parent);
  if (auto vec = source->getPathElems()) {
    auto clone_vec = target->getPathElems(true);
    Any* previous = nullptr;
    for (auto obj : *vec) {
      Any* current = clone(obj, target);
      clone_vec->emplace_back(current);
      bool found = false;
      if (RefObj* ref = any_cast<RefObj>(current)) {
        if (current->getName() == "this") {
          const Any* tmp = current;
          while (tmp) {
            if (tmp->getUhdmType() == UhdmType::ClassDefn) {
              ref->setActual((Any*)tmp);
              found = true;
              break;
            }
            tmp = tmp->getParent();
          }
        } else if (current->getName() == "super") {
          const Any* tmp = current;
          while (tmp) {
            if (tmp->getUhdmType() == UhdmType::ClassDefn) {
              ClassDefn* def = (ClassDefn*)tmp;
              if (const Extends* ext = def->getExtends()) {
                if (const RefTypespec* rt = ext->getClassTypespec()) {
                  if (const ClassTypespec* ctps =
                          rt->getActual<ClassTypespec>()) {
                    ref->setActual((Any*)ctps->getClassDefn());
                    found = true;
                    break;
                  }
                }
              }
              break;
            }
            tmp = tmp->getParent();
          }
        }
      }
      if (previous) {
        std::string_view name = obj->getName();
        if (name.empty() || name.find('[') == 0) {
          if (RefObj* ro = any_cast<RefObj>(obj)) {
            if (const Any* actual = ro->getActual()) {
              name = actual->getName();
            }
            //  a[i][j]
            if (previous->getUhdmType() == UhdmType::BitSelect) {
              BitSelect* prev = (BitSelect*)previous;
              ro->setActual((Any*)prev->getActual());
              found = true;
            }
          }
        }
        std::string nameIndexed(name);
        if (obj->getUhdmType() == UhdmType::BitSelect) {
          BitSelect* bs = static_cast<BitSelect*>(obj);
          const Expr* index = bs->getIndex();
          std::string_view indexName = index->getDecompile();
          if (!indexName.empty()) {
            nameIndexed.append("[").append(indexName).append("]");
          }
        }
        if (RefObj* pro = any_cast<RefObj>(previous)) {
          const Any* actual = pro->getActual();
          if ((actual == nullptr) && (previous->getName() == "$root")) {
            actual = currentDesign();
          }
          if (actual) {
            UhdmType actual_type = actual->getUhdmType();
            switch (actual_type) {
              case UhdmType::Design: {
                Design* scope = (Design*)actual;
                if (scope->getTopModules()) {
                  for (auto m : *scope->getTopModules()) {
                    const std::string_view modName = m->getName();
                    if (modName == name || modName == nameIndexed ||
                        modName == std::string("work@").append(name)) {
                      found = true;
                      previous = m;
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(m);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              case UhdmType::GenScope: {
                GenScope* scope = (GenScope*)actual;
                if (obj->getUhdmType() == UhdmType::MethodFuncCall) {
                  MethodFuncCall* call = (MethodFuncCall*)current;
                  if (scope->getTaskFuncs()) {
                    for (auto tf : *scope->getTaskFuncs()) {
                      if (tf->getName() == name) {
                        call->setTaskFunc(tf);
                        previous = call->getTaskFunc();
                        found = true;
                        break;
                      }
                    }
                  }
                } else if (obj->getUhdmType() == UhdmType::MethodTaskCall) {
                  MethodTaskCall* call = (MethodTaskCall*)current;
                  if (scope->getTaskFuncs()) {
                    for (auto tf : *scope->getTaskFuncs()) {
                      if (tf->getName() == name) {
                        call->setTaskFunc(tf);
                        previous = call->getTaskFunc();
                        found = true;
                        break;
                      }
                    }
                  }
                } else {
                  if (!found && scope->getModules()) {
                    for (auto m : *scope->getModules()) {
                      if (m->getName() == name || m->getName() == nameIndexed) {
                        found = true;
                        previous = m;
                        if (RefObj* cro = any_cast<RefObj>(current)) {
                          cro->setActual(m);
                        }
                        break;
                      }
                    }
                  }
                  if (!found && scope->getNets()) {
                    for (auto m : *scope->getNets()) {
                      if (m->getName() == name || m->getName() == nameIndexed) {
                        found = true;
                        previous = m;
                        if (RefObj* cro = any_cast<RefObj>(current)) {
                          cro->setActual(m);
                        }
                        break;
                      }
                    }
                  }
                  if (!found && scope->getVariables()) {
                    for (auto m : *scope->getVariables()) {
                      if (m->getName() == name || m->getName() == nameIndexed) {
                        found = true;
                        previous = m;
                        if (RefObj* cro = any_cast<RefObj>(current)) {
                          cro->setActual(m);
                        }
                        break;
                      }
                    }
                  }
                  if (!found && scope->getGenScopeArrays()) {
                    for (auto gsa : *scope->getGenScopeArrays()) {
                      if (gsa->getName() == name ||
                          gsa->getName() == nameIndexed) {
                        if (!gsa->getGenScopes()->empty()) {
                          auto gs = gsa->getGenScopes()->front();
                          if (RefObj* cro = any_cast<RefObj>(current)) {
                            cro->setActual(gs);
                          }
                          previous = gs;
                          found = true;
                          break;
                        }
                      }
                    }
                  }
                }
                break;
              }
              case UhdmType::Modport: {
                Modport* mp = (Modport*)actual;
                if (mp->getIODecls()) {
                  for (IODecl* decl : *mp->getIODecls()) {
                    if (decl->getName() == name) {
                      found = true;
                      previous = decl;
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(decl);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              case UhdmType::NamedEvent: {
                if (name == "triggered") {
                  // Builtin
                  found = true;
                }
                break;
              }
              // case UhdmType::ArrayNet: {
              //   ArrayNet* anet = (ArrayNet*)actual;
              //   NetCollection* vars = anet->Nets();
              //   if (vars && vars->size()) {
              //     actual = vars->at(0);
              //     actual_type = actual->getUhdmType();
              //   }
              //   if (name == "size" || name == "exists" || name == "find" ||
              //       name == "max" || name == "min") {
              //     FuncCall* call = m_serializer->MakeFunc_call();
              //     call->getName(name);
              //     call->setParent(target);
              //     if (RefObj* cro = any_cast<RefObj>(current)) {
              //       cro->setActual(call);
              //     }
              //     // Builtin method
              //     found = true;
              //     previous = (Any*)call;
              //   } else if (name == "") {
              //     // One of the Index(es)
              //     found = true;
              //   }
              //   break;
              // }
              case UhdmType::Variable: {
                const Typespec* tps = nullptr;
                Variable* avar = (Variable*)actual;
                // if (Variable* vars = avar->getVariables()) {
                //   if (!vars->empty()) {
                //     actual = vars->front();
                //     actual_type = actual->getUhdmType();
                //   }
                // }
                if (const RefTypespec* rt = avar->getTypespec()) {
                  tps = rt->getActual();
                  if (const ArrayTypespec* atps =
                          rt->getActual<ArrayTypespec>()) {
                    if (const RefTypespec* ert = atps->getElemTypespec()) {
                      tps = ert->getActual();
                    }
                  }
                }
                if (name == "size" || name == "exists" || name == "find" ||
                    name == "max" || name == "min") {
                  FuncCall* call = m_serializer->make<FuncCall>();
                  call->setName(name);
                  call->setParent(target);
                  if (RefObj* cro = any_cast<RefObj>(current)) {
                    cro->setActual(call);
                  }
                  // Builtin method
                  found = true;
                  previous = (Any*)call;
                }
                if (found == false) {
                  if (tps) {
                    UhdmType ttype = tps->getUhdmType();
                    if (ttype == UhdmType::ArrayTypespec) {
                      ArrayTypespec* ptps = (ArrayTypespec*)tps;
                      tps = (Typespec*)ptps->getElemTypespec();
                      if (tps) ttype = tps->getUhdmType();
                    }
                    if (ttype == UhdmType::StringTypespec) {
                      found = true;
                    } else if (ttype == UhdmType::ClassTypespec) {
                      ClassTypespec* ctps = (ClassTypespec*)tps;
                      Any* tmp = bindClassTypespec(ctps, current, name, found);
                      if (found) {
                        previous = tmp;
                      }
                    } else if (ttype == UhdmType::StructTypespec) {
                      StructTypespec* stpt = (StructTypespec*)tps;
                      for (TypespecMember* member : *stpt->getMembers()) {
                        if (member->getName() == name) {
                          if (RefObj* cro = any_cast<RefObj>(current)) {
                            cro->setActual(member);
                          }
                          previous = member;
                          found = true;
                          break;
                        }
                      }
                      if (name == "name") {
                        // Builtin introspection
                        found = true;
                      }
                    } else if (ttype == UhdmType::EnumTypespec) {
                      if (name == "name") {
                        // Builtin introspection
                        found = true;
                      }
                    } else if (ttype == UhdmType::UnionTypespec) {
                      UnionTypespec* stpt = (UnionTypespec*)tps;
                      for (TypespecMember* member : *stpt->getMembers()) {
                        if (member->getName() == name) {
                          if (RefObj* cro = any_cast<RefObj>(current)) {
                            cro->setActual(member);
                          }
                          previous = member;
                          found = true;
                          break;
                        }
                      }
                      if (name == "name") {
                        // Builtin introspection
                        found = true;
                      }
                    }
                  }
                }
                break;
              }
              default:
                break;
            }

            switch (actual_type) {
              case UhdmType::ClockingBlock: {
                ClockingBlock* block = (ClockingBlock*)actual;
                if (block->getClockingIODecls()) {
                  for (ClockingIODecl* decl : *block->getClockingIODecls()) {
                    if (decl->getName() == name) {
                      found = true;
                      previous = decl;
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(decl);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              case UhdmType::Module: {
                Module* mod = (Module*)actual;
                if (!found && mod->getVariables()) {
                  for (Variable* var : *mod->getVariables()) {
                    if (var->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(var);
                      }
                      previous = var;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && mod->getNets()) {
                  for (Net* n : *mod->getNets()) {
                    if (n->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(n);
                      }
                      previous = n;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && mod->getModules()) {
                  for (auto m : *mod->getModules()) {
                    if (m->getName() == name || m->getName() == nameIndexed) {
                      found = true;
                      previous = m;
                      break;
                    }
                  }
                }
                if (!found && mod->getInterfaces()) {
                  for (auto m : *mod->getInterfaces()) {
                    if (m->getName() == name || m->getName() == nameIndexed) {
                      found = true;
                      previous = m;
                      break;
                    }
                  }
                }
                if (!found && mod->getGenScopeArrays()) {
                  for (auto gsa : *mod->getGenScopeArrays()) {
                    if (gsa->getName() == name ||
                        gsa->getName() == nameIndexed) {
                      if (!gsa->getGenScopes()->empty()) {
                        auto gs = gsa->getGenScopes()->front();
                        if (RefObj* cro = any_cast<RefObj>(current)) {
                          cro->setActual(gs);
                        }
                        previous = gs;
                        found = true;
                        break;
                      }
                    }
                  }
                }
                if (!found && mod->getTaskFuncs()) {
                  for (auto tsf : *mod->getTaskFuncs()) {
                    if (tsf->getName() == name ||
                        tsf->getName() == nameIndexed) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(tsf);
                      }
                      previous = tsf;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && mod->getParamAssigns()) {
                  for (auto pa : *mod->getParamAssigns()) {
                    if (pa->getLhs()->getName() == name ||
                        pa->getLhs()->getName() == nameIndexed) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(pa->getRhs());
                      }
                      previous = pa;
                      found = true;
                      break;
                    }
                  }
                }
                break;
              }
              case UhdmType::Variable: {
                const Typespec* ts = getTypespec(actual);
                if (ts == nullptr) break;

                if (const ClassTypespec* const cts =
                        any_cast<ClassTypespec>(ts)) {
                  Any* tmp = bindClassTypespec(const_cast<ClassTypespec*>(cts),
                                               current, name, found);
                  if (found) {
                    previous = tmp;
                  }
                } else if (const StructTypespec* const sts =
                               any_cast<StructTypespec>(ts)) {
                  for (TypespecMember* member : *sts->getMembers()) {
                    if (member->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                } else if (const UnionTypespec* const uts =
                               any_cast<UnionTypespec>(ts)) {
                  for (TypespecMember* member : *uts->getMembers()) {
                    if (member->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                } else if (ts->getUhdmType() == UhdmType::ArrayTypespec) {
                  if (current->getUhdmType() == UhdmType::MethodFuncCall)
                    found = true;
                  else if (current->getUhdmType() == UhdmType::BitSelect)
                    found = true;
                } else if (ts->getUhdmType() == UhdmType::StringTypespec) {
                  if (current->getUhdmType() == UhdmType::MethodFuncCall)
                    found = true;
                  else if (current->getUhdmType() == UhdmType::BitSelect)
                    found = true;
                }
                if (current->getUhdmType() == UhdmType::MethodFuncCall) {
                  found = true;
                }
              } break;
              case UhdmType::Interface: {
                Interface* interf = (Interface*)actual;
                if (!found && interf->getVariables()) {
                  for (Variable* var : *interf->getVariables()) {
                    if (var->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(var);
                      }
                      previous = var;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && interf->getParameters()) {
                  for (Any* var : *interf->getParameters()) {
                    if (var->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(var);
                      }
                      previous = var;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && interf->getTaskFuncs()) {
                  for (auto tf : *interf->getTaskFuncs()) {
                    if (tf->getName() == name) {
                      previous = any_cast<Function>(tf);
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && interf->getModports()) {
                  for (Modport* mport : *interf->getModports()) {
                    if (mport->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(mport);
                      }
                      previous = mport;
                      found = true;
                      break;
                    }
                    if (mport->getIODecls()) {
                      for (IODecl* decl : *mport->getIODecls()) {
                        if (decl->getName() == name) {
                          Any* actual_decl = decl;
                          if (Any* exp = decl->getExpr()) {
                            actual_decl = exp;
                          }
                          if (actual_decl->getUhdmType() == UhdmType::RefObj) {
                            RefObj* ref = (RefObj*)actual_decl;
                            if (const Any* act = ref->getActual()) {
                              actual_decl = (Any*)act;
                            }
                          }
                          if (RefObj* cro = any_cast<RefObj>(current)) {
                            cro->setActual(actual_decl);
                          }
                          previous = actual_decl;
                          found = true;
                          break;
                        }
                      }
                    }
                    if (found) break;
                  }
                }
                if (!found && interf->getNets()) {
                  for (Nets* n : *interf->getNets()) {
                    if (n->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(n);
                      }
                      previous = n;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && interf->getPorts()) {
                  for (Port* p : *interf->getPorts()) {
                    if (p->getName() == name) {
                      if (Any* ref = p->getLowConn()) {
                        if (RefObj* nref = any_cast<RefObj>(ref)) {
                          Any* n = nref->getActual();
                          if (RefObj* cro = any_cast<RefObj>(current)) {
                            cro->setActual(n);
                          }
                          previous = n;
                          found = true;
                          break;
                        }
                      }
                    }
                  }
                }
                if (!found && interf->getGenScopeArrays()) {
                  for (auto gsa : *interf->getGenScopeArrays()) {
                    if (gsa->getName() == name ||
                        gsa->getName() == nameIndexed) {
                      if (!gsa->getGenScopes()->empty()) {
                        auto gs = gsa->getGenScopes()->front();
                        if (RefObj* cro = any_cast<RefObj>(current)) {
                          cro->setActual(gs);
                        }
                        previous = gs;
                        found = true;
                        break;
                      }
                    }
                  }
                }
                break;
              }
              case UhdmType::ClassTypespec: {
                ClassTypespec* ctps = (ClassTypespec*)actual;
                Any* tmp = bindClassTypespec(ctps, current, name, found);
                if (found) {
                  previous = tmp;
                }
                break;
              }
              case UhdmType::IODecl: {
                IODecl* decl = (IODecl*)actual;
                Typespec* tps = nullptr;
                if (RefTypespec* rt = decl->getTypespec()) {
                  tps = rt->getActual();
                }
                if (tps == nullptr) break;
                UhdmType ttype = tps->getUhdmType();
                if (ttype == UhdmType::StringTypespec) {
                  found = true;
                } else if (ttype == UhdmType::ClassTypespec) {
                  ClassTypespec* ctps = (ClassTypespec*)tps;
                  Any* tmp = bindClassTypespec(ctps, current, name, found);
                  if (found) {
                    previous = tmp;
                  }
                } else if (ttype == UhdmType::StructTypespec) {
                  StructTypespec* stpt = (StructTypespec*)tps;
                  for (TypespecMember* member : *stpt->getMembers()) {
                    if (member->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                } else if (ttype == UhdmType::EnumTypespec) {
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                } else if (ttype == UhdmType::UnionTypespec) {
                  UnionTypespec* stpt = (UnionTypespec*)tps;
                  for (TypespecMember* member : *stpt->getMembers()) {
                    if (member->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                }
                if (decl->getRanges()) {
                  if (current->getUhdmType() == UhdmType::MethodFuncCall)
                    found = true;
                  else if (current->getUhdmType() == UhdmType::BitSelect)
                    found = true;
                }
                // TODO: class method support
                if (current->getUhdmType() == UhdmType::MethodFuncCall)
                  found = true;
                break;
              }
              case UhdmType::Parameter: {
                Parameter* param = (Parameter*)actual;
                const Typespec* tps = nullptr;
                if (const RefTypespec* rt = param->getTypespec()) {
                  tps = rt->getActual();
                }
                if (tps == nullptr) break;
                UhdmType ttype = tps->getUhdmType();
                if (ttype == UhdmType::ArrayTypespec) {
                  ArrayTypespec* ptps = (ArrayTypespec*)tps;
                  if (const RefTypespec* ert = ptps->getElemTypespec()) {
                    if (const Typespec* ets = ert->getActual()) {
                      tps = ets;
                      ttype = ets->getUhdmType();
                    }
                  }
                }
                if (ttype == UhdmType::StringTypespec) {
                  found = true;
                } else if (ttype == UhdmType::ClassTypespec) {
                  ClassTypespec* ctps = (ClassTypespec*)tps;
                  Any* tmp = bindClassTypespec(ctps, current, name, found);
                  if (found) {
                    previous = tmp;
                  }
                } else if (ttype == UhdmType::StructTypespec) {
                  StructTypespec* stpt = (StructTypespec*)tps;
                  for (TypespecMember* member : *stpt->getMembers()) {
                    if (member->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                } else if (ttype == UhdmType::EnumTypespec) {
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                } else if (ttype == UhdmType::UnionTypespec) {
                  UnionTypespec* stpt = (UnionTypespec*)tps;
                  for (TypespecMember* member : *stpt->getMembers()) {
                    if (member->getName() == name) {
                      if (RefObj* cro = any_cast<RefObj>(current)) {
                        cro->setActual(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                }
                if (param->getRanges()) {
                  if (current->getUhdmType() == UhdmType::MethodFuncCall)
                    found = true;
                  else if (current->getUhdmType() == UhdmType::BitSelect)
                    found = true;
                }
                break;
              }
              case UhdmType::Operation: {
                Operation* op = (Operation*)actual;
                if (op->getOpType() != vpiAssignmentPatternOp) {
                  break;
                }
                const StructTypespec* stps = nullptr;
                if (const RefTypespec* rt = op->getTypespec()) {
                  stps = rt->getActual<StructTypespec>();
                }
                if (stps == nullptr) break;
                std::vector<std::string_view> fieldNames;
                std::vector<const Typespec*> fieldTypes;
                for (TypespecMember* memb : *stps->getMembers()) {
                  if (const RefTypespec* rt = memb->getTypespec()) {
                    fieldNames.emplace_back(memb->getName());
                    fieldTypes.emplace_back(rt->getActual());
                  }
                }
                std::vector<Any*> tmp(fieldNames.size());
                AnyCollection* orig = op->getOperands();
                Any* defaultOp = nullptr;
                Any* res = nullptr;
                int32_t index = 0;
                for (auto oper : *orig) {
                  if (oper->getUhdmType() == UhdmType::TaggedPattern) {
                    TaggedPattern* tp = (TaggedPattern*)oper;
                    const Typespec* ttp = nullptr;
                    if (const RefTypespec* rt = tp->getTypespec()) {
                      ttp = rt->getActual();
                    }
                    const std::string_view tname = ttp->getName();
                    bool oper_found = false;
                    if (tname == "default") {
                      defaultOp = oper;
                      oper_found = true;
                    }
                    for (uint32_t i = 0; i < fieldNames.size(); i++) {
                      if (tname == fieldNames[i]) {
                        tmp[i] = oper;
                        oper_found = true;
                        res = tmp[i];
                        break;
                      }
                    }
                    if (oper_found == false) {
                      for (uint32_t i = 0; i < fieldTypes.size(); i++) {
                        if (ttp->getUhdmType() ==
                            fieldTypes[i]->getUhdmType()) {
                          tmp[i] = oper;
                          oper_found = true;
                          res = tmp[i];
                          break;
                        }
                      }
                    }
                  } else {
                    if (index < (int32_t)tmp.size()) {
                      tmp[index] = oper;
                      found = true;
                      res = tmp[index];
                    }
                  }
                  index++;
                }
                if (res == nullptr) {
                  if (defaultOp) {
                    res = defaultOp;
                  }
                }
                previous = res;
                break;
              }
              default:
                // TODO: class method support
                if (current->getUhdmType() == UhdmType::MethodFuncCall)
                  found = true;
                break;
            }
            if (!found) {
              if ((!muteErrors()) && (!isInUhdmAllIterator())) {
                const std::string errMsg(source->getName());
                m_serializer->getErrorHandler()(
                    ErrorType::UHDM_UNRESOLVED_HIER_PATH, errMsg, source,
                    nullptr);
              }
            }
          } else {
            if ((!muteErrors()) && (!isInUhdmAllIterator())) {
              if (previous->getUhdmType() == UhdmType::BitSelect) {
                break;
              }
              const std::string errMsg(source->getName());
              m_serializer->getErrorHandler()(
                  ErrorType::UHDM_UNRESOLVED_HIER_PATH, errMsg, source,
                  nullptr);
            }
          }
        } else if (previous->getUhdmType() == UhdmType::TypespecMember) {
          TypespecMember* member = (TypespecMember*)previous;
          const Typespec* tps = nullptr;
          if (const RefTypespec* rt = member->getTypespec()) {
            tps = rt->getActual();
          }
          if (tps == nullptr) break;
          UhdmType ttype = tps->getUhdmType();
          if (ttype == UhdmType::ArrayTypespec) {
            ArrayTypespec* ptps = (ArrayTypespec*)tps;
            if (const RefTypespec* rt = ptps->getElemTypespec()) {
              tps = rt->getActual();
              ttype = tps->getUhdmType();
            }
          }
          if (ttype == UhdmType::StructTypespec) {
            StructTypespec* stpt = (StructTypespec*)tps;
            for (TypespecMember* tsmember : *stpt->getMembers()) {
              if (tsmember->getName() == name) {
                if (RefObj* cro = any_cast<RefObj>(current)) {
                  cro->setActual(tsmember);
                  previous = tsmember;
                  found = true;
                  break;
                }
              }
            }
          } else if (ttype == UhdmType::UnionTypespec) {
            UnionTypespec* stpt = (UnionTypespec*)tps;
            for (TypespecMember* tsmember : *stpt->getMembers()) {
              if (tsmember->getName() == name) {
                if (RefObj* cro = any_cast<RefObj>(current)) {
                  cro->setActual(tsmember);
                  previous = tsmember;
                  found = true;
                  break;
                }
              }
            }
          } else if (ttype == UhdmType::StringTypespec) {
            if (name == "len") {
              found = true;
            }
          }
        //} else if (previous->getUhdmType() == UhdmType::array_var) {
        //  array_var* avar = (array_var*)previous;
        //  if (VariableCollection* vars = avar->getVariables()) {
        //    if (!vars->empty()) {
        //      Variable* actual = vars->front();
        //      UhdmType actual_type = actual->getUhdmType();
        //      switch (actual_type) {
        //        case UhdmType::StructNet:
        //        case UhdmType::struct_var: {
        //          TypespecMemberCollection* members = nullptr;
        //          if (actual->getUhdmType() == UhdmType::StructNet) {
        //            if (RefTypespec* rt = ((StructNet*)actual)->getTypespec()) {
        //              if (StructTypespec* sts =
        //                      rt->getActual<StructTypespec>()) {
        //                members = sts->Members();
        //              } else if (UnionTypespec* uts =
        //                             rt->getActual<UnionTypespec>()) {
        //                members = uts->getMembers();
        //              }
        //            }
        //          } else if (actual->getUhdmType() == UhdmType::struct_var) {
        //            if (RefTypespec* rt =
        //                    ((struct_var*)actual)->getTypespec()) {
        //              if (StructTypespec* sts =
        //                      rt->getActual<StructTypespec>()) {
        //                members = sts->getMembers();
        //              }
        //            }
        //          }
        //          if (members) {
        //            for (TypespecMember* member : *members) {
        //              if (member->getName() == name) {
        //                if (RefObj* cro = any_cast<RefObj>(current)) {
        //                  cro->setActual(member);
        //                }
        //                previous = member;
        //                found = true;
        //                break;
        //              }
        //            }
        //          }
        //          break;
        //        }
        //        default:
        //          break;
        //      }
        //    }
        //  }
        //} else if (previous->getUhdmType() == UhdmType::struct_var ||
        //           previous->getUhdmType() == UhdmType::StructNet) {
        //  TypespecMemberCollection* members = nullptr;
        //  if (previous->getUhdmType() == UhdmType::StructNet) {
        //    if (RefTypespec* rt = ((StructNet*)previous)->getTypespec()) {
        //      if (StructTypespec* sts = rt->getActual<StructTypespec>()) {
        //        members = sts->Members();
        //      } else if (UnionTypespec* uts = rt->getActual<UnionTypespec>()) {
        //        members = uts->getMembers();
        //      }
        //    }
        //  } else if (previous->getUhdmType() == UhdmType::struct_var) {
        //    if (RefTypespec* rt = ((struct_var*)previous)->getTypespec()) {
        //      if (StructTypespec* sts = rt->getActual<StructTypespec>()) {
        //        members = sts->getMembers();
        //      }
        //    }
        //  }
        //  if (members) {
        //    for (TypespecMember* member : *members) {
        //      if (member->getName() == name) {
        //        if (RefObj* cro = any_cast<RefObj>(current)) {
        //          cro->setActual(member);
        //        }
        //        previous = member;
        //        found = true;
        //        break;
        //      }
        //    }
        //  }
        } else if (previous->getUhdmType() == UhdmType::Module) {
          Module* mod = (Module*)previous;
          if (mod->getVariables()) {
            for (Variable* var : *mod->getVariables()) {
              if (var->getName() == name) {
                if (RefObj* cro = any_cast<RefObj>(current)) {
                  cro->setActual(var);
                }
                previous = var;
                found = true;
                break;
              }
            }
          }

          if (!found && mod->getNets()) {
            for (Nets* n : *mod->getNets()) {
              if (n->getName() == name) {
                if (RefObj* cro = any_cast<RefObj>(current)) {
                  cro->setActual(n);
                }
                previous = n;
                found = true;
                break;
              }
            }
          }
          if (!found && mod->getModules()) {
            for (auto m : *mod->getModules()) {
              if (m->getName() == name || m->getName() == nameIndexed) {
                found = true;
                previous = m;
                break;
              }
            }
          }
          break;
        } else if (previous->getUhdmType() == UhdmType::GenScope) {
          GenScope* scope = (GenScope*)previous;
          if ((obj->getUhdmType() == UhdmType::MethodFuncCall) ||
              (obj->getUhdmType() == UhdmType::MethodTaskCall)) {
            MethodFuncCall* call = (MethodFuncCall*)current;
            if (scope->getTaskFuncs()) {
              for (auto tf : *scope->getTaskFuncs()) {
                if (tf->getName() == name) {
                  call->setTaskFunc(tf);
                  previous = call->getTaskFunc();
                  found = true;
                  break;
                }
              }
            }
          } else {
            if (!found && scope->getNets()) {
              for (Nets* n : *scope->getNets()) {
                if (n->getName() == name) {
                  if (RefObj* cro = any_cast<RefObj>(current)) {
                    cro->setActual(n);
                  }
                  previous = n;
                  found = true;
                  break;
                }
              }
            }
            if (!found && scope->getVariables()) {
              for (auto m : *scope->getVariables()) {
                if (m->getName() == name || m->getName() == nameIndexed) {
                  found = true;
                  previous = m;
                  if (RefObj* cro = any_cast<RefObj>(current)) {
                    cro->setActual(m);
                  }
                  break;
                }
              }
            }
            if (!found && scope->getModules()) {
              for (auto m : *scope->getModules()) {
                if (m->getName() == name || m->getName() == nameIndexed) {
                  found = true;
                  previous = m;
                  if (RefObj* cro = any_cast<RefObj>(current)) {
                    cro->setActual(m);
                  }
                  break;
                }
              }
            }
          }
        }
      }
      if (!found) previous = current;
    }
  }
  if (auto vec = source->getUses()) target->setUses(cloneT(vec, target));
  if (auto obj = source->getTypespec()) target->setTypespec(clone(obj, target));
  return target;
}

Typespec* Elaborator::clone(const ArrayTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ArrayTypespec*>(source);
}
Typespec* Elaborator::clone(const BitTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<BitTypespec*>(source);
}
Typespec* Elaborator::clone(const ByteTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ByteTypespec*>(source);
}
Typespec* Elaborator::clone(const ChandleTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ChandleTypespec*>(source);
}
Typespec* Elaborator::clone(const ClassTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ClassTypespec*>(source);
}
Typespec* Elaborator::clone(const EnumTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<EnumTypespec*>(source);
}
Typespec* Elaborator::clone(const EventTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<EventTypespec*>(source);
}
Typespec* Elaborator::clone(const ImportTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ImportTypespec*>(source);
}
Typespec* Elaborator::clone(const IntTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<IntTypespec*>(source);
}
Typespec* Elaborator::clone(const IntegerTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<IntegerTypespec*>(source);
}
Typespec* Elaborator::clone(const InterfaceTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<InterfaceTypespec*>(source);
}
Typespec* Elaborator::clone(const LogicTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<LogicTypespec*>(source);
}
Typespec* Elaborator::clone(const LongIntTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<LongIntTypespec*>(source);
}
Typespec* Elaborator::clone(const ModuleTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ModuleTypespec*>(source);
}
Typespec* Elaborator::clone(const PropertyTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<PropertyTypespec*>(source);
}
Typespec* Elaborator::clone(const RealTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<RealTypespec*>(source);
}
Typespec* Elaborator::clone(const SequenceTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<SequenceTypespec*>(source);
}
Typespec* Elaborator::clone(const ShortIntTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ShortIntTypespec*>(source);
}
Typespec* Elaborator::clone(const ShortRealTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<ShortRealTypespec*>(source);
}
Typespec* Elaborator::clone(const StringTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<StringTypespec*>(source);
}
Typespec* Elaborator::clone(const StructTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<StructTypespec*>(source);
}
Typespec* Elaborator::clone(const TimeTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<TimeTypespec*>(source);
}
Typespec* Elaborator::clone(const TypeParameter* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<TypeParameter*>(source);
}
Typespec* Elaborator::clone(const UnionTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<UnionTypespec*>(source);
}
Typespec* Elaborator::clone(const UnsupportedTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<UnsupportedTypespec*>(source);
}
Typespec* Elaborator::clone(const VoidTypespec* source, Any* parent) {
  return uniquifyTypespec() ? Cloner::clone(source, parent)
                            : const_cast<VoidTypespec*>(source);
}
}  // namespace uhdm
