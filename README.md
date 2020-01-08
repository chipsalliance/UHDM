# UHDM
Universal Hardware Data Model

# Purpose

 * Auto generate a concrete C++ implementation of the SystemVerilog Object Model following the IEEE standard object model
 * Auto generate a standard VPI interface as a facade to the C++ model
 * Auto generate a serialization/deserialization of the data model

# HowTo
 * git clone https://github.com/alainmarcel/UHDM.git
 * cd UHDM
 * make all

# Features
 * All SystemVerilog models are expression in a Yaml type syntax (One file per Verilog Object Model)
 * From this Yaml description, all the code (C++ headers, VPI Interface, Serialization) is automatically generated.
 * Model inheritance is supported (To follow the IEEE standard)
 * Supports the concept of "design" on top of the IEEE standard to support partitioning and multi-language (SystemVerilog - VHDL)
 * Any deviation/addition from the standard is cleary indicated by a uhdm prefix, IEEE standard API is either prefixed by vpi (Verilog) or vhpi (VHDL).

 
# Model
 * The model is captured in .yaml files, one per object models detailed pages 976-1050 of the SystemVerilog 2017 IEEE standard.
 * To match the standard, several concepts are observed in the model:
    * obj_def : A leaf object specification (Object can be allocated and persisted)
    * class_def : A virtual class specification (Class is used either with inheritance - extends:, or as composition of a - class_ref)
    * property : typically an int, bool, string property with a name and a vpi access type (ie: vpiModule) accessed by the vpi_get function
    * obj_ref : a reference to one (accessed by vpi_handle) or many (accessed by vpi_iterate) leaf objects 
    * class_ref : a reference to one or many virtual class, actual objects returned will be of a leaf type
    * Class inheritance specified by the extends keyword
   