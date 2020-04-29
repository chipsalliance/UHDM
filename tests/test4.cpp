// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>

#include "headers/uhdm.h"
#include "headers/vpi_visitor.h"

using namespace UHDM;

int main (int argc, char** argv) {
  std::string fileName = "surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }
  Serializer serializer1;
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns1 = serializer1.Restore(fileName);
  std::string restored1 = visit_designs(restoredDesigns1);
  std::cout << restored1;

  Serializer serializer2;
  fileName = "surelog3.uhdm";
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns2 = serializer2.Restore(fileName);
  std::string restored2 = visit_designs(restoredDesigns2);
  std::cout << restored2;

  return 0;
}
