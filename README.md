# UHDM
Universal Hardware Data Model

# Presentation
* [UHDM Presentation](https://docs.google.com/presentation/d/1evu8aBWMFwi_UrK-DfWXowfsXM4Bp9knQuHV24lwkPc/edit#slide=id.p)

# Purpose

 * Auto generate a concrete C++ implementation of the SystemVerilog (VHDL in future) Object Model following the IEEE standard object model
 * Auto generate a standard VPI interface as a facade to the C++ model
 * Auto generate a serialization/deserialization of the data model
 * Auto generate a Visitor function that exercise the entire VPI interface (used in uhdm-dump executable)
 * The generated Object Model can, for a given design, be:
    * Populated by parsers like [Surelog](https://github.com/alainmarcel/Surelog/) or [Verible](https://github.com/google/verible)
    * Consumed by tools like Yosys or Verilator
 
# HowTo

```bash
 * git clone https://github.com/alainmarcel/UHDM.git
 * cd UHDM
 * git submodule update --init --recursive
 * make all
```

# Features
 * All SystemVerilog models are expression in a Yaml type syntax (One file per Verilog Object Model)
 * From this Yaml description, all the code (C++ headers, VPI Interface, Serialization) is automatically generated.
 * Model inheritance and object/class grouping is supported (To follow the IEEE standard)
 * Supports the concept of "design" on top of the IEEE standard to support partitioning and multi-language (SystemVerilog - VHDL)
 * Any deviation/addition from the standard is cleary indicated by a uhdm prefix, IEEE standard API is either prefixed by vpi (Verilog) or vhpi (VHDL).

 
# Model Concepts
 * The model is captured in .yaml files, one per object models detailed pages 976-1050 of the SystemVerilog 2017 IEEE standard.
 * To match the standard, several concepts are observed in the model:
    * obj_def: A leaf object specification (Object can be allocated and persisted)
    * class_def: A virtual class specification (Class is used either with inheritance - extends:, or as composition of a - class_ref)
    * property: typically an int, bool, string property with a name and a vpi access type (ie: vpiModule) accessed by the vpi_get function
    * obj_ref: a reference to one (accessed by vpi_handle) or many (accessed by vpi_iterate) leaf objects 
    * class_ref: a reference to one or many virtual class, actual objects returned will be of a leaf type
    * extends: Class inheritance specified by the extends keyword
    * group_def: Grouping of objects in a named or unnamed group (We actually give a representative name to unnamed groups)
    * group_ref: a reference to one or many members of a group of objects
 * Keywords used to capture the model in Yaml
    * all of the above (obj_def...), plus for each definition (obj_ref, class_ref, group_ref, property), the following sub fields:
    * name: the name of the field (spaces accepted), verbatim from the standard
    * vpi: the name of the VPI access type to access this object member (Has to match a defined value in vpi_user.h or sv_vpi_user.h)
    * type: the formal type of the field:
      * obj_ref
      * class_ref
      * group_ref
      * int
      * unsigned int
      * bool
      * string
    * card: cardinality of the field
      * 1
      * any (0 or more)


# Model creation
 * The model creation task consists in converting the Object Model diagrams into their Yaml representation and invoking the creation of the concrete
 C++ classes, iterators, serialization code by invoking "make"
 * [How to create the model (presentation)](https://docs.google.com/presentation/d/1SGpgeeWmxJ-1AU8EKABrTyKwcfHOe-pfK8yXArTKIz8/edit?usp=sharing)
 
# Actual Design creation
 * The design creation task consists in invoking:
   * the proper concrete object factory methods to get serializable objects
   * populate the properties to the obtained objects
   * assemble the model by creating legal object relations (compile time and runtime checking) following the IEEE standard
   * invoking the serialization call
 * Read [`test1.cpp`](tests/test1.cpp)
 
# Design Navigation
 * After Deserialization of the persisted design (Read [`test2.cpp`](tests/test2.cpp))
 * client applications need to use the VPI interface to navigate the Object Model and create their own internal data structures (Read [`test_helper.h`](tests/test_helper.h))
 * An example Visitor is auto-generated to print the content of the data model (src/vpi_visitor.cpp)
 * The uhdm-dump executable creates a human readable view of the UHDM serialized data model.

# Linking libuhdm.a to your application 
 * After instaling (make install), create your own executable (Read [`Makefile`](Makefile)) , ie:
 * $(CXX) -std=c++14 tests/test1.cpp -I/usr/local/include/uhdm -I/usr/local/include/uhdm/include /usr/local/lib/uhdm/libuhdm.a -lcapnp -lkj -ldl -lutil -lm -lrt -lpthread -o test_inst

# Generating uhdm databases
 * Surelog generates natively UHDM databases (surelog.uhdm)
 * Other parsers are welcome to generate UHDM databases

# Useful links
* [SystemVerilog 2017](http://ecee.colorado.edu/~mathys/ecen2350/IntelSoftware/pdf/IEEE_Std1800-2017_8299595.pdf) - System Verilog Standard
* [Surelog](https://github.com/alainmarcel/Surelog/) - Surelog parser
* [Verible](https://github.com/google/verible) - Verible linter

[capnproto]: https://capnproto.org/
