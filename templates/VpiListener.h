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
 * File:   VpiListener.h
 * Author: alaindargelas
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_VPILISTENER_H
#define UHDM_VPILISTENER_H

#include <uhdm/containers.h>
#include <uhdm/vpi_user.h>

namespace UHDM {
class VpiListener {
protected:
  typedef std::vector<const any *> any_stack_t;

  VisitedContainer visited;
  any_stack_t callstack;

public:
  // Use implicit constructor to initialize all members
  // VpiListener()

  virtual ~VpiListener() = default;

public:
  void listenAny(vpiHandle handle);
  void listenDesigns(const std::vector<vpiHandle>& designs);
<VPI_PUBLIC_LISTEN_DECLARATIONS>

  virtual void enterAny(const any* object, vpiHandle handle) {}
  virtual void leaveAny(const any* object, vpiHandle handle) {}

<VPI_ENTER_LEAVE_DECLARATIONS>
  bool isInUhdmAllIterator() const { return uhdmAllIterator; }
  bool inCallstackOfType(UHDM_OBJECT_TYPE type);
  virtual void ignoreLastInstance(bool ignore) {}
  design* currentDesign() { return currentDesign_; }
protected:
  bool uhdmAllIterator = false;
  design* currentDesign_ = nullptr;
private:
<VPI_PRIVATE_LISTEN_DECLARATIONS>
};
}  // namespace UHDM

#endif  // UHDM_VPILISTENER_H
