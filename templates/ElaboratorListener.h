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

#include <uhdm/VpiListener.h>
#include <uhdm/clone_tree.h>

// Since there is a lot of implementation happening inside this
// header, we need to include this.
// TODO: move implementation in ElaboratorListener.cpp
#include <uhdm/Serializer.h>

namespace UHDM {

class ElaboratorListener : public VpiListener {
  friend function;
  friend task;
  friend gen_scope_array;
public:

  ElaboratorListener (Serializer* serializer, bool debug = false) : serializer_(serializer), debug_(debug) {}
  void uniquifyTypespec(bool uniquify) { uniquifyTypespec_ = uniquify; }
  bool uniquifyTypespec() { return uniquifyTypespec_; }
  void bindOnly(bool bindOnly) { clone_ = !bindOnly; }
  bool bindOnly() { return !clone_; }
  bool isFunctionCall(const std::string& name, const expr* prefix);

  bool isTaskCall(const std::string& name, const expr* prefix);

  // Bind to a net in the current instance
  any* bindNet(const std::string& name);

  // Bind to a net or parameter in the current instance
  any* bindAny(const std::string& name);

  // Bind to a param in the current instance
  any* bindParam(const std::string& name);

  // Bind to a function or task in the current scope
  any* bindTaskFunc(const std::string& name, const class_var* prefix = nullptr);

  void scheduleTaskFuncBinding(tf_call* clone) { scheduledTfCallBinding_.push_back(clone); }

protected:
  typedef std::map<std::string, const BaseClass*> ComponentMap;

  void leaveDesign(const design* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle) override {
    design* root = (design*) object;
    root->VpiElaborated(true);
  }

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
      
      // Collect instance parameters, defparams
      ComponentMap paramMap;
      if (object->Param_assigns()) {
        for (param_assign* passign : *object->Param_assigns()) {
          paramMap.insert(std::make_pair(passign->Lhs()->VpiName(), passign->Rhs()));
        }
      }
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

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    for (tf_call* call : scheduledTfCallBinding_) {
      if (call->UhdmType() == uhdmfunc_call) {
        if (function* f = dynamic_cast<function*>(bindTaskFunc(call->VpiName()))) {
          ((func_call*)call)->Function(f);
        }
      } else {
        if (task* f = dynamic_cast<task*>(bindTaskFunc(call->VpiName()))) {
          ((task_call*)call)->Task(f);
        }
      }
    }
    scheduledTfCallBinding_.clear();
    if (inHierarchy_) {
      instStack_.pop_back();
      if (instStack_.empty()) {
        inHierarchy_ = false;
      }
    }
  }

  void enterPackage(const package* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle) override {

      ComponentMap netMap;

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
      instStack_.push_back(std::make_pair(object, std::make_tuple(netMap, paramMap, funcMap)));

      if (clone_) {
        if (auto vec = object->Task_funcs()) {
          auto clone_vec = serializer_->MakeTask_funcVec();
          ((package*)object)->Task_funcs(clone_vec);
          for (auto obj : *vec) {
            enterTask_func(obj, object, nullptr, nullptr);
            auto* tf = obj->DeepClone(serializer_, this, (package*) object);
            ComponentMap& funcMap = std::get<2>((instStack_.at(instStack_.size()-2)).second);
            funcMap.insert(std::make_pair(tf->VpiName(), tf));
            leaveTask_func(obj, object, nullptr, nullptr);
            tf->VpiParent((package*) object);
            clone_vec->push_back(tf);
          }
        }
      }

  }

  void leavePackage(const package* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle) override {
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
    if (clone_) {
<CLASS_ELABORATOR_LISTENER>
    }
  }

  void leaveClass_defn(const class_defn* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    instStack_.pop_back();
  }

  void enterVariables(const variables* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;

  void leaveVariables(const variables* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;


  void enterTask_func(const task_func* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTask_func(const task_func* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void enterGen_scope(const gen_scope* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveGen_scope(const gen_scope* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveRef_obj(const ref_obj* object, const BaseClass* parent,
		    vpiHandle handle, vpiHandle parentHandle) override;
private:

  // Instance context stack
  typedef std::vector<std::pair<const BaseClass*, std::tuple<ComponentMap, ComponentMap, ComponentMap>>> InstStack;
  InstStack instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;

  Serializer* serializer_ = nullptr;
  bool inHierarchy_ = false;
  bool debug_ = false;
  bool uniquifyTypespec_ = true;
  bool clone_ = true;
  std::vector<tf_call*> scheduledTfCallBinding_;
};

};
