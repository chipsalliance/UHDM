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
  factoryDistribution @5 :List(Distribution);
  factoryOperation @6 :List(Operation);
  factoryRefobj @7 :List(Refobj);
  factoryTask @8 :List(Task);
  factoryFunction @9 :List(Function);
  factoryModport @10 :List(Modport);
  factoryInterfacetfdecl @11 :List(Interfacetfdecl);
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
  factoryInterface @26 :List(Interface);
  factoryProgram @27 :List(Program);
  factoryArraynet @28 :List(Arraynet);
  factoryLogicvar @29 :List(Logicvar);
  factoryArrayvar @30 :List(Arrayvar);
  factoryNamedevent @31 :List(Namedevent);
  factoryNamedeventarray @32 :List(Namedeventarray);
  factorySpecparam @33 :List(Specparam);
  factoryClassdefn @34 :List(Classdefn);
  factoryPackage @35 :List(Package);
  factoryDesign @36 :List(Design);

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
struct Distribution {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
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
vpiIndex @4 :UInt64;
vpiName @5 :UInt64;
vpiTopModule @6 :Bool;
vpiDefDecayTime @7 :Int64;
exprdist @8 :ObjIndexType;
instancearray @9 :ObjIndexType;
scope @10 :List(ObjIndexType);
process @11 :List(ObjIndexType);
primitives @12 :List(ObjIndexType);
primitivearrays @13 :List(ObjIndexType);
globalclocking @14 :UInt64;
defaultclocking @15 :UInt64;
modulearray @16 :UInt64;
ports @17 :List(UInt64);
interfaces @18 :List(UInt64);
interfacearrays @19 :List(UInt64);
contassigns @20 :List(UInt64);
modules @21 :List(UInt64);
modulearrays @22 :List(UInt64);
modpaths @23 :List(UInt64);
tchks @24 :List(UInt64);
defparams @25 :List(UInt64);
iodecls @26 :List(UInt64);
aliasstmts @27 :List(UInt64);
clockingblocks @28 :List(UInt64);
}
struct Interface {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiIndex @4 :UInt64;
exprdist @5 :ObjIndexType;
instancearray @6 :ObjIndexType;
process @7 :List(ObjIndexType);
interfacetfdecls @8 :List(UInt64);
modports @9 :List(UInt64);
globalclocking @10 :UInt64;
defaultclocking @11 :UInt64;
modpaths @12 :List(UInt64);
contassigns @13 :List(UInt64);
interfaces @14 :List(UInt64);
interfacearrays @15 :List(UInt64);
}
struct Program {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
vpiIndex @4 :UInt64;
vpiName @5 :UInt64;
instancearray @6 :ObjIndexType;
exprdist @7 :ObjIndexType;
process @8 :List(ObjIndexType);
defaultclocking @9 :UInt64;
interfaces @10 :List(UInt64);
interfacearrays @11 :List(UInt64);
contassigns @12 :List(UInt64);
clockingblocks @13 :List(UInt64);
}
struct Arraynet {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Logicvar {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Arrayvar {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Namedevent {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Namedeventarray {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Specparam {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
}
struct Classdefn {
vpiParent @0 :UInt64;
uhdmParentType @1 :UInt64;
vpiFile @2 :UInt64;
vpiLineNo @3 :UInt32;
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
net @23 :List(ObjIndexType);
arraynet @24 :List(ObjIndexType);
variables @25 :List(ObjIndexType);
parameters @26 :List(ObjIndexType);
assertion @27 :List(ObjIndexType);
typespec @28 :List(ObjIndexType);
classdefn @29 :List(ObjIndexType);
programs @30 :List(UInt64);
programarrays @31 :List(UInt64);
logicvar @32 :List(UInt64);
arrayvar @33 :List(UInt64);
arrayvarmem @34 :List(UInt64);
namedevent @35 :List(UInt64);
namedeventarray @36 :List(UInt64);
specparam @37 :List(UInt64);
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



