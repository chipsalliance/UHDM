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
 * File:   SynthSubset.cpp
 * Author: alaindargelas
 *
 * Created on Feb 16, 2022, 9:03 PM
 */
#include <string.h>
#include <uhdm/SynthSubset.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>

namespace UHDM {

SynthSubset::SynthSubset(Serializer* serializer,
                         std::set<const any*>& nonSynthesizableObjects,
                         bool reportErrors)
    : serializer_(serializer),
      nonSynthesizableObjects_(nonSynthesizableObjects),
      reportErrors_(reportErrors) {
  for (auto s :
       {// "display",
        "write", "strobe", "monitor", "monitoron", "monitoroff", "displayb",
        "writeb", "strobeb", "monitorb", "displayo", "writeo", "strobeo",
        "monitoro", "displayh", "writeh", "strobeh", "monitorh", "fopen",
        "fclose", "frewind", "fflush", "fseek", "ftell", "fdisplay", "fwrite",
        "swrite", "fstrobe", "fmonitor", "fread", "fscanf", "fdisplayb",
        "fwriteb", "swriteb", "fstrobeb", "fmonitorb", "fdisplayo", "fwriteo",
        "swriteo", "fstrobeo", "fmonitoro", "fdisplayh", "fwriteh", "swriteh",
        "fstrobeh", "fmonitorh", "sscanf", "sdf_annotate", "sformat",
        // "cast",
        "assertkill", "assertoff", "asserton",
        // "bits",
        // "bitstoshortreal",
        "countones", "coverage_control", "coverage_merge", "coverage_save",
        // "dimensions",
        // "error",
        "exit",
        // "fatal",
        "fell", "get_coverage", "coverage_get", "coverage_get_max",
        // "high",
        //"increment",
        "info", "isunbounded", "isunknown",
        // "left",
        "load_coverage_db",
        // "low",
        "onehot", "past",
        // "readmemb",
        // "readmemh",
        //"right",
        "root", "rose", "sampled", "set_coverage_db_name",
        // "shortrealtobits",
        // "size",
        "stable",
        // "typename",
        // "typeof",
        "unit", "urandom", "srandom", "urandom_range", "set_randstate",
        "get_randstate", "dist_uniform", "dist_normal", "dist_exponential",
        "dist_poisson", "dist_chi_square", "dist_t", "dist_erlang",
        // "warning",
        // "writememb",
        // "writememh",
        "value$plusargs"}) {
    nonSynthSysCalls_.insert(std::string("$") + s);
  }
}

SynthSubset::~SynthSubset() {}

void SynthSubset::report(std::ostream& out) {
  for (auto object : nonSynthesizableObjects_) {
    VisitedContainer visited;

    vpiHandle dh =
        object->GetSerializer()->MakeUhdmHandle(object->UhdmType(), object);

    visit_object(dh, 0, "", &visited, out, true);
    vpi_release_handle(dh);
  }
}

void SynthSubset::leaveSys_task_call(const sys_task_call* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  const std::string& name = object->VpiName();
  if (nonSynthSysCalls_.find(name) != nonSynthSysCalls_.end()) {
    reportError(object);
  }
}

void SynthSubset::leaveSys_func_call(const sys_func_call* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  const std::string& name = object->VpiName();
  if (nonSynthSysCalls_.find(name) != nonSynthSysCalls_.end()) {
    reportError(object);
  }
}

void SynthSubset::reportError(const any* object) {
  const any* tmp = object;
  while (tmp && tmp->VpiFile().empty()) {
    tmp = tmp->VpiParent();
  }
  if (tmp) object = tmp;
  if (reportErrors_ && !reportedParent(object)) {
    if (!object->VpiFile().empty())
      serializer_->GetErrorHandler()(ErrorType::UHDM_NON_SYNTHESIZABLE,
                                     object->VpiName(), object, nullptr);
  }
  mark(object);
}

void SynthSubset::leaveVirtual_interface_var(
    const virtual_interface_var* object, const BaseClass* parent,
    vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveFinal_stmt(const final_stmt* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveDelay_control(const delay_control* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveDelay_term(const delay_term* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveThread_obj(const thread_obj* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveWait_stmt(const wait_stmt* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveWait_fork(const wait_fork* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveOrdered_wait(const ordered_wait* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveDisable(const disable* object, const BaseClass* parent,
                               vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveDisable_fork(const disable_fork* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveForce(const force* object, const BaseClass* parent,
                             vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveDeassign(const deassign* object, const BaseClass* parent,
                                vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveRelease(const release* object, const BaseClass* parent,
                               vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveExpect_stmt(const expect_stmt* object,
                                   const BaseClass* parent, vpiHandle handle,
                                   vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveCover(const cover* object, const BaseClass* parent,
                             vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveAssume(const assume* object, const BaseClass* parent,
                              vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveRestrict(const restrict* object, const BaseClass* parent,
                                vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveImmediate_assume(const immediate_assume* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveImmediate_cover(const immediate_cover* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveSequence_inst(const sequence_inst* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveSeq_formal_decl(const seq_formal_decl* object,
                                       const BaseClass* parent,
                                       vpiHandle handle,
                                       vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveSequence_decl(const sequence_decl* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveProp_formal_decl(const prop_formal_decl* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveProperty_inst(const property_inst* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveProperty_spec(const property_spec* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveProperty_decl(const property_decl* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClocked_property(const clocked_property* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveCase_property_item(const case_property_item* object,
                                          const BaseClass* parent,
                                          vpiHandle handle,
                                          vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveCase_property(const case_property* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveMulticlock_sequence_expr(
    const multiclock_sequence_expr* object, const BaseClass* parent,
    vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClocked_seq(const clocked_seq* object,
                                   const BaseClass* parent, vpiHandle handle,
                                   vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveReal_var(const real_var* object, const BaseClass* parent,
                                vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTime_var(const time_var* object, const BaseClass* parent,
                                vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveChandle_var(const chandle_var* object,
                                   const BaseClass* parent, vpiHandle handle,
                                   vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTask(const task* object, const BaseClass* parent,
                            vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveChecker_port(const checker_port* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveChecker_inst_port(const checker_inst_port* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveSwitch_tran(const switch_tran* object,
                                   const BaseClass* parent, vpiHandle handle,
                                   vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveUdp(const udp* object, const BaseClass* parent,
                           vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveMod_path(const mod_path* object, const BaseClass* parent,
                                vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTchk(const tchk* object, const BaseClass* parent,
                            vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveUdp_defn(const udp_defn* object, const BaseClass* parent,
                                vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTable_entry(const table_entry* object,
                                   const BaseClass* parent, vpiHandle handle,
                                   vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClocking_block(const clocking_block* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClocking_io_decl(const clocking_io_decl* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveProgram_array(const program_array* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveSwitch_array(const switch_array* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveUdp_array(const udp_array* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTchk_term(const tchk_term* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTime_net(const time_net* object, const BaseClass* parent,
                                vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveNamed_event(const named_event* object,
                                   const BaseClass* parent, vpiHandle handle,
                                   vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClass_typespec(const class_typespec* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  if (const any* def = object->Class_defn())
    reportError(def);
  else
    reportError(object);
}

void SynthSubset::leaveExtends(const extends* object, const BaseClass* parent,
                               vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClass_defn(const class_defn* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClass_obj(const class_obj* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveProgram(const program* object, const BaseClass* parent,
                               vpiHandle handle, vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveChecker_decl(const checker_decl* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveChecker_inst(const checker_inst* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveClass_var(const class_var* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  if (const class_typespec* spec = (class_typespec*)object->Typespec()) {
    if (const class_defn* def = spec->Class_defn()) {
      if (reportedParent(def)) {
        mark(object);
        return;
      }
    }
  }
  reportError(object);
}

void SynthSubset::leaveShort_real_typespec(const short_real_typespec* object,
                                           const BaseClass* parent,
                                           vpiHandle handle,
                                           vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveReal_typespec(const real_typespec* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTime_typespec(const time_typespec* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveChandle_typespec(const chandle_typespec* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveSequence_typespec(const sequence_typespec* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveProperty_typespec(const property_typespec* object,
                                         const BaseClass* parent,
                                         vpiHandle handle,
                                         vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveUser_systf(const user_systf* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveTask_call(const task_call* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveMethod_func_call(const method_func_call* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveMethod_task_call(const method_task_call* object,
                                        const BaseClass* parent,
                                        vpiHandle handle,
                                        vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveConstraint_ordering(const constraint_ordering* object,
                                           const BaseClass* parent,
                                           vpiHandle handle,
                                           vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveConstraint(const constraint* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveDistribution(const distribution* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveDist_item(const dist_item* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveImplication(const implication* object,
                                   const BaseClass* parent, vpiHandle handle,
                                   vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveConstr_if(const constr_if* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveConstr_if_else(const constr_if_else* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveConstr_foreach(const constr_foreach* object,
                                      const BaseClass* parent, vpiHandle handle,
                                      vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::leaveSoft_disable(const soft_disable* object,
                                    const BaseClass* parent, vpiHandle handle,
                                    vpiHandle parentHandle) {
  reportError(object);
}

void SynthSubset::mark(const any* object) {
  nonSynthesizableObjects_.insert(object);
}

bool SynthSubset::reportedParent(const any* object) {
  if (object->UhdmType() == uhdmpackage) {
    if (object->VpiName() == "builtin") return true;
  } else if (object->UhdmType() == uhdmclass_defn) {
    if (object->VpiName() == "work@semaphore" ||
        object->VpiName() == "work@process" ||
        object->VpiName() == "work@mailbox")
      return true;
  }
  if (nonSynthesizableObjects_.find(object) != nonSynthesizableObjects_.end()) {
    return true;
  }
  if (const any* parent = object->VpiParent()) {
    return reportedParent(parent);
  }
  return false;
}

}  // namespace UHDM
