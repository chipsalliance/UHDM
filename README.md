# Universal Hardware Data Model (UHDM)


![UHDM Overview](/images/UHDM.png)

![UHDM Roadmap](/images/UHDM_future.png)

# Presentation
* [WOSET 2020 Paper and Presentation](https://woset-workshop.github.io/WOSET2020.html#article-10)
* [UHDM Presentation](https://docs.google.com/presentation/d/1evu8aBWMFwi_UrK-DfWXowfsXM4Bp9knQuHV24lwkPc/edit#slide=id.p)

# Purpose

 * Auto generate a concrete C++ implementation of the SystemVerilog (VHDL in future) Object Model following the IEEE standard object model
 * Auto generate a standard VPI interface as a facade to the C++ model
 * Auto generate a serialization/deserialization of the data model
 * Auto generate a Visitor (Walker) function that exercise the entire VPI interface (used in uhdm-dump executable)
 * Auto generate a C++ Listener Design Pattern that traverse the entire VPI data model (used in uhdm-listener executable)
 * Auto generate an Elaborator that uniquifies nets, variables...
 * The generated Object Model can, for a given design, be:
    * Populated by parsers like [Surelog](https://github.com/alainmarcel/Surelog/) or [Verible](https://github.com/google/verible)
    * Consumed by tools like Yosys or Verilator

# HowTo

```bash
 * git clone https://github.com/alainmarcel/UHDM.git
 * cd UHDM
 * git submodule update --init --recursive
 * make
```

# Features
 * All SystemVerilog models are expressed in a Yaml type syntax (One file per Verilog Object Model)
 * From this Yaml description, all the code (C++ headers, VPI Interface, Serialization) is automatically generated.
 * Model inheritance and object/class grouping is supported (To follow the IEEE standard)
 * Supports the concept of "design" on top of the IEEE standard to support partitioning and multi-language (SystemVerilog - VHDL)
 * Any deviation/addition from the standard is cleary indicated by a uhdm prefix, IEEE standard API is either prefixed by vpi (Verilog) or vhpi (VHDL).


# Model Concepts
 * The model is captured in .yaml files, one per object models detailed pages 976-1050 of the SystemVerilog 2017 IEEE standard.
 * To match the standard, several concepts are observed in the model:
    * obj_def: A leaf object specification (Object can be allocated and persisted)
    * class_def: A virtual class specification (Class is used either with inheritance - extends:, or as composition of a - class_ref)
    * property: Typically an int, bool, string property with a name and a vpi access type (ie: vpiModule) accessed by the vpi_get function
    * obj_ref: A reference to one (accessed by vpi_handle) or many (accessed by vpi_iterate) leaf objects
    * class_ref: A reference to one or many virtual class, actual objects returned will be of a leaf type
    * extends: Class inheritance specified by the extends keyword
    * group_def: Grouping of objects in a named or unnamed group (We actually give a representative name to unnamed groups)
    * group_ref: A reference to one or many members of a group of objects
 * Keywords used to capture the model in Yaml
    * all of the above keywords (obj_def...group_ref),
    * For each reference (obj_def, class_def, group_def) and property, the following sub fields:
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
      * value (VPI s_vpi_value)
      * delay (VPI s_vpi_delay)
    * card: cardinality of the field
      * 1
      * any (0 or more)
 * The Standard VPI Data Model is Fully Elaborated, in contrast:
 * When created by Surelog, the UHDM/VPI Data Model is a Folded Model that we found most suitable for applications like Yosys and Verilator:
    * The Instance tree contains the Design Hierarchy and Elaborated Nets/Ports with High conn and Low conn connections done.
    * The module definitions contain the logic elements (non-elaborated, and only outside generate statements)
    * Generate statements and underlying logic are only visible in the elaborated model (topModules)
    * To get the complete picture of the design one has to use both views (Example in [`listener_elab_test.cpp`](tests/listener_elab_test.cpp))
    * Applications where the UHDM data model is used as a precursor to another internal datastructure like a Synthesis or Simulator tool will prefer using the Folded Model.
    * Nets, Ports, Variables in the flat module list (allModules) don't necessary have the correct data type as not enough elaboration steps were performed on them
    * On the other hand, Nets, Ports, Variables have the correct type in the elaborated view (topModules)
    * Lhs vs Rhs expression padding is not performed at this point (We welcome PR contributions)
 * UHDM offers an optional Elaboration step that uniquifies nets, ports, variables and function by performing a deep cloning and ref_obj binding.
    * See [`full_elab_test.cpp`](tests/full_elab_test.cpp) and [`uhdm-dump.cpp`](util/uhdm-dump.cpp)
    * Applications where the UHDM data model is free standing and is the sole data structure for the design representation will prefer the Fully Elaborated Data Model, examples: Linters or Code Analyzers.
    * At this point, UHDM does not offer:
       * the full bit blasted model available in the commercial EDA applications (We welcome contributions).
       * an expression evaluator that operates on the UHDM expression tree (We welcome contribuitons).
    * [Issue 319](https://github.com/chipsalliance/UHDM/issues/319) discusses more on the topic of elaboration
    
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
 * Read [`module-port_test.cpp`](tests/module-port_test.cpp)


# Design Navigation
 * After Deserialization of the persisted design (elaborated or not) (Read [`module-port_test.cpp`](tests/module-port_test.cpp))
 * Client applications can elaborate optionally and use the VPI interface to navigate the Object Model and create their own internal data structures (Read [`tests/listener_elab_test.cpp`](tests/listener_elab_test.cpp))
 * Or use the Visitor (More like a Walker)
   * An example Visitor is auto-generated to print the content of the data model [`vpi_visitor.cpp`](templates/vpi_visitor.cpp)
 * Or use the Listener Design Pattern
   * Examples can be found in tests/vpi_listener.cpp or tests/uhdm_listener.cpp
   * The listener enables client application development with minimum disruption while the data model evolves.
   * An Custom Elaborator example code uses the Listener Design Pattern in [`listener_elab_test.cpp`](tests/listener_elab_test.cpp)
   * A Full Elaboration example is demonstrated in [`full_elab_test.cpp`](tests/full_elab_test.cpp) and [`uhdm-dump.cpp`](util/uhdm-dump.cpp)
 * The uhdm-dump [`uhdm-dump`](util/uhdm-dump.cpp) executable creates a human readable view of the UHDM serialized data model using the visitor [`visitor.cpp`](templates/vpi_visitor.cpp).
 * An optional linter (Listener) that warns about non-Synthesizable constructs can be found here: [`SynthSubset.cpp`](templates/SynthSubset.cpp)
 * An optional linter (Listener) that warns about diverse Verilog compliances post-elaboration can be found here: [`UhdmLint.cpp`](templates/UhdmLint.cpp)
 * An optionnal expression evaluator can be found here: [`ExprEval.cpp`](templates/ExprEval.cpp)
 * The Yosys-UHDM plugin code (most comprehensive open-source usage of UHDM) can be found here: https://github.com/chipsalliance/yosys-f4pga-plugins/tree/main/systemverilog-plugin
 * The Verilator-UHDM plugin code can be found here: https://github.com/antmicro/verilator/blob/uhdm-verilator/src/UhdmAst.cpp

# Linking libuhdm.a to your application
 * After instaling (`make install`), create your own executable (Read [`Makefile`](Makefile)) , ie:
 * `$(CXX) -std=c++17 tests/test1.cpp -I/usr/local/include/uhdm -I/usr/local/include/uhdm/include /usr/local/lib/uhdm/libuhdm.a /usr/local/lib/uhdm/libcapnp.a /usr/local/lib/uhdm/libkj.a -ldl -lutil -lm -lrt -lpthread -o test_inst`


# Generating uhdm databases
 * Surelog generates natively UHDM databases (surelog.uhdm)
 * Other parsers are welcome to generate UHDM databases


# Useful links
* [Verilog_Object_Model.pdf](third_party/Verilog_Object_Model.pdf) - Object Model section of the IEEE_Std1800-2017_8299595.pdf (Practical for searches)
* [SystemVerilog 2017](http://ecee.colorado.edu/~mathys/ecen2350/IntelSoftware/pdf/IEEE_Std1800-2017_8299595.pdf) - System Verilog Standard
* [Surelog](https://github.com/alainmarcel/Surelog/) - Surelog parser
* [Verible](https://github.com/google/verible) - Verible linter
* [capnproto](https://capnproto.org/) - Cap'n Proto serialization
