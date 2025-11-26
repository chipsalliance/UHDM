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
 * File:   Elaborator.h
 * Author: hs
 *
 * Created on November 24, 2025, 10:03 PM
 */

#ifndef UHDM_ELABORATOR_H
#define UHDM_ELABORATOR_H

#include <uhdm/Cloner.h>
#include <uhdm/VpiListener.h>
#include <uhdm/containers.h>

#include <map>
#include <vector>

namespace uhdm {
class Serializer;

class Elaborator final : public VpiListener, public Cloner {
 public:
  explicit Elaborator(Serializer* serializer, bool debug = false,
                      bool muteErrors = false);

  void uniquifyTypespec(bool uniquify) { m_uniquifyTypespec = uniquify; }
  bool uniquifyTypespec() const { return m_uniquifyTypespec; }
  void bindOnly(bool bindOnly) { m_clone = !bindOnly; }
  bool bindOnly() const { return !m_clone; }
  bool isFunctionCall(std::string_view name, const Expr* prefix) const;
  bool muteErrors() const { return m_muteErrors; }
  bool isTaskCall(std::string_view name, const Expr* prefix) const;
  void ignoreLastInstance(bool ignore) { m_ignoreLastInstance = ignore; }
  using Cloner::clone;

 private:
  // Bind to a net in the current instance
  Any* bindNet(std::string_view name) const;

  // Bind to a net or parameter in the current instance
  Any* bindAny(std::string_view name) const;

  // Bind to a param in the current instance
  Any* bindParam(std::string_view name) const;

  // Bind to a function or task in the current scope
  TaskFunc* bindTaskFunc(std::string_view name,
                         const Variable* prefix = nullptr) const;

  void bindScheduledTaskFunc();

  void enterAny(const Any* object, vpiHandle handle) final;

  void leaveDesign(const Design* object, vpiHandle handle) final;

  void enterModule(const Module* object, vpiHandle handle) final;
  void leaveModule(const Module* object, vpiHandle handle) final;

  void enterInterface(const Interface* object, vpiHandle handle) final;
  void leaveInterface(const Interface* object, vpiHandle handle) final;

  void enterPackage(const Package* object, vpiHandle handle) final;
  void leavePackage(const Package* object, vpiHandle handle) final;

  void enterClassDefn(const ClassDefn* object, vpiHandle handle) final;
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

  void pushVar(Any* var);
  void popVar(Any* var);

  Any* bindClassTypespec(ClassTypespec* ctps, Any* current,
                         std::string_view name, bool& found);

  Any* cloneAny(const Any* source, Any* parent) final;

  Constant* clone(const Constant* source, Any* parent) final;
  ContAssign* clone(const ContAssign* source, Any* parent) final;
  Function* clone(const Function* source, Any* parent) final;
  GenScopeArray* clone(const GenScopeArray* source, Any* parent) final;
  HierPath* clone(const HierPath* source, Any* parent) final;
  SysFuncCall* clone(const SysFuncCall* source, Any* parent) final;
  SysTaskCall* clone(const SysTaskCall* source, Any* parent) final;
  TaggedPattern* clone(const TaggedPattern* source, Any* parent) final;
  Task* clone(const Task* source, Any* parent) final;
  TFCall* clone(const FuncCall* source, Any* parent) final;
  TFCall* clone(const MethodFuncCall* source, Any* parent) final;
  TFCall* clone(const MethodTaskCall* source, Any* parent) final;
  TFCall* clone(const TaskCall* source, Any* parent) final;

  Typespec* clone(const ArrayTypespec* source, Any* parent) final;
  Typespec* clone(const BitTypespec* source, Any* parent) final;
  Typespec* clone(const ByteTypespec* source, Any* parent) final;
  Typespec* clone(const ChandleTypespec* source, Any* parent) final;
  Typespec* clone(const ClassTypespec* source, Any* parent) final;
  Typespec* clone(const EnumTypespec* source, Any* parent) final;
  Typespec* clone(const EventTypespec* source, Any* parent) final;
  Typespec* clone(const ImportTypespec* source, Any* parent) final;
  Typespec* clone(const IntTypespec* source, Any* parent) final;
  Typespec* clone(const IntegerTypespec* source, Any* parent) final;
  Typespec* clone(const InterfaceTypespec* source, Any* parent) final;
  Typespec* clone(const LogicTypespec* source, Any* parent) final;
  Typespec* clone(const LongIntTypespec* source, Any* parent) final;
  Typespec* clone(const ModuleTypespec* source, Any* parent) final;
  Typespec* clone(const PropertyTypespec* source, Any* parent) final;
  Typespec* clone(const RealTypespec* source, Any* parent) final;
  Typespec* clone(const SequenceTypespec* source, Any* parent) final;
  Typespec* clone(const ShortIntTypespec* source, Any* parent) final;
  Typespec* clone(const ShortRealTypespec* source, Any* parent) final;
  Typespec* clone(const StringTypespec* source, Any* parent) final;
  Typespec* clone(const StructTypespec* source, Any* parent) final;
  Typespec* clone(const TimeTypespec* source, Any* parent) final;
  Typespec* clone(const TypeParameter* source, Any* parent) final;
  Typespec* clone(const UnionTypespec* source, Any* parent) final;
  Typespec* clone(const UnsupportedTypespec* source, Any* parent) final;
  Typespec* clone(const VoidTypespec* source, Any* parent) final;

  // clang-format off
  using Cloner::copy;
//<COPY_DECLARATIONS>
  // clang-format on

 private:
  void enterVariable(const Variable* object, vpiHandle handle);

  void enterTaskFunc(const TaskFunc* object, vpiHandle handle);
  void leaveTaskFunc(const TaskFunc* object, vpiHandle handle);

  using ComponentMap = std::map<std::string, const BaseClass*, std::less<>>;
  // Instance context stack
  using InstStack =
      std::vector<std::tuple<const BaseClass*, ComponentMap, ComponentMap,
                             ComponentMap, ComponentMap>>;
  using ScheduledTfCallBinding =
      std::vector<std::pair<TFCall*, const Variable*>>;

  bool m_debug = false;
  bool m_muteErrors = false;
  InstStack m_instStack;
  // Flat list of components (modules, udps, interfaces)
  ComponentMap m_flatComponentMap;
  ScheduledTfCallBinding m_scheduledTfCallBinding;
  bool m_inHierarchy = false;
  bool m_uniquifyTypespec = true;
  bool m_clone = true;
  bool m_ignoreLastInstance = false;
  bool m_isInUhdmAllIterator = false;
};
};  // namespace uhdm

#endif  // UHDM_ELABORATOR_H
