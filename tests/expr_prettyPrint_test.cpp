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

std::vector<vpiHandle> build_designs_MinusOp(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module_inst* dut = s->MakeModule_inst();
  {
    dut->VpiDefName("M1");
    dut->VpiParent(d);
    // dut->VpiFile("fake1.sv");
    // dut->VpiLineNo(10);
    VectorOfport* vp = s->MakePortVec();
    dut->Ports(vp);
    port* p = s->MakePort();
    vp->push_back(p);
    p->VpiName("wire_i");
    p->VpiDirection(vpiInput);

    VectorOftypespec* typespecs = s->MakeTypespecVec();
    dut->Typespecs(typespecs);

    logic_typespec* tps = s->MakeLogic_typespec();
    typespecs->emplace_back(tps);

    ref_typespec* tps_rt = s->MakeRef_typespec();
    tps_rt->Actual_typespec(tps);
    tps_rt->VpiParent(p);
    p->Typespec(tps_rt);

    VectorOfrange* ranges = s->MakeRangeVec();
    tps->Ranges(ranges);
    range* range = s->MakeRange();
    ranges->push_back(range);

    operation* oper = s->MakeOperation();
    range->Left_expr(oper);
    oper->VpiOpType(vpiSubOp);
    VectorOfany* operands = s->MakeAnyVec();
    oper->Operands(operands);

    ref_obj* SIZE = s->MakeRef_obj();
    operands->push_back(SIZE);
    SIZE->VpiName("SIZE");
    logic_net* n = s->MakeLogic_net();
    SIZE->Actual_group(n);

    constant* c1 = s->MakeConstant();
    c1->VpiValue("UINT:1");
    c1->VpiConstType(vpiIntConst);
    c1->VpiDecompile("1");
    operands->push_back(c1);

    constant* c2 = s->MakeConstant();
    c2->VpiValue("UINT:0");
    c2->VpiConstType(vpiIntConst);
    c2->VpiDecompile("0");

    range->Right_expr(c2);
  }

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
  d->TopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_MinusOp) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs_MinusOp(&serializer);
  // serializer.Save("expr_MinusOp_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto p : *m->Ports()) {
      const ref_typespec* rt = p->Typespec();
      const logic_typespec* typespec = rt->Actual_typespec<logic_typespec>();
      VectorOfrange* ranges = typespec->Ranges();
      for (auto range : *ranges) {
        expr* left = (expr*)range->Left_expr();
        std::string left_str = eval.prettyPrint((any*)left);
        EXPECT_EQ(left_str, "SIZE - 1");

        expr* right = (expr*)range->Right_expr();
        std::string right_str = eval.prettyPrint((any*)right);
        EXPECT_EQ(right_str, "0");
      }
    }
  }
}

std::vector<vpiHandle> build_designs_ConditionOp(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module_inst* dut = s->MakeModule_inst();
  {
    dut->VpiDefName("M1");
    dut->VpiParent(d);
    dut->Parameters(s->MakeAnyVec());

    VectorOfparam_assign* vpa = s->MakeParam_assignVec();

    param_assign* param = s->MakeParam_assign();
    vpa->push_back(param);
    dut->Param_assigns(vpa);

    parameter* p = s->MakeParameter();
    dut->Parameters()->push_back(p);
    p->VpiName("a");
    param->Lhs(p);

    operation* oper = s->MakeOperation();
    oper->VpiOpType(vpiConditionOp);
    VectorOfany* operands = s->MakeAnyVec();
    oper->Operands(operands);
    ref_obj* b = s->MakeRef_obj();
    b->VpiName("b");
    operands->push_back(b);

    constant* c1 = s->MakeConstant();
    c1->VpiValue("UINT:1");
    c1->VpiConstType(vpiIntConst);
    c1->VpiDecompile("1");
    operands->push_back(c1);

    constant* c2 = s->MakeConstant();
    c2->VpiValue("UINT:3");
    c2->VpiConstType(vpiIntConst);
    c2->VpiDecompile("3");
    operands->push_back(c2);
    param->Rhs(oper);
  }

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
  d->TopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_ConditionOp) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs =
      build_designs_ConditionOp(&serializer);
  // serializer.Save("expr_ConditionOp_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto pa : *m->Param_assigns()) {
      const any* rhs = pa->Rhs();
      std::string result = eval.prettyPrint((any*)rhs);
      EXPECT_EQ(result, "b ? 1 : 3");
    }
  }
}

