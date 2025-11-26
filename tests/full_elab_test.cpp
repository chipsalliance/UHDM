/*

 Copyright 2020 Alain Dargelas

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
 * File:   listener_elab
 * Author: alain
 *
 * Created on May 4, 2020, 10:03 PM
 */

#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/Elaborator.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

//-------------------------------------------
// This self-contained example demonstrate how one can navigate the Folded Model
// of UHDM By extracting the complete information in between in the instance
// tree and the module definitions using the Listener Design Pattern
//-------------------------------------------

//-------------------------------------------
// Unit test Design

std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* m1 = s->make<Module>();
  {
    m1->setDefName("M1");
    m1->setParent(d);
    m1->setFile("fake1.sv");
    m1->setStartLine(10);
  }

  //-------------------------------------------
  // Module definition M2 (non elaborated)
  Module* m2 = s->make<Module>();
  {
    m2->setDefName("M2");
    m2->setFile("fake2.sv");
    m2->setStartLine(20);
    m2->setParent(d);

    // M2 Ports
    Port* p = s->make<Port>();
    p->setName("i1");
    p->setDirection(vpiInput);
    p->setParent(m2);
    p = s->make<Port>();
    p->setName("o1");
    p->setDirection(vpiOutput);
    p->setParent(m2);

    // M2 Nets
    Net* n = s->make<Net>();
    n->setName("i1");
    n->setParent(m2);
    n = s->make<Net>();
    n->setName("o1");
    n->setParent(m2);

    // M2 continuous assignment
    ContAssign* cassign = s->make<ContAssign>();
    cassign->setParent(m2);
    RefObj* lhs = s->make<RefObj>();
    RefObj* rhs = s->make<RefObj>();
    lhs->setName("o1");
    rhs->setName("i1");
    cassign->setLhs(lhs);
    cassign->setRhs(rhs);
  }

  //-------------------------------------------
  // Instance tree (Elaborated tree)
  // Top level module
  Module* m3 = s->make<Module>();
  ModuleCollection* v1 = s->makeCollection<Module>();
  {
    m3->setDefName("M1");  // Points to the module def (by name)
    m3->setName("M1");     // Instance name
    m3->setTopModule(true);
    m3->setModules(v1);
    m3->setParent(d);
  }

  //-------------------------------------------
  // Sub Instance
  Module* m4 = s->make<Module>();
  {
    m4->setDefName("M2");         // Points to the module def (by name)
    m4->setName("inst1");         // Instance name
    m4->setFullName("M1.inst1");  // Instance full name
    Port* p1 = s->make<Port>();
    p1->setName("i1");
    p1->setParent(m4);
    Port* p2 = s->make<Port>();
    p2->setName("o1");
    p2->setParent(m4);

    // M2 Nets
    Net* n = s->make<Net>();
    n->setName("i1");
    n->setFullName("M1.inst.i1");
    RefObj* low_conn = s->make<RefObj>();
    low_conn->setActual(n);
    low_conn->setName("i1");
    low_conn->setParent(p1);
    p1->setLowConn(low_conn);
    n->setParent(m4);
    n = s->make<Net>();
    n->setName("o1");
    n->setFullName("M1.inst.o1");
    low_conn = s->make<RefObj>();
    low_conn->setActual(n);
    low_conn->setName("o1");
    low_conn->setParent(p2);
    p2->setLowConn(low_conn);
    n->setParent(m4);
  }

  // Create parent-child relation in between the 2 modules in the instance tree
  v1->push_back(m4);
  m4->setParent(m3);

  //-------------------------------------------
  // Create both non-elaborated and elaborated lists
  ModuleCollection* allModules = s->makeCollection<Module>();
  d->setAllModules(allModules);
  allModules->emplace_back(m1);
  allModules->emplace_back(m2);

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->emplace_back(
      m3);  // Only m3 goes there as it is the top level module

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

std::string dumpStats(const Serializer& serializer) {
  std::string result;
  auto stats = serializer.getObjectStats();
  for (const auto& stat : stats) {
    if (!stat.second) continue;
    result += stat.first + " " + std::to_string(stat.second) + "\n";
  }
  return result;
}

// TODO: this is too coarse-grained.
TEST(FullElabTest, ElaborationRoundtrip) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  const std::string before = designs_to_string(designs);

  bool elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  EXPECT_EQ(dumpStats(serializer),
            "ContAssign 1\n"
            "Design 1\n"
            "Identifier 2\n"
            "Module 4\n"
            "Net 4\n"
            "Port 4\n"
            "RefObj 4\n");

  Elaborator elaborator(&serializer);
  elaborator.listenDesigns(designs);

  elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  // TODO: this needs more fine-grained object-level inspection.

  const std::string after = designs_to_string(designs);
  EXPECT_NE(before, after);  // We expect the elaboration to have some impact :)
}
