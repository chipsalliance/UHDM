// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>

#include "gtest/gtest.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

#include "test_util.h"

using namespace UHDM;

// TODO: These tests are 'too big', i.e. they don't test a particular aspect
// of serialization.

static std::vector<vpiHandle> buildStatementDesign(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design3");
  module_inst* m1 = s->MakeModule_inst();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);
  module_inst* m2 = s->MakeModule_inst();
  m2->VpiDefName("M2");
  m2->VpiName("u1");
  m2->VpiFullName("M1.u1");
  m2->VpiParent(m1);
  m2->Instance(m1);
  m2->Module_inst(m1);
  m2->VpiFile("fake2.sv");
  m2->VpiLineNo(20);

  initial* init = s->MakeInitial();
  VectorOfprocess_stmt* processes = s->MakeProcess_stmtVec();
  processes->push_back(init);
  begin* begin_block = s->MakeBegin();
  init->Stmt(begin_block);
  VectorOfany* statements = s->MakeAnyVec();
  ref_obj* lhs_rf = s->MakeRef_obj();
  lhs_rf->VpiName("out");
  assignment* assign1 = s->MakeAssignment();
  assign1->Lhs(lhs_rf);
  constant* c1 = s->MakeConstant();
  c1->VpiValue("INT:0");
  assign1->Rhs(c1);
  statements->push_back(assign1);

  assignment* assign2 = s->MakeAssignment();
  assign2->Lhs(lhs_rf);
  constant* c2 = s->MakeConstant();
  c2->VpiValue("STRING:a string");
  assign2->Rhs(c2);
  statements->push_back(assign2);

  delay_control* dc = s->MakeDelay_control();
  dc->VpiDelay("#100");

  assignment* assign3 = s->MakeAssignment();
  assign3->Lhs(lhs_rf);
  constant* c3 = s->MakeConstant();
  s_vpi_value val;
  val.format = vpiIntVal;
  val.value.integer = 1;
  c3->VpiValue(VpiValue2String(&val));
  assign3->Rhs(c3);
  dc->Stmt(assign3);
  statements->push_back(dc);

  begin_block->Stmts(statements);
  m2->Process(processes);

  module_inst* m3 = s->MakeModule_inst();
  m3->VpiDefName("M3");
  m3->VpiName("u2");
  m3->VpiFullName("M1.u2");
  m3->VpiParent(m1);
  m3->Instance(m1);
  m3->Module_inst(m1);
  m3->VpiFile("fake3.sv");
  m3->VpiLineNo(30);
  VectorOfmodule_inst* v1 = s->MakeModule_instVec();
  v1->push_back(m1);
  d->AllModules(v1);
  VectorOfmodule_inst* v2 = s->MakeModule_instVec();
  v2->push_back(m2);
  v2->push_back(m3);
  m1->Modules(v2);
  package* p1 = s->MakePackage();
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = s->MakePackageVec();
  v3->push_back(p1);
  d->AllPackages(v3);
  designs.push_back(s->MakeUhdmHandle(uhdmdesign, d));

  return designs;
}

TEST(Serialization, SerializeStatementDesign_e2e) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = buildStatementDesign(&serializer);

  const std::string orig = designs_to_string(designs);

  const std::string filename =
      testing::TempDir() + "/serialize-statement-roundrip.uhdm";
  serializer.Save(filename);

  const std::vector<vpiHandle>& restoredDesigns = serializer.Restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);
  EXPECT_EQ(orig, restored);
}
