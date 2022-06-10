/*
 (c) 2022 The UHDM authors.

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

#ifndef UHDM_TEST_UTIL_H
#define UHDM_TEST_UTIL_H

#include <sstream>
#include "uhdm/vpi_visitor.h"

inline std::string designs_to_string(const std::vector<vpiHandle>& designs) {
  std::stringstream out;
  UHDM::visit_designs(designs, out);
  return out.str();
}

#endif  // UHDM_TEST_UTIL_H
