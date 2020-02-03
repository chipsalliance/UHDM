#include "headers/uhdm.h"
#include <iostream>
#include "headers/vpi_listener.h"

using namespace UHDM;


class MyVpiListener : public VpiListener {

void enterModule(const module* object, const BaseClass* parent,
		   vpiHandle handle, vpiHandle parentHandle) override {
std::cout << "Module: " << object->VpiName() << std::endl;
}
void leaveModule(const module* object, const BaseClass* parent,
		   vpiHandle handle, vpiHandle parentHandle) override {
}

};

int main (int argc, char** argv) {
  std::string fileName = "surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }
  Serializer serializer1;
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer1.Restore(fileName);  
  VpiListener* listener = new MyVpiListener();
  listen_designs(restoredDesigns,listener);
  return 0;
};
