@0xfff7299129556877;

struct UhdmRoot {
  designs @0 : List(Design);
  factoryProcess @1 :List(Process);
  factoryScope @2 :List(Scope);
  factoryInterface @3 :List(Interface);
  factoryInterfacearray @4 :List(Interfacearray);
  factoryContassign @5 :List(Contassign);
  factoryPort @6 :List(Port);
  factoryModulearray @7 :List(Modulearray);
  factoryPrimitive @8 :List(Primitive);
  factoryPrimitivearray @9 :List(Primitivearray);
  factoryModpath @10 :List(Modpath);
  factoryTchk @11 :List(Tchk);
  factoryDefparam @12 :List(Defparam);
  factoryIodecl @13 :List(Iodecl);
  factoryAliasstmt @14 :List(Aliasstmt);
  factoryClockingblock @15 :List(Clockingblock);
  factoryInstancearray @16 :List(Instancearray);
  factoryModule @17 :List(Module);
  factoryDesign @18 :List(Design);

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



