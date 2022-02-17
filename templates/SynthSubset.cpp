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
       {"display", "write", "strobe", "monitor", "monitoron", "monitoroff",
        "displayb", "writeb", "strobeb", "monitorb", "displayo", "writeo",
        "strobeo", "monitoro", "displayh", "writeh", "strobeh", "monitorh",
        "fopen", "fclose", "frewind", "fflush", "fseek", "ftell", "fdisplay",
        "fwrite", "swrite", "fstrobe", "fmonitor", "fread", "fscanf",
        "fdisplayb", "fwriteb", "swriteb", "fstrobeb", "fmonitorb", "fdisplayo",
        "fwriteo", "swriteo", "fstrobeo", "fmonitoro", "fdisplayh", "fwriteh",
        "swriteh", "fstrobeh", "fmonitorh", "sscanf", "sdf_annotate", "sformat",
        // "cast",
        "assertkill", "assertoff", "asserton",
        // "bits",
        // "bitstoshortreal",
        "countones", "coverage_control", "coverage_merge", "coverage_save",
        // "dimensions",
        // "error",
        "exit",
        // "fatal",
        "fell", "get_coverage", "coverage_get", "coverage_get_max", "high",
        "increment", "info", "isunbounded", "isunknown", "left",
        "load_coverage_db", "low", "onehot", "past",
        // "readmemb",
        // "readmemh",
        "right", "root", "rose", "sampled", "set_coverage_db_name",
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
    nonSynthesizableObjects_.insert(object);
    if (reportErrors_)
      serializer_->GetErrorHandler()(ErrorType::UHDM_NON_SYNTHESIZABLE,
                                     object->VpiName(), object, nullptr);
  }
}

void SynthSubset::leaveSys_func_call(const sys_func_call* object,
                                     const BaseClass* parent, vpiHandle handle,
                                     vpiHandle parentHandle) {
  const std::string& name = object->VpiName();
  if (nonSynthSysCalls_.find(name) != nonSynthSysCalls_.end()) {
    nonSynthesizableObjects_.insert(object);
    if (reportErrors_)
      serializer_->GetErrorHandler()(ErrorType::UHDM_NON_SYNTHESIZABLE,
                                     object->VpiName(), object, nullptr);
  }
}

}  // namespace UHDM
