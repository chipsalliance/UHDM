#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

#include "test_helper.h"

std::vector<vpiHandle> build_designs () {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = designFactory::make();
  d->set_vpiName("design1");
  module* m1 = moduleFactory::make();
  m1->set_vpiTopModule(true);
  m1->set_vpiName("M1");
  m1->set_vpiParent(d);
  m1->set_uhdmParentType(uhdmdesign);
  m1->set_vpiFile("fake1.sv");
  m1->set_vpiLineNo(10);
  module* m2 = moduleFactory::make();
  m2->set_vpiName("M2");
  m2->set_vpiParent(m1);
  m2->set_uhdmParentType(uhdmmodule);
  m2->set_vpiFile("fake2.sv");
  m2->set_vpiLineNo(20);
  module* m3 = moduleFactory::make();
  m3->set_vpiName("M3");
  m3->set_vpiParent(m1);
  m3->set_uhdmParentType(uhdmmodule);
  m3->set_vpiFile("fake3.sv");
  m3->set_vpiLineNo(30);
  VectorOfmodule* v1 = VectorOfmoduleFactory::make();
  v1->push_back(m1);
  d->set_allModules(v1);
  VectorOfmodule* v2 = VectorOfmoduleFactory::make();
  v2->push_back(m2);
  v2->push_back(m3);
  m1->set_modules(v2);
  designs.push_back(uhdm_handleFactory::make(uhdmdesign, d));
  return designs;
}

int main (int argc, char** argv) {
  std::cout << "Make design" << std::endl;
  std::string orig = print_designs(build_designs());
  std::cout << orig; 
  std::cout << "\nSave design" << std::endl;
  Serializer::save("design.uhdmap");
  
  std::cout << "Restore design" << std::endl;
  std::vector<vpiHandle> restoredDesigns = Serializer::restore("design.uhdmap");
  
  std::string restored = print_designs(restoredDesigns);
  std::cout << restored;
  return (orig != restored);
};
