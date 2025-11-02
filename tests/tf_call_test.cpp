#include <iostream>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

static std::vector<vpiHandle> build_tfCallDesign(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("designTF");
  Module* m1 = s->make<Module>();
  m1->setTopModule(true);
  m1->setDefName("M1");
  m1->setParent(d);
  m1->setFile("fake1.sv");
  m1->setStartLine(10);

  Initial* init = s->make<Initial>();
  ProcessCollection* processes = s->makeCollection<Process>();
  //processes->push_back(init);
  init->setParent(m1);
  Begin* begin_block = s->make<Begin>();
  init->setStmt(begin_block);
  begin_block->setParent(init);
  AnyCollection* statements = s->makeCollection<Any>();

  SysFuncCall* display = s->make<SysFuncCall>();
  display->setName("display");
  display->setParent(begin_block);
  AnyCollection* arguments = s->makeCollection<Any>();
  Constant* cA = s->make<Constant>();
  cA->setValue("INT:0");
  cA->setParent(display);
  arguments->push_back(cA);
  Constant* cA1 = s->make<Constant>();
  cA1->setValue("INT:8");
  cA1->setParent(display);

  arguments->push_back(cA1);
  display->setArguments(arguments);
  statements->push_back(display);

  FuncCall* my_func_call = s->make<FuncCall>();
  Function* my_func = s->make<Function>();
  my_func->setName("a_func");
  my_func_call->setTaskFunc(my_func);
  my_func->setParent(begin_block);
  AnyCollection* arguments2 = s->makeCollection<Any>();
  Constant* cA2 = s->make<Constant>();
  cA2->setValue("INT:1");
  arguments2->push_back(cA2);
  cA2->setParent(my_func);
  Constant* cA3 = s->make<Constant>();
  cA3->setValue("INT:2");
  arguments2->push_back(cA3);
  cA3->setParent(my_func);
  my_func_call->setArguments(arguments2);

  statements->push_back(my_func);

  begin_block->setStmts(statements);
  m1->setProcesses(processes);

  ModuleCollection* v1 = s->makeCollection<Module>();
  v1->push_back(m1);
  d->setAllModules(v1);

  Package* p1 = s->make<Package>();
  p1->setDefName("P0");
  p1->setParent(d);
  PackageCollection* v3 = s->makeCollection<Package>();
  v3->push_back(p1);
  d->setAllPackages(v3);
  designs.push_back(s->makeUhdmHandle(UhdmType::Design, d));
  return designs;
}

TEST(Serialization, TFCallDesign) {
  Serializer serializer;
  const std::string orig = designs_to_string(build_tfCallDesign(&serializer));
  const std::string filename = testing::TempDir() + "/surelog_tf_call.uhdm";
  serializer.save(filename);

  std::vector<vpiHandle> restoredDesigns = serializer.restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);
  EXPECT_EQ(orig, restored);
}
