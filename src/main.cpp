#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

int main (int argc, char** argv) {
  design* d = new design();
  module* m = new module();
  VectorOfmodulePtr v1 = new VectorOfmodule;
  v1->push_back(m);
  v1->push_back(m);
  d->set_allModules(v1);
  vpiHandle top = (vpiHandle) new uhdm_handle(designID, d);
  
  vpiHandle modItr = vpi_iterate(allModules,top); 
  while (vpiHandle obj_h = vpi_scan(modItr) ) {
    std::cout << obj_h << std::endl;
  }
  return 0;
};
