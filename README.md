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

 
 