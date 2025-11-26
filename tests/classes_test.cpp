// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#include <iostream>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/Elaborator.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

static std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;

  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  // Module
  Module* m1 = s->make<Module>();
  m1->setTopModule(true);
  m1->setDefName("M1");
  m1->setParent(d);
  m1->setFile("fake1.sv");
  m1->setStartLine(10);

  /* Base class */
  ClassDefn* base = s->make<ClassDefn>();
  base->setName("Base");
  base->setParent(m1);

  Parameter* param = s->make<Parameter>();
  param->setName("P1");
  param->setParent(base);

  Function* f1 = s->make<Function>();
  f1->setName("f1");
  f1->setMethod(true);
  f1->setParent(base);

  AssignStmt* as = s->make<AssignStmt>();
  f1->setStmt(as);
  as->setParent(f1);

  RefObj* lhs = s->make<RefObj>();
  lhs->setName("a");
  lhs->setParent(as);

  RefObj* rhs = s->make<RefObj>();
  rhs->setName("P1");
  rhs->setParent(as);
  as->setLhs(lhs);
  as->setRhs(rhs);

  Function* f2 = s->make<Function>();
  f2->setName("f2");
  f2->setMethod(true);
  f2->setParent(base);

  MethodFuncCall* fcall = s->make<MethodFuncCall>();
  f2->setStmt(fcall);
  fcall->setName("f1");
  fcall->setParent(f2);

  /* Child class */
  ClassDefn* child = s->make<ClassDefn>();
  child->setName("Child");
  child->setParent(m1);

  ClassDefn* derived = child;
  ClassDefn* parent = base;
  Extends* extends = s->make<Extends>();
  ClassTypespec* tps = s->make<ClassTypespec>();
  RefTypespec* rt = s->make<RefTypespec>();
  tps->setParent(child);
  rt->setActual(tps);
  rt->setParent(extends);
  extends->setParent(child);
  extends->setClassTypespec(rt);
  tps->setClassDefn(parent);
  derived->setExtends(extends);

  parent->getDerivedClasses(true)->emplace_back(derived);

  Function* f3 = s->make<Function>();
  f3->setName("f3");
  f3->setMethod(true);
  f3->setParent(child);

  MethodFuncCall* fcall2 = s->make<MethodFuncCall>();
  f3->setStmt(fcall);
  fcall2->setName("f1");  // parent class function
  fcall->setParent(f3);

  d->getTopModules(true)->emplace_back(m1);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.emplace_back(dh);
  return designs;
}

TEST(ClassesTest, DesignSaveRestoreRoundtrip) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  const std::string before = designs_to_string(designs);

  const std::string filename = testing::TempDir() + "/classes_test.uhdm";
  serializer.save(filename);

  const std::vector<vpiHandle>& restoredDesigns = serializer.restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);

  EXPECT_EQ(before, restored);

  // Elaborate restored designs
  Elaborator elaborator(&serializer);
  elaborator.listenDesigns(restoredDesigns);

  const std::string elaborated = designs_to_string(restoredDesigns);
  EXPECT_NE(restored, elaborated);  // Elaboration should've done _something_
}
