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
  VectorOfmodule* v1 = new VectorOfmodule();
  v1->push_back(m1);
  v1->push_back(m2);
  d->set_allModules(v1);
  vpiHandle top = (vpiHandle) new uhdm_handle(designID, d);

  // VPI test
  vpiHandle modItr = vpi_iterate(allModules,top); 
  while (vpiHandle obj_h = vpi_scan(modItr) ) {
    std::cout << vpi_get_str(vpiName, obj_h)
	      << " " 
              << vpi_get(vpiTopModule, obj_h)
	      << std::endl;
    vpi_release_handle (obj_h);
  }
  vpi_release_handle(modItr);

  return 0;
};
