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

  void leaveStruct_typespec(const struct_typespec* object,
                            vpiHandle handle) override;

  void leaveModule_inst(const module_inst* object, vpiHandle handle) override;

  void leaveAssignment(const assignment* object, vpiHandle handle) override;

  void leaveCont_assign(const cont_assign* object, vpiHandle handle) override;

  void checkUnpackedArrayAssign(const any* lhs, const any* rhs,
                                const any* where);

  void leaveLogic_net(const logic_net* object, vpiHandle handle) override;

  void leaveEnum_typespec(const enum_typespec* object,
                          vpiHandle handle) override;

  void leaveProperty_spec(const property_spec* object,
                          vpiHandle handle) override;

  void leaveSys_func_call(const sys_func_call* object, vpiHandle handle) override;

  void leavePort(const port* object, vpiHandle handle) override;

  void leavePart_select(const part_select* object, vpiHandle handle) override;

  void leaveRange(const range* object, vpiHandle handle) override;

  void leaveIndexed_part_select(const indexed_part_select* object,
                                vpiHandle handle) override;

  void leaveOperation(const operation* object, vpiHandle handle) override;

  void checkNonPositive(const any* ex, const any* where, int64_t offset,
                        bool allowZero);

  void leaveRef_obj(const ref_obj* object, vpiHandle handle) override;

  void leaveVar_select(const var_select* object, vpiHandle handle) override;

  void leaveFunc_call(const func_call* object, vpiHandle handle) override;

  void leaveTask_call(const task_call* object, vpiHandle handle) override;

  void leaveDesign(const design* object, vpiHandle handle) override;

  void leaveAny(const any* object, vpiHandle handle) override;

  void noteImplicitNetRef(const ref_obj* ref);

  void reportUndefinedTaskFunc(const tf_call* call, bool bound);

  void leaveParam_assign(const param_assign* object,
                         vpiHandle handle) override;

  void checkConstantExpr(const any* expr, bool netsOnly);

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
  // Every name the design DECLARES somewhere.  Surelog also fabricates a net
  // for a declared name it fails to bind (a block-local localparam, a $unit
  // parameter, a property formal); only a name declared nowhere is reported.
  std::set<std::string, std::less<>> declaredNames_;
  std::vector<std::pair<const tf_call*, std::string>> unboundCalls_;
  // Definitions with at least one ELABORATED instance.  A module nothing
  // instantiates is not elaborated, so its names are not resolved.
  std::set<std::string, std::less<>> elaboratedDefs_;
};

}  // namespace UHDM

#endif
