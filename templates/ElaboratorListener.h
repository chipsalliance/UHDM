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

#include <uhdm/VpiListener.h>

#include <map>
#include <unordered_set>
#include <vector>

namespace UHDM {

class Serializer;

class ElaboratorListener : public VpiListener {
  friend function;
  friend task;
  friend gen_scope_array;

 public:
  ElaboratorListener(Serializer* serializer, bool debug = false)
      : serializer_(serializer), debug_(debug) {}
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

  void scheduleTaskFuncBinding(tf_call* clone) {
    scheduledTfCallBinding_.push_back(clone);
  }

 protected:
  typedef std::map<std::string, const BaseClass*> ComponentMap;

  void leaveDesign(const design* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) final;

  void enterModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) final;

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) final;

  void enterPackage(const package* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) final;

  void leavePackage(const package* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) final;

  void enterClass_defn(const class_defn* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) final;

  void leaveClass_defn(const class_defn* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) final;

  void enterGen_scope(const gen_scope* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) final;

  void leaveGen_scope(const gen_scope* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) final;

  void leaveRef_obj(const ref_obj* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) final;

  void enterRef_var(const ref_var* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) final;

  void enterShort_real_var(const short_real_var* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) final;

  void enterReal_var(const real_var* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) final;

  void enterByte_var(const byte_var* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) final;

  void enterShort_int_var(const short_int_var* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) final;

  void enterInt_var(const int_var* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) final;

  void enterLong_int_var(const long_int_var* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) final;

  void enterInteger_var(const integer_var* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) final;

  void enterTime_var(const time_var* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) final;

  void enterArray_var(const array_var* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) final;

  void enterPacked_array_var(const packed_array_var* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) final;

  void enterBit_var(const bit_var* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) final;

  void enterLogic_var(const logic_var* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) final;

  void enterStruct_var(const struct_var* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) final;

  void enterUnion_var(const union_var* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) final;

  void enterEnum_var(const enum_var* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) final;

  void enterString_var(const string_var* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) final;

  void enterChandle_var(const chandle_var* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) final;

  void enterVar_bit(const var_bit* object, const BaseClass* parent,
                  vpiHandle handle, vpiHandle parentHandle) final;

  void enterClass_var(const class_var* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) final;

  void enterFunction(const function* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) final;
  void leaveFunction(const function* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) final;

  void enterTask(const task* object, const BaseClass* parent, vpiHandle handle,
                 vpiHandle parentHandle) final;
  void leaveTask(const task* object, const BaseClass* parent, vpiHandle handle,
                 vpiHandle parentHandle) final;

 private:
  void enterVariables(const variables* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle);

  void enterTask_func(const task_func* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle);

  void leaveTask_func(const task_func* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle);

  typedef std::unordered_set<const UHDM::any*> VisitedSet;
  VisitedSet visited_;

  // Instance context stack
  typedef std::vector<std::pair<
      const BaseClass*, std::tuple<ComponentMap, ComponentMap, ComponentMap>>>
      InstStack;
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

};  // namespace UHDM

#endif  // UHDM_ELABORATORLISTENER_H
