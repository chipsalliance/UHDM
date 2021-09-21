// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

/*

 Copyright 2020 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   listener_elab
 * Author: alain
 *
 * Created on May 4, 2020, 10:03 PM
 */

#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <vector>

// Verifies that the forward declaration header compiles
#include <uhdm/uhdm.h>
#include <uhdm/uhdm_forward_decl.h>
#include <uhdm/vpi_listener.h>
#include <uhdm/vpi_visitor.h>

using namespace UHDM;

//-------------------------------------------
// This self-contained example demonstrate how one can navigate the Folded Model
// of UHDM By extracting the complete information in between in the instance
// tree and the module definitions using the Listener Design Pattern
//-------------------------------------------

//-------------------------------------------
// Unit test design

std::vector<vpiHandle> build_designs(Serializer& s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s.MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module* m1 = s.MakeModule();
  {
    m1->VpiDefName("M1");
    m1->VpiParent(d);
    m1->VpiFile("fake1.sv");
    m1->VpiLineNo(10);
  }

  //-------------------------------------------
  // Module definition M2 (non elaborated)
  module* m2 = s.MakeModule();
  {
    m2->VpiDefName("M2");
    m2->VpiFile("fake2.sv");
    m2->VpiLineNo(20);
    m2->VpiParent(d);
    // M2 Ports
    VectorOfport* vp = s.MakePortVec();
    port* p = s.MakePort();
    p->VpiName("i1");
    p->VpiDirection(vpiInput);
    vp->push_back(p);
    p = s.MakePort();
    p->VpiName("o1");
    p->VpiDirection(vpiOutput);
    vp->push_back(p);
    m2->Ports(vp);
    // M2 Nets
    VectorOfnet* vn = s.MakeNetVec();
    logic_net* n = s.MakeLogic_net();
    n->VpiName("i1");
    vn->push_back(n);
    n = s.MakeLogic_net();
    n->VpiName("o1");
    vn->push_back(n);
    m2->Nets(vn);

    // M2 continuous assignment
    VectorOfcont_assign* assigns = s.MakeCont_assignVec();
    cont_assign* cassign = s.MakeCont_assign();
    assigns->push_back(cassign);
    ref_obj* lhs = s.MakeRef_obj();
    ref_obj* rhs = s.MakeRef_obj();
    lhs->VpiName("o1");
    rhs->VpiName("i1");
    cassign->Lhs(lhs);
    cassign->Rhs(rhs);
    m2->Cont_assigns(assigns);
  }

  //-------------------------------------------
  // Instance tree (Elaborated tree)
  // Top level module
  module* m3 = s.MakeModule();
  VectorOfmodule* v1 = s.MakeModuleVec();
  {
    m3->VpiDefName("M1");  // Points to the module def (by name)
    m3->VpiName("M1");     // Instance name
    m3->VpiTopModule(true);
    m3->Modules(v1);
    m3->VpiParent(d);
  }

  //-------------------------------------------
  // Sub Instance
  module* m4 = s.MakeModule();
  {
    m4->VpiDefName("M2");         // Points to the module def (by name)
    m4->VpiName("inst1");         // Instance name
    m4->VpiFullName("M1.inst1");  // Instance full name
    VectorOfport* inst_vp = s.MakePortVec();  // Create elaborated ports
    m4->Ports(inst_vp);
    port* p1 = s.MakePort();
    p1->VpiName("i1");
    inst_vp->push_back(p1);
    port* p2 = s.MakePort();
    p2->VpiName("o1");
    inst_vp->push_back(p2);
    // M2 Nets
    VectorOfnet* vn = s.MakeNetVec();  // Create elaborated nets
    logic_net* n = s.MakeLogic_net();
    n->VpiName("i1");
    n->VpiFullName("M1.inst.i1");
    ref_obj* low_conn = s.MakeRef_obj();
    low_conn->Actual_group(n);
    low_conn->VpiName("i1");
    p1->Low_conn(low_conn);
    vn->push_back(n);
    n = s.MakeLogic_net();
    n->VpiName("o1");
    n->VpiFullName("M1.inst.o1");
    low_conn = s.MakeRef_obj();
    low_conn->Actual_group(n);
    low_conn->VpiName("o1");
    p2->Low_conn(low_conn);
    vn->push_back(n);
    m4->Nets(vn);
  }

  // Create parent-child relation in between the 2 modules in the instance tree
  v1->push_back(m4);
  m4->VpiParent(m3);

  //-------------------------------------------
  // Create both non-elaborated and elaborated lists
  VectorOfmodule* allModules = s.MakeModuleVec();
  d->AllModules(allModules);
  allModules->push_back(m1);
  allModules->push_back(m2);

  VectorOfmodule* topModules = s.MakeModuleVec();
  d->TopModules(topModules);
  topModules->push_back(
      m3);  // Only m3 goes there as it is the top level module

  vpiHandle dh = s.MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}

//-------------------------------------------
// Elaboration based on the Listener pattern

class MyElaboratorListener : public VpiListener {
 public:
  MyElaboratorListener() {}

 protected:
  typedef std::map<std::string, const BaseClass*> ComponentMap;

  void leaveDesign(const design* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    design* root = (design*)object;
    root->VpiElaborated(true);
  }

  void enterModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    bool topLevelModule = object->VpiTopModule();
    const std::string& instName = object->VpiName();
    const std::string& defName = object->VpiDefName();
    bool flatModule =
        (instName == "") && ((object->VpiParent() == 0) ||
                             ((object->VpiParent() != 0) &&
                              (object->VpiParent()->VpiType() != vpiModule)));
    // false when it is a module in a hierachy tree
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

