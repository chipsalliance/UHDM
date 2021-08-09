// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>

#include "headers/uhdm.h"
#include "headers/vpi_visitor.h"

#include "test-util.h"

using namespace UHDM;


int main (int argc, char** argv) {
  std::string fileName = uhdm_test::getTmpDir() + "/surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }
  Serializer serializer;
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore(fileName);

  std::string restored = visit_designs(restoredDesigns);
  std::cout << restored;
  return 0;
}
