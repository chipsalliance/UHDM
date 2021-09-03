// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "headers/uhdm.h"
#include "headers/vpi_visitor.h"

#include "test-util.h"

#include <iostream>

using namespace UHDM;

std::vector<vpiHandle> build_designs (Serializer& s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s.MakeDesign();
  d->VpiName("design1");
  
  // Module
  module* m1 = s.MakeModule();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);

  // Module
  module* m2 = s.MakeModule();
  m2->VpiDefName("M2");
  m2->VpiName("u1");
  
  VectorOfmodule* v1 = s.MakeModuleVec();
  v1->push_back(m1);
  d->TopModules(v1);
  
  vpiHandle dh = s.MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

int main (int argc, char** argv) {
  std::cout << "Make design" << std::endl;
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(serializer);
  std::string orig;
  orig += "VISITOR:\n";
  orig += visit_designs(designs);
  std::cout << orig;
  const std::string filename = uhdm_test::getTmpDir() + "/surelog.uhdm";
  std::cout << "\nSave design" << std::endl;
  serializer.Save(filename);

  std::cout << "Restore design" << std::endl;
  const std::vector<vpiHandle>& restoredDesigns = serializer.Restore(filename);
  std::string restored;
  restored += "VISITOR:\n";
  restored += visit_designs(restoredDesigns);

  restored += "\nALL OBJECTS:\n";
  for (auto objIndexPair : serializer.AllObjects()) {
    if (objIndexPair.first) {
      restored += "OBJECT:\n";
      restored += decompile((any*) objIndexPair.first);
    }
  }
  std::cout << restored;

  return 0;
}
