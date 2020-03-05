#include <iostream>

#include "headers/uhdm.h"
#include "headers/vpi_visitor.h"

using namespace UHDM;

#include "vpi_visitor.h"

std::vector<vpiHandle> build_designs (Serializer& s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s.MakeDesign();
  d->VpiName("design3");
  module* m1 = s.MakeModule();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);

  always* proc_always = s.MakeAlways();
	  begin* begin_block = s.MakeBegin();
	  proc_always->Stmt(begin_block);
  VectorOfprocess* processes = s.MakeProcessVec();
  processes->push_back(proc_always);

  event_control* at = s.MakeEvent_control();
  expression* posedge_clk = s.MakeExpression();

  VectorOfany* statements = s.MakeAnyVec();
  ref_obj* lhs_rf = s.MakeRef_obj();
  lhs_rf->VpiName("out");
  assignment* assign1 = s.MakeAssignment();
  assign1->Lhs(lhs_rf);
  constant* c1 = s.MakeConstant();
  c1->VpiValue("INT:0");
  assign1->Rhs(c1);
  statements->push_back(assign1);

  delay_control* dc = s.MakeDelay_control();
  dc->VpiDelay("#100");

  assignment* assign2 = s.MakeAssignment();
  assign2->Lhs(lhs_rf);
  constant* c2 = s.MakeConstant();
  s_vpi_value val;
  val.format  = vpiIntVal;
  val.value.integer = 1;
  c2->VpiValue(VpiValue2String(&val));
  assign2->Rhs(c2);
  at->Stmt(assign2);
  statements->push_back(at);

  begin_block->Stmts(statements);
  m1->Process(processes);

  VectorOfmodule* v1 = s.MakeModuleVec();
  v1->push_back(m1);
  d->AllModules(v1);
  package* p1 = s.MakePackage();
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = s.MakePackageVec();
  v3->push_back(p1);
  d->AllPackages(v3);
  designs.push_back(s.MakeUhdmHandle(uhdmdesign, d));
  return designs;
}

int main (int argc, char** argv) {
  std::cout << "Make design" << std::endl;
  Serializer serializer;

  std::string orig = visit_designs(build_designs(serializer));

  std::cout << orig;
  std::cout << "\nSave design" << std::endl;
  serializer.Save("surelog3.uhdm");

  std::cout << "Restore design" << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore("surelog3.uhdm");

  std::string restored = visit_designs(restoredDesigns);
  std::cout << restored;
  return (orig != restored);
}

