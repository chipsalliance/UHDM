#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

#include "test_helper.h"

int main (int argc, char** argv) {
  std::string fileName = "surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }
  Serializer serializer;
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore(fileName);
  
  std::string restored = print_designs(restoredDesigns);
  std::cout << restored;
  return 0;
};
