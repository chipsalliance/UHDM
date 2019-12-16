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
 * File:   vpi_user.cpp
 * Author: 
 *
 * Created on December 14, 2019, 10:03 PM
 */
#include <string>
#include <vector>
#include <iostream>
#include "include/vpi_user.h"
#include "include/vpi_uhdm.h"
#include "headers/containers.h"

<HEADERS>

using namespace UHDM;

vpiHandle vpi_iterate (PLI_INT32 type, vpiHandle refHandle) {
  uhdm_handle* handle = (uhdm_handle*) refHandle;
  base_class*  object = (base_class*) handle->m_object;
  <VPI_ITERATE_BODY>
}

vpiHandle vpi_scan (vpiHandle iterator) {
  uhdm_handle* handle = (uhdm_handle*) iterator;
  void* vect = handle->m_object;
  <VPI_SCAN_BODY>
  return 0;
}
