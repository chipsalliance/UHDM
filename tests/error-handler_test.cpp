#include <iostream>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>
#include <uhdm/vpi_visitor.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace UHDM;
using testing::HasSubstr;

static std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design3");
  module* m1 = s->MakeModule();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);
  module* m2 = s->MakeModule();
  m2->VpiDefName("M2");
  m2->VpiName("u1");
  m2->VpiFullName("M1.u1");
  m2->VpiParent(m1);
  m2->Instance(m1);
  m2->Module(m1);
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
  assign1->Rhs(m1);  // Triggers error handler!
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

  module* m3 = s->MakeModule();
  m3->VpiDefName("M3");
  m3->VpiName("u2");
  m3->VpiFullName("M1.u2");
  m3->VpiParent(m1);
  m3->Instance(m1);
  m3->Module(m1);
  m3->VpiFile("fake3.sv");
  m3->VpiLineNo(30);
  VectorOfmodule* v1 = s->MakeModuleVec();
  v1->push_back(m1);
  d->AllModules(v1);
  VectorOfmodule* v2 = s->MakeModuleVec();
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

TEST(ErrorHandlerTest, ErrorHandlerIsCalled) {
  Serializer serializer;

  // Install a customer error handler
  bool issuedError = false;
  std::string last_msg;
  UHDM::ErrorHandler MyErrorHandler = [&](ErrorType errType,
                                          const std::string& msg, any* object) {
    last_msg = msg;
    issuedError = true;
  };
  serializer.SetErrorHandler(MyErrorHandler);

  issuedError = false;
  const std::string before = visit_designs(build_designs(&serializer));

  EXPECT_TRUE(issuedError);  // Issue in design.
  EXPECT_THAT(last_msg, HasSubstr("adding wrong object type"));

  issuedError = false;
  const std::string filename = testing::TempDir() + "/error-handler_test.uhdm";
  serializer.Save(filename);

  EXPECT_FALSE(issuedError);

  // Save/restore will not contain the errornous part.
  issuedError = false;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore(filename);
  const std::string restored = visit_designs(restoredDesigns);
  EXPECT_FALSE(issuedError);

  EXPECT_EQ(before, restored);
}
