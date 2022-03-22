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
 * File:   UhdmListener.h
 * Author: hs
 *
 * Created on March 11, 2022, 00:00 AM
 */

#ifndef UHDM_UHDMLISTENER_H
#define UHDM_UHDMLISTENER_H

#include <uhdm/containers.h>
#include <uhdm/sv_vpi_user.h>

#include <algorithm>
#include <unordered_set>
#include <vector>


namespace UHDM {
class ScopedVpiHandle final {
 public:
  ScopedVpiHandle(const UHDM::any *const any);
  ~ScopedVpiHandle();

  operator vpiHandle() const { return handle; }

 private:
  const vpiHandle handle = nullptr;
};

class UhdmListener {
protected:
  typedef std::unordered_set<const any *> any_set_t;
  typedef std::vector<const any *> any_stack_t;

  any_set_t visited;
  any_stack_t callstack;

public:
  // Use implicit constructor to initialize all members
  // VpiListener()
  virtual ~UhdmListener() = default;

public:
  any_set_t &getVisited() { return visited; }
  const any_set_t &getVisited() const { return visited; }

  const any_stack_t &getCallstack() const { return callstack; }

  bool isOnCallstack(const any *const what) const {
    return std::find(callstack.crbegin(), callstack.crend(), what) !=
           callstack.rend();
  }

  bool isOnCallstack(const std::unordered_set<UHDM_OBJECT_TYPE> &types) const {
    return std::find_if(callstack.crbegin(), callstack.crend(),
                        [&types](const any *const which) {
                          return types.find(which->UhdmType()) != types.end();
                        }) != callstack.rend();
  }

  bool didVisitAll(const Serializer &serializer) const;

  void listenAny(const any *const object);
<UHDM_PUBLIC_LISTEN_DECLARATIONS>
  
<UHDM_ENTER_LEAVE_DECLARATIONS>
<UHDM_ENTER_LEAVE_VECTOR_DECLARATIONS>
private:
<UHDM_PRIVATE_LISTEN_DECLARATIONS>
};
}  // namespace UHDM


#endif  // UHDM_UHDMLISTENER_H
