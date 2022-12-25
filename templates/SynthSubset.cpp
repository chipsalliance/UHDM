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
                         bool reportErrors, bool allowFormal)
    : serializer_(serializer),
      nonSynthesizableObjects_(nonSynthesizableObjects),
      reportErrors_(reportErrors), allowFormal_(allowFormal) {
  constexpr std::string_view kDollar("$");
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
    nonSynthSysCalls_.emplace(std::move(std::string(kDollar).append(s)));
  }
}

void SynthSubset::report(std::ostream& out) {
  for (auto object : nonSynthesizableObjects_) {
    VisitedContainer visited;

    vpiHandle dh =
        object->GetSerializer()->MakeUhdmHandle(object->UhdmType(), object);

    visit_object(dh, 0, "", &visited, out, true);
    vpi_release_handle(dh);
  }
}

void SynthSubset::reportError(const any* object) {
  const any* tmp = object;
  while (tmp && tmp->VpiFile().empty()) {
    tmp = tmp->VpiParent();
  }
  if (tmp) object = tmp;
  if (reportErrors_ && !reportedParent(object)) {
    if (!object->VpiFile().empty()) {
      const std::string errMsg(object->VpiName());
      serializer_->GetErrorHandler()(ErrorType::UHDM_NON_SYNTHESIZABLE,
                                     errMsg, object, nullptr);
    }
  }
  mark(object);
}

void SynthSubset::leaveAny(const any* object, vpiHandle handle) {
  switch (object->UhdmType()) {
    case uhdmfinal_stmt:
    case uhdmdelay_control:
    case uhdmdelay_term:
    case uhdmthread_obj:
    case uhdmwait_stmt:
    case uhdmwait_fork:
    case uhdmordered_wait:
    case uhdmdisable:
    case uhdmdisable_fork:
    case uhdmforce:
    case uhdmdeassign:
    case uhdmrelease:
    case uhdmsequence_inst:
    case uhdmseq_formal_decl:
    case uhdmsequence_decl:
    case uhdmprop_formal_decl:
    case uhdmproperty_inst:
    case uhdmproperty_spec:
    case uhdmproperty_decl:
    case uhdmclocked_property:
    case uhdmcase_property_item:
    case uhdmcase_property:
    case uhdmmulticlock_sequence_expr:
    case uhdmclocked_seq:
    case uhdmreal_var:
    case uhdmtime_var:
    case uhdmchandle_var:
    case uhdmchecker_port:
    case uhdmchecker_inst_port:
    case uhdmswitch_tran:
    case uhdmudp:
    case uhdmmod_path:
    case uhdmtchk:
    case uhdmudp_defn:
    case uhdmtable_entry:
    case uhdmclocking_block:
    case uhdmclocking_io_decl:
    case uhdmprogram_array:
    case uhdmswitch_array:
    case uhdmudp_array:
    case uhdmtchk_term:
    case uhdmtime_net:
    case uhdmnamed_event:
    case uhdmvirtual_interface_var:
    case uhdmextends:
    case uhdmclass_defn:
    case uhdmclass_obj:
    case uhdmprogram:
    case uhdmchecker_decl:
    case uhdmchecker_inst:
    case uhdmshort_real_typespec:
    case uhdmreal_typespec:
    case uhdmtime_typespec:
    case uhdmchandle_typespec:
    case uhdmsequence_typespec:
    case uhdmproperty_typespec:
    case uhdmuser_systf:
    case uhdmmethod_func_call:
    case uhdmmethod_task_call:
    case uhdmconstraint_ordering:
    case uhdmconstraint:
    case uhdmdistribution:
    case uhdmdist_item:
    case uhdmimplication:
    case uhdmconstr_if:
    case uhdmconstr_if_else:
    case uhdmconstr_foreach:
    case uhdmsoft_disable:
    case uhdmfork_stmt:
    case uhdmnamed_fork:
    case uhdmevent_stmt:
    case uhdmevent_typespec:
      reportError(object);
      break;
    case uhdmexpect_stmt:
    case uhdmcover:
    case uhdmassume:
    case uhdmrestrict:
    case uhdmimmediate_assume:
    case uhdmimmediate_cover:
      if (!allowFormal_)
        reportError(object);
      break;  
    default:
      break;
  }
}

void SynthSubset::leaveTask(const task* topobject, vpiHandle handle) {
  // Give specific error for non-synthesizable tasks
  std::function<void(const any*, const any*)> inst_visit =
      [&inst_visit, this](const any* stmt, const any* top) {
        UHDM_OBJECT_TYPE type = stmt->UhdmType();
        UHDM::VectorOfany* stmts = nullptr;
        if (type == uhdmbegin) {
          begin* b = (begin*)stmt;
          stmts = b->Stmts();
        } else if (type == uhdmnamed_begin) {
          named_begin* b = (named_begin*)stmt;
          stmts = b->Stmts();
        }
        if (stmts) {
          for (auto st : *stmts) {
            UHDM_OBJECT_TYPE sttype = st->UhdmType();
            switch (sttype) {
              case uhdmwait_stmt:
              case uhdmwait_fork:
              case uhdmordered_wait:
              case uhdmdisable:
              case uhdmdisable_fork:
              case uhdmforce:
              case uhdmdeassign:
              case uhdmrelease:
              case uhdmsoft_disable:
              case uhdmfork_stmt:
              case uhdmnamed_fork:
              case uhdmevent_stmt: {
                reportError(top);
                break;
              }
              default: {
              }
            }
            inst_visit(st, top);
          }
        }
      };

  if (const any* stmt = topobject->Stmt()) {
    inst_visit(stmt, topobject);
  }
}

void SynthSubset::leaveSys_task_call(const sys_task_call* object,
                                     vpiHandle handle) {
  const std::string_view name = object->VpiName();
  if (nonSynthSysCalls_.find(name) != nonSynthSysCalls_.end()) {
    reportError(object);
  }
}

void SynthSubset::leaveSys_func_call(const sys_func_call* object,
                                     vpiHandle handle) {
  const std::string_view name = object->VpiName();
  if (nonSynthSysCalls_.find(name) != nonSynthSysCalls_.end()) {
    reportError(object);
  }
}

void SynthSubset::leaveClass_typespec(const class_typespec* object,
                                      vpiHandle handle) {
  if (const any* def = object->Class_defn())
    reportError(def);
  else
    reportError(object);
}

void SynthSubset::leaveClass_var(const class_var* object, vpiHandle handle) {
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
