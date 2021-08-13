// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>

#include "headers/uhdm.h"
#include "headers/vpi_visitor.h"

#include "test-util.h"

using namespace UHDM;

// TODO: this test assumes that test1 and test3 have run before
int main (int argc, char** argv) {
  std::string fileName = uhdm_test::getTmpDir() + "/surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }
  Serializer serializer1;
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns1 = serializer1.Restore(fileName);
  std::string restored1 = visit_designs(restoredDesigns1);
  std::cout << restored1;

  Serializer serializer2;
  fileName = uhdm_test::getTmpDir() + "/surelog3.uhdm";
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns2 = serializer2.Restore(fileName);
  std::string restored2 = visit_designs(restoredDesigns2);
  std::cout << restored2;

  return 0;
}
