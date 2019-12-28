#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

#include "test_helper.h"

int main (int argc, char** argv) {
  std::cout << "Restore design" << std::endl;
  std::vector<vpiHandle> restoredDesigns = Serializer::restore("design.uhdmap");
  
  std::string restored = print_designs(restoredDesigns);
  std::cout << restored;
  return 0;
};
