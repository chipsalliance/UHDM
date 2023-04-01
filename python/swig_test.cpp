#include "swig_test.h"
#include "uhdm.h"
using namespace UHDM;

std::vector<vpiHandle> buildTestDesign(Serializer* s) {
  std::vector<vpiHandle> designs;

  design* d = s->MakeDesign();
  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign,d);
  designs.push_back(dh);

  VectorOfmodule_inst* vm = s->MakeModule_instVec();
  d->AllModules(vm);

  module_inst* m1 = s->MakeModule_inst();
  m1->VpiName("module1");
  vm->push_back(m1);

  module_inst* m2 = s->MakeModule_inst();
  m2->VpiName("module2");
  vm->push_back(m2);

  return designs;
}

