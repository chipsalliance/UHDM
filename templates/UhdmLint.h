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

  void leaveLogic_net(const logic_net* object, vpiHandle handle) override;

  void leaveEnum_typespec(const enum_typespec* object,
                          vpiHandle handle) override;

  void leaveProperty_spec(const property_spec* object,
                          vpiHandle handle) override;

  void checkMultiContAssign(const std::vector<UHDM::cont_assign*>* assigns);

  Serializer* serializer_ = nullptr;
  design* design_ = nullptr;
};

}  // namespace UHDM

#endif
