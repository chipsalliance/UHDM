// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "uhdm/vpi_listener.h"

#include <iostream>
#include <memory>
#include <stack>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "uhdm/uhdm.h"

using namespace UHDM;
using testing::ElementsAre;

class MyVpiListener : public VpiListener {
 protected:
  void enterModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    CollectLine("Module", object, parentHandle);
    stack_.push(object);
  }

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

  void enterProgram(const program* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override {
    CollectLine("Program", object, parentHandle);
    stack_.push(object);
  }

  void leaveProgram(const program* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

 public:
  void CollectLine(std::string_view prefix, const BaseClass* object,
                   vpiHandle parentHandle) {
    const char* const parentName = vpi_get_str(vpiName, parentHandle);
    collected_.push_back(
        std::string(prefix)
            .append(": ")
            .append(object->VpiName())
            .append("/")
            .append(object->VpiDefName())
            .append(" parent: ")
            .append(((parentName != nullptr) ? parentName : "-")));
  }

  const std::vector<std::string>& collected() const { return collected_; }

 private:
  std::vector<std::string> collected_;
  std::stack<const BaseClass*> stack_;
};

static std::vector<vpiHandle> buildModuleProg(Serializer* s) {
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");
  // Module
  module* m1 = s->MakeModule();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiFullName("top::M1");
  m1->VpiParent(d);

  // Module
  module* m2 = s->MakeModule();
  m2->VpiDefName("M2");
  m2->VpiName("u1");
  m2->VpiParent(m1);

  // Module
  module* m3 = s->MakeModule();
  m3->VpiDefName("M3");
  m3->VpiName("u2");
  m3->VpiParent(m1);

  // Instance
  module* m4 = s->MakeModule();
  m4->VpiDefName("M4");
  m4->VpiName("u3");
  m4->VpiParent(m3);
  m4->Instance(m3);

  VectorOfmodule* v1 = s->MakeModuleVec();
  v1->push_back(m1);
  d->AllModules(v1);

  VectorOfmodule* v2 = s->MakeModuleVec();
  v2->push_back(m2);
  v2->push_back(m3);
  m1->Modules(v2);

  // Package
  package* p1 = s->MakePackage();
  p1->VpiName("P1");
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = s->MakePackageVec();
  v3->push_back(p1);
  d->AllPackages(v3);

  // Instance items, illustrates the use of groups
  program* pr1 = s->MakeProgram();
  pr1->VpiDefName("PR1");
  pr1->VpiParent(m1);
  VectorOfany* inst_items = s->MakeAnyVec();
  inst_items->push_back(pr1);
  m1->Instance_items(inst_items);

  return {s->MakeUhdmHandle(uhdmdesign, d)};
}

TEST(VpiListenerTest, ProgramModule) {
  Serializer serializer;
  const std::vector<vpiHandle>& design = buildModuleProg(&serializer);

  std::unique_ptr<MyVpiListener> listener(new MyVpiListener());
  listen_designs(design, listener.get());
  const std::vector<std::string> expected = {
      "Module: /M1 parent: design1",
      "Module: u1/M2 parent: -",
      "Module: u2/M3 parent: -",
      "Program: /PR1 parent: -",
  };
  EXPECT_EQ(listener->collected(), expected);
}
