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

#include <iostream>
#include <sstream>

namespace UHDM {
class Serializer;
class SynthSubset final : public VpiListener {
 public:
  SynthSubset(Serializer* serializer,
              std::set<const any*>& nonSynthesizableObjects, bool reportErrors, bool allowFormal);
  ~SynthSubset() override = default;
  void report(std::ostream& out);

 private:
  void leaveAny(const any* object, vpiHandle handle) override;
  void leaveSys_task_call(const sys_task_call* object,
                          vpiHandle handle) override;

  void leaveSys_func_call(const sys_func_call* object,
                          vpiHandle handle) override;

  void leaveTask(const task* object, vpiHandle handle) override;

  void leaveClass_typespec(const class_typespec* object,
                           vpiHandle handle) override;

  void leaveClass_var(const class_var* object, vpiHandle handle) override;

  void reportError(const any* object);
  void mark(const any* object);
  bool reportedParent(const any* object);

  Serializer* serializer_ = nullptr;
  std::set<const any*>& nonSynthesizableObjects_;
  std::set<std::string, std::less<>> nonSynthSysCalls_;
  bool reportErrors_;
  bool allowFormal_;
};

}  // namespace UHDM

#endif
