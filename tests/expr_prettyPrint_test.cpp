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
    //dut->VpiFile("fake1.sv");
    //dut->VpiLineNo(10);
    VectorOfport *vp = s->MakePortVec();
    dut->Ports(vp);
    port* p = s->MakePort();
    vp->push_back(p);
    p->VpiName("wire_i");
    p->VpiDirection(vpiInput);

    logic_typespec * tps = s->MakeLogic_typespec();
    p->Typespec(tps);

    VectorOfrange* ranges = s->MakeRangeVec();
    tps->Ranges(ranges);
    range * range = s->MakeRange();
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


TEST(exprVal,prettyPrint_MinusOp) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs_MinusOp(&serializer);
  //serializer.Save("expr_reduce_test.uhdm");
  //const std::string before = designs_to_string(designs);
  //std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto p : *m->Ports()) {
	logic_typespec *  typespec = (logic_typespec *) p->Typespec();
    	VectorOfrange* ranges = typespec->Ranges();
	for (auto range : *ranges) {

	  expr * left = (expr *) range->Left_expr();
	  std::string left_str = eval.prettyPrint((any *) left);
	  EXPECT_EQ(left_str,"SIZE - 1");

	  expr * right = (expr *) range->Right_expr();
	  std::string right_str = eval.prettyPrint((any *) right);
	  EXPECT_EQ(right_str,"0");
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

    VectorOfparam_assign *vpa = s->MakeParam_assignVec();
    
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

TEST(exprVal,prettyPrint_ConditionOp) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs_ConditionOp(&serializer);
  //serializer.Save("expr_reduce_test.uhdm");
  //const std::string before = designs_to_string(designs);
  //std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto pa : *m->Param_assigns()) {
      const any* rhs = pa->Rhs();
      std::string result = eval.prettyPrint( (any*) rhs);
      EXPECT_EQ(result,"b ? 1 : 3");
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

    VectorOfparam_assign *vpa = s->MakeParam_assignVec();
    
    param_assign* param = s->MakeParam_assign();
    vpa->push_back(param);
    dut->Param_assigns(vpa);

    parameter* p = s->MakeParameter();
    dut->Parameters()->push_back(p);
    p->VpiName("a");
    param->Lhs(p);

    sys_func_call* sfc  = s->MakeSys_func_call();
    param->Rhs(sfc);

    sfc->VpiName("$sformatf");


    VectorOfany * args = s->MakeAnyVec();
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

TEST(exprVal,prettyPrint_functionCall) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs_functionCall(&serializer);
  serializer.Save("expr_reduce_test.uhdm");
  const std::string before = designs_to_string(designs);
  std::cout << before <<std::endl;

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

  ExprEval eval;
  for (auto m : *d->TopModules()) {
    for (auto pa : *m->Param_assigns()) {
      const any* rhs = pa->Rhs();
      std::string result = eval.prettyPrint( (any*) rhs);
      std::string expected_result = "$sformatf(\"%d\",b)";
      std::cout << expected_result << std::endl;

      EXPECT_EQ(expected_result,result);
    }
  }

}





