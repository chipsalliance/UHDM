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
#include "test_util.h"
#include "uhdm/Elaborator.h"
#include "uhdm/ExprEval.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

//-------------------------------------------
// This self-contained example demonstrate how one can use expression reduction
// in UHDM
//-------------------------------------------

//-------------------------------------------
// Unit test Design

std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> Designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("Design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* m1 = s->make<Module>();
  {
    m1->setDefName("M1");
    m1->setParent(d);
    m1->setFile("fake1.sv");
    m1->setStartLine(10);
    Constant* c1 = s->make<Constant>();
    c1->setValue("INT:2");
    c1->setConstType(vpiIntConst);
    Constant* c2 = s->make<Constant>();
    c2->setValue("INT:3");
    c2->setConstType(vpiIntConst);
    Operation* oper = s->make<Operation>();
    oper->setOpType(vpiAddOp);
    AnyCollection* operands = s->makeCollection<Any>();
    oper->setOperands(operands);
    operands->push_back(c1);
    operands->push_back(c2);
    Parameter* p = s->make<Parameter>();
    p->setName("param");
    ParamAssign* pass = s->make<ParamAssign>();
    pass->setLhs(p);
    pass->setRhs(oper);
    m1->setParameters(s->makeCollection<Any>());
    m1->getParameters()->push_back(p);
    m1->setParamAssigns(s->makeCollection<ParamAssign>());
    m1->getParamAssigns()->push_back(pass);
  }

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->push_back(m1);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  Designs.push_back(dh);

  return Designs;
}

TEST(FullElabTest, ElaborationRoundtrip) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  const std::string before = designs_to_string(designs);

  bool elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  Elaborator elaborator(&serializer);
  elaborator.listenDesigns(designs);

  elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  vpiHandle dh = designs.at(0);
  Design* d = UhdmDesignFromVpiHandle(dh);
  for (auto m : *d->getTopModules()) {
    for (auto pass : *m->getParamAssigns()) {
      const Any* rhs = pass->getRhs();
      ExprEval eval;
      bool invalidValue = false;
      Expr* reduced = eval.reduceExpr(rhs, invalidValue, m, pass);
      int64_t val = eval.get_value(invalidValue, reduced);
      EXPECT_EQ(val, 5);
      EXPECT_EQ(invalidValue, false);
    }
  }
}
