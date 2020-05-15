// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>
#include <stack>

#include "headers/uhdm.h"
#include "headers/vpi_listener.h"

using namespace UHDM;

class MyVpiListener : public VpiListener {
protected:
  void enterModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    const char* const parentName = vpi_get_str(vpiName, parentHandle);
    std::cout << "Module: " << object->VpiName()
              << ", parent: " << ((parentName != nullptr) ? parentName : "")
              << std::endl;
    stack_.push(object);
  }

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    stack_.pop();
  }

  void enterProgram(const program* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override {
    const char* const parentName = vpi_get_str(vpiName, parentHandle);
    std::cout << "Program: " << object->VpiName()
              << ", parent: " << ((parentName != nullptr) ? parentName : "")
              << std::endl;
    stack_.push(object);
  }

  void leaveProgram(const program* object, const BaseClass* parent,
                    vpiHandle handle, vpiHandle parentHandle) override {
    stack_.pop();
  }

private:
  std::stack<const BaseClass*> stack_;
};

int main (int argc, char** argv) {
  std::string fileName = "surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }
  UHDM::Serializer serializer1;
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer1.Restore(fileName);
  MyVpiListener* listener = new MyVpiListener();
  listen_designs(restoredDesigns,listener);
  return 0;
}
