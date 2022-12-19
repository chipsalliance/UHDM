#include <iostream>

#include "gtest/gtest.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

#include "test_util.h"

using namespace UHDM;

static std::vector<vpiHandle> build_tfCallDesign(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("designTF");
  module_inst* m1 = s->MakeModule_inst();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);

  initial* init = s->MakeInitial();
  VectorOfprocess_stmt* processes = s->MakeProcess_stmtVec();
  processes->push_back(init);
  begin* begin_block = s->MakeBegin();
  init->Stmt(begin_block);
  VectorOfany* statements = s->MakeAnyVec();

  sys_func_call* display = s->MakeSys_func_call();
  display->VpiName("display");
  VectorOfany* arguments = s->MakeAnyVec();
  constant* cA = s->MakeConstant();
  cA->VpiValue("INT:0");
  arguments->push_back(cA);
  constant* cA1 = s->MakeConstant();
  cA1->VpiValue("INT:8");
  arguments->push_back(cA1);
  display->Tf_call_args(arguments);
  statements->push_back(display);

  func_call* my_func_call = s->MakeFunc_call();
  function* my_func = s->MakeFunction();
  my_func->VpiName("a_func");
  my_func_call->Function(my_func);
  VectorOfany* arguments2 = s->MakeAnyVec();
  constant* cA2 = s->MakeConstant();
  cA2->VpiValue("INT:1");
  arguments2->push_back(cA2);
  constant* cA3 = s->MakeConstant();
  cA3->VpiValue("INT:2");
  arguments2->push_back(cA3);
  my_func_call->Tf_call_args(arguments2);
  statements->push_back(my_func_call);

  begin_block->Stmts(statements);
  m1->Process(processes);

  VectorOfmodule_inst* v1 = s->MakeModule_instVec();
  v1->push_back(m1);
  d->AllModules(v1);

  package* p1 = s->MakePackage();
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = s->MakePackageVec();
  v3->push_back(p1);
  d->AllPackages(v3);
  designs.push_back(s->MakeUhdmHandle(uhdmdesign, d));
  return designs;
}

TEST(Serialization, TFCallDesign) {
  Serializer serializer;
  const std::string orig = designs_to_string(build_tfCallDesign(&serializer));
  const std::string filename = testing::TempDir() + "/surelog_tf_call.uhdm";
  serializer.Save(filename);

  std::vector<vpiHandle> restoredDesigns = serializer.Restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);
  EXPECT_EQ(orig, restored);
}
