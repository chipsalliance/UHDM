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

std::vector<vpiHandle> build_designs(Serializer* s) {
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


TEST(exprVal,prettyPrint) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  serializer.Save("expr_reduce_test.uhdm");
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
	  EXPECT_EQ(left_str,"SIZE-1");

	  expr * right = (expr *) range->Right_expr();
	  std::string right_str = eval.prettyPrint((any *) right);
	  EXPECT_EQ(right_str,"0");
	}

    }
  }

}
