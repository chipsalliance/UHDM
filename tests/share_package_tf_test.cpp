// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

// Serializer::ForEachObject and ElaboratorListener::sharePackageTaskFuncs.

#include <vector>

#include "gtest/gtest.h"
#include "uhdm/ElaboratorListener.h"
#include "uhdm/uhdm.h"

using namespace UHDM;

namespace {

struct Design {
  std::vector<vpiHandle> handles;
  package* pkg = nullptr;
  module_inst* inst1 = nullptr;
  module_inst* inst2 = nullptr;
};

// package P; function f; endpackage
// module M2; import P::*; endmodule
// module M1; M2 inst1(); M2 inst2(); endmodule
Design build(Serializer* s) {
  Design r;
  design* d = s->MakeDesign();
  d->VpiName("design1");

  r.pkg = s->MakePackage();
  r.pkg->VpiName("P");
  r.pkg->VpiDefName("P");
  r.pkg->VpiParent(d);
  function* f = s->MakeFunction();
  f->VpiName("f");
  f->VpiParent(r.pkg);
  VectorOftask_func* ptf = s->MakeTask_funcVec();
  ptf->push_back(f);
  r.pkg->Task_funcs(ptf);
  VectorOfpackage* pkgs = s->MakePackageVec();
  pkgs->push_back(r.pkg);
  d->AllPackages(pkgs);

  module_inst* m1 = s->MakeModule_inst();
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  module_inst* m2 = s->MakeModule_inst();
  m2->VpiDefName("M2");
  m2->VpiParent(d);
  VectorOfmodule_inst* all = s->MakeModule_instVec();
  all->push_back(m1);
  all->push_back(m2);
  d->AllModules(all);

  module_inst* top = s->MakeModule_inst();
  top->VpiDefName("M1");
  top->VpiName("M1");
  top->VpiTopModule(true);
  top->VpiParent(d);
  VectorOfmodule_inst* kids = s->MakeModule_instVec();
  top->Modules(kids);
  for (const char* n : {"inst1", "inst2"}) {
    module_inst* i = s->MakeModule_inst();
    i->VpiDefName("M2");
    i->VpiName(n);
    i->VpiFullName(std::string("M1.") + n);
    i->VpiParent(top);
    // The imported package function, as Surelog lists it on the instance.
    VectorOftask_func* itf = s->MakeTask_funcVec();
    itf->push_back(f);
    i->Task_funcs(itf);
    kids->push_back(i);
    (r.inst1 ? r.inst2 : r.inst1) = i;
  }
  VectorOfmodule_inst* tops = s->MakeModule_instVec();
  tops->push_back(top);
  d->TopModules(tops);

  r.handles.push_back(s->MakeUhdmHandle(uhdmdesign, d));
  return r;
}

const task_func* only_tf(const module_inst* m) {
  if (m->Task_funcs() == nullptr || m->Task_funcs()->size() != 1)
    return nullptr;
  return m->Task_funcs()->front();
}

void elaborate(Serializer* s, Design& r, bool share) {
  ElaboratorContext* ctx = new ElaboratorContext(s, false, false);
  ctx->m_elaborator.sharePackageTaskFuncs(share);
  ctx->m_elaborator.listenDesigns(r.handles);
  delete ctx;
}

}  // namespace

TEST(ForEachObjectTest, VisitsExactlyOneType) {
  Serializer s;
  Design r = build(&s);
  int modules = 0, functions = 0, packages = 0;
  s.ForEachObject(uhdmmodule_inst, [&](BaseClass* o) {
    EXPECT_EQ(o->UhdmType(), uhdmmodule_inst);
    ++modules;
  });
  s.ForEachObject(uhdmfunction, [&](BaseClass*) { ++functions; });
  s.ForEachObject(uhdmpackage, [&](BaseClass*) { ++packages; });
  EXPECT_EQ(modules, 5);  // M1, M2 definitions + M1, inst1, inst2
  EXPECT_EQ(functions, 1);
  EXPECT_EQ(packages, 1);
  int none = 0;
  s.ForEachObject(uhdmtask, [&](BaseClass*) { ++none; });
  EXPECT_EQ(none, 0);
}

TEST(SharePackageTaskFuncsTest, OffClonesPerInstance) {
  Serializer s;
  Design r = build(&s);
  elaborate(&s, r, false);
  const task_func* a = only_tf(r.inst1);
  const task_func* b = only_tf(r.inst2);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(a, b);
}

TEST(SharePackageTaskFuncsTest, OnSharesThePackageCopy) {
  Serializer s;
  Design r = build(&s);
  elaborate(&s, r, true);
  const task_func* a = only_tf(r.inst1);
  const task_func* b = only_tf(r.inst2);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a, b);
  ASSERT_NE(r.pkg->Task_funcs(), nullptr);
  ASSERT_EQ(r.pkg->Task_funcs()->size(), 1u);
  EXPECT_EQ(a, r.pkg->Task_funcs()->front());
  EXPECT_EQ(a->VpiParent(), r.pkg);
}
