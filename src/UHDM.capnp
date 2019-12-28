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
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Scope {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Interface {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Interfacearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Contassign {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Port {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Modulearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Primitive {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Primitivearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Modpath {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Tchk {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Defparam {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Iodecl {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Aliasstmt {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Clockingblock {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Instancearray {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;

}
struct Module {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;
  vpiName @4 :UInt64;
  vpiTopModule @5 :Bool;
  vpiDefDecayTime @6 :Int64;
  instancearray @7 :UInt64;
  scope @8 :List(UInt64);
  process @9 :List(UInt64);
  primitive @10 :List(UInt64);
  primitivearray @11 :List(UInt64);
  globalclocking @12 :UInt64;
  defaultclocking @13 :UInt64;
  ports @14 :List(UInt64);
  interfaces @15 :List(UInt64);
  interfacearrays @16 :List(UInt64);
  contassigns @17 :List(UInt64);
  modules @18 :List(UInt64);
  modulearray @19 :List(UInt64);
  modpath @20 :List(UInt64);
  tchk @21 :List(UInt64);
  defparam @22 :List(UInt64);
  iodecl @23 :List(UInt64);
  aliasstmt @24 :List(UInt64);
  clockingblock @25 :List(UInt64);

}
struct Design {
  vpiParent @0 :UInt64;
  uhdmParentType @1 :UInt64;
  vpiFile @2 :UInt64;
  vpiLineNo @3 :UInt32;
  vpiName @4 :UInt64;
  allModules @5 :List(UInt64);
  topModules @6 :List(UInt64);

}



