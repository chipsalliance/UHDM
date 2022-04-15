/*
 Do not modify, auto-generated by model_gen.tcl

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
 * File:   UhdmListener.cpp
 * Author: hs
 *
 * Created on March 11, 2022, 00:00 AM
 */
#include <uhdm/UhdmListener.h>
#include <uhdm/uhdm.h>

namespace UHDM {
ScopedVpiHandle::ScopedVpiHandle(const UHDM::any* const any)
    : handle(NewVpiHandle(any)) {}

ScopedVpiHandle::~ScopedVpiHandle() {
  if (handle != nullptr) {
    vpi_release_handle(handle);
  }
}

bool UhdmListener::didVisitAll(const Serializer& serializer) const {
  std::set<const any*> allVisited;
  std::copy(visited.begin(), visited.end(),
            std::inserter(allVisited, allVisited.begin()));

  std::set<const any*> allObjects;
  std::transform(
      serializer.AllObjects().begin(), serializer.AllObjects().end(),
      std::inserter(allObjects, allObjects.begin()),
      [](std::unordered_map<const BaseClass*, unsigned long>::const_reference
             entry) { return entry.first; });

  std::set<const any*> diffObjects;
  std::set_difference(allObjects.begin(), allObjects.end(), allVisited.begin(),
                      allVisited.end(),
                      std::inserter(diffObjects, diffObjects.begin()));

  return diffObjects.empty();
}

<UHDM_PRIVATE_LISTEN_IMPLEMENTATIONS>
<UHDM_PUBLIC_LISTEN_IMPLEMENTATIONS>
void UhdmListener::listenAny(const any* const object) {
  const bool revisiting = visited.find(object) != visited.end();
  if (!revisiting) enterAny(object);

  switch (object->UhdmType()) {
<UHDM_LISTENANY_IMPLEMENTATION>
  default: break;
  }

  if (!revisiting) leaveAny(object);
}

} // namespace UHDM
