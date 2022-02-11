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

#ifndef UHDM_ELABORATORLISTENER_H
#define UHDM_ELABORATORLISTENER_H

#include <map>

#include <uhdm/VpiListener.h>


namespace UHDM {

class Serializer;

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

  void leaveDesign(const design* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;

  void enterModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;

  void enterPackage(const package* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;

  void leavePackage(const package* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;

  void enterClass_defn(const class_defn* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

  void leaveClass_defn(const class_defn* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

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

#endif  // UHDM_ELABORATORLISTENER_H
