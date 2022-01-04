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
 * File:   UhdmLint.cpp
 * Author: alaindargelas
 *
 * Created on Jan 3, 2022, 9:03 PM
 */

#include <uhdm/UhdmLint.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

// #include <uhdm/Serializer.h>

namespace UHDM {

void UhdmLint::leaveBit_select(const bit_select* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) {
  const expr* index = object->VpiIndex();
  if (index) {
    if (index->UhdmType() == uhdmref_obj) {
      ref_obj* ref = (ref_obj*) index;
      const any* act = ref->Actual_group();
      if (act && act->UhdmType() == uhdmreal_var) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_NO_REAL_TYPE_AS_SELECT, act->VpiName(),
                              ref); 
      }
    }
  }

}

} // namespace UHDM
