// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "uhdm/Cloner.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/UhdmComparer.h"
#include "uhdm/uhdm.h"

using namespace uhdm;
using testing::ElementsAre;

static Design* buildModuleProg(Serializer* s) {
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  ModuleCollection* const allModules = d->getAllModules(true);
  PackageCollection* const allPackages = d->getAllPackages(true);
  ProgramCollection* const allPrograms = d->getAllPrograms(true);

  // Module
  Module* const m1 = s->make<Module>();
  m1->setTopModule(true);
  m1->setDefName("M1");
  m1->setFullName("top::M1");
  m1->setParent(d);
  allModules->emplace_back(m1);

  // Module
  Module* const m2 = s->make<Module>();
  m2->setDefName("M2");
  m2->setName("u1");
  m2->setParent(m1);
  allModules->emplace_back(m2);

  // Module
  Module* const m3 = s->make<Module>();
  m3->setDefName("M3");
  m3->setName("u2");
  m3->setParent(m1);
  allModules->emplace_back(m3);

  // Instance
  Module* const m4 = s->make<Module>();
  m4->setDefName("M4");
  m4->setName("u3");
  m3->setParent(m3);
  m4->setInstance(m3);
  allModules->emplace_back(m4);

  // Module
  Module* const m5 = s->make<Module>();
  m5->setDefName("M5");
  m5->setFullName("top::M1");
  m5->setParent(d);
  m5->setTopModule(true);
  allModules->emplace_back(m5);

  // Package
  Package* const p1 = s->make<Package>();
  p1->setName("P1");
  p1->setDefName("P0");
  p1->setParent(d);
  allPackages->emplace_back(p1);

  // Instance items, illustrates the use of groups
  Program* pr1 = s->make<Program>();
  pr1->setDefName("PR1");
  pr1->setParent(d);
  allPrograms->emplace_back(pr1);
  return d;
}

TEST(ClonerTest, Default) {
  Serializer serializer;
  Design* const source = buildModuleProg(&serializer);

  const size_t countBeforeClone = serializer.getAllObjects().size();

  Cloner cloner(&serializer);
  Design* const target = cloner.clone<>(source, nullptr);

  const size_t countAfterCloning = serializer.getAllObjects().size();

  EXPECT_GE(countAfterCloning, countBeforeClone * 2);

  UhdmComparer comparer;
  EXPECT_EQ(comparer.compare(source, target), 0);
}

TEST(ClonerTest, PassThroughTest) {
  Serializer serializer;
  Design* const source = buildModuleProg(&serializer);

  const size_t countBeforeClone = serializer.getAllObjects().size();

  PassThroughCloner cloner(&serializer);
  Design* const target = cloner.clone<>(source, nullptr);

  const size_t countAfterCloning = serializer.getAllObjects().size();

  EXPECT_EQ(countBeforeClone, countAfterCloning);

  UhdmComparer comparer;
  EXPECT_EQ(comparer.compare(source, target), 0);
}

TEST(ClonerTest, Mirrored) {
  Serializer serializer;
  Design* const source = buildModuleProg(&serializer);

  const size_t countBeforeClone = serializer.getAllObjects().size();

  MirrorCloner cloner(&serializer);
  Design* const target = cloner.clone<>(source, nullptr);

  const size_t countAfterCloning = serializer.getAllObjects().size();

  EXPECT_EQ(countAfterCloning, countBeforeClone * 2);

  // UhdmComparer comparer;
  // EXPECT_EQ(comparer.compare(source, target), 0);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