std::vector<vpiHandle> build_designs_functionCall(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module_inst* dut = s->MakeModule_inst();
  {
    dut->VpiDefName("M1");
    dut->VpiParent(d);
    dut->Parameters(s->MakeAnyVec());

    VectorOfparam_assign* vpa = s->MakeParam_assignVec();

    param_assign* param = s->MakeParam_assign();
    vpa->push_back(param);
    dut->Param_assigns(vpa);

    parameter* p = s->MakeParameter();
    dut->Parameters()->push_back(p);
    p->VpiName("a");
    param->Lhs(p);

    sys_func_call* sfc = s->MakeSys_func_call();
    param->Rhs(sfc);

    sfc->VpiName("$sformatf");

    VectorOfany* args = s->MakeAnyVec();
    sfc->Tf_call_args(args);
    constant* c1 = s->MakeConstant();
    c1->VpiValue("%d");
    c1->VpiConstType(vpiStringConst);
    c1->VpiDecompile("\"%d\"");
    args->push_back(c1);

    ref_obj* b = s->MakeRef_obj();
    b->VpiName("b");
    args->push_back(b);
    logic_net* n = s->MakeLogic_net();
    b->Actual_group(n);
  }

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
  d->TopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_functionCall) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs =
      build_designs_functionCall(&serializer);
  // serializer.Save("expr_functionCall_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto pa : *m->Param_assigns()) {
      const any* rhs = pa->Rhs();
      std::string result = eval.prettyPrint((any*)rhs);
      std::string expected_result = "$sformatf(\"%d\",b)";
      std::cout << expected_result << std::endl;

      EXPECT_EQ(expected_result, result);
    }
  }
}

std::vector<vpiHandle> build_designs_select(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module_inst* dut = s->MakeModule_inst();
  {
    dut->VpiDefName("M1");
    dut->VpiParent(d);
    dut->Parameters(s->MakeAnyVec());

    VectorOfparam_assign* vpa = s->MakeParam_assignVec();

    param_assign* param = s->MakeParam_assign();
    vpa->push_back(param);
    dut->Param_assigns(vpa);

    parameter* p = s->MakeParameter();
    dut->Parameters()->push_back(p);
    p->VpiName("a");
    param->Lhs(p);

    var_select* vs = s->MakeVar_select();
    param->Rhs(vs);

    vs->VpiName("b");
    VectorOfexpr* exprs = s->MakeExprVec();
    vs->Exprs(exprs);

    constant* c1 = s->MakeConstant();
    c1->VpiValue("UINT:3");
    c1->VpiConstType(vpiIntConst);
    c1->VpiDecompile("3");
    exprs->push_back(c1);

    constant* c2 = s->MakeConstant();
    c2->VpiValue("UINT:2");
    c2->VpiConstType(vpiIntConst);
    c2->VpiDecompile("2");
    exprs->push_back(c2);

    part_select* ps = s->MakePart_select();
    exprs->push_back(ps);
    ps->VpiConstantSelect(true);

    constant* c3 = s->MakeConstant();
    c3->VpiValue("UINT:1");
    c3->VpiConstType(vpiIntConst);
    c3->VpiDecompile("1");
    ps->Left_range(c3);

    constant* c4 = s->MakeConstant();
    c4->VpiValue("UINT:0");
    c4->VpiConstType(vpiIntConst);
    c4->VpiDecompile("0");
    ps->Right_range(c4);
  }

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
  d->TopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_select) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs_select(&serializer);
  // serializer.Save("expr_select_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto pa : *m->Param_assigns()) {
      const any* rhs = pa->Rhs();
      std::string result = eval.prettyPrint((any*)rhs);
      std::string expected_result = "b[3][2][1:0]";
      std::cout << expected_result << std::endl;

      EXPECT_EQ(expected_result, result);
    }
  }
}

std::vector<vpiHandle> build_designs_AssignmentPatternOp(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module_inst* dut = s->MakeModule_inst();
  {
    dut->VpiDefName("M1");
    dut->VpiParent(d);
    dut->Parameters(s->MakeAnyVec());

    VectorOfparam_assign* vpa = s->MakeParam_assignVec();

    param_assign* param = s->MakeParam_assign();
    vpa->push_back(param);
    dut->Param_assigns(vpa);

    parameter* p = s->MakeParameter();
    dut->Parameters()->push_back(p);
    p->VpiName("a");
    param->Lhs(p);

    operation* op = s->MakeOperation();
    param->Rhs(op);

    op->VpiOpType(vpiAssignmentPatternOp);
    VectorOfany* operands = s->MakeAnyVec();
    op->Operands(operands);

    constant* c1 = s->MakeConstant();
    c1->VpiValue("UINT:1");
    c1->VpiConstType(vpiIntConst);
    c1->VpiDecompile("1");
    operands->push_back(c1);

    constant* c2 = s->MakeConstant();
    c2->VpiValue("UINT:2");
    c2->VpiConstType(vpiIntConst);
    c2->VpiDecompile("2");
    operands->push_back(c2);

    constant* c3 = s->MakeConstant();
    c3->VpiValue("UINT:3");
    c3->VpiConstType(vpiIntConst);
    c3->VpiDecompile("3");
    operands->push_back(c3);
  }

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
  d->TopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_array) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs =
      build_designs_AssignmentPatternOp(&serializer);
  // serializer.Save("expr_AssignmentPatternOp_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto pa : *m->Param_assigns()) {
      const any* rhs = pa->Rhs();
      std::string result = eval.prettyPrint((any*)rhs);
      std::string expected_result = "'{1,2,3}";
      std::cout << expected_result << std::endl;

      EXPECT_EQ(expected_result, result);
    }
  }
}
