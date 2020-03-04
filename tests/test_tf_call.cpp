#include <iostream>

#include "headers/uhdm.h"
#include "headers/vpi_visitor.h"

using namespace UHDM;

#include "vpi_visitor.h"

std::vector<vpiHandle> build_designs (Serializer& s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s.MakeDesign();
  d->VpiName("designTF");
  module* m1 = s.MakeModule();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);

  initial* init = s.MakeInitial();
  VectorOfprocess* processes = s.MakeProcessVec();
  processes->push_back(init);
  begin* begin_block = s.MakeBegin();
  init->Stmt(begin_block);
  VectorOfany* statements = s.MakeAnyVec();

  sys_func_call* display = s.MakeSys_func_call();
  display->VpiName("display");
  VectorOfany *arguments = s.MakeAnyVec();
  constant* cA = s.MakeConstant();
  cA->VpiValue("INT:0");
  arguments->push_back(cA);
  constant* cA1 = s.MakeConstant();
  cA1->VpiValue("INT:8");
  arguments->push_back(cA1);
  display->Tf_call_args(arguments);
  statements->push_back(display);

  begin_block->Stmts(statements);
  m1->Process(processes);

  VectorOfmodule* v1 = s.MakeModuleVec();
  v1->push_back(m1);
  d->AllModules(v1);

  package* p1 = s.MakePackage();
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = s.MakePackageVec();
  v3->push_back(p1);
  d->AllPackages(v3);
  designs.push_back(s.MakeUhdmHandle(uhdmdesign, d));
  return designs;
}

int main (int argc, char** argv) {
  std::cout << "Make design" << std::endl;
  Serializer serializer;

  std::string orig = visit_designs(build_designs(serializer));

  std::cout << orig;
  std::cout << "\nSave design" << std::endl;
  serializer.Save("surelog3.uhdm");

  std::cout << "Restore design" << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore("surelog3.uhdm");

  std::string restored = visit_designs(restoredDesigns);
  std::cout << restored;
  return (orig != restored);
}
