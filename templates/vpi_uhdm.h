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

#include <uhdm/uhdm_types.h>

namespace UHDM {
  class Serializer;
  class design;
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
    return (vpiHandle) new uhdm_handle(type, object);
  }

  bool Erase(vpiHandle handle) {
    delete (uhdm_handle*)handle;
    return true;
  }

  void Purge() {}
};

/** Obtain a vpiHandle from a BaseClass (any) object */
vpiHandle NewVpiHandle (const UHDM::BaseClass* object);

s_vpi_value* String2VpiValue(const std::string& s);

s_vpi_delay* String2VpiDelays(const std::string& s);

std::string VpiValue2String(const s_vpi_value* value);

std::string VpiDelay2String(const s_vpi_delay* delay);

/** Obtain a UHDM::design pointer from a vpiHandle */
UHDM::design* UhdmDesignFromVpiHandle(vpiHandle hdesign);

/** Shows unique IDs in vpi_visitor dump (uhdmdump) */
void vpi_show_ids(bool show);

#endif
