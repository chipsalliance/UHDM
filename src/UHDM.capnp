@0xfff7299129556877;

struct UhdmRoot {
  designs @0 : List(Design);
  symbols @1 : List(Text);
  factoryProcess @2 :List(Process);
  factoryScope @3 :List(Scope);
  factoryTask @4 :List(Task);
  factoryFunction @5 :List(Function);
  factoryModport @6 :List(Modport);
  factoryInterfacetfdecl @7 :List(Interfacetfdecl);
  factoryInterface @8 :List(Interface);
  factoryInterfacearray @9 :List(Interfacearray);
  factoryContassign @10 :List(Contassign);
  factoryPort @11 :List(Port);
  factoryModulearray @12 :List(Modulearray);
  factoryPrimitive @13 :List(Primitive);
  factoryPrimitivearray @14 :List(Primitivearray);
  factoryModpath @15 :List(Modpath);
  factoryTchk @16 :List(Tchk);
  factoryDefparam @17 :List(Defparam);
  factoryIodecl @18 :List(Iodecl);
  factoryAliasstmt @19 :List(Aliasstmt);
  factoryClockingblock @20 :List(Clockingblock);
  factoryInstancearray @21 :List(Instancearray);
  factoryModule @22 :List(Module);
  factoryProgram @23 :List(Program);
  factoryPackage @24 :List(Package);
  factoryDesign @25 :List(Design);

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
struct Task {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Function {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Modport {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiName @4 :UInt64;
iodecls @5 :List(UInt64);
interface @6 :UInt64;
}
struct Interfacetfdecl {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiAccessType @4 :UInt64;
tasks @5 :List(UInt64);
functions @6 :List(UInt64);
}
struct Interface {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
process @4 :List(UInt64);
interfacetfdecls @5 :List(UInt64);
modports @6 :List(UInt64);
globalclocking @7 :UInt64;
defaultclocking @8 :UInt64;
modpaths @9 :List(UInt64);
contassigns @10 :List(UInt64);
interfaces @11 :List(UInt64);
interfacearrays @12 :List(UInt64);
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
primitives @10 :List(UInt64);
primitivearrays @11 :List(UInt64);
globalclocking @12 :UInt64;
defaultclocking @13 :UInt64;
ports @14 :List(UInt64);
interfaces @15 :List(UInt64);
interfacearrays @16 :List(UInt64);
contassigns @17 :List(UInt64);
modules @18 :List(UInt64);
modulearrays @19 :List(UInt64);
modpaths @20 :List(UInt64);
tchks @21 :List(UInt64);
defparams @22 :List(UInt64);
iodecls @23 :List(UInt64);
aliasstmts @24 :List(UInt64);
clockingblocks @25 :List(UInt64);
}
struct Program {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiName @4 :UInt64;
instancearray @5 :UInt64;
process @6 :List(UInt64);
defaultclocking @7 :UInt64;
interfaces @8 :List(UInt64);
interfacearrays @9 :List(UInt64);
contassigns @10 :List(UInt64);
clockingblocks @11 :List(UInt64);
}
struct Package {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiUnit @4 :Bool;
vpiName @5 :UInt64;
vpiFullName @6 :UInt64;
vpiDefName @7 :UInt64;
vpiArrayMember @8 :Bool;
vpiCellInstance @9 :Bool;
vpiDefNetType @10 :Int64;
vpiDefFile @11 :UInt64;
vpiDefDelayMode @12 :Int64;
vpiProtected @13 :Bool;
vpiTimePrecision @14 :Int64;
vpiTimeUnit @15 :Int64;
vpiUnconnDrive @16 :Int64;
vpiLibrary @17 :UInt64;
vpiCell @18 :UInt64;
vpiConfig @19 :UInt64;
vpiAutomatic @20 :Bool;
vpiTop @21 :Bool;
}
struct Design {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiName @4 :UInt64;
allModules @5 :List(UInt64);
topModules @6 :List(UInt64);
allPrograms @7 :List(UInt64);
allPackages @8 :List(UInt64);
}



