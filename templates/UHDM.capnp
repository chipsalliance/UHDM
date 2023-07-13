@0xfff7299129556877;

struct ObjIndexType {
  index @0 : UInt32;
  type  @1 : UInt32;
}

struct UhdmRoot {
  version @0 : UInt32;
  designs @1 : List(Design);
  symbols @2 : List(Text);
<CAPNP_ROOT_SCHEMA>
}


<CAPNP_SCHEMA>
