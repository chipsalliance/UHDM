@0xfff7299129556877;

struct ObjIndexType {
  index @0 : UInt64;
  type  @1 : UInt32;
}

struct UhdmRoot {
  version  @0 : UInt32;
  objectId @1 : UInt32;
  designs  @2 : List(Design);
  symbols  @3 : List(Text);
<CAPNP_ROOT_SCHEMA>
}


<CAPNP_SCHEMA>
