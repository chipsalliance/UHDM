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
#include <uhdm/uhdm_types.h>

namespace uhdm {
class Serializer;

class ScopedVpiHandle final {
 public:
  ScopedVpiHandle(const Any *any);
  ~ScopedVpiHandle();

  operator vpiHandle() const { return handle; }

 private:
  const vpiHandle handle = nullptr;
};

class UhdmListener {
protected:
  using any_stack_t = std::vector<const Any *>;

public:
  // Use implicit constructor to initialize all members
  // UhdmListener()
  virtual ~UhdmListener() = default;

public:
  AnySet &getVisited() { return m_visited; }
 const AnySet &getVisited() const { return m_visited; }

  const any_stack_t &getCallstack() const { return m_callstack; }

  bool isOnCallstack(const Any *what) const;
  bool isOnCallstack(const std::set<UhdmType> &types) const;

  void requestAbort() { m_abortRequested = true; }

  bool didVisitAll(const Serializer &serializer) const;

  void listenAny(const Any* object, uint32_t vpiRelation = 0);
<UHDM_PUBLIC_LISTEN_DECLARATIONS>

  virtual void enterAny(const Any* object, uint32_t vpiRelation) {}
  virtual void leaveAny(const Any* object, uint32_t vpiRelation) {}

<UHDM_ENTER_LEAVE_DECLARATIONS>
<UHDM_ENTER_LEAVE_COLLECTION_DECLARATIONS>
private:
  void listenAny_(const Any* object);
<UHDM_PRIVATE_LISTEN_DECLARATIONS>

protected:
  AnySet m_visited;
  any_stack_t m_callstack;
  bool m_abortRequested = false;
};
}  // namespace uhdm

#endif  // UHDM_UHDMLISTENER_H
