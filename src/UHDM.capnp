@0xfff7299129556877;

struct UhdmRoot {
  designs @0 : List(Design);

}


struct Process {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Scope {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Interface {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Interfacearray {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Contassign {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Port {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Modulearray {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Primitive {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Primitivearray {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Modpath {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Tchk {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Defparam {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Iodecl {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Aliasstmt {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Clockingblock {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Instancearray {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;

}
struct Module {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;
  vpiName @2 :Text;
  vpiTopModule @3 :Bool;
  vpiDefDecayTime @4 :Int32;
  instancearray @5 :Instancearray;
  scope @6 :List(Scope);
  process @7 :List(Process);
  primitive @8 :List(Primitive);
  primitivearray @9 :List(Primitivearray);
  globalclocking @10 :Clockingblock;
  defaultclocking @11 :Clockingblock;
  ports @12 :List(Port);
  interfaces @13 :List(Interface);
  interfacearrays @14 :List(Interfacearray);
  contassigns @15 :List(Contassign);
  modules @16 :List(Module);
  modulearray @17 :List(Modulearray);
  modpath @18 :List(Modpath);
  tchk @19 :List(Tchk);
  defparam @20 :List(Defparam);
  iodecl @21 :List(Iodecl);
  aliasstmt @22 :List(Aliasstmt);
  clockingblock @23 :List(Clockingblock);

}
struct Design {
  vpiParent @0 :UInt32;
  uhdmParentType @1 :UInt32;
  vpiName @2 :Text;
  allModules @3 :List(Module);
  topModules @4 :List(Module);

}



