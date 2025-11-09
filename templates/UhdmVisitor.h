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
 * File:   UhdmVisitor.h
 * Author: hs
 *
 * Created on October 01, 2025, 00:00 AM
 */

#ifndef UHDM_UHDMVISITOR_H
#define UHDM_UHDMVISITOR_H

#include <uhdm/containers.h>
#include <uhdm/sv_vpi_user.h>
#include <uhdm/uhdm_types.h>

namespace uhdm {
class Serializer;

class UhdmVisitor {
public:
  // Use implicit constructor to initialize all members
  // VpiListener()
  virtual ~UhdmVisitor() = default;

public:
  AnySet &getVisited() { return m_visited; }
  const AnySet &getVisited() const { return m_visited; }

  void requestAbort() { m_abortRequested = true; }

  bool didVisitAll(const Serializer &serializer) const;
  void visit(const Any *object);

  // clang-format off
  virtual void visitAny(const Any* object) {}
// <UHDMVISITOR_PUBLIC_VISIT_ANY_DECLARATIONS>

// <UHDMVISITOR_PUBLIC_VISIT_COLLECTION_DECLARATIONS>
  // clang-format on

private:
  // clang-format off
  void visitAny_(const Any* object);
// <UHDMVISITOR_PRIVATE_VISIT_ANY_DECLARATIONS>
  // clang-format on

protected:
  AnySet m_visited;
  bool m_abortRequested = false;
};
}  // namespace uhdm

#endif  // UHDM_UHDMVISITOR_H
