// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#include <iostream>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/ElaboratorListener.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace UHDM;

static std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");
  // Module
  module_inst* m1 = s->MakeModule_inst();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);
  VectorOfmodule_inst* v1 = s->MakeModule_instVec();
  v1->push_back(m1);
  d->AllModules(v1);

  VectorOfclass_defn* classes = s->MakeClass_defnVec();
  m1->Class_defns(classes);

  /* Base class */
  class_defn* base = s->MakeClass_defn();
  base->VpiName("Base");
  base->VpiParent(m1);
  classes->push_back(base);

  parameter* param = s->MakeParameter();
  param->VpiName("P1");
  VectorOfany* params = s->MakeAnyVec();
  params->push_back(param);
  base->Parameters(params);

  VectorOftask_func* funcs = s->MakeTask_funcVec();
  base->Task_funcs(funcs);

  function* f1 = s->MakeFunction();
  f1->VpiName("f1");
  f1->VpiMethod(true);
  funcs->push_back(f1);
  assign_stmt* as = s->MakeAssign_stmt();
  f1->Stmt(as);
  ref_obj* lhs = s->MakeRef_obj();
  lhs->VpiName("a");
  ref_obj* rhs = s->MakeRef_obj();
  rhs->VpiName("P1");
  as->Lhs(lhs);
  as->Rhs(rhs);

  function* f2 = s->MakeFunction();
  f2->VpiName("f2");
  f2->VpiMethod(true);
  funcs->push_back(f2);
  method_func_call* fcall = s->MakeMethod_func_call();
  f2->Stmt(fcall);
  fcall->VpiName("f1");

  /* Child class */
  class_defn* child = s->MakeClass_defn();
  child->VpiName("Child");
  child->VpiParent(m1);
  classes->push_back(child);

  UHDM::class_defn* derived = child;
  UHDM::class_defn* parent = base;
  UHDM::extends* extends = s->MakeExtends();
  UHDM::class_typespec* tps = s->MakeClass_typespec();
  UHDM::ref_obj* ro = s->MakeRef_obj();
  ro->Actual_group(tps);
  ro->VpiParent(extends);
  extends->Class_typespec(ro);
  tps->Class_defn(parent);
  derived->Extends(extends);
  UHDM::VectorOfclass_defn* all_derived = s->MakeClass_defnVec();
  parent->Deriveds(all_derived);
  all_derived->push_back(derived);

  VectorOftask_func* funcs2 = s->MakeTask_funcVec();
  child->Task_funcs(funcs2);

  function* f3 = s->MakeFunction();
  f3->VpiName("f3");
  f3->VpiMethod(true);
  funcs2->push_back(f3);
  method_func_call* fcall2 = s->MakeMethod_func_call();
  f3->Stmt(fcall);
  fcall2->VpiName("f1");  // parent class function

  VectorOfmodule_inst* topModules = s->MakeModule_instVec();
  d->TopModules(topModules);
  topModules->push_back(m1);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);
  return designs;
}

TEST(ClassesTest, DesignSaveRestoreRoundtrip) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  const std::string before = designs_to_string(designs);

  const std::string filename = testing::TempDir() + "/classes_test.uhdm";
  serializer.Save(filename);

  const std::vector<vpiHandle>& restoredDesigns = serializer.Restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);

  EXPECT_EQ(before, restored);

  // Elaborate restored designs
  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(restoredDesigns);
  delete elaboratorContext;

  const std::string elaborated = designs_to_string(restoredDesigns);
  EXPECT_NE(restored, elaborated);  // Elaboration should've done _something_
}
