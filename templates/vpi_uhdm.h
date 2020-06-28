// -*- c++ -*-

/*

 Copyright 2019 Alain Dargelas

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
 * File:   vpi_uhdm.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef VPI_UHDM_H
#define VPI_UHDM_H

#include <unordered_map>
#include <vector>

#include "uhdm_types.h"

namespace UHDM {
  class Serializer;
};

struct uhdm_handle {
  uhdm_handle(UHDM::UHDM_OBJECT_TYPE type, const void* object) :
    type(type), object(object), index(0) {}
  const UHDM::UHDM_OBJECT_TYPE type;
  const void* object;
  unsigned int index;
};

class uhdm_handleFactory {
  friend UHDM::Serializer;
  public:
  vpiHandle Make(UHDM::UHDM_OBJECT_TYPE type, const void* object) {
    uhdm_handle* obj = new uhdm_handle(type, object);
    objects_.push_back(obj);
    return (vpiHandle) obj;
  }
  private:
    std::vector<uhdm_handle*> objects_;
};

/** Obtain a vpiHandle from a BaseClass (any) object */
vpiHandle NewVpiHandle (const UHDM::BaseClass* object);

s_vpi_value* String2VpiValue(const std::string& s);

s_vpi_delay* String2VpiDelays(const std::string& s);

std::string VpiValue2String(const s_vpi_value* value);

std::string VpiDelay2String(const s_vpi_delay* delay);

#endif
