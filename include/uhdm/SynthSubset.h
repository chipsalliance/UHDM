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

#ifndef UHDM_SYNTHSUBSET_H
#define UHDM_SYNTHSUBSET_H

#include <uhdm/VpiListener.h>

#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

namespace uhdm {
class Serializer;
class SynthSubset final : public VpiListener {
 public:
  SynthSubset(Serializer* serializer,
              AnySet& nonSynthesizableObjects, Design* des,
              bool reportErrors, bool allowFormal);
  ~SynthSubset() override = default;
  void filterNonSynthesizable();
  void report(std::ostream& out);

 private:
  void leaveAny(const Any* object, vpiHandle handle) override;
  void leaveSysTaskCall(const SysTaskCall* object, vpiHandle handle) override;

  void leaveSysFuncCall(const SysFuncCall* object, vpiHandle handle) override;

  void leaveTask(const Task* object, vpiHandle handle) override;

  void leaveClassTypespec(const ClassTypespec* object,
                          vpiHandle handle) override;

  // Typespec substitution to allow Yosys to perform RAM  Inference
  void leaveVariable(const Variable* object, vpiHandle handle) override;

  // Apply some rewrite rule for Yosys limitations
  void leaveForStmt(const ForStmt* object, vpiHandle handle) override;

  // Apply some rewrite rule for Yosys limitations
  void leaveAlways(const Always* object, vpiHandle handle) override;

  // Apply some rewrite rule for Synlig limitations
  void leaveRefTypespec(const RefTypespec* object, vpiHandle handle) override;

  // Signed/Unsigned port transform to allow Yosys to Synthesize
  void leavePort(const Port* object, vpiHandle handle) override;

  // Remove Typespec information on allModules to allow Yosys to perform RAM
  // Inference
  void leaveNet(const Net* object, vpiHandle handle) override;

  void reportError(const Any* object);
  void mark(const Any* object);
  bool reportedParent(const Any* object);

  void sensitivityListRewrite(const Always* object, vpiHandle handle);
  void blockingToNonBlockingRewrite(const Always* object, vpiHandle handle);

  void removeFromVector(AnyCollection* vec, const Any* object);
  void removeFromStmt(Any* parent, const Any* object);
  SysFuncCall* makeStubDisplayStmt(const Any* object);

  Serializer* m_serializer = nullptr;
  AnySet& m_nonSynthesizableObjects;
  std::set<std::string, std::less<>> m_nonSynthSysCalls;
  Design* m_design = nullptr;
  bool m_reportErrors = false;
  bool m_allowFormal = false;
  std::vector<std::pair<AnyCollection*, const Any*>> m_scheduledFilteredObjectsInVector;
  std::vector<std::pair<Any*, const Any*>> m_scheduledFilteredObjectsInStmt;
};

}  // namespace uhdm

#endif  // UHDM_SYNTHSUBSET_H
