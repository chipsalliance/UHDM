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

// `$left` / `$right` / `$high` / `$low` with a DIMENSION argument answer for
// that dimension, like `$size` already did.  On `logic [31:0] arr [15:0]` the
// generic path read `ranges->at(0)` whatever was asked and, for an unpacked
// array net (whose element typespec carries no range it looks at), fell
// through to the bit-count sum: `$left(arr, 2)` came back 512.
TEST(ExprReduceTest, BoundQueriesWalkTheRequestedDimension) {
  Serializer s;
  auto make_const = [&](const std::string& v, int size, int ctype) {
    constant* c = s.MakeConstant();
    c->VpiValue(v);
    c->VpiSize(size);
    c->VpiConstType(ctype);
    return c;
  };
  auto make_range = [&](int l, int r) {
    range* rg = s.MakeRange();
    rg->Left_expr(make_const("UINT:" + std::to_string(l), 32, vpiUIntConst));
    rg->Right_expr(make_const("UINT:" + std::to_string(r), 32, vpiUIntConst));
    return rg;
  };
  // module top; logic [31:0] arr [15:0];
  module_inst* top = s.MakeModule_inst();
  top->VpiName("top");
  logic_typespec* elem_ts = s.MakeLogic_typespec();
  VectorOfrange* packed = s.MakeRangeVec();
  packed->push_back(make_range(31, 0));
  elem_ts->Ranges(packed);
  logic_net* elem = s.MakeLogic_net();
  elem->VpiName("arr");
  ref_typespec* elem_rt = s.MakeRef_typespec();
  elem_rt->Actual_typespec(elem_ts);
  elem->Typespec(elem_rt);
  array_net* arr = s.MakeArray_net();
  arr->VpiName("arr");
  VectorOfrange* unpacked = s.MakeRangeVec();
  unpacked->push_back(make_range(15, 0));
  arr->Ranges(unpacked);
  VectorOfnet* nets = s.MakeNetVec();
  nets->push_back(elem);
  arr->Nets(nets);
  VectorOfarray_net* array_nets = s.MakeArray_netVec();
  array_nets->push_back(arr);
  top->Array_nets(array_nets);

  auto query = [&](const std::string& fn, int dim) -> int64_t {
    sys_func_call* call = s.MakeSys_func_call();
    call->VpiName(fn);
    VectorOfany* args = s.MakeAnyVec();
    ref_obj* ref = s.MakeRef_obj();
    ref->VpiName("arr");
    ref->Actual_group(arr);
    args->push_back(ref);
    args->push_back(make_const("UINT:" + std::to_string(dim), 32, vpiUIntConst));
    call->Tf_call_args(args);
    ExprEval eval;
    bool invalidValue = false;
    expr* r = eval.reduceExpr(call, invalidValue, top, nullptr);
    if (r == nullptr || invalidValue || r->UhdmType() != uhdmconstant) return -999;
    return eval.get_value(invalidValue, r);
  };
  EXPECT_EQ(query("$size", 1), 16);
  EXPECT_EQ(query("$size", 2), 32);
  EXPECT_EQ(query("$left", 1), 15);
  EXPECT_EQ(query("$right", 1), 0);
  EXPECT_EQ(query("$left", 2), 31);
  EXPECT_EQ(query("$right", 2), 0);
  EXPECT_EQ(query("$high", 2), 31);
  EXPECT_EQ(query("$low", 2), 0);
}

