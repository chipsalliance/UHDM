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
    VisitedContainer visitedObjects;

    vpiHandle dh =
        object->GetSerializer()->MakeUhdmHandle(object->UhdmType(), object);

    visit_object(dh, out, true);
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
    case UHDM_OBJECT_TYPE::uhdmfinal_stmt:
    case UHDM_OBJECT_TYPE::uhdmdelay_control:
    case UHDM_OBJECT_TYPE::uhdmdelay_term:
    case UHDM_OBJECT_TYPE::uhdmthread_obj:
    case UHDM_OBJECT_TYPE::uhdmwait_stmt:
    case UHDM_OBJECT_TYPE::uhdmwait_fork:
    case UHDM_OBJECT_TYPE::uhdmordered_wait:
    case UHDM_OBJECT_TYPE::uhdmdisable:
    case UHDM_OBJECT_TYPE::uhdmdisable_fork:
    case UHDM_OBJECT_TYPE::uhdmforce:
    case UHDM_OBJECT_TYPE::uhdmdeassign:
    case UHDM_OBJECT_TYPE::uhdmrelease:
    case UHDM_OBJECT_TYPE::uhdmsequence_inst:
    case UHDM_OBJECT_TYPE::uhdmseq_formal_decl:
    case UHDM_OBJECT_TYPE::uhdmsequence_decl:
    case UHDM_OBJECT_TYPE::uhdmprop_formal_decl:
    case UHDM_OBJECT_TYPE::uhdmproperty_inst:
    case UHDM_OBJECT_TYPE::uhdmproperty_spec:
    case UHDM_OBJECT_TYPE::uhdmproperty_decl:
    case UHDM_OBJECT_TYPE::uhdmclocked_property:
    case UHDM_OBJECT_TYPE::uhdmcase_property_item:
    case UHDM_OBJECT_TYPE::uhdmcase_property:
    case UHDM_OBJECT_TYPE::uhdmmulticlock_sequence_expr:
    case UHDM_OBJECT_TYPE::uhdmclocked_seq:
    case UHDM_OBJECT_TYPE::uhdmreal_var:
    case UHDM_OBJECT_TYPE::uhdmtime_var:
    case UHDM_OBJECT_TYPE::uhdmchandle_var:
    case UHDM_OBJECT_TYPE::uhdmchecker_port:
    case UHDM_OBJECT_TYPE::uhdmchecker_inst_port:
    case UHDM_OBJECT_TYPE::uhdmswitch_tran:
    case UHDM_OBJECT_TYPE::uhdmudp:
    case UHDM_OBJECT_TYPE::uhdmmod_path:
    case UHDM_OBJECT_TYPE::uhdmtchk:
    case UHDM_OBJECT_TYPE::uhdmudp_defn:
    case UHDM_OBJECT_TYPE::uhdmtable_entry:
    case UHDM_OBJECT_TYPE::uhdmclocking_block:
    case UHDM_OBJECT_TYPE::uhdmclocking_io_decl:
    case UHDM_OBJECT_TYPE::uhdmprogram_array:
    case UHDM_OBJECT_TYPE::uhdmswitch_array:
    case UHDM_OBJECT_TYPE::uhdmudp_array:
    case UHDM_OBJECT_TYPE::uhdmtchk_term:
    case UHDM_OBJECT_TYPE::uhdmtime_net:
    case UHDM_OBJECT_TYPE::uhdmnamed_event:
    case UHDM_OBJECT_TYPE::uhdmvirtual_interface_var:
    case UHDM_OBJECT_TYPE::uhdmextends:
    case UHDM_OBJECT_TYPE::uhdmclass_defn:
    case UHDM_OBJECT_TYPE::uhdmclass_obj:
    case UHDM_OBJECT_TYPE::uhdmprogram:
    case UHDM_OBJECT_TYPE::uhdmchecker_decl:
    case UHDM_OBJECT_TYPE::uhdmchecker_inst:
    case UHDM_OBJECT_TYPE::uhdmshort_real_typespec:
    case UHDM_OBJECT_TYPE::uhdmreal_typespec:
    case UHDM_OBJECT_TYPE::uhdmtime_typespec:
    case UHDM_OBJECT_TYPE::uhdmchandle_typespec:
    case UHDM_OBJECT_TYPE::uhdmsequence_typespec:
    case UHDM_OBJECT_TYPE::uhdmproperty_typespec:
    case UHDM_OBJECT_TYPE::uhdmuser_systf:
    case UHDM_OBJECT_TYPE::uhdmmethod_func_call:
    case UHDM_OBJECT_TYPE::uhdmmethod_task_call:
    case UHDM_OBJECT_TYPE::uhdmconstraint_ordering:
    case UHDM_OBJECT_TYPE::uhdmconstraint:
    case UHDM_OBJECT_TYPE::uhdmdistribution:
    case UHDM_OBJECT_TYPE::uhdmdist_item:
    case UHDM_OBJECT_TYPE::uhdmimplication:
    case UHDM_OBJECT_TYPE::uhdmconstr_if:
    case UHDM_OBJECT_TYPE::uhdmconstr_if_else:
    case UHDM_OBJECT_TYPE::uhdmconstr_foreach:
    case UHDM_OBJECT_TYPE::uhdmsoft_disable:
    case UHDM_OBJECT_TYPE::uhdmfork_stmt:
    case UHDM_OBJECT_TYPE::uhdmnamed_fork:
    case UHDM_OBJECT_TYPE::uhdmevent_stmt:
    case UHDM_OBJECT_TYPE::uhdmevent_typespec:
      reportError(object);
      break;
    case UHDM_OBJECT_TYPE::uhdmexpect_stmt:
    case UHDM_OBJECT_TYPE::uhdmcover:
    case UHDM_OBJECT_TYPE::uhdmassume:
    case UHDM_OBJECT_TYPE::uhdmrestrict:
    case UHDM_OBJECT_TYPE::uhdmimmediate_assume:
    case UHDM_OBJECT_TYPE::uhdmimmediate_cover:
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
        if (type == UHDM_OBJECT_TYPE::uhdmbegin) {
          begin* b = (begin*)stmt;
          stmts = b->Stmts();
        } else if (type == UHDM_OBJECT_TYPE::uhdmnamed_begin) {
          named_begin* b = (named_begin*)stmt;
          stmts = b->Stmts();
        }
        if (stmts) {
          for (auto st : *stmts) {
            UHDM_OBJECT_TYPE sttype = st->UhdmType();
            switch (sttype) {
              case UHDM_OBJECT_TYPE::uhdmwait_stmt:
              case UHDM_OBJECT_TYPE::uhdmwait_fork:
              case UHDM_OBJECT_TYPE::uhdmordered_wait:
              case UHDM_OBJECT_TYPE::uhdmdisable:
              case UHDM_OBJECT_TYPE::uhdmdisable_fork:
              case UHDM_OBJECT_TYPE::uhdmforce:
              case UHDM_OBJECT_TYPE::uhdmdeassign:
              case UHDM_OBJECT_TYPE::uhdmrelease:
              case UHDM_OBJECT_TYPE::uhdmsoft_disable:
              case UHDM_OBJECT_TYPE::uhdmfork_stmt:
              case UHDM_OBJECT_TYPE::uhdmnamed_fork:
              case UHDM_OBJECT_TYPE::uhdmevent_stmt: {
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

void removeFromVector(VectorOfany* vec, const any* object) {
  VectorOfany::iterator itr = vec->begin();
  for (auto s : *vec) {
    if (s == object) {
      vec->erase(itr);
      break;
    }
    itr++;
  }
}

void SynthSubset::filterNonSynthesizable() {
  for (auto p : m_scheduledFilteredObjects) {
    removeFromVector(p.first, p.second);
  }
}


void SynthSubset::leaveSys_func_call(const sys_func_call* object,
                                     vpiHandle handle) {
  const std::string_view name = object->VpiName();
  if (nonSynthSysCalls_.find(name) != nonSynthSysCalls_.end()) {
    reportError(object);
  }
  // Filter out sys func calls stmt from initial block
  if (name == "$error" || name == "$finish" || name == "$display") {
    bool inInitialBlock = false;
    const any* parent = object->VpiParent();
    while (parent) {
      if (parent->UhdmType() == uhdminitial) {
        inInitialBlock = true;
        break;
      }
      parent = parent->VpiParent();
    }
    if (inInitialBlock) {
      parent = object->VpiParent();
      if (parent->UhdmType() == uhdmbegin) {
        begin* st = (begin*) parent;
        if (st->Stmts()) {
          m_scheduledFilteredObjects.emplace_back(st->Stmts(), object);
        }
      } else if (parent->UhdmType() == uhdmnamed_begin) {
        named_begin* st = (named_begin*) parent;
        if (st->Stmts()) {
          m_scheduledFilteredObjects.emplace_back(st->Stmts(), object);
        }
      }
    }
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
  if (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmpackage) {
    if (object->VpiName() == "builtin") return true;
  } else if (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_defn) {
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
