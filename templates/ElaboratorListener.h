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
      
      // Push instance context on the stack
      instStack_.push(std::make_pair(object, netMap));
      
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

  
  // Bind to a net in the parent instance
  net* bindParentNet(const std::string& name) {
    std::pair<const BaseClass*, ComponentMap> mem = instStack_.top();
    instStack_.pop();
    ComponentMap& netMap = instStack_.top().second;
    instStack_.push(mem);
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (net*) (*netItr).second;
    }
    return nullptr;
  }
    
  // Bind to a net in the current instance
  net* bindNet(const std::string& name) {
    ComponentMap& netMap = instStack_.top().second;
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (net*) (*netItr).second;
    }
    return nullptr;
  }

private:

  // Instance context stack
  std::stack<std::pair<const BaseClass*, ComponentMap>> instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;

  Serializer* serializer_;

  bool debug_;
};

};
