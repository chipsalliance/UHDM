#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

int main (int argc, char** argv) {
  // Design building
  design* d = new design();
  module* m1 = new module();
  m1->set_vpiTopModule(true);
  m1->set_vpiName("M1");
  module* m2 = new module();
  m2->set_vpiName("M2");
  module* m3 = new module();
  m3->set_vpiName("M3");
  VectorOfmodule* v1 = new VectorOfmodule();
  v1->push_back(m1);
  v1->push_back(m2);
  d->set_allModules(v1);
  VectorOfmodule* v2 = new VectorOfmodule();
  v2->push_back(m3);
  m1->set_modules(v2);
  vpiHandle design1 = (vpiHandle) new uhdm_handle(uhdmdesign, d);

  // VPI test
  vpiHandle modItr = vpi_iterate(uhdmallModules,design1); 
  while (vpiHandle obj_h = vpi_scan(modItr) ) {
    std::cout << vpi_get_str(vpiName, obj_h)
	      << " " 
              << vpi_get(vpiTopModule, obj_h);

    vpiHandle submodItr = vpi_iterate(vpiModule, obj_h); 
    while (vpiHandle sub_h = vpi_scan(submodItr) ) {
      std::cout << " (" << vpi_get_str(vpiName, sub_h) << ") ";
    }
    
    std::cout << std::endl;
    vpi_release_handle (obj_h);
  }
  vpi_release_handle(modItr);

  return 0;
};