    if (flatModule) {
      // Flat list of module (unelaborated)
      flatComponentMap_.insert(std::make_pair(object->VpiDefName(), object));
    } else {
      // Hierachical module list (elaborated)

      // Collect instance elaborated nets
      ComponentMap netMap;
      if (object->Nets()) {
        for (net* net : *object->Nets()) {
          netMap.insert(std::make_pair(net->VpiName(), net));
        }
      }

      // Push instance context on the stack
      instStack_.push(std::make_pair(object, netMap));

      // Check if Module instance has a definition
      ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
      if (itrDef != flatComponentMap_.end()) {
        const BaseClass* comp = (*itrDef).second;
        int compType = comp->VpiType();
        switch (compType) {
          case vpiModule: {
            module* defMod = (module*)comp;

            // 1) This section illustrates how one can walk the data model in
            // the listener context

            // Bind the cont assign lhs and rhs to elaborated nets
            if (defMod->Cont_assigns()) {
              for (cont_assign* assign :
                   *defMod->Cont_assigns()) {  // explicit walking
                net* lnet = nullptr;
                net* rnet = nullptr;
                const expr* lhs = assign->Lhs();
                if (lhs->VpiType() == vpiRefObj) {
                  ref_obj* lref = (ref_obj*)lhs;
                  lnet = bindNet_(lref->VpiName());
                }
                const expr* rhs = assign->Rhs();
                if (rhs->VpiType() == vpiRefObj) {
                  ref_obj* rref = (ref_obj*)rhs;
                  rnet = bindNet_(rref->VpiName());
                }
                // Client code has now access the cont assign and the
                // hierarchical nets
                std::cout << "[2] assign " << lnet->VpiFullName() << " = "
                          << rnet->VpiFullName() << "\n";
              }
            }

            // Or

            // 2) This section illustrates how one can use the listener pattern
            // all the way

            // Trigger a listener of the definition module with the instance
            // context on the stack (hirarchical nets) enterCont_assign listener
            // method below will be trigerred to capture the same data as the
            // walking above in (1)
            vpiHandle defModule = NewVpiHandle(defMod);
            VisitedContainer visited;
            listen_module(defModule, this, &visited);
            vpi_free_object(defModule);

            break;
          }
          default:
            break;
        }
      }
    }
  }

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    const std::string& instName = object->VpiName();
    bool flatModule =
        (instName == "") && ((object->VpiParent() == 0) ||
                             ((object->VpiParent() != 0) &&
                              (object->VpiParent()->VpiType() != vpiModule)));
    // false when it is a module in a hierachy tree
    if (!flatModule) instStack_.pop();
  }

  // Make full use of the listener pattern for all objects in a module, example
  // with "cont assign":
  void enterCont_assign(const cont_assign* assign, const BaseClass* parent,
                        vpiHandle handle, vpiHandle parentHandle) override {
    net* lnet = nullptr;
    net* rnet = nullptr;
    ref_obj* lref = nullptr;
    ref_obj* rref = nullptr;
    const expr* lhs = assign->Lhs();
    if (lhs->VpiType() == vpiRefObj) {
      lref = (ref_obj*)lhs;
    }
    const expr* rhs = assign->Rhs();
    if (rhs->VpiType() == vpiRefObj) {
      rref = (ref_obj*)rhs;
    }

    if (instStack_.size() == 0) {
      // Flat module traversal
      std::cout << "[1] assign " << lref->VpiName() << " = " << rref->VpiName()
                << "\n";

    } else {
      // In the instance context (through the trigered listener)

      lnet = bindNet_(lref->VpiName());
      rnet = bindNet_(rref->VpiName());

      // Client code has now access the cont assign and the hierarchical nets
      std::cout << "[3] assign " << lnet->VpiFullName() << " = "
                << rnet->VpiFullName() << "\n";
    }
  }

  // Listen to processes, stmts....

 private:
  // Bind to a net in the parent instace
  net* bindParentNet_(const std::string& name) {
    std::pair<const BaseClass*, ComponentMap> mem = instStack_.top();
    instStack_.pop();
    ComponentMap& netMap = instStack_.top().second;
    instStack_.push(mem);
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (net*)(*netItr).second;
    }
    return nullptr;
  }

  // Bind to a net in the current instace
  net* bindNet_(const std::string& name) {
    ComponentMap& netMap = instStack_.top().second;
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (net*)(*netItr).second;
    }
    return nullptr;
  }

  // Instance context stack
  std::stack<std::pair<const BaseClass*, ComponentMap>> instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;
};

int main(int argc, char** argv) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(serializer);
  std::string orig;
  orig += "DUMP Design content:\n";
  orig += visit_designs(designs);
  std::cout << orig;
  bool elaborated = false;
  for (auto design : designs) {
    elaborated |= vpi_get(vpiElaborated, design);
  }
  if (!elaborated) {
    std::cout << "Elaborating...\n";
    MyElaboratorListener* listener = new MyElaboratorListener();
    listen_designs(designs, listener);
  }
  std::string post_elab1 = visit_designs(designs);
  for (auto design : designs) {
    elaborated |= vpi_get(vpiElaborated, design);
  }
  // Make sure we are not elaborating twice
  if (!elaborated) {
    std::cout << "Elaborating...\n";
    MyElaboratorListener* listener = new MyElaboratorListener();
    listen_designs(designs, listener);
  }
  std::string post_elab2 = visit_designs(designs);
  if ((!elaborated) || (post_elab1 != post_elab2)) {
    std::cout << "ERROR: Elaborating twice!\n";
    return 1;
  }
  return 0;
}
