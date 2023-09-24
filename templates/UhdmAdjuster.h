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
 * File:   UhdmAdjuster.h
 * Author: alaindargelas
 *
 * Created on Nov 29, 2022, 9:03 PM
 */

#ifndef UHDM_ADJUSTER_H
#define UHDM_ADJUSTER_H

#include <uhdm/VpiListener.h>

namespace UHDM {
class Serializer;
class UhdmAdjuster final : public VpiListener {
 public:
  UhdmAdjuster(Serializer* serializer, design* des) : serializer_(serializer), design_(des) {}

 private:
  
  void leaveCase_stmt(const case_stmt* object, vpiHandle handle) final;
  void leaveOperation(const operation* object, vpiHandle handle) final;
  void leaveSys_func_call(const sys_func_call* object, vpiHandle handle) final;
  void leaveConstant(const constant* object, vpiHandle handle) final;
  void enterModule_inst(const module_inst* object, vpiHandle handle) final;
  void leaveModule_inst(const module_inst* object, vpiHandle handle) final;
  void enterGen_scope(const gen_scope* object, vpiHandle handle) final;
  void leaveGen_scope(const gen_scope* object, vpiHandle handle) final;
  const any* resize(const any* object, int32_t maxsize, bool is_unsigned);
  void updateParentWithReducedExpression(const any* object, const any* parent);
  Serializer* serializer_ = nullptr;
  design* design_ = nullptr;
  const scope* currentInstance_ = nullptr;
};

}  // namespace UHDM

#endif
