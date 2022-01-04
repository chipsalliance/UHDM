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

#include <uhdm/typespec.h>
#include <uhdm/expr.h>
#include <iostream>
#include <sstream>

#include <uhdm/VpiListener.h>

namespace UHDM {
  class Serializer;
  class UhdmLint : public VpiListener {
 
  public:
     UhdmLint (Serializer* serializer) : serializer_(serializer) {}

    void leaveBit_select(const bit_select* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override;
  private:
    Serializer* serializer_ = nullptr;
  };
  
  

}

#endif
