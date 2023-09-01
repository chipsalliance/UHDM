#include "swig_test.h"
#include "uhdm.h"
using namespace UHDM;

std::vector<vpiHandle> buildTestDesign(Serializer* s) {
  std::vector<vpiHandle> designs;

  design* d = s->MakeDesign();
  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign,d);
  designs.push_back(dh);

  VectorOfmodule_inst* vm = s->MakeModule_instVec();
  d->AllModules(vm);

  module_inst* m1 = s->MakeModule_inst();
  m1->VpiName("module1");
  vm->push_back(m1);

  module_inst* m2 = s->MakeModule_inst();
  m2->VpiName("module2");
  vm->push_back(m2);

  return designs;
}

std::vector<vpiHandle> buildTestTypedef(UHDM::Serializer* s){
  std::vector<vpiHandle> designs;

  design* d = s->MakeDesign();
  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign,d);
  designs.push_back(dh);

  VectorOftypespec* typespecs = s->MakeTypespecVec();
  d->Typespecs(typespecs);

  struct_typespec* typespec1 = s->MakeStruct_typespec();
  typespec1->VpiName("IR");
  typespecs->push_back(typespec1);

  VectorOftypespec_member* members = s->MakeTypespec_memberVec();
  typespec1->Members(members);

  typespec_member* member1 = s->MakeTypespec_member();
  member1->VpiName("opcode");
  member1->VpiParent(typespec1);
  members->push_back(member1);

  bit_typespec* btps = s->MakeBit_typespec();

  ref_obj* btps_ro = s->MakeRef_obj();
  btps_ro->Actual_group(btps);
  member1->Typespec(btps_ro);

  VectorOfrange* ranges = s->MakeRangeVec();
  btps->VpiParent(member1);
  btps->Ranges(ranges);
  range* range = s->MakeRange();
  range->VpiParent(btps);
  ranges->push_back(range);

  constant* c1 = s->MakeConstant();
  c1->VpiParent(range);
  c1->VpiValue("UNIT:7");
  c1->VpiConstType(vpiUIntConst);
  c1->VpiDecompile("7");
  c1->VpiSize(64);
  range->Left_expr(c1);
  constant* c2 = s->MakeConstant();
  c2->VpiParent(range);
  c2->VpiValue("UNIT:0");
  c2->VpiConstType(vpiUIntConst);
  c2->VpiDecompile("0");
  c2->VpiSize(64);
  range->Right_expr(c2);

  typespec_member* member2 = s->MakeTypespec_member();
  member2->VpiName("addr");
  member2->VpiParent(typespec1);
  members->push_back(member2);

  btps = s->MakeBit_typespec();

  btps_ro = s->MakeRef_obj();
  btps_ro->Actual_group(btps);
  member2->Typespec(btps_ro);

  ranges = s->MakeRangeVec();
  btps->VpiParent(member2);
  btps->Ranges(ranges);
  range = s->MakeRange();
  range->VpiParent(btps);
  ranges->push_back(range);


  ranges = s->MakeRangeVec();
  btps->Ranges(ranges);
  range = s->MakeRange();
  ranges->push_back(range);

  c1 = s->MakeConstant();
  c1->VpiParent(range);
  c1->VpiValue("UNIT:23");
  c1->VpiConstType(vpiUIntConst);
  c1->VpiDecompile("23");
  //c1->VpiSize(64);
  range->Left_expr(c1);
  c2 = s->MakeConstant();
  c2->VpiParent(range);
  c2->VpiValue("UNIT:0");
  c2->VpiConstType(vpiUIntConst);
  c2->VpiDecompile("0");
  //c2->VpiSize(64);
  range->Right_expr(c2);



  return designs;
}
// debug
/*
void visit_designs(const std::vector<vpiHandle>& designs) {
  visit_designs(designs, std::cout);
}
*/
