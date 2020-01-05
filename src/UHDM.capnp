@0xfff7299129556877;

struct ObjIndexType {
   index @0 : UInt64;
   type  @1 : UInt32;
}

struct UhdmRoot {
  designs @0 : List(Design);
  symbols @1 : List(Text);
  factoryProcess @2 :List(Process);
  factoryBegin @3 :List(Begin);
  factoryNamedbegin @4 :List(Namedbegin);
  factoryOperation @5 :List(Operation);
  factoryRefobj @6 :List(Refobj);
  factoryTask @7 :List(Task);
  factoryFunction @8 :List(Function);
  factoryModport @9 :List(Modport);
  factoryInterfacetfdecl @10 :List(Interfacetfdecl);
  factoryInterface @11 :List(Interface);
  factoryInterfacearray @12 :List(Interfacearray);
  factoryContassign @13 :List(Contassign);
  factoryPort @14 :List(Port);
  factoryModulearray @15 :List(Modulearray);
  factoryPrimitive @16 :List(Primitive);
  factoryPrimitivearray @17 :List(Primitivearray);
  factoryModpath @18 :List(Modpath);
  factoryTchk @19 :List(Tchk);
  factoryDefparam @20 :List(Defparam);
  factoryIodecl @21 :List(Iodecl);
  factoryAliasstmt @22 :List(Aliasstmt);
  factoryClockingblock @23 :List(Clockingblock);
  factoryInstancearray @24 :List(Instancearray);
  factoryModule @25 :List(Module);
  factoryProgram @26 :List(Program);
  factoryPackage @27 :List(Package);
  factoryDesign @28 :List(Design);

}


struct Process {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Begin {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiName @4 :UInt64;
vpiFullName @5 :UInt64;
}
struct Namedbegin {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiName @4 :UInt64;
vpiFullName @5 :UInt64;
}
struct Operation {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiOpType @4 :Int64;
operands @5 :List(ObjIndexType);
vpiDecompile @6 :UInt64;
vpiSize @7 :Int64;
}
struct Refobj {
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
vpiName @4 :UInt64;
vpiFullName @5 :UInt64;
vpiMethod @6 :Bool;
vpiAccessType @7 :Int64;
vpiVisibility @8 :Int64;
vpiVirtual @9 :Bool;
vpiAutomatic @10 :Bool;
vpiDPIContext @11 :Bool;
vpiDPICStr @12 :Int64;
vpiDPICIdentifier @13 :UInt64;
leftexpr @14 :ObjIndexType;
rightexpr @15 :ObjIndexType;
variables @16 :ObjIndexType;
classdefn @17 :UInt64;
refobj @18 :UInt64;
iodecl @19 :UInt64;
}
struct Function {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiSigned @4 :Bool;
vpiSize @5 :Int64;
vpiFuncType @6 :Int64;
vpiName @7 :UInt64;
vpiFullName @8 :UInt64;
vpiMethod @9 :Bool;
vpiAccessType @10 :Int64;
vpiVisibility @11 :Int64;
vpiVirtual @12 :Bool;
vpiAutomatic @13 :Bool;
vpiDPIContext @14 :Bool;
vpiDPICStr @15 :Int64;
vpiDPICIdentifier @16 :UInt64;
leftexpr @17 :ObjIndexType;
rightexpr @18 :ObjIndexType;
variables @19 :ObjIndexType;
classdefn @20 :UInt64;
refobj @21 :UInt64;
iodecl @22 :UInt64;
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
process @4 :List(ObjIndexType);
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
instancearray @7 :ObjIndexType;
scope @8 :List(ObjIndexType);
process @9 :List(ObjIndexType);
primitives @10 :List(ObjIndexType);
primitivearrays @11 :List(ObjIndexType);
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
instancearray @5 :ObjIndexType;
process @6 :List(ObjIndexType);
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
taskfunc @22 :List(ObjIndexType);
programs @23 :List(UInt64);
programarrays @24 :List(UInt64);
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



