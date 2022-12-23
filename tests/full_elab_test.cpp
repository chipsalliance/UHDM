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
#include "uhdm/ElaboratorListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/VpiListener.h"
#include "uhdm/vpi_visitor.h"

#include "test_util.h"

using namespace UHDM;

//-------------------------------------------
// This self-contained example demonstrate how one can navigate the Folded Model
// of UHDM By extracting the complete information in between in the instance
// tree and the module definitions using the Listener Design Pattern
//-------------------------------------------

//-------------------------------------------
// Unit test design

std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module_inst* m1 = s->MakeModule_inst();
  {
    m1->VpiDefName("M1");
    m1->VpiParent(d);
    m1->VpiFile("fake1.sv");
    m1->VpiLineNo(10);
  }

  //-------------------------------------------
  // Module definition M2 (non elaborated)
  module_inst* m2 = s->MakeModule_inst();
  {
    m2->VpiDefName("M2");
    m2->VpiFile("fake2.sv");
    m2->VpiLineNo(20);
    m2->VpiParent(d);
    // M2 Ports
    VectorOfport* vp = s->MakePortVec();
    port* p = s->MakePort();
    p->VpiName("i1");
    p->VpiDirection(vpiInput);
    vp->push_back(p);
    p = s->MakePort();
    p->VpiName("o1");
    p->VpiDirection(vpiOutput);
    vp->push_back(p);
    m2->Ports(vp);
    // M2 Nets
    VectorOfnet* vn = s->MakeNetVec();
    logic_net* n = s->MakeLogic_net();
    n->VpiName("i1");
    vn->push_back(n);
    n = s->MakeLogic_net();
    n->VpiName("o1");
    vn->push_back(n);
    m2->Nets(vn);

    // M2 continuous assignment
    VectorOfcont_assign* assigns = s->MakeCont_assignVec();
    cont_assign* cassign = s->MakeCont_assign();
    assigns->push_back(cassign);
    ref_obj* lhs = s->MakeRef_obj();
    ref_obj* rhs = s->MakeRef_obj();
    lhs->VpiName("o1");
    rhs->VpiName("i1");
    cassign->Lhs(lhs);
    cassign->Rhs(rhs);
    m2->Cont_assigns(assigns);
  }

  //-------------------------------------------
  // Instance tree (Elaborated tree)
  // Top level module
  module_inst* m3 = s->MakeModule_inst();
  VectorOfmodule_inst* v1 = s->MakeModule_instVec();
  {
    m3->VpiDefName("M1");  // Points to the module def (by name)
    m3->VpiName("M1");     // Instance name
    m3->VpiTopModule(true);
    m3->Modules(v1);
    m3->VpiParent(d);
  }

  //-------------------------------------------
  // Sub Instance
  module_inst* m4 = s->MakeModule_inst();
  {
    m4->VpiDefName("M2");         // Points to the module def (by name)
    m4->VpiName("inst1");         // Instance name
    m4->VpiFullName("M1.inst1");  // Instance full name
    VectorOfport* inst_vp = s->MakePortVec();  // Create elaborated ports
    m4->Ports(inst_vp);
    port* p1 = s->MakePort();
    p1->VpiName("i1");
    inst_vp->push_back(p1);
    port* p2 = s->MakePort();
    p2->VpiName("o1");
    inst_vp->push_back(p2);
    // M2 Nets
    VectorOfnet* vn = s->MakeNetVec();  // Create elaborated nets
    logic_net* n = s->MakeLogic_net();
    n->VpiName("i1");
    n->VpiFullName("M1.inst.i1");
    ref_obj* low_conn = s->MakeRef_obj();
    low_conn->Actual_group(n);
    low_conn->VpiName("i1");
    p1->Low_conn(low_conn);
    vn->push_back(n);
    n = s->MakeLogic_net();
    n->VpiName("o1");
    n->VpiFullName("M1.inst.o1");
    low_conn = s->MakeRef_obj();
    low_conn->Actual_group(n);
    low_conn->VpiName("o1");
    p2->Low_conn(low_conn);
    vn->push_back(n);
    m4->Nets(vn);
  }

  // Create parent-child relation in between the 2 modules in the instance tree
  v1->push_back(m4);
  m4->VpiParent(m3);

  //-------------------------------------------
  // Create both non-elaborated and elaborated lists
  VectorOfmodule_inst* allModules = s->MakeModule_instVec();
  d->AllModules(allModules);
  allModules->push_back(m1);
  allModules->push_back(m2);

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
  d->TopModules(topModules);
  topModules->push_back(
      m3);  // Only m3 goes there as it is the top level module

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

std::string dumpStats(const Serializer& serializer) {
  std::string result;
  auto stats = serializer.ObjectStats();
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
  for (auto design : designs) {
    elaborated = vpi_get(vpiElaborated, design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  EXPECT_EQ(dumpStats(serializer),
            "cont_assign 1\n"
            "design 1\n"
            "logic_net 4\n"
            "module_inst 4\n"
            "port 4\n"
            "ref_obj 4\n");

  ElaboratorListener* listener = new ElaboratorListener(&serializer, true);
  listener->listenDesigns(designs);
  delete listener;

  elaborated = false;
  for (auto design : designs) {
    elaborated = vpi_get(vpiElaborated, design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  // TODO: this needs more fine-grained object-level inspection.

  const std::string after = designs_to_string(designs);
  EXPECT_NE(before, after);  // We expect the elaboration to have some impact :)
}
