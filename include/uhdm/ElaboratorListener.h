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

namespace uhdm {

class ElaboratorContext;
class ElaboratorListener;
class Serializer;

class ElaboratorListener final : public VpiListener {
  friend class Function;
  friend class Task;
  friend class GenScopeArray;

 public:
  void setContext(ElaboratorContext* context) { m_context = context; }
  void uniquifyTypespec(bool uniquify) { m_uniquifyTypespec = uniquify; }
  bool uniquifyTypespec() { return m_uniquifyTypespec; }
  void bindOnly(bool bindOnly) { m_clone = !bindOnly; }
  bool bindOnly() { return !m_clone; }
  bool isFunctionCall(std::string_view name, const Expr* prefix) const;
  bool muteErrors() { return m_muteErrors; }
  bool isTaskCall(std::string_view name, const Expr* prefix) const;
  void ignoreLastInstance(bool ignore) { m_ignoreLastInstance = ignore; }

  // Bind to a net in the current instance
  Any* bindNet(std::string_view name) const;

  // Bind to a net or parameter in the current instance
  Any* bindAny(std::string_view name) const;

  // Bind to a param in the current instance
  Any* bindParam(std::string_view name) const;

  // Bind to a function or task in the current scope
  TaskFunc* bindTaskFunc(std::string_view name,
                         const Variable* prefix = nullptr) const;

  void scheduleTaskFuncBinding(TFCall* clone, const Variable* prefix) {
    m_scheduledTfCallBinding.emplace_back(clone, prefix);
  }
  void bindScheduledTaskFunc();

  using ComponentMap = std::map<std::string, const BaseClass*, std::less<>>;

  void leaveDesign(const Design* object, vpiHandle handle) final;

  void enterModule(const Module* object, vpiHandle handle) final;
  void elabModule(const Module* object, vpiHandle handle);
  void leaveModule(const Module* object, vpiHandle handle) final;

  void enterInterface(const Interface* object, vpiHandle handle) final;
  void leaveInterface(const Interface* object, vpiHandle handle) final;

  void enterPackage(const Package* object, vpiHandle handle) final;
  void leavePackage(const Package* object, vpiHandle handle) final;

  void enterClassDefn(const ClassDefn* object, vpiHandle handle) final;
  void elabClassDefn(const ClassDefn* object, vpiHandle handle);
  void leaveClassDefn(const ClassDefn* object, vpiHandle handle) final;

  void enterGenScope(const GenScope* object, vpiHandle handle) final;
  void leaveGenScope(const GenScope* object, vpiHandle handle) final;

  void leaveRefObj(const RefObj* object, vpiHandle handle) final;
  void leaveBitSelect(const BitSelect* object, vpiHandle handle) final;
  void leaveIndexedPartSelect(const IndexedPartSelect* object,
                              vpiHandle handle) final;
  void leavePartSelect(const PartSelect* object, vpiHandle handle) final;
  void leaveVarSelect(const VarSelect* object, vpiHandle handle) final;

  void enterFunction(const Function* object, vpiHandle handle) final;
  void leaveFunction(const Function* object, vpiHandle handle) final;

  void enterTask(const Task* object, vpiHandle handle) final;
  void leaveTask(const Task* object, vpiHandle handle) final;

  void enterForeachStmt(const ForeachStmt* object, vpiHandle handle) final;
  void leaveForeachStmt(const ForeachStmt* object, vpiHandle handle) final;

  void enterForStmt(const ForStmt* object, vpiHandle handle) final;
  void leaveForStmt(const ForStmt* object, vpiHandle handle) final;

  void enterBegin(const Begin* object, vpiHandle handle) final;
  void leaveBegin(const Begin* object, vpiHandle handle) final;

  void enterForkStmt(const ForkStmt* object, vpiHandle handle) final;
  void leaveForkStmt(const ForkStmt* object, vpiHandle handle) final;

  void enterMethodFuncCall(const MethodFuncCall* object,
                           vpiHandle handle) final;
  void leaveMethodFuncCall(const MethodFuncCall* object,
                           vpiHandle handle) final;

  void enterVariable(const Variable* object, vpiHandle handle) final;

  void pushVar(Any* var);
  void popVar(Any* var);

 private:
  explicit ElaboratorListener(Serializer* serializer, bool debug = false,
                              bool muteErrors = false)
      : m_serializer(serializer), m_debug(debug), m_muteErrors(muteErrors) {}

  void enterTaskFunc(const TaskFunc* object, vpiHandle handle);
  void leaveTaskFunc(const TaskFunc* object, vpiHandle handle);

  // Instance context stack
  using InstStack =
      std::vector<std::tuple<const BaseClass*, ComponentMap, ComponentMap,
                             ComponentMap, ComponentMap>>;
  InstStack m_instStack;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap m_flatComponentMap;

  Serializer* m_serializer = nullptr;
  ElaboratorContext* m_context = nullptr;
  bool m_inHierarchy = false;
  bool m_debug = false;
  bool m_muteErrors = false;
  bool m_uniquifyTypespec = true;
  bool m_clone = true;
  bool m_ignoreLastInstance = false;
  std::vector<std::pair<TFCall*, const Variable*>> m_scheduledTfCallBinding;

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

}  // namespace uhdm

#endif  // UHDM_ELABORATORLISTENER_H
