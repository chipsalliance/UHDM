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
#include "uhdm/ElaboratorListener.h"
#include "uhdm/ExprEval.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

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
  module_inst* m1 = s->MakeModule_inst();
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

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
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

  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(designs);
  delete elaboratorContext;

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

// A reduction, logical or `inside` operator yields ONE bit (LRM 11.6.1),
// whatever the width of its operand.  reduceExpr sized every such fold as a
// 64-bit integer; inside a concatenation that shifts every other operand --
// Hazard3's `localparam MEPC_MASK = {{XLEN-2{1'b1}}, |EXTENSION_C, 1'b0}`
// folded to 95 bits, and its `casez ({is_32bit, |EXTENSION_ZCMP,
// instr[15:0]})` compared an 81-bit selector against 18-bit patterns.
TEST(ExprReduceTest, OneBitOperatorResultWidth) {
  Serializer s;
  auto make_const = [&](const std::string& v, int size) {
    constant* c = s.MakeConstant();
    c->VpiValue(v);
    c->VpiSize(size);
    c->VpiConstType(vpiUIntConst);
    return c;
  };
  auto make_op = [&](int type, std::vector<any*> ops) {
    operation* op = s.MakeOperation();
    op->VpiOpType(type);
    VectorOfany* operands = s.MakeAnyVec();
    for (any* o : ops) operands->push_back(o);
    op->Operands(operands);
    return op;
  };
  ExprEval eval;
  for (int type : {vpiUnaryAndOp, vpiUnaryNandOp, vpiUnaryOrOp, vpiUnaryNorOp,
                   vpiUnaryXorOp, vpiUnaryXNorOp, vpiNotOp}) {
    bool invalidValue = false;
    expr* r = eval.reduceExpr(make_op(type, {make_const("UINT:6", 32)}),
                              invalidValue, nullptr, nullptr);
    ASSERT_NE(r, nullptr) << type;
    EXPECT_FALSE(invalidValue) << type;
    EXPECT_EQ(r->VpiSize(), 1) << type;
  }
  for (int type : {vpiLogAndOp, vpiLogOrOp}) {
    bool invalidValue = false;
    expr* r = eval.reduceExpr(
        make_op(type, {make_const("UINT:6", 32), make_const("UINT:1", 32)}),
        invalidValue, nullptr, nullptr);
    ASSERT_NE(r, nullptr) << type;
    EXPECT_FALSE(invalidValue) << type;
    EXPECT_EQ(r->VpiSize(), 1) << type;
  }
  // {1'b1, |32'd6, 1'b0} is three bits wide and equals 3'b110.
  {
    bool invalidValue = false;
    expr* r = eval.reduceExpr(
        make_op(vpiConcatOp,
                {make_const("UINT:1", 1),
                 make_op(vpiUnaryOrOp, {make_const("UINT:6", 32)}),
                 make_const("UINT:0", 1)}),
        invalidValue, nullptr, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_FALSE(invalidValue);
    EXPECT_EQ(r->VpiSize(), 3);
    EXPECT_EQ(eval.get_value(invalidValue, r), 6);
  }
}

// A conditional operator whose selected arm is a SIZED constant keeps that
// constant's width: `{1'b1, (0 > 0) ? 1'b1 : 1'b0, 1'b0}` is three bits,
// 3'b100 -- not 1 + 64 + 1 bits (ZipCPU's w_cpu_info lost every operand left
// of its ternaries that way).
TEST(ExprReduceTest, ConditionalKeepsSizedArmWidth) {
  Serializer s;
  auto make_const = [&](const std::string& v, int size, int ctype) {
    constant* c = s.MakeConstant();
    c->VpiValue(v);
    c->VpiSize(size);
    c->VpiConstType(ctype);
    return c;
  };
  auto make_op = [&](int type, std::vector<any*> ops) {
    operation* op = s.MakeOperation();
    op->VpiOpType(type);
    VectorOfany* operands = s.MakeAnyVec();
    for (any* o : ops) operands->push_back(o);
    op->Operands(operands);
    return op;
  };
  ExprEval eval;
  bool invalidValue = false;
  operation* cond = make_op(vpiConditionOp,
                            {make_op(vpiGtOp, {make_const("UINT:0", 32, vpiUIntConst),
                                               make_const("UINT:0", 64, vpiUIntConst)}),
                             make_const("BIN:1", 1, vpiBinaryConst),
                             make_const("BIN:0", 1, vpiBinaryConst)});
  expr* r = eval.reduceExpr(cond, invalidValue, nullptr, nullptr);
  ASSERT_NE(r, nullptr);
  EXPECT_FALSE(invalidValue);
  EXPECT_EQ(r->VpiSize(), 1);
  expr* cat = eval.reduceExpr(
      make_op(vpiConcatOp, {make_const("BIN:1", 1, vpiBinaryConst), cond,
                            make_const("BIN:0", 1, vpiBinaryConst)}),
      invalidValue, nullptr, nullptr);
  ASSERT_NE(cat, nullptr);
  EXPECT_FALSE(invalidValue);
  EXPECT_EQ(cat->VpiSize(), 3);
  EXPECT_EQ(eval.get_value(invalidValue, cat), 4);
}
