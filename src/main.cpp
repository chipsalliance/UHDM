#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

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
  module* m2 = moduleFactory::make();
  m2->set_vpiName("M2");
  m2->set_vpiParent(m1);
  m2->set_uhdmParentType(uhdmmodule);
  module* m3 = moduleFactory::make();
  m3->set_vpiName("M3");
  m3->set_vpiParent(m1);
  m3->set_uhdmParentType(uhdmmodule);
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

std::string print_designs (std::vector<vpiHandle> designs) {
  std::string result;
  for (auto restoredDesign : designs) {
    result += "Design name: " + std::string(vpi_get_str(vpiName, restoredDesign)) + "\n";
    
    // VPI test
    vpiHandle modItr = vpi_iterate(uhdmallModules,restoredDesign); 
    while (vpiHandle obj_h = vpi_scan(modItr) ) {
      result +=  "mod:" + std::string(vpi_get_str(vpiName, obj_h))
	+ ", top:" 
	+ std::to_string(vpi_get(vpiTopModule, obj_h))
	+ ", parent:" + std::string(vpi_get_str(vpiName, vpi_handle(vpiParent, obj_h)));
      vpiHandle submodItr = vpi_iterate(vpiModule, obj_h); 
      while (vpiHandle sub_h = vpi_scan(submodItr) ) {
	result += "\n  \\_ mod:" + std::string(vpi_get_str(vpiName, sub_h)) 
	  + ", top:" + std::to_string(vpi_get(vpiTopModule, sub_h))
	  + ", parent:" + std::string(vpi_get_str(vpiName, vpi_handle(vpiParent, sub_h)));
	vpi_release_handle (sub_h);
      }
    vpi_release_handle (submodItr);
    
    result += "\n";
    vpi_release_handle (obj_h);
    }
    vpi_release_handle(modItr);
  }
  return result;
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
