// -*- c++ -*-

/*

 Copyright 2019-2022 Alain Dargelas

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
 * File:   UhdmLint.h
 * Author: alaindargelas
 *
 * Created on Jan 3, 2022, 9:03 PM
 */

#ifndef UHDM_LINT_H
#define UHDM_LINT_H

#include <uhdm/VpiListener.h>

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace UHDM {
class Serializer;
class UhdmLint final : public VpiListener {
 public:
  UhdmLint(Serializer* serializer, design* des) : serializer_(serializer), design_(des) {}

 private:
  void leaveBit_select(const bit_select* object, vpiHandle handle) override;

  void leaveFunction(const function* object, vpiHandle handle) override;
  void leaveTask(const task* object, vpiHandle handle) override;
  void checkTfArgTypes(const task_func* tf);

  void leaveStruct_typespec(const struct_typespec* object,
                            vpiHandle handle) override;

  void leaveUnion_typespec(const union_typespec* object,
                           vpiHandle handle) override;

  void checkAggregateTypespec(const typespec* object, bool packed);

  void leaveModule_inst(const module_inst* object, vpiHandle handle) override;
  void leaveClass_defn(const class_defn* object, vpiHandle handle) override;

  void leaveAssignment(const assignment* object, vpiHandle handle) override;

  void leaveArray_var(const array_var* object, vpiHandle handle) override;
  void leaveAssign_stmt(const assign_stmt* object, vpiHandle handle) override;
  void leaveDeassign(const deassign* object, vpiHandle handle) override;
  void leaveForce(const force* object, vpiHandle handle) override;
  void leaveRelease(const release* object, vpiHandle handle) override;
  void checkProcContTarget(const any* lhs, const any* where);
  void checkQueueBound(const VectorOfrange* ranges);
  void leaveArray_net(const array_net* object, vpiHandle handle) override;
  void leaveForeach_stmt(const foreach_stmt* object, vpiHandle handle) override;
  void leaveBit_var(const bit_var* object, vpiHandle handle) override;
  void leaveLogic_var(const logic_var* object, vpiHandle handle) override;
  void checkPatternToScalar(const any* var, const any* rhs);
  void noteContDrivenVariable(const cont_assign* object);
  void noteProcDrivenVariable(const assignment* object);
  void reportContProcDrivenVariables();
  void leaveCont_assign(const cont_assign* object, vpiHandle handle) override;

  void checkClassAssign(const any* lhs, const any* rhs, const any* where);
  void checkArrayPatternCount(const any* arr, const any* rhs, const any* where);
  void checkUnpackedArrayAssign(const any* lhs, const any* rhs,
                                const any* where);

  void leaveLogic_net(const logic_net* object, vpiHandle handle) override;

  void leaveEnum_typespec(const enum_typespec* object,
                          vpiHandle handle) override;

  void leaveProperty_spec(const property_spec* object,
                          vpiHandle handle) override;

  void leaveSys_func_call(const sys_func_call* object, vpiHandle handle) override;

  void leavePort(const port* object, vpiHandle handle) override;
  void leaveSwitch_tran(const switch_tran* object, vpiHandle handle) override;

  void leavePart_select(const part_select* object, vpiHandle handle) override;

  void leaveRange(const range* object, vpiHandle handle) override;

  void leaveIndexed_part_select(const indexed_part_select* object,
                                vpiHandle handle) override;

  void leaveOperation(const operation* object, vpiHandle handle) override;

  void checkNonPositive(const any* ex, const any* where, int64_t offset,
                        bool allowZero);

  void leaveRef_obj(const ref_obj* object, vpiHandle handle) override;

  void leaveHier_path(const hier_path* object, vpiHandle handle) override;

  // LRM 26.3: `P::name` must name a member of package P.
  void checkPackageScopedRef(const ref_obj* ref);
  // LRM 26.3: a name a module imports is not reachable as `inst.name`.
  void checkImportedHierRef(const hier_path* path);
  bool reportOnce(const any* where, const char* tag);

  void leaveVar_select(const var_select* object, vpiHandle handle) override;

  void leaveFunc_call(const func_call* object, vpiHandle handle) override;

  void leaveTask_call(const task_call* object, vpiHandle handle) override;

  void leaveDesign(const design* object, vpiHandle handle) override;

  void leaveReturn_stmt(const return_stmt* object, vpiHandle handle) override;

  // LRM 6.19.3: a value assigned to an enum must be of that enum type.
  void checkEnumAssign(const any* lhs, const any* rhs, const any* where,
                       bool compound);
  void checkEnumValue(const enum_typespec* target, const any* rhs,
                      const any* where);
  void checkEnumCallArgs(const tf_call* call, const task_func* tf);

  void leaveAny(const any* object, vpiHandle handle) override;

  void noteImplicitNetRef(const ref_obj* ref);

  void reportUndefinedTaskFunc(const tf_call* call, bool bound);

  void leaveParam_assign(const param_assign* object,
                         vpiHandle handle) override;

  void checkConstantExpr(const any* expr, bool netsOnly);
  void checkContAssignLhsSelects(const any* lhs);
  bool checkUnknownBits(const any* ex, const any* where);
  void checkTfArgCount(const tf_call* call, const task_func* tf);
  void checkOperandTypes(const operation* op);
  void checkEventArgs(const tf_call* call, const task_func* tf);
  void checkPatternCount(const operation* pattern);
  void checkScopeNameClashes(const module_inst* mi);
  const module_inst* elabInstanceOf(const module_inst* mi);
  const any* instHierVar(const hier_path* hp, const any* where);
  void checkEnumDuplicateValues(const enum_typespec* object,
                                uint64_t baseSize);

  void checkMultiContAssign(const std::vector<UHDM::cont_assign*>* assigns);

  Serializer* serializer_ = nullptr;
  design* design_ = nullptr;
  // Nets Surelog fabricated for an undeclared name, keyed by full name: the
  // first reference, and whether ANY reference is in a context where LRM
  // 6.10 infers an implicit net.
  struct ImplicitNetUse {
    const any* firstRef = nullptr;
    std::string ownerDef;
    bool legal = false;
  };
  std::map<std::string, ImplicitNetUse> implicitNets_;
  std::vector<std::string> implicitNetOrder_;
  std::set<std::tuple<std::string, uint32_t, uint16_t>> reportedCalls_;
  std::set<std::tuple<std::string, uint32_t, uint16_t>> reportedOnce_;
  std::set<std::tuple<std::string, uint32_t, uint16_t>> reportedEnumAssigns_;
  // Every name the design DECLARES somewhere.  Surelog also fabricates a net
  // for a declared name it fails to bind (a block-local localparam, a $unit
  // parameter, a property formal); only a name declared nowhere is reported.
  std::set<std::string, std::less<>> declaredNames_;
  std::set<std::string, std::less<>> scopeNames_;
  std::map<const any*, const cont_assign*> contDrivenVars_;
  std::vector<const any*> contDrivenOrder_;
  std::map<const any*, const assignment*> procDrivenVars_;
  std::vector<std::pair<const tf_call*, std::string>> unboundCalls_;
  std::vector<std::pair<const ref_obj*, std::string>> unboundSelects_;
  std::vector<std::pair<const unsupported_typespec*, std::string>>
      unresolvedArgTypes_;
  // Definitions with at least one ELABORATED instance.  A module nothing
  // instantiates is not elaborated, so its names are not resolved.
  std::set<std::string, std::less<>> elaboratedDefs_;
  std::map<std::string, const module_inst*, std::less<>> elabByDef_;
  bool elabByDefBuilt_ = false;
};

}  // namespace UHDM

#endif
