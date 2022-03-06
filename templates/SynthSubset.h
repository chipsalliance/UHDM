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
 * File:   SynthSubset.h
 * Author: alaindargelas
 *
 * Created on Feb 16, 2022, 9:03 PM
 */

#ifndef SYNTH_SUBSET_H
#define SYNTH_SUBSET_H

#include <uhdm/VpiListener.h>
#include <uhdm/expr.h>
#include <uhdm/typespec.h>

#include <iostream>
#include <sstream>

namespace UHDM {
class Serializer;
class SynthSubset : public VpiListener {
 public:
  SynthSubset(Serializer* serializer,
              std::set<const any*>& nonSynthesizableObjects, bool reportErrors);
  ~SynthSubset();
  void report(std::ostream& out);

 private:
  void leaveSys_task_call(const sys_task_call* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveSys_func_call(const sys_func_call* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveVirtual_interface_var(const virtual_interface_var* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) override;

  void leaveFinal_stmt(const final_stmt* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

  void leaveDelay_control(const delay_control* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveDelay_term(const delay_term* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

  void leaveThread_obj(const thread_obj* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

  void leaveWait_stmt(const wait_stmt* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveWait_fork(const wait_fork* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveOrdered_wait(const ordered_wait* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void leaveDisable(const disable* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;

  void leaveDisable_fork(const disable_fork* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void leaveForce(const force* object, const BaseClass* parent,
                  vpiHandle handle, vpiHandle parentHandle) override;

  void leaveDeassign(const deassign* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) override;

  void leaveRelease(const release* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;

  void leaveExpect_stmt(const expect_stmt* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override;

  void leaveCover(const cover* object, const BaseClass* parent,
                  vpiHandle handle, vpiHandle parentHandle) override;

  void leaveAssume(const assume* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override;

  void leaveRestrict(const restrict* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) override;

  void leaveImmediate_assume(const immediate_assume* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) override;

  void leaveImmediate_cover(const immediate_cover* object,
                            const BaseClass* parent, vpiHandle handle,
                            vpiHandle parentHandle) override;

  void leaveSequence_inst(const sequence_inst* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveSeq_formal_decl(const seq_formal_decl* object,
                            const BaseClass* parent, vpiHandle handle,
                            vpiHandle parentHandle) override;

  void leaveSequence_decl(const sequence_decl* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveProp_formal_decl(const prop_formal_decl* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) override;

  void leaveProperty_inst(const property_inst* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveProperty_spec(const property_spec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveProperty_decl(const property_decl* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveClocked_property(const clocked_property* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) override;

  void leaveCase_property_item(const case_property_item* object,
                               const BaseClass* parent, vpiHandle handle,
                               vpiHandle parentHandle) override;

  void leaveCase_property(const case_property* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveMulticlock_sequence_expr(const multiclock_sequence_expr* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) override;

  void leaveClocked_seq(const clocked_seq* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override;

  void leaveReal_var(const real_var* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTime_var(const time_var* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) override;

  void leaveChandle_var(const chandle_var* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTask(const task* object, const BaseClass* parent, vpiHandle handle,
                 vpiHandle parentHandle) override;

  void leaveChecker_port(const checker_port* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void leaveChecker_inst_port(const checker_inst_port* object,
                              const BaseClass* parent, vpiHandle handle,
                              vpiHandle parentHandle) override;

  void leaveSwitch_tran(const switch_tran* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override;

  void leaveUdp(const udp* object, const BaseClass* parent, vpiHandle handle,
                vpiHandle parentHandle) override;

  void leaveMod_path(const mod_path* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTchk(const tchk* object, const BaseClass* parent, vpiHandle handle,
                 vpiHandle parentHandle) override;

  void leaveUdp_defn(const udp_defn* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTable_entry(const table_entry* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override;

  void leaveClocking_block(const clocking_block* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) override;

  void leaveClocking_io_decl(const clocking_io_decl* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) override;

  void leaveProgram_array(const program_array* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveSwitch_array(const switch_array* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void leaveUdp_array(const udp_array* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTchk_term(const tchk_term* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTime_net(const time_net* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) override;

  void leaveNamed_event(const named_event* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override;

  void leaveClass_typespec(const class_typespec* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) override;

  void leaveExtends(const extends* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;

  void leaveClass_defn(const class_defn* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

  void leaveClass_obj(const class_obj* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveProgram(const program* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;

  void leaveChecker_decl(const checker_decl* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void leaveChecker_inst(const checker_inst* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void leaveClass_var(const class_var* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveShort_real_typespec(const short_real_typespec* object,
                                const BaseClass* parent, vpiHandle handle,
                                vpiHandle parentHandle) override;

  void leaveReal_typespec(const real_typespec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTime_typespec(const time_typespec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;

  void leaveChandle_typespec(const chandle_typespec* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) override;

  void leaveSequence_typespec(const sequence_typespec* object,
                              const BaseClass* parent, vpiHandle handle,
                              vpiHandle parentHandle) override;

  void leaveProperty_typespec(const property_typespec* object,
                              const BaseClass* parent, vpiHandle handle,
                              vpiHandle parentHandle) override;

  void leaveUser_systf(const user_systf* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

  void leaveTask_call(const task_call* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;

  void leaveMethod_func_call(const method_func_call* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) override;

  void leaveMethod_task_call(const method_task_call* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) override;

  void leaveConstraint_ordering(const constraint_ordering* object,
                                const BaseClass* parent, vpiHandle handle,
                                vpiHandle parentHandle) override;

  void leaveConstraint(const constraint* object, const BaseClass* parent,
                       vpiHandle handle, vpiHandle parentHandle) override;

  void leaveDistribution(const distribution* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void leaveDist_item(const dist_item* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveImplication(const implication* object, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override;

  void leaveConstr_if(const constr_if* object, const BaseClass* parent,
                      vpiHandle handle, vpiHandle parentHandle) override;

  void leaveConstr_if_else(const constr_if_else* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) override;

  void leaveConstr_foreach(const constr_foreach* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) override;

  void leaveSoft_disable(const soft_disable* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) override;

  void reportError(const any* object);
  void mark(const any* object);
  bool reportedParent(const any* object);

  Serializer* serializer_ = nullptr;
  std::set<const any*>& nonSynthesizableObjects_;
  std::set<std::string> nonSynthSysCalls_;
  bool reportErrors_;
};

}  // namespace UHDM

#endif
