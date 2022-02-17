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

#include <uhdm/typespec.h>
#include <uhdm/expr.h>
#include <iostream>
#include <sstream>

#include <uhdm/VpiListener.h>

namespace UHDM {
class Serializer;
class SynthSubset : public VpiListener {
 public:
  SynthSubset(Serializer* serializer,
              std::set<const any*>& nonSynthesizableObjects, bool reportErrors);
  ~SynthSubset();
  void report(std::ostream &out);
 private:
  void leaveSys_task_call(const sys_task_call* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;
  void leaveSys_func_call(const sys_func_call* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) override;
  Serializer* serializer_ = nullptr;
  std::set<const any*>& nonSynthesizableObjects_;
  std::set<std::string> nonSynthSysCalls_;
  bool reportErrors_;
};

}  // namespace UHDM

#endif
