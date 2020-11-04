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

  bool isFunctionCall(const std::string& name, const expr* prefix); 
  
  bool isTaskCall(const std::string& name, const expr* prefix); 

  // Bind to a net in the current instance
  net* bindNet(const std::string& name) {
    if (instStack_.size()) {
      ComponentMap& netMap = std::get<0>(instStack_.back().second);
      ComponentMap::iterator netItr = netMap.find(name);
      if (netItr != netMap.end()) {
        return (net*) (*netItr).second;
      }
    }
    return nullptr;
  }

  // Bind to a net or parameter in the current instance
  any* bindAny(const std::string& name) {
    if (instStack_.size()) {
      ComponentMap& netMap = std::get<0>(instStack_.back().second);
      ComponentMap::iterator netItr = netMap.find(name);
      if (netItr != netMap.end()) {
        return (any*) (*netItr).second;
      }
  
      ComponentMap& paramMap = std::get<1>(instStack_.back().second);
      ComponentMap::iterator paramItr = paramMap.find(name);
      if (paramItr != paramMap.end()) {
        return (any*) (*paramItr).second;
      }
    }
    return nullptr;
  }

  // Bind to a param in the current instance
  any* bindParam(const std::string& name) {
    if (instStack_.size()) {
      ComponentMap& paramMap = std::get<1>(instStack_.back().second);
      ComponentMap::iterator paramItr = paramMap.find(name);
      if (paramItr != paramMap.end()) {
        return (any*) (*paramItr).second;
      }
    }
    return nullptr;
  }

  // Bind to a function or task in the current scope
  any* bindTaskFunc(const std::string& name, const class_var* prefix = nullptr) {
    if (instStack_.size()) {
      for (InstStack::reverse_iterator i = instStack_.rbegin(); 
        i != instStack_.rend(); ++i ) { 
	      ComponentMap& funcMap = std::get<2>((*i).second);
        ComponentMap::iterator funcItr = funcMap.find(name);
        if (funcItr != funcMap.end()) {
          return (any*) (*funcItr).second;
        }
      }
    }
    if (prefix) {
      const typespec* tps = prefix->Typespec();
      if (tps && tps->UhdmType() == uhdmclass_typespec) {
        const class_defn* def = ((class_typespec*) tps)->Class_defn();
        while (def) {
          if (def->Task_funcs()) {
            for (task_func* tf : *def->Task_funcs()) {
              if (tf->VpiName() == name) 
                return tf;
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
            module* defMod = (module*) comp;
            if (defMod->Typespecs()) {
              for (typespec* tps : *defMod->Typespecs()) {
                if (tps->UhdmType() == uhdmenum_typespec) {
                  enum_typespec* etps = (enum_typespec*)tps;
                  for (enum_const* econst : * etps->Enum_consts()) {
                    paramMap.insert(std::make_pair(econst->VpiName(), econst));
                  }
                }
              }
            }
          }
        }
      }
       
      // Push instance context on the stack
      instStack_.push_back(std::make_pair(object, std::make_tuple(netMap, paramMap, funcMap)));
      
      // Check if Module instance has a definition
      if (itrDef != flatComponentMap_.end()) {
        const BaseClass* comp = (*itrDef).second;
        int compType = comp->VpiType();
        switch (compType) {
        case vpiModule: {
          module* defMod = (module*) comp;

<MODULE_ELABORATOR_LISTENER>

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
      instStack_.pop_back();
  }

  void enterClass_defn(const class_defn* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    class_defn* cl = (class_defn*) object;

    // Collect instance elaborated nets
    ComponentMap varMap;
    if (object->Variables()) {
      for (variables* var : *object->Variables()) {
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
    instStack_.push_back(std::make_pair(object, std::make_tuple(varMap, paramMap, funcMap)));

<CLASS_ELABORATOR_LISTENER>
    
  }

  void leaveClass_defn(const class_defn* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    instStack_.pop_back();
  }

  void enterVariables(const variables* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;

  void leaveVariables(const variables* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;

private:

  // Instance context stack
  typedef std::vector<std::pair<const BaseClass*, std::tuple<ComponentMap, ComponentMap, ComponentMap>>> InstStack;
  InstStack instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;

  Serializer* serializer_;

  bool debug_;
};

};
