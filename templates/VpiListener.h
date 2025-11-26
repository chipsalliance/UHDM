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

#include <uhdm/BaseClass.h>
#include <uhdm/containers.h>
#include <uhdm/uhdm_types.h>
#include <uhdm/vpi_user.h>

#include <ostream>
#include <set>
#include <vector>

#define TRACE_CONTEXT                   \
  "[" << ((const Any *)object)->getStartLine() <<      \
  "," << ((const Any *)object)->getStartColumn() <<    \
  ":" << ((const Any *)object)->getEndLine() <<        \
  "," << ((const Any *)object)->getEndColumn() <<      \
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

class ScopedVpiHandle final {
 public:
  ScopedVpiHandle(const Any *any);
  ~ScopedVpiHandle();

  operator vpiHandle() const { return m_handle; }

 private:
  const vpiHandle m_handle = nullptr;
};

class VpiListener {
protected:
  using visited_t = AnySet;
  using any_stack_t = std::vector<const Any *>;

public:
  // Use implicit constructor to initialize all members
  // VpiListener()

  virtual ~VpiListener() = default;

public:
  void listenAny(vpiHandle handle);
  void listenDesigns(const std::vector<vpiHandle>& designs);
//<VPI_PUBLIC_LISTEN_DECLARATIONS>

  virtual void enterAny(const Any* object, vpiHandle handle) {}
  virtual void leaveAny(const Any* object, vpiHandle handle) {}

//<VPI_ENTER_LEAVE_DECLARATIONS>
  bool isInUhdmAllIterator() const { return uhdmAllIterator; }
  bool inCallstackOfType(UhdmType type);
  Design* currentDesign() { return m_currentDesign; }

  void requestAbort() { m_abortRequested = true; }

protected:
  visited_t m_visited;
  any_stack_t m_callstack;
  bool m_abortRequested = false;
  bool uhdmAllIterator = false;
  Design* m_currentDesign = nullptr;

private:
  void listenBaseClass_(vpiHandle handle);
//<VPI_PRIVATE_LISTEN_DECLARATIONS>
};

class VpiListenerTracer : public VpiListener {
  public:
    VpiListenerTracer(std::ostream &strm) : strm(strm) {}
    ~VpiListenerTracer() final = default;

//<VPI_LISTENER_TRACER_METHODS>
  protected:
   std::ostream &strm;
   int32_t indent = -1;
};
}  // namespace uhdm

#endif  // UHDM_VPILISTENER_H