// An element select of a packed table whose folded value is spelled HEX:
// (or UINT:) must still be an ELEMENT select: `T[10]` on a
// `logic [15:0][3:0] T = 64'h21748F3EDA09B65C` is 4'hF, not bit 10 (1'b1).
// reducePackedElemSelect used to accept BIN: values only.
TEST(ExprReduceTest, PackedElemSelectOnHexTable) {
  Serializer s;
  auto make_const = [&](const std::string& v, int size, int ctype) {
    constant* c = s.MakeConstant();
    c->VpiValue(v);
    c->VpiSize(size);
    c->VpiConstType(ctype);
    return c;
  };
  auto make_range = [&](int l, int r) {
    range* rg = s.MakeRange();
    rg->Left_expr(make_const("UINT:" + std::to_string(l), 32, vpiUIntConst));
    rg->Right_expr(make_const("UINT:" + std::to_string(r), 32, vpiUIntConst));
    return rg;
  };
  // module top; localparam logic [15:0][3:0] T = 64'h21748F3EDA09B65C;
  module_inst* top = s.MakeModule_inst();
  top->VpiName("top");
  logic_typespec* tts = s.MakeLogic_typespec();
  VectorOfrange* rgs = s.MakeRangeVec();
  rgs->push_back(make_range(15, 0));
  rgs->push_back(make_range(3, 0));
  tts->Ranges(rgs);
  parameter* T = s.MakeParameter();
  T->VpiName("T");
  ref_typespec* trt = s.MakeRef_typespec();
  trt->Actual_typespec(tts);
  T->Typespec(trt);
  constant* value = make_const("HEX:21748F3EDA09B65C", 64, vpiHexConst);
  param_assign* pa = s.MakeParam_assign();
  pa->Lhs(T);
  pa->Rhs(value);
  VectorOfparam_assign* pas = s.MakeParam_assignVec();
  pas->push_back(pa);
  top->Param_assigns(pas);

  auto elem = [&](int idx) -> int64_t {
    bit_select* sel = s.MakeBit_select();
    sel->VpiName("T");
    sel->VpiIndex(make_const("UINT:" + std::to_string(idx), 32, vpiUIntConst));
    ExprEval eval;
    // Surelog hands the base parameter's DECLARED typespec to the evaluator
    // through this functor (the folded value constant carries none).
    eval.setGetTypespecFunctor(
        [&](std::string_view name, const any*, const any*) -> any* {
          return (name == "T") ? (any*)tts : nullptr;
        });
    bool invalidValue = false;
    expr* r = eval.reduceExpr(sel, invalidValue, top, nullptr);
    if (r == nullptr || invalidValue || r->UhdmType() != uhdmconstant) return -999;
    if (r->VpiSize() != 4) return -(1000 + r->VpiSize());
    return eval.get_value(invalidValue, r);
  };
  EXPECT_EQ(elem(10), 0xF);
  EXPECT_EQ(elem(1), 0x5);
  EXPECT_EQ(elem(15), 0x2);
  EXPECT_EQ(elem(0), 0xC);
}

// A replication count ABOVE the old 1000 clamp must fold to the FULL width,
// and an absurd count must FAIL the reduction rather than be truncated to a
// plausible-looking one.
//
// The clamp (`if (n > 1000) n = 1000;`) silently produced a SHORTER constant
// than the source asks for, and every consumer then zero-extends it to the
// expected width: T-Head's OpenC910 masks 1024 PLIC interrupts with
// `{{(INT_NUM-1){1'b1}}, 1'b0}`, so the mask folded to 1001 bits instead of
// 1024 and the top 23 bits of each hart's interrupt-enable were forced to 0,
// with no diagnostic anywhere.
TEST(ExprReduceTest, ReplicationCountAboveOneThousand) {
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

  // {1023{1'b1}} is 1023 bits, not 1000.
  bool invalidValue = false;
  expr* r = eval.reduceExpr(
      make_op(vpiMultiConcatOp, {make_const("UINT:1023", 32, vpiUIntConst),
                                 make_const("BIN:1", 1, vpiBinaryConst)}),
      invalidValue, nullptr, nullptr);
  ASSERT_NE(r, nullptr);
  EXPECT_FALSE(invalidValue);
  EXPECT_EQ(r->VpiSize(), 1023);

  // The OpenC910 mask shape: {{1023{1'b1}}, 1'b0} is 1024 bits.
  invalidValue = false;
  expr* mask = eval.reduceExpr(
      make_op(vpiConcatOp,
              {make_op(vpiMultiConcatOp,
                       {make_const("UINT:1023", 32, vpiUIntConst),
                        make_const("BIN:1", 1, vpiBinaryConst)}),
               make_const("BIN:0", 1, vpiBinaryConst)}),
      invalidValue, nullptr, nullptr);
  ASSERT_NE(mask, nullptr);
  EXPECT_FALSE(invalidValue);
  EXPECT_EQ(mask->VpiSize(), 1024);

  // An absurd count is reported invalid, never silently truncated.
  invalidValue = false;
  eval.reduceExpr(
      make_op(vpiMultiConcatOp,
              {make_const("UINT:4294967295", 64, vpiUIntConst),
               make_const("BIN:1", 1, vpiBinaryConst)}),
      invalidValue, nullptr, nullptr);
  EXPECT_TRUE(invalidValue);
}
