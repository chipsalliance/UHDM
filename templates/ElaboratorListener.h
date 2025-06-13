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

#include <uhdm/BaseClass.h>
#include <uhdm/VpiListener.h>

#include <map>
#include <unordered_set>
#include <vector>

namespace UHDM {

class ElaboratorContext;
class ElaboratorListener;
class Serializer;

class ElaboratorListener final : public VpiListener {
  friend function;
  friend task;
  friend gen_scope_array;

 public:
  void setContext(ElaboratorContext* context) { context_ = context; }
  void uniquifyTypespec(bool uniquify) { uniquifyTypespec_ = uniquify; }
  bool uniquifyTypespec() { return uniquifyTypespec_; }
  void bindOnly(bool bindOnly) { clone_ = !bindOnly; }
  bool bindOnly() { return !clone_; }
  bool isFunctionCall(std::string_view name, const expr* prefix) const;
  bool muteErrors() { return muteErrors_; }
  bool isTaskCall(std::string_view name, const expr* prefix) const;
  void ignoreLastInstance(bool ignore) override { ignoreLastInstance_ = ignore; }

  // Bind to a net in the current instance
  any* bindNet(std::string_view name) const;

  // Bind to a net or parameter in the current instance
  any* bindAny(std::string_view name) const;

  // Bind to a param in the current instance
  any* bindParam(std::string_view name) const;

  // Bind to a function or task in the current scope
  any* bindTaskFunc(std::string_view name,
                    const class_var* prefix = nullptr) const;

  void scheduleTaskFuncBinding(tf_call* clone, const class_var* prefix) {
    scheduledTfCallBinding_.push_back(std::make_pair(clone, prefix));
  }
  void bindScheduledTaskFunc();

  typedef std::map<std::string, const BaseClass*, std::less<>> ComponentMap;

  void enterAny(const any* object, vpiHandle handle) final;

  void leaveDesign(const design* object, vpiHandle handle) final;

  void enterModule_inst(const module_inst* object, vpiHandle handle) final;
  void elabModule_inst(const module_inst* object, vpiHandle handle);
  void leaveModule_inst(const module_inst* object, vpiHandle handle) final;

  void enterInterface_inst(const interface_inst* object,
                           vpiHandle handle) final;
  void leaveInterface_inst(const interface_inst* object,
                           vpiHandle handle) final;

  void enterPackage(const package* object, vpiHandle handle) final;
  void leavePackage(const package* object, vpiHandle handle) final;

  void enterClass_defn(const class_defn* object, vpiHandle handle) final;
  void elabClass_defn(const class_defn* object, vpiHandle handle);
  void leaveClass_defn(const class_defn* object, vpiHandle handle) final;

  void enterGen_scope(const gen_scope* object, vpiHandle handle) final;
  void leaveGen_scope(const gen_scope* object, vpiHandle handle) final;

  void leaveRef_obj(const ref_obj* object, vpiHandle handle) final;
  void leaveBit_select(const bit_select* object, vpiHandle handle) final;
  void leaveIndexed_part_select(const indexed_part_select* object,
                                vpiHandle handle) final;
  void leavePart_select(const part_select* object, vpiHandle handle) final;
  void leaveVar_select(const var_select* object, vpiHandle handle) final;

  void enterFunction(const function* object, vpiHandle handle) final;
  void leaveFunction(const function* object, vpiHandle handle) final;

  void enterTask(const task* object, vpiHandle handle) final;
  void leaveTask(const task* object, vpiHandle handle) final;

  void enterForeach_stmt(const foreach_stmt* object, vpiHandle handle) final;
  void leaveForeach_stmt(const foreach_stmt* object, vpiHandle handle) final;

  void enterFor_stmt(const for_stmt* object, vpiHandle handle) final;
  void leaveFor_stmt(const for_stmt* object, vpiHandle handle) final;

  void enterBegin(const begin* object, vpiHandle handle) final;
  void leaveBegin(const begin* object, vpiHandle handle) final;

  void enterNamed_begin(const named_begin* object, vpiHandle handle) final;
  void leaveNamed_begin(const named_begin* object, vpiHandle handle) final;

  void enterFork_stmt(const fork_stmt* object, vpiHandle handle) final;
  void leaveFork_stmt(const fork_stmt* object, vpiHandle handle) final;

  void enterNamed_fork(const named_fork* object, vpiHandle handle) final;
  void leaveNamed_fork(const named_fork* object, vpiHandle handle) final;

  void enterMethod_func_call(const method_func_call* object,
                             vpiHandle handle) final;
  void leaveMethod_func_call(const method_func_call* object,
                             vpiHandle handle) final;

  void pushVar(any* var);
  void popVar(any* var);

 private:
  explicit ElaboratorListener(Serializer* serializer, bool debug = false,
                              bool muteErrors = false)
      : serializer_(serializer), debug_(debug), muteErrors_(muteErrors) {}

  void enterVariables(const variables* object, vpiHandle handle);

  void enterTask_func(const task_func* object, vpiHandle handle);
  void leaveTask_func(const task_func* object, vpiHandle handle);

  // Instance context stack
  typedef std::vector<std::tuple<const BaseClass*, ComponentMap, ComponentMap,
                                 ComponentMap, ComponentMap>>
      InstStack;
  InstStack instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;

  Serializer* serializer_ = nullptr;
  ElaboratorContext* context_ = nullptr;
  bool inHierarchy_ = false;
  bool debug_ = false;
  bool muteErrors_ = false;
  bool uniquifyTypespec_ = true;
  bool clone_ = true;
  bool ignoreLastInstance_ = false;
  std::vector<std::pair<tf_call*, const class_var*>> scheduledTfCallBinding_;

  friend class ElaboratorContext;
};

class ElaboratorContext final : public CloneContext {
  UHDM_IMPLEMENT_RTTI(ElaboratorContext, CloneContext)

 public:
  explicit ElaboratorContext(Serializer* serializer, bool debug = false,
                             bool muteErrors = false)
      : CloneContext(serializer), m_elaborator(serializer, debug, muteErrors) {
    m_elaborator.setContext(this);
  }
  ~ElaboratorContext() final = default;

  ElaboratorListener m_elaborator;
};

};  // namespace UHDM

#endif  // UHDM_ELABORATORLISTENER_H
