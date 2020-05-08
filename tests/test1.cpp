// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "headers/uhdm.h"
#include "headers/vpi_visitor.h"

#include <iostream>

using namespace UHDM;

class MyPayLoad : public ClientData {
public:
  MyPayLoad(int f) { foo_ = f; }
private:
  int foo_;
};

std::vector<vpiHandle> build_designs (Serializer& s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s.MakeDesign();
  d->VpiName("design1");
  // Module
  module* m1 = s.MakeModule();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);
  // Module
  module* m2 = s.MakeModule();
  m2->VpiDefName("M2");
  m2->VpiName("u1");
  m2->VpiParent(m1);
  m2->VpiFile("fake2.sv");
  m2->VpiLineNo(20);
  // Ports
  VectorOfport* vp = s.MakePortVec();
  port* p = s.MakePort();
  p->VpiName("i1");
  p->VpiDirection(vpiInput);
  vp->push_back(p);
  p = s.MakePort();
  p->VpiName("o1");
  p->VpiDirection(vpiOutput);
  vp->push_back(p);
  m2->Ports(vp);
  // Module
  module* m3 = s.MakeModule();
  m3->VpiDefName("M3");
  m3->VpiName("u2");
  m3->VpiParent(m1);
  m3->VpiFile("fake3.sv");
  m3->VpiLineNo(30);
  // Instance
  module* m4 = s.MakeModule();
  m4->VpiDefName("M4");
  m4->VpiName("u3");
  m4->Ports(vp);
  m4->VpiParent(m3);
  m4->Instance(m3);
  VectorOfmodule* v1 = s.MakeModuleVec();
  v1->push_back(m1);
  d->AllModules(v1);
  VectorOfmodule* v2 = s.MakeModuleVec();
  v2->push_back(m2);
  v2->push_back(m3);
  m1->Modules(v2);
  // Package
  package* p1 = s.MakePackage();
  p1->VpiName("P1");
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = s.MakePackageVec();
  v3->push_back(p1);
  d->AllPackages(v3);
  // Function
  function* f1 = s.MakeFunction();
  f1->VpiName("MyFunc1");
  f1->VpiSize(100);
  function* f2 = s.MakeFunction();
  f2->VpiName("MyFunc2");
  f2->VpiSize(200);
  VectorOftask_func* v4 = s.MakeTask_funcVec();
  v4->push_back(f1);
  v4->push_back(f2);
  p1->Task_funcs(v4);
  // Instance items, illustrates the use of groups
  program* pr1 = s.MakeProgram();
  pr1->VpiDefName("PR1");
  pr1->VpiParent(m1);
  VectorOfany* inst_items = s.MakeAnyVec();
  inst_items->push_back(pr1);
  function* f3 = s.MakeFunction();
  f3->VpiName("MyFunc3");
  f3->VpiSize(300);
  f3->VpiParent(m1);
  inst_items->push_back(f3);
  m1->Instance_items(inst_items);

  MyPayLoad* pl = new MyPayLoad(10);
  m1->Data(pl);
  
  vpiHandle dh = s.MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  char name[]{"P1"};
  vpiHandle obj_h = vpi_handle_by_name(name, dh);
  if (obj_h == 0) {
    exit(1);
  } else {
    char name[]{"MyFunc1"};
    vpiHandle obj_h1 = vpi_handle_by_name(name, obj_h);
    if (obj_h1 == 0) {
      exit(1);
    }
  }
  
  return designs;
}

int main (int argc, char** argv) {
  std::cout << "Make design" << std::endl;
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(serializer);
  std::string orig;
  orig += "VISITOR:\n";
  orig += visit_designs(designs);
  std::cout << orig;
  std::cout << "\nSave design" << std::endl;
  serializer.Save("surelog.uhdm");

  std::cout << "Restore design" << std::endl;
  const std::vector<vpiHandle>& restoredDesigns = serializer.Restore("surelog.uhdm");
  std::string restored;
  restored += "VISITOR:\n";
  restored += visit_designs(restoredDesigns);
  std::cout << restored;

  return (orig != restored);
}
