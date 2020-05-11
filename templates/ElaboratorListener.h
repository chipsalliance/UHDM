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

#include <map>
#include <stack>
#include <iostream>

#include "headers/VpiListener.h"
#include "headers/clone_tree.h"

namespace UHDM {
  
class ElaboratorListener : public VpiListener {
  
public:
  
  ElaboratorListener (Serializer* serializer, bool debug = false) : serializer_(serializer), debug_(debug) {}

  // Bind to a net in the current instance
  net* bindNet(const std::string& name) {
    ComponentMap& netMap = instStack_.top().second.first;
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (net*) (*netItr).second;
    }
    return nullptr;
  }

  // Bind to a net or parameter in the current instance
  any* bindAny(const std::string& name) {
    ComponentMap& netMap = instStack_.top().second.first;
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (any*) (*netItr).second;
    }
    ComponentMap& paramMap = instStack_.top().second.second;
    ComponentMap::iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      return (any*) (*paramItr).second;
    }
    return nullptr;
  }
  
protected:
  typedef std::map<std::string, const BaseClass*> ComponentMap;

  void enterModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    module* inst = (module*) object;
    bool topLevelModule         = object->VpiTopModule();
    const std::string& instName = object->VpiName(); 
    const std::string& defName  = object->VpiDefName(); 
    bool flatModule             = (instName == "") && ((object->VpiParent() == 0) ||
                                                       ((object->VpiParent() != 0) && (object->VpiParent()->VpiType() != vpiModule)));
                                  // false when it is a module in a hierachy tree
    if (debug_)
      std::cout << "Module: " << defName << " (" << instName << ") Flat:"  << flatModule << ", Top:" << topLevelModule << std::endl;

    if (flatModule) {
      // Flat list of module (unelaborated)
      flatComponentMap_.insert(std::make_pair(object->VpiDefName(), object));
    } else {
      // Hierachical module list (elaborated)

      // Collect instance elaborated nets
      ComponentMap netMap;
      if (object->Nets()) {
        for (net* net : *object->Nets()) {
          netMap.insert(std::make_pair(net->VpiName(), net));
        }
      }

      // Collect instance parameters
      ComponentMap paramMap;
      if (object->Parameters()) {
        for (parameters* param : *object->Parameters()) {
          paramMap.insert(std::make_pair(param->VpiName(), param));
        }
      }
      if (object->Def_params()) {
        for (def_param* param : *object->Def_params()) {
          paramMap.insert(std::make_pair(param->VpiName(), param));
        }
      }
           
      // Push instance context on the stack
      instStack_.push(std::make_pair(object, std::make_pair(netMap, paramMap)));
      
      // Check if Module instance has a definition
      ComponentMap::iterator itrDef = flatComponentMap_.find(defName);      
      if (itrDef != flatComponentMap_.end()) {
        const BaseClass* comp = (*itrDef).second;
        int compType = comp->VpiType();
        switch (compType) {
        case vpiModule: {
          module* defMod = (module*) comp;

<ELABORATOR_LISTENER>

          break;
        }
        default:
          break;
        }
      }
      
    }
  }

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    const std::string& instName = object->VpiName(); 
    bool flatModule             = (instName == "") && ((object->VpiParent() == 0) ||
                                                       ((object->VpiParent() != 0) && (object->VpiParent()->VpiType() != vpiModule)));
                                  // false when it is a module in a hierachy tree
    if (!flatModule) 
      instStack_.pop();
  }

private:

  // Instance context stack
  std::stack<std::pair<const BaseClass*, std::pair<ComponentMap, ComponentMap>>> instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;

  Serializer* serializer_;

  bool debug_;
};

};
