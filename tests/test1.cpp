#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

#include "test_helper.h"

std::vector<vpiHandle> build_designs () {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = designFactory::Make();
  d->VpiName("design1");
  module* m1 = moduleFactory::Make();
  m1->VpiTopModule(true);
  m1->VpiName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);
  module* m2 = moduleFactory::Make();
  m2->VpiName("M2");
  m2->VpiParent(m1);
  m2->VpiFile("fake2.sv");
  m2->VpiLineNo(20);
  module* m3 = moduleFactory::Make();
  m3->VpiName("M3");
  m3->VpiParent(m1);
  m3->VpiFile("fake3.sv");
  m3->VpiLineNo(30);
  VectorOfmodule* v1 = VectorOfmoduleFactory::Make();
  v1->push_back(m1);
  d->AllModules(v1);
  VectorOfmodule* v2 = VectorOfmoduleFactory::Make();
  v2->push_back(m2);
  v2->push_back(m3);
  m1->Modules(v2);
  package* p1 = packageFactory::Make();
  p1->VpiName("P1");
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = VectorOfpackageFactory::Make();
  v3->push_back(p1);
  d->AllPackages(v3);
  // Function
  function* f1 = functionFactory::Make();
  f1->VpiName("MyFunc1");
  f1->VpiSize(100);
  function* f2 = functionFactory::Make();
  f2->VpiName("MyFunc2");
  f2->VpiSize(200);
  VectorOftask_func* v4 = VectorOftask_funcFactory::Make();
  v4->push_back(f1);
  v4->push_back(f2);
  p1->Task_funcs(v4);
  // Instance items, illustrates the use of groups
  program* pr1 = programFactory::Make();
  pr1->VpiName("PR1");
  VectorOfany* inst_items = VectorOfanyFactory::Make();
  inst_items->push_back(pr1);
  function* f3 = functionFactory::Make();
  f3->VpiName("MyFunc3");
  f3->VpiSize(300);
  inst_items->push_back(f3);
  
  m1->Instance_items(inst_items);
  designs.push_back(uhdm_handleFactory::Make(uhdmdesign, d));
  return designs;
}

int main (int argc, char** argv) {
  std::cout << "Make design" << std::endl;
  std::string orig = print_designs(build_designs());
  std::cout << orig; 
  std::cout << "\nSave design" << std::endl;
  Serializer::Save("surelog.uhdm");
  
  std::cout << "Restore design" << std::endl;
  std::vector<vpiHandle> restoredDesigns = Serializer::Restore("surelog.uhdm");
  
  std::string restored = print_designs(restoredDesigns);
  std::cout << restored;
  return (orig != restored);
};
