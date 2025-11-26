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

#include <uhdm/BaseClass.h>
#include <uhdm/containers.h>
#include <uhdm/sv_vpi_user.h>
#include <uhdm/uhdm_types.h>

#include <ostream>
#include <string>

#define TRACE_CONTEXT                   \
  "[" << ((const Any*)object)->getStartLine() <<      \
  "," << ((const Any*)object)->getStartColumn() <<    \
  ":" << ((const Any*)object)->getEndLine() <<        \
  "," << ((const Any*)object)->getEndColumn() <<      \
  "]"

#define TRACE_ENTER strm                \
  << std::string(++indent * 2, ' ')     \
  << __func__ << ": " << TRACE_CONTEXT  \
  << std::endl
#define TRACE_LEAVE strm                \
  << std::string(2 * indent--, ' ')     \
  << __func__ << ": " << TRACE_CONTEXT  \
  << std::endl

namespace uhdm {
class Serializer;
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
//<UHDM_PUBLIC_LISTEN_DECLARATIONS>

  virtual void enterAny(const Any* object, uint32_t vpiRelation) {}
  virtual void leaveAny(const Any* object, uint32_t vpiRelation) {}

//<UHDM_ENTER_LEAVE_DECLARATIONS>
//<UHDM_ENTER_LEAVE_COLLECTION_DECLARATIONS>
private:
  void listenAny_(const Any* object);
//<UHDM_PRIVATE_LISTEN_DECLARATIONS>

protected:
  AnySet m_visited;
  any_stack_t m_callstack;
  bool m_abortRequested = false;
};

class UhdmListenerTracer : public UhdmListener {
  public:
    UhdmListenerTracer(std::ostream &strm) : strm(strm) {}
    ~UhdmListenerTracer() final = default;

//<UHDM_LISTENER_OBJECT_TRACER_METHODS>
//<UHDM_LISTENER_COLLECTION_TRACER_METHODS>
  protected:
   std::ostream &strm;
   int32_t indent = -1;
};
}  // namespace uhdm

#endif  // UHDM_UHDMLISTENER_H
