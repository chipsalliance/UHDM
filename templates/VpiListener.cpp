/*
 Do not modify, auto-generated by script

 Copyright 2019-2020 Alain Dargelas

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
 * File:   VpiListener.cpp
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */
#include <uhdm/VpiListener.h>
#include <uhdm/uhdm.h>

namespace UHDM {
<VPI_PRIVATE_LISTEN_IMPLEMENTATIONS>
<VPI_PUBLIC_LISTEN_IMPLEMENTATIONS>

bool VpiListener::inCallstackOfType(UHDM_OBJECT_TYPE type) {
  for (any_stack_t::reverse_iterator itr = callstack.rbegin(); itr != callstack.rend(); ++itr) {
    if ((*itr)->UhdmType() == type) {
      return true;
    }
  }
  return false;
}

void VpiListener::listenAny(vpiHandle handle) {
  const any* object = (const any*)((const uhdm_handle*)handle)->object;
  const bool revisiting = visited.find(object) != visited.end();
  if (!revisiting) enterAny(object, handle);

  UHDM_OBJECT_TYPE type = ((const uhdm_handle*)handle)->type;
  switch (type) {
<VPI_LISTENANY_IMPLEMENTATION>
    default : break;
  }

  if (!revisiting) leaveAny(object, handle);
}

void VpiListener::listenDesigns(const std::vector<vpiHandle>& designs) {
  for (auto design_h : designs) {
    currentDesign_ = (design*) ((const uhdm_handle*)design_h)->object;
    listenAny(design_h);
  }
}
}  // namespace UHDM
