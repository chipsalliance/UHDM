/*

 Copyright 2022 Alain Dargelas

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
 * File:   expr_reduce.cpp
 * Author: alain
 *
 * Created on March 18, 2022, 10:03 PM
 */

#include <iostream>
#include <stack>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "uhdm/ElaboratorListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/VpiListener.h"
#include "uhdm/vpi_visitor.h"
#include "uhdm/ExprEval.h"

#include "test_util.h"

using namespace UHDM;

//-------------------------------------------
// This self-contained example demonstrate how one can use expression reduction
// in UHDM
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
  module* m1 = s->MakeModule();
  {
    m1->VpiDefName("M1");
    m1->VpiParent(d);
    m1->VpiFile("fake1.sv");
    m1->VpiLineNo(10);
    constant* c1 = s->MakeConstant();
    c1->VpiValue("INT:2");
    c1->VpiConstType(vpiIntConst);
    constant* c2 = s->MakeConstant();
    c2->VpiValue("INT:3");
    c2->VpiConstType(vpiIntConst);
    operation* oper = s->MakeOperation();
    oper->VpiOpType(vpiAddOp);
    VectorOfany* operands = s->MakeAnyVec();
    oper->Operands(operands);
    operands->push_back(c1);
    operands->push_back(c2);
    parameter* p = s->MakeParameter();
    p->VpiName("param");
    param_assign* pass = s->MakeParam_assign();
    pass->Lhs(p);
    pass->Rhs(oper);
    m1->Parameters(s->MakeAnyVec());
    m1->Parameters()->push_back(p);
    m1->Param_assigns(s->MakeParam_assignVec());
    m1->Param_assigns()->push_back(pass);
  }

  VectorOfmodule* topModules = s->MakeModuleVec();
  d->TopModules(topModules);
  topModules->push_back(m1);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

TEST(FullElabTest, ElaborationRoundtrip) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  const std::string before = designs_to_string(designs);

  bool elaborated = false;
  for (auto design : designs) {
    elaborated = vpi_get(vpiElaborated, design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  ElaboratorListener* listener = new ElaboratorListener(&serializer, true);
  listener->listenDesigns(designs);
  delete listener;

  elaborated = false;
  for (auto design : designs) {
    elaborated = vpi_get(vpiElaborated, design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  vpiHandle dh = designs.at(0);
  design* d = UhdmDesignFromVpiHandle(dh);
  for (auto m : *d->TopModules()) {
    for (auto pass : *m->Param_assigns()) {
      const any* rhs = pass->Rhs();
      ExprEval eval;
      bool invalidValue = false;
      expr* reduced = eval.reduceExpr(rhs, invalidValue, m, pass);
      int64_t val = eval.get_value(invalidValue, reduced);
      EXPECT_EQ(val, 5);
      EXPECT_EQ(invalidValue, false);
    }
  }

}
