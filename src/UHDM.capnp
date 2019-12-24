@0xfff7299129556877;

struct UhdmRoot {
  designs @0 : List(Design);
  symbols @1 : List(Text);
  factoryProcess @2 :List(Process);
  factoryScope @3 :List(Scope);
  factoryInterface @4 :List(Interface);
  factoryInterfacearray @5 :List(Interfacearray);
  factoryContassign @6 :List(Contassign);
  factoryPort @7 :List(Port);
  factoryModulearray @8 :List(Modulearray);
  factoryPrimitive @9 :List(Primitive);
  factoryPrimitivearray @10 :List(Primitivearray);
  factoryModpath @11 :List(Modpath);
  factoryTchk @12 :List(Tchk);
  factoryDefparam @13 :List(Defparam);
  factoryIodecl @14 :List(Iodecl);
  factoryAliasstmt @15 :List(Aliasstmt);
  factoryClockingblock @16 :List(Clockingblock);
  factoryInstancearray @17 :List(Instancearray);
  factoryModule @18 :List(Module);
  factoryDesign @19 :List(Design);

}


struct Process {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Scope {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Interface {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Interfacearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Contassign {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Port {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Modulearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Primitive {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Primitivearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Modpath {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Tchk {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Defparam {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Iodecl {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Aliasstmt {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Clockingblock {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Instancearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;

}
struct Module {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiName @2 :Text;
  vpiTopModule @3 :Bool;
  vpiDefDecayTime @4 :Int64;
  instancearray @5 :UInt64;
  scope @6 :List(UInt64);
  process @7 :List(UInt64);
  primitive @8 :List(UInt64);
  primitivearray @9 :List(UInt64);
  globalclocking @10 :UInt64;
  defaultclocking @11 :UInt64;
  ports @12 :List(UInt64);
  interfaces @13 :List(UInt64);
  interfacearrays @14 :List(UInt64);
  contassigns @15 :List(UInt64);
  modules @16 :List(UInt64);
  modulearray @17 :List(UInt64);
  modpath @18 :List(UInt64);
  tchk @19 :List(UInt64);
  defparam @20 :List(UInt64);
  iodecl @21 :List(UInt64);
  aliasstmt @22 :List(UInt64);
  clockingblock @23 :List(UInt64);

}
struct Design {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiName @2 :Text;
  allModules @3 :List(UInt64);
  topModules @4 :List(UInt64);

}



