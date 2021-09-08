#!/usr/bin/tclsh
# -*- mode: Tcl; c-basic-offset: 4; indent-tabs-mode: nil; -*-
#
# Copyright 2019-2020 Alain Dargelas
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

variable myLocation [file normalize [info script]]
set myProjetPathNoNormalize [file dirname [file dirname [info script]]]

proc project_path {} {
    variable myLocation
    return [file dirname [file dirname $myLocation]]
}

set model_files [lindex $argv 0]
set working_dir [lindex $argv 1]
set codegen_base [expr {$argc > 2} ? {[lindex $argv 2]} : {[project_path]}]

proc codegen_base {} {
    global codegen_base
    return $codegen_base
}

puts "UHDM MODEL GENERATION"
puts "Working dir: $working_dir; codegen_base: [codegen_base]"

set DEBUG 0

proc log { arg } {
    global DEBUG
    if {$DEBUG == 1} {
        puts $arg
    }
}

proc lognnl { arg } {
    global DEBUG
    if {$DEBUG == 1} {
        puts -nonewline $arg
    }
}

proc exec_path {} {
    variable myLocation
    return [file dirname $myLocation]
}


file mkdir [codegen_base]/src
file mkdir [codegen_base]/headers

source [exec_path]/file_utils.tcl
source [exec_path]/pdict.tcl
source [exec_path]/parse_model.tcl
source [exec_path]/generate_elaborator.tcl

proc parse_vpi_user_defines { } {
    global ID
    set fid [open "[project_path]/include/vpi_user.h"]
    set content [read $fid]
    set lines [split $content "\n"]
    foreach line $lines {
        if [regexp {^#define[ ]+(vpi[a-zA-Z0-9]*)[ ]+([0-9]+)} $line tmp name value] {
            set ID($name) $value
        }
    }
    close $fid
}

# Above sv_vpi_user.h and vhpi_user.h ranges
set OBJECTID 2000

proc printMethods { classname type vpi card {real_type ""} } {
    global methods_cpp group_headers
    set methods ""
    if {$type == "string"} {
        set type "std::string"
    }
    if {$type == "value"} {
        set type "std::string"
    }
    if {$type == "delay"} {
        set type "std::string"
    }
    if {$vpi == "uhdmType"} {
        set type "UHDM_OBJECT_TYPE"
    }
    set final ""
    set virtual ""
    if {$vpi == "vpiParent" || $vpi == "uhdmParentType" || $vpi == "uhdmType" || $vpi == "vpiLineNo" || $vpi == "vpiColumnNo" || $vpi == "vpiEndLineNo" || $vpi == "vpiEndColumnNo" || $vpi == "vpiFile" || $vpi == "vpiName" || $vpi == "vpiDefName" || $vpi == "uhdmId" } {
        set final " final"
        set virtual "virtual "
    }
    set check ""
    if {$type == "any"} {
        append group_headers "#include \"${real_type}.h\"\n"
        set check "if (!${real_type}GroupCompliant(data)) return false;"
    }
    if {$card == "1"} {
        set pointer ""
        set const ""
        if {($type != "unsigned int") && ($type != "int") && ($type != "bool") && ($type != "std::string")} {
            set pointer "*"
            set const "const "
        }


        if {$type == "std::string"} {
            append methods "\n  ${virtual}bool [string toupper ${vpi} 0 0](const ${type}${pointer}\\& data)$final;\n"
            if {$vpi == "vpiFullName" } {
                append methods "\n  ${virtual}const ${type}${pointer}\\&  [string toupper ${vpi} 0 0]() const$final;\n"
                append methods_cpp "\nconst ${type}${pointer}\\&  ${classname}::[string toupper ${vpi} 0 0]() const {
  if (${vpi}_) {
    return serializer_->symbolMaker.GetSymbol(${vpi}_);
  } else {
    std::vector<std::string> names;
    const BaseClass* parent = this;
    const BaseClass* child = nullptr;
    bool column = false;
    while (parent) {
      const BaseClass* actual_parent = parent->VpiParent();
      if (parent->UhdmType() == uhdmdesign) break;
      if (parent->UhdmType() == uhdmpackage || parent->UhdmType() == uhdmclass_defn) column = true;
      const std::string\\& name = (!parent->VpiName().empty()) ? parent->VpiName() : parent->VpiDefName();
      UHDM_OBJECT_TYPE parent_type = (parent != nullptr) ? parent->UhdmType() : uhdmunsupported_stmt;
      UHDM_OBJECT_TYPE actual_parent_type = (actual_parent != nullptr) ? actual_parent->UhdmType() : uhdmunsupported_stmt;
      bool skip_name = (actual_parent_type == uhdmref_obj) || (parent_type == uhdmmethod_func_call) ||
                       (parent_type == uhdmmethod_task_call) || (parent_type == uhdmfunc_call) ||
                       (parent_type == uhdmtask_call) || (parent_type == uhdmsys_func_call) ||
                       (parent_type == uhdmsys_task_call);
      if (child) {
        UHDM_OBJECT_TYPE child_type = child->UhdmType();
        if (child_type == uhdmbit_select \\&\\& parent_type == uhdmport) {
          skip_name = true;
        }
        if (child_type == uhdmref_obj \\&\\& parent_type == uhdmbit_select) {
          skip_name = true;
        }
        if (child_type == uhdmref_obj \\&\\& parent_type == uhdmindexed_part_select) {
          skip_name = true;
        }
        if (child_type == uhdmref_obj \\&\\& parent_type == uhdmhier_path) {
          skip_name = true;
        }
      }
      if ((!name.empty()) \\&\\& (!skip_name))
        names.push_back(name);
      child = parent;
      parent = parent->VpiParent();
    }
    std::string fullName;
    if (names.size()) {
      unsigned int index = names.size() -1;
      while(1) {
        fullName += names\[index\];
        if (index > 0) fullName += (column) ? \"::\" : \".\";
        if (index == 0) break;
        index--;
      }
    }
    if (!fullName.empty()) {
      ((${classname}*)this)->VpiFullName(fullName);
    }
    return serializer_->symbolMaker.GetSymbol(${vpi}_);
  }
}\n"
            } else {
                append methods "\n  ${virtual}const ${type}${pointer}\\& [string toupper ${vpi} 0 0]() const$final;\n"
                append methods_cpp "\nconst ${type}${pointer}\\& ${classname}::[string toupper ${vpi} 0 0]() const { return serializer_->symbolMaker.GetSymbol(${vpi}_); }\n"
            }
            append methods_cpp "\nbool ${classname}::[string toupper ${vpi} 0 0](const ${type}${pointer}\\& data) { ${vpi}_ = serializer_->symbolMaker.Make(data); return true; }\n"
        } else {
            append methods "\n  ${virtual}${const}${type}${pointer} [string toupper ${vpi} 0 0]() const$final { return ${vpi}_; }\n"
            if {$vpi == "vpiParent"} {
                append methods "\n  virtual bool [string toupper ${vpi} 0 0](${type}${pointer} data) final {${check} ${vpi}_ = data; if (data) uhdmParentType_ = data->UhdmType(); return true;}\n"
            } else {
                append methods "\n  ${virtual}bool [string toupper ${vpi} 0 0](${type}${pointer} data)$final {${check} ${vpi}_ = data; return true;}\n"
            }
        }
    } elseif {$card == "any"} {
        append methods "\n  VectorOf${type}* [string toupper ${vpi} 0 0]() const { return ${vpi}_; }\n"
        append methods "\n  bool [string toupper ${vpi} 0 0](VectorOf${type}* data) {${check} ${vpi}_ = data; return true;}\n"
    }
    return $methods
}

proc printCapnpSchema {type vpi card} {
    if {$type == "string"} {
        set type "UInt64"
    }
    if {$type == "unsigned int"} {
        set type "UInt64"
    }
    if {$type == "int"} {
        set type "Int64"
    }
    if {$type == "bool"} {
        set type "Bool"
    }
    if {$type == "any"} {
        set type "Int64"
    }
    if {$type == "value"} {
        set type "UInt64"
    }
    if {$type == "delay"} {
        set type "UInt64"
    }
    if {$card == "1"} {
        return [list ${vpi} ${type}]
    } elseif {$card == "any"} {
        return [list ${vpi} List(${type})]
    }
}

proc printMembers { type vpi card } {
    set members ""
    if {$type == "string" || $type == "value" || $type == "delay"} {
        set type "std::string"
    }
    if {$card == "1"} {
        set pointer ""
        set default_assignment "0"
        if {($type != "unsigned int") && ($type != "int") && ($type != "bool") && ($type != "std::string")} {
            set pointer "*"
            set default_assignment "nullptr"
        }
        if {$type == "std::string"} {
            append members "\n  SymbolFactory::ID ${vpi}_ = 0;\n"
        } else {
            append members "\n  ${type}${pointer} ${vpi}_ = ${default_assignment};\n"
        }
    } elseif {$card == "any"} {
        append members "\n  VectorOf${type}* ${vpi}_ = nullptr;\n"
    }
    return $members
}

proc printTypeDefs { type card } {
    global CONTAINER
    set typedefs ""
    if {$card == "any"} {
        if ![info exist CONTAINER($type)] {
            set CONTAINER($type) 1
            if {$type != "any"} {
                append typedefs "class $type;\n"
            }
            append typedefs "typedef std::vector<${type}*> VectorOf${type};\n"
            append typedefs "typedef std::vector<${type}*>::iterator VectorOf${type}Itr;\n"
        }
    }
    return $typedefs
}

proc printIterateBody { name classname vpi card } {
    set vpi_iterate_body ""
    if {$card == "any"} {
        append vpi_iterate_body "
  if (handle->type == uhdm${classname} \\&\\& type == $vpi) {
    if ((($classname*)(object))->[string toupper ${name} 0 0]())
      return NewHandle(uhdm${name}, (($classname*)(object))->[string toupper ${name} 0 0]());
    else return 0;
  }"
        printVpiVisitor $classname $vpi $card
        return $vpi_iterate_body
    }
}

proc printGetHandleByNameBody { name classname vpi card } {
    if {$card == 1} {
        set vpi_handle_by_name_body "
  if (handle->type == uhdm${classname}) {
    const $classname* const obj = (const $classname*)(object);
    if (obj->[string toupper ${name} 0 0]() \\&\\& obj->[string toupper ${name} 0 0]()->VpiName() == name) {
      return NewVpiHandle(obj->[string toupper ${name} 0 0]());
    }
  }"
    } else {
        set vpi_handle_by_name_body "
  if (handle->type == uhdm${classname}) {
    const $classname* const obj_parent = (const $classname*)(object);
    if (obj_parent->[string toupper ${name} 0 0]()) {
      for (const BaseClass *obj : *obj_parent->[string toupper ${name} 0 0]())
        if (obj->VpiName() == name) return NewVpiHandle(obj);
      }
    }"
    }
    return $vpi_handle_by_name_body
}

proc printGetBodyPrefix {classname} {
    return "  if (handle->type == uhdm${classname}) \{
    switch (property) \{"
}

proc printGetBodySuffix {} {
    return "
    \}
  \}
"
}

proc printGetBody {classname type vpi card} {
    # These are already handled by base class
    if {$vpi == "vpiType"} return ""
    if {$vpi == "vpiLineNo"} return ""
    if {$vpi == "vpiColumnNo"} return ""
    if {$vpi == "vpiEndLineNo"} return ""
    if {$vpi == "vpiEndColumnNo"} return ""
    set vpi_get_body ""
    if {($card == 1) && ($type != "string") && ($type != "value") && ($type != "delay")} {
        append vpi_get_body "
      case $vpi: return ((const $classname*)(obj))->[string toupper ${vpi} 0 0]();"
    }
    return $vpi_get_body
}

proc printGetValueBody {classname type vpi card} {
    set vpi_get_value_body ""
    if {($card == 1) && ($type == "value")} {
        append vpi_get_value_body "
  if (handle->type == uhdm${classname}) {
    const s_vpi_value* v = String2VpiValue((($classname*)(obj))->VpiValue());
    if (v) {
      *value_p = *v;
    }
  }"
    }
    return $vpi_get_value_body
}

proc printGetDelayBody {classname type vpi card} {
    set vpi_get_delay_body ""
    if {($card == 1) && ($type == "delay")} {
        append vpi_get_delay_body "
  if (handle->type == uhdm${classname}) {
    const s_vpi_delay* v = String2VpiDelays((($classname*)(obj))->VpiDelay());
    if (v) {
      *delay_p = *v;
    }
  }"
    }
    return $vpi_get_delay_body
}


proc printGetHandleBody { classname type vpi object card } {
    if {$type == "BaseClass"} {
        set type "(($classname*)(object))->UhdmParentType()"
    }
    set vpi_get_handle_body ""
    set need_casting 1
    if {$vpi == "vpiParent" && $object == "vpiParent"} {
        set need_casting 0
    }
    if {$card == 1} {
        set casted_object1 "(object"
        set casted_object2 "object"
        if {$need_casting == 1} {
            set casted_object1 "((BaseClass*)(($classname*)(object))"
            set casted_object2 "(($classname*)(object))"
        }
        append vpi_get_handle_body "
  if (handle->type == uhdm${classname} \\&\\& type == $vpi) {
     if ($casted_object1->[string toupper ${object} 0 0]()))
       return NewHandle($casted_object1->[string toupper ${object} 0 0]())->UhdmType(), $casted_object2->[string toupper ${object} 0 0]());
     else return 0;
  }"
        printVpiVisitor $classname $vpi $card
    }
    return $vpi_get_handle_body
}

proc printGetStrVisitor {classname type vpi card} {
    set vpi_get_str_body ""
    if {($card == 1) && ($type == "string") && ($vpi != "vpiFile")} {
        append vpi_get_str_body "    if (const char* s = vpi_get_str($vpi, obj_h))
      stream_indent(out, indent) << \"|$vpi:\" << s << \"\\n\";
"
    }
    return $vpi_get_str_body
}

proc printGetVisitor {classname type vpi card} {
    set vpi_get_body ""
    if {$vpi == "vpiValue"} {
        append vpi_get_body "    s_vpi_value value;
    vpi_get_value(obj_h, \\&value);
    if (value.format) {
      std::string val = visit_value(\\&value);
      if (!val.empty()) {
        stream_indent(out, indent) << val;
      }
    }
"
    } elseif {$vpi == "vpiDelay"} {
        append vpi_get_body "    s_vpi_delay delay;
    vpi_get_delays(obj_h, \\&delay);
    if (delay.da != nullptr) {
      stream_indent(out, indent) << visit_delays(\\&delay);
    }
"
    } elseif {($card == 1) && ($type != "string") && ($vpi != "vpiLineNo") && ($vpi != "vpiColumnNo")  && ($vpi != "vpiEndLineNo") && ($vpi != "vpiEndColumnNo") && ($vpi != "vpiType")} {
        append vpi_get_body "    if (const int n = vpi_get($vpi, obj_h))
      stream_indent(out, indent) << \"|$vpi:\" << n << \"\\n\";
"
    }
    return $vpi_get_body
}

proc printGetStrBody {classname type vpi card} {
    # Already handled by the base class
    if {$vpi == "vpiFile"} return ""
    if {$vpi == "vpiName"} return ""
    if {$vpi == "vpiDefName"} return ""

    set vpi_get_str_body ""
    if {$card == 1 && ($type == "string")} {
        if {$vpi == "vpiFullName"} {
            append vpi_get_str_body "
  if (handle->type == uhdm${classname} \\&\\& property == $vpi) {
    const $classname* const o = (const $classname*)(obj);
    return (o->[string toupper ${vpi} 0 0]().empty() || o->[string toupper ${vpi} 0 0]() == o->VpiName())
        ? 0
        : (PLI_BYTE8*) o->[string toupper ${vpi} 0 0]().c_str();
  }"
        } else {
            append vpi_get_str_body "
  if (handle->type == uhdm${classname} \\&\\& property == $vpi) {
    const $classname* const o = (const $classname*)(obj);
    return (PLI_BYTE8*) (o->[string toupper ${vpi} 0 0]().empty() ? 0 : o->[string toupper ${vpi} 0 0]().c_str());
  }"
        }
    }
    return $vpi_get_str_body
}

proc printVpiListener {classname vpi type card} {
    global VPI_LISTENERS VPI_LISTENERS_HEADER VPI_ANY_LISTENERS MODEL_TYPE
    if {$card == 0} {
        set VPI_LISTENERS_HEADER($classname) "void listen_${classname}(vpiHandle object, UHDM::VpiListener* listener, UHDM::VisitedContainer* visited);
"
        if {$MODEL_TYPE($classname) != "class_def"} {
            set VPI_ANY_LISTENERS($classname) "  case uhdm${classname} :
    listen_${classname}(object, listener, visited);
    break;
"
        }

        set VPI_LISTENERS($classname) "void UHDM::listen_${classname}(vpiHandle object, VpiListener* listener, UHDM::VisitedContainer* visited) \{
  ${classname}* d = (${classname}*) ((const uhdm_handle*)object)->object;
  const bool alreadyVisited = (visited->find(d) != visited->end());
  visited->insert(d);
  const BaseClass* parent = d->VpiParent();
  vpiHandle parent_h = parent ? NewVpiHandle(parent) : 0;
  listener->enter[string toupper ${classname} 0 0](d, parent, object, parent_h);
  if (!alreadyVisited) \{
"
        return
    }
    if {($vpi == "vpiParent") || ($vpi == "vpiInstance") || ($vpi == "vpiExtends")} {
        # To prevent infinite loops in visitors as these 3 relations are pointing upward in the tree
        return
    }
    if {(($vpi == "vpiModule") || ($vpi == "vpiInterface")) && ($card == 1)} {
        # upward vpiModule, vpiInterface relation (when card == 1, pointing to the parent object) creates loops in visitors
        return
    }
    if {[regexp {func_call} $classname] && ($vpi == "vpiFunction") && ($card == 1)} {
        # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
        return
    }
    if {[regexp {task_call} $classname] && ($vpi == "vpiTask") && ($card == 1)} {
        # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
        return
    }
    set vpi_listener ""
    if ![info exist VPI_LISTENERS($classname)] {
        set vpi_listener "    vpiHandle itr;
"
    } else {
        if ![regexp "vpiHandle itr;" $VPI_LISTENERS($classname)] {
            set vpi_listener "    vpiHandle itr;
"
        }
    }

    if {$card == 1} {
        append vpi_listener "    itr = vpi_handle($vpi,object);
    if (itr) {
      listen_any (itr, listener, visited);
      vpi_free_object(itr);
    }
"
    } else {
        append vpi_listener "    itr = vpi_iterate($vpi,object);
    if (itr) {
      while (vpiHandle obj = vpi_scan(itr) ) {
        listen_any (obj, listener, visited);
        vpi_free_object(obj);
      }
      vpi_free_object(itr);
    }
"
    }

    append VPI_LISTENERS($classname) $vpi_listener
}

proc closeVpiListener {classname} {
    global VPI_LISTENERS
    append  VPI_LISTENERS($classname) "  \}
  listener->leave[string toupper ${classname} 0 0](d, parent, object, parent_h);
  vpi_release_handle(parent_h);
\}

"
}

proc printClassListener {classname} {
    global CLASS_LISTENER
    set listener "    virtual void enter[string toupper ${classname} 0 0](const ${classname}* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle) { }
"
    append listener "    virtual void leave[string toupper ${classname} 0 0](const ${classname}* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle) { }

"
    set CLASS_LISTENER($classname) $listener
}

proc printVpiVisitor {classname vpi card} {
    global VISITOR_RELATIONS

    set vpi_visitor ""
    if ![info exist VISITOR_RELATIONS($classname)] {
        set vpi_visitor "    vpiHandle itr;
"
    } else {
        if ![regexp "vpiHandle itr;" $VISITOR_RELATIONS($classname)] {
            set vpi_visitor "    vpiHandle itr;
"
        }
    }

    if {($vpi == "vpiParent") && ($classname !="part_select")} {
        return
    }

    set shallowVisit ""
    if {[regexp {func_call} $classname] && ($vpi == "vpiFunction") && ($card == 1)} {
        # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
        set shallowVisit ", true"
    }
    if {[regexp {task_call} $classname] && ($vpi == "vpiTask") && ($card == 1)} {
        # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
        set shallowVisit ", true"
    }

    if {$card == 1} {
        # Prevent loop in Standard VPI
        if {($vpi != "vpiModule") && ($vpi != "vpiInterface")} {
            append vpi_visitor "    itr = vpi_handle($vpi,obj_h);
    visit_object(itr, subobject_indent, \"$vpi\", visited, out $shallowVisit);
    release_handle(itr);
"
        }
    } else {
        if {$classname == "design"} {
            append vpi_visitor "    if (indent == 0) visited->clear();
"
        }
        # Prevent loop in Standard VPI
        if {$vpi != "vpiUse"} {
            append vpi_visitor "    itr = vpi_iterate($vpi,obj_h);
    while (vpiHandle obj = vpi_scan(itr) ) {
      visit_object(obj, subobject_indent, \"$vpi\", visited, out $shallowVisit);
      release_handle(obj);
    }
    release_handle(itr);
"
        }
    }

    append VISITOR_RELATIONS($classname) $vpi_visitor
}

proc makeVpiName { classname } {
    set vpiclasstype $classname
    set vpiclasstype vpi[string toupper $vpiclasstype 0 0]
    for {set i 0} {$i < [string length $vpiclasstype]} {incr i} {
        if {[string index $vpiclasstype $i] == "_"} {
            set vpiclasstype [string toupper $vpiclasstype [expr $i +1] [expr $i +1]]
        }
    }
    regsub -all "_" $vpiclasstype "" vpiclasstype

    set vpiName $vpiclasstype
    # Exceptions where the vpi_user relation does not match the class name
    if {$vpiName == "vpiForkStmt"} {
        set vpiName "vpiFork"
    } elseif {$vpiName == "vpiForStmt"} {
        set vpiName "vpiFor"
    } elseif {$vpiName == "vpiIoDecl"} {
        set vpiName "vpiIODecl"
    } elseif {$vpiName == "vpiClockingIoDecl"} {
        set vpiName "vpiClockingIODecl"
    } elseif {$vpiName == "vpiTfCall"} {
        set vpiName "vpiSysTfCall"
    } elseif {$vpiName == "vpiAtomicStmt"} {
        set vpiName "vpiStmt"
    } elseif {$vpiName == "vpiAssertStmt"} {
        set vpiName "vpiAssert"
    } elseif {$vpiName == "vpiClockedProperty"} {
        set vpiName "vpiClockedProp"
    } elseif {$vpiName == "vpiIfStmt"} {
        set vpiName "vpiIf"
    } elseif {$vpiName == "vpiWhileStmt"} {
        set vpiName "vpiWhile"
    } elseif {$vpiName == "vpiCaseStmt"} {
        set vpiName "vpiCase"
    } elseif {$vpiName == "vpiContinueStmt"} {
        set vpiName "vpiContinue"
    } elseif {$vpiName == "vpiBreakStmt"} {
        set vpiName "vpiBreak"
    } elseif {$vpiName == "vpiReturnStmt"} {
        set vpiName "vpiReturn"
    } elseif {$vpiName == "vpiProcessStmt"} {
        set vpiName "vpiProcess"
    } elseif {$vpiName == "vpiForeverStmt"} {
        set vpiName "vpiForever"
    } elseif {$vpiName == "vpiConstrForeach"} {
        set vpiName "vpiConstrForEach"
    } elseif {$vpiName == "vpiFinalStmt"} {
        set vpiName "vpiFinal"
    } elseif {$vpiName == "vpiWaitStmt"} {
        set vpiName "vpiWait"
    } elseif {$vpiName == "vpiThreadObj"} {
        set vpiName "vpiThread"
    } elseif {$vpiName == "vpiSwitchTran"} {
        set vpiName "vpiSwitch"
    }

    return $vpiName
}

proc printScanBody { name classname type card } {
    set vpi_scan_body ""
    if {$card == "any"} {
        append vpi_scan_body "
  if (handle->type == uhdm${name}) {
    VectorOf${type}* the_vec = (VectorOf${type}*)vect;
    if (handle->index < the_vec->size()) {
      uhdm_handle* h = new uhdm_handle(((BaseClass*)the_vec->at(handle->index))->UhdmType(), the_vec->at(handle->index));
      handle->index++;
      return (vpiHandle) h;
    }
  }"
    }
    return $vpi_scan_body
}

proc defineType { def name vpiType } {
    global ID OBJECTID DEFINE_ID DEFINE_NAME
    set id ""
    set define ""
    if [info exist ID($name)] {
        set id $ID($name)
        if {$def != 0} {
            #set define "$name = $id,"
            set DEFINE_ID($id) $name
            set DEFINE_NAME($name) $id
        }
    } elseif [info exist ID($vpiType)] {
        set id $ID($vpiType)
    } else {
        set id $OBJECTID
        incr OBJECTID
        set ID($name) $id
        if {$def != 0} {
            set define "$name = $id,"
            set DEFINE_ID($id) $name
            set DEFINE_NAME($name) $id
        }
    }
    return [list $id $define]
}

proc generate_group_checker { model } {
    global $model BASECLASS ALL_CHILDREN GROUP_MEMBERS
    set data [subst $$model]
    set groupname [dict get $data name]
    set modeltype [dict get $data type]

    set files [list [list "[project_path]/templates/group_header.h" "[codegen_base]/headers/${groupname}.h"] \
                   [list "[project_path]/templates/group_header.cpp" "[codegen_base]/src/${groupname}.cpp"]]
    set GROUP_MEMBERS($groupname) ""
    foreach pair $files {
        foreach {input output} $pair {}

        set fid [open "$input"]
        set template [read $fid]
        close $fid

        regsub -all {<GROUPNAME>} $template $groupname template
        regsub -all {<UPPER_GROUPNAME>} $template [string toupper $groupname] template

        set checktype ""
        dict for {key val} $data {
            if {($key == "obj_ref") || ($key == "class_ref")  || ($key == "group_ref")} {
                dict for {iter content} $val {
                    set name $iter
                    lappend GROUP_MEMBERS($groupname) [list $name $key]
                    set uhdmclasstype uhdm$name
                    if {$key == "group_ref"} {
                        if ![info exist GROUP_MEMBERS($name)] {
                            puts "ERROR: Group $name unknown while processing group $groupname"
                        }
                        set members $GROUP_MEMBERS($name)
                        foreach member $members {
                            set uhdmgroupmember uhdm[lindex $member 0]
                            if {$checktype != ""} {
                                append checktype " \\&\\& "
                            }
                            append checktype "(uhdmtype != $uhdmgroupmember)"
                        }
                    } else {
                        if {$checktype != ""} {
                            append checktype " \\&\\& "
                        }
                        append checktype "(uhdmtype != $uhdmclasstype)"
                    }
                    if {$key == "class_ref"} {
                        if [info exist ALL_CHILDREN($name)] {
                            foreach child $ALL_CHILDREN($name) {
                                set name $child
                                if {$checktype != ""} {
                                    append checktype " \\&\\& "
                                }
                                set uhdmclasstype uhdm$name
                                append checktype "(uhdmtype != $uhdmclasstype)"
                            }
                        }
                    }
                }
            }
        }
        regsub -all {<CHECKTYPE>} $template $checktype template
        set_content_if_change $output $template
    }
}

proc write_vpi_listener_cpp {} {
    global VPI_LISTENERS VPI_ANY_LISTENERS

    set fid [open "[project_path]/templates/vpi_listener.cpp"]
    set listener_cpp [read $fid]
    close $fid
    set vpi_listener ""
    foreach classname [array name VPI_LISTENERS] {
        append vpi_listener $VPI_LISTENERS($classname)
    }
    set vpi_any_listener ""
    foreach classname [array name VPI_ANY_LISTENERS] {
        append vpi_any_listener $VPI_ANY_LISTENERS($classname)
    }
    regsub {<VPI_LISTENERS>} $listener_cpp $vpi_listener listener_cpp
    regsub {<VPI_ANY_LISTENERS>} $listener_cpp $vpi_any_listener listener_cpp

    set_content_if_change "[codegen_base]/src/vpi_listener.cpp" $listener_cpp
}

proc write_vpi_listener_h {} {
    global VPI_LISTENERS_HEADER

    set fid [open "[project_path]/templates/vpi_listener.h"]
    set listener_h [read $fid]
    close $fid
    set vpi_listener ""
    foreach classname [array name VPI_LISTENERS_HEADER] {
        append vpi_listener $VPI_LISTENERS_HEADER($classname)
    }
    regsub {<VPI_LISTENERS_HEADER>} $listener_h $vpi_listener listener_h

    set_content_if_change "[codegen_base]/headers/vpi_listener.h" $listener_h
}

proc write_uhdm_forward_decl {} {
    global VPI_LISTENERS_HEADER

    set fid [open "[project_path]/templates/uhdm_forward_decl.h"]
    set uhdm_forward_h [read $fid]
    close $fid
    set forward_declaration ""
    foreach classname [array name VPI_LISTENERS_HEADER] {
        append forward_declaration "class $classname;\n"
    }
    regsub {<UHDM_FORWARD_DECL>} $uhdm_forward_h $forward_declaration uhdm_forward_h

    set_content_if_change "[codegen_base]/headers/uhdm_forward_decl.h" $uhdm_forward_h
}

proc write_VpiListener_h {} {
    global CLASS_LISTENER

    set fid [open "[project_path]/templates/VpiListener.h"]
    set listener_content [read $fid]
    close $fid
    set vpi_listener ""
    foreach classname [array name CLASS_LISTENER] {
        append vpi_listener $CLASS_LISTENER($classname)
    }
    regsub {<VPI_LISTENER_METHODS>} $listener_content $vpi_listener listener_content
    set_content_if_change "[codegen_base]/headers/VpiListener.h" $listener_content
}

set SHORT_VISITOR_LIST { class_obj
    assertion
    immediate_assert
    tchk_term
    primitive
    clocked_property
    enum_const
    attribute
    task_call
    parameter
    program_array
    chandle_var
    return_stmt
    switch_array
    cont_assign
    while_stmt
    property_typespec
    fork_stmt
    repeat
    assert_stmt
    logic_typespec
    property_inst
    gen_var
    bit_typespec
    packed_array_net
    byte_var
    break_stmt
    sys_func_call
    typespec
    modport
    enum_var
    event_typespec
    named_event
    int_typespec
    forever_stmt
    interface_tf_decl
    final_stmt
    repeat_control
    packed_array_typespec
    port_bit
    short_real_var
    let_decl
    immediate_assume
    union_typespec
    param_assign
    integer_var
    method_func_call
    user_systf
    prim_term
    string_var
    property_spec
    delay_control
    expect_stmt
    operation
    class_typespec
    short_int_var
    event_control
    case_item
    gen_scope
    path_term
    property_decl
    assign_stmt
    tf_call
    sequence_typespec
    net_bit
    udp_defn
    short_int_typespec
    function
    sequence_inst
    delay_term
    named_fork
    time_var
    byte_typespec
    ports
    distribution
    initial
    string_typespec
    int_var
    do_while
    case_stmt
    sys_task_call
    package
    mod_path
    real_var
    atomic_stmt
    if_stmt
    virtual_interface_var
    if_else
    foreach_stmt
    alias_stmt
    release
    type_parameter
    class_defn
    null_stmt
    time_typespec
    enum_net
    module_array
    continue_stmt
    method_task_call
    task_func
    packed_array_var
    for_stmt
    func_call
    def_param
    array_var
    force
    scope
    typespecs
    sequence_decl
    named_begin
    spec_param
    instance_array
    integer_net
    reg_array
    constraint
    interface_typespec
    begin
    void_typespec
    cont_assign_bit
    class_var
    deassign
    udp_array
    gate_array
    unsupported_expr
    real_typespec
    program
    unsupported_stmt
    union_var
    always
    gen_scope_array
    integer_typespec
    tchk
    long_int_var
    array_typespec
    task
    named_event_array
    clocking_block
    time_net
    multiclock_sequence_expr
    concurrent_assertions
    immediate_cover
    long_int_typespec
    short_real_typespec
    primitive_array
    interface_array
    io_decl
    var_bit
    bit_var
    design
}


proc write_vpi_visitor_cpp {} {
    global VISITOR VISITOR_RELATIONS SHORT_VISITOR_LIST MODEL_TYPE

    foreach item $SHORT_VISITOR_LIST {
        set filter($item) 1
    }

    set fid [open "[project_path]/templates/vpi_visitor.cpp"]
    set visitor_cpp [read $fid]
    close $fid
    set vpi_visitor ""
    foreach classname [array name VISITOR] {
        #if [info exist filter($classname)] {
        #    continue
        #}
        if {$MODEL_TYPE($classname) == "class_def"} {
            # Skip all classes
            continue
        }

        set vpiName [makeVpiName $classname]
        set relations ""
        if [info exist VISITOR_RELATIONS($classname)] {
            set relations $VISITOR_RELATIONS($classname)
        }

        append vpi_visitor "  if (objectType == $vpiName) {
$VISITOR($classname)
$relations
    return;
  }
"
    }
    regsub {<OBJECT_VISITORS>} $visitor_cpp $vpi_visitor visitor_cpp
    set_content_if_change "[codegen_base]/src/vpi_visitor.cpp" $visitor_cpp
}

proc write_capnp { capnp_schema_all capnp_root_schema } {
    set fid [open "[project_path]/templates/UHDM.capnp"]
    set capnp_content [read $fid]
    close $fid
    regsub {<CAPNP_SCHEMA>} $capnp_content $capnp_schema_all capnp_content
    regsub {<CAPNP_ROOT_SCHEMA>} $capnp_content $capnp_root_schema capnp_content

    return [set_content_if_change "[codegen_base]/src/UHDM.capnp" $capnp_content]
}

proc write_uhdm_h { headers} {
    global DEFINE_ID uhdm_name_map

    set fid [open "[project_path]/templates/uhdm.h"]
    set uhdm_content [read $fid]
    close $fid

    set name_id_map "\nstd::string UhdmName(UHDM_OBJECT_TYPE type) \{
      switch (type) \{
"
    foreach id [array names DEFINE_ID] {
        set printed_name $DEFINE_ID($id)
        regsub uhdm $printed_name "" printed_name
        append name_id_map "case $id: return \"$printed_name\";\n"
    }
    append name_id_map "default: return \"NO TYPE\";
\}
\}\n"

    append uhdm_name_map $name_id_map

    regsub -all {<INCLUDE_FILES>} $uhdm_content $headers uhdm_content
    set_content_if_change "[codegen_base]/headers/uhdm.h" $uhdm_content
}

proc write_uhdm_types_h { defines } {
    set fid [open "[project_path]/templates/uhdm_types.h"]
    set uhdm_content [read $fid]
    close $fid
    regsub -all {<DEFINES>} $uhdm_content $defines uhdm_content
    set_content_if_change "[codegen_base]/headers/uhdm_types.h" $uhdm_content
}

proc write_containers_h { containers } {
    set fid [open "[project_path]/templates/containers.h"]
    set container_content [read $fid]
    close $fid
    regsub -all {<CONTAINERS>} $container_content $containers container_content
    set_content_if_change "[codegen_base]/headers/containers.h" $container_content
}

proc update_vpi_inst { baseclass classname lvl } {
    global VISITOR

    upvar $lvl vpi_get_str_body_inst vpi_get_str_body_inst_l
    upvar $lvl vpi_get_body_inst vpi_get_body_inst_l
    upvar $lvl vpi_get_str_body vpi_get_str_body_l
    upvar $lvl vpi_get_body vpi_get_body_l vpi_get_value_body vpi_get_value_body_l vpi_get_delay_body vpi_get_delay_body_l

    if [info exist vpi_get_str_body_inst_l($baseclass)] {
        foreach inst $vpi_get_str_body_inst_l($baseclass) {
            append vpi_get_str_body_l [printGetStrBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
            append VISITOR($classname) [printGetStrVisitor $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
        }
    }
    if [info exist vpi_get_body_inst_l($baseclass)] {

        set vpi_case_body ""
        foreach inst $vpi_get_body_inst_l($baseclass) {
            append vpi_case_body [printGetBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
        }
        # The case body can be empty if all propeerties have been handled
        # in the base class. So only if non-empty, add the if/switch
        if {$vpi_case_body != ""} {
            append vpi_get_body_l [printGetBodyPrefix $classname] $vpi_case_body [printGetBodySuffix]
        }

        foreach inst $vpi_get_body_inst_l($baseclass) {
            append vpi_get_value_body_l [printGetValueBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
            append vpi_get_delay_body_l [printGetDelayBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
            append VISITOR($classname) [printGetVisitor $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
        }
    }
}

proc process_baseclass { baseclass classname modeltype capnpIndex } {
    global SAVE RESTORE BASECLASS vpi_iterator vpi_handle_body vpi_iterate_body vpi_handle_by_name_body
    upvar capnp_schema capnp_schema_l capnp_schema_all capnp_schema_all_l
    upvar vpi_iterate_body_all vpi_iterate_body_all_l vpi_handle_body_all vpi_handle_body_all_l
    upvar vpi_handle_by_name_body_all vpi_handle_by_name_body_all_l
    set idx $capnpIndex

    set Classname [string toupper $classname 0 0]
    regsub -all  {_} $Classname "" Classname

    while {$baseclass != ""} {

        # Capnp schema
        if {$modeltype != "class_def"} {
            foreach member $capnp_schema_l($baseclass) {
                foreach {name type} $member {}
                append capnp_schema_all_l "$name @$idx :$type;\n"
                incr idx
            }
        }
        # Save
        set save ""
        foreach line [split $SAVE($baseclass) "\n"] {
            set base $baseclass
            set tmp $line
            regsub -all  {_} $baseclass "" base
            regsub -all " [string toupper $base 0 0]s" $line " ${Classname}s" tmp
            append save "$tmp\n"
        }
        append SAVE($classname) $save

        # Restore
        set restore $RESTORE($baseclass)
        regsub -all " ${baseclass}Maker" $RESTORE($baseclass) " ${classname}Maker" restore

        append RESTORE($classname) $restore

        # VPI
        update_vpi_inst $baseclass $classname 2

        if [info exist vpi_iterate_body($baseclass)] {
            set vpi_iterate $vpi_iterate_body($baseclass)
            regsub -all "= uhdm$baseclass" $vpi_iterate "= uhdm$classname" vpi_iterate
            append vpi_iterate_body_all_l $vpi_iterate
        }

        if [info exist vpi_handle_body($baseclass)] {
            set vpi_handle $vpi_handle_body($baseclass)
            regsub -all "= uhdm$baseclass" $vpi_handle "= uhdm$classname" vpi_handle
            regsub -all "$baseclass\\*" $vpi_handle "$classname\*" vpi_handle
            append vpi_handle_body_all_l $vpi_handle
        }

        if [info exist vpi_handle_by_name_body($baseclass)] {
            set vpi_handle_by_name $vpi_handle_by_name_body($baseclass)
            regsub -all "= uhdm$baseclass" $vpi_handle_by_name "= uhdm$classname" vpi_handle_by_name
            append vpi_handle_by_name_body_all_l $vpi_handle_by_name
        }

        if [info exist vpi_iterator($baseclass)] {
            foreach {vpi type card} $vpi_iterator($baseclass) {
                printVpiVisitor $classname $vpi $card
                printVpiListener $classname $vpi $type $card
            }
        }

        # Parent class
        if [info exist BASECLASS($baseclass)] {
            set baseclass $BASECLASS($baseclass)
        } else {
            set baseclass ""
        }
    }

    return idx
}

proc generate_code { models } {
    global ID BASECLASS DEFINE_ID SAVE RESTORE working_dir methods_cpp group_headers VISITOR VISITOR_RELATIONS CLASS_LISTENER
    global VPI_LISTENERS VPI_LISTENERS_HEADER VPI_ANY_LISTENERS MODEL_TYPE
    global uhdm_name_map headers vpi_handle_body_all vpi_handle_body vpi_iterator vpi_iterate_body vpi_handle_by_name_body vpi_handle_by_name_body_all
    global tcl_platform myProjetPathNoNormalize

    log "=========="
    file mkdir [exec_path]/src
    file mkdir [exec_path]/headers
    set fid [open "[project_path]/templates/class_header.h"]
    set template_content [read $fid]
    close $fid

    if { $tcl_platform(platform) == "windows" } {
        set exeext ".exe"
    } else {
        set exeext ""
    }

    set vpi_iterate_body_all ""
    set vpi_handle_by_name_body_all ""
    set vpi_scan_body ""
    set vpi_handle_body_all ""
    set vpi_get_body ""
    set vpi_get_value_body ""
    set vpi_get_delay_body ""
    set vpi_get_str_body ""
    set headers ""
    set defines ""
    set typedefs ""
    set containers ""
    set capnp_save ""
    set capnpRootSchemaIndex 2
    set classes ""
    set factory_object_type_map ""
    set factory_purge ""
    set capnp_schema_all ""
    set capnp_root_schema ""
    foreach model $models {
        global $model
        log "** $model **"
        set data [subst $$model]
        set classname [dict get $data name]
        set template $template_content
        set modeltype [dict get $data type]
        set MODEL_TYPE($classname) $modeltype
        set baseclass ""
        set methods($classname) ""
        set members($classname) ""
        set group_headers ""
        set forward_declare ""
        set SAVE($classname) ""
        set RESTORE($classname) ""
        set capnp_schema($classname) ""
        set vpi_iterate_body($classname) ""
        set vpi_iterator($classname) ""

        if {$modeltype == "class_def"} {
            regsub -all {<FINAL_CLASS>} $template "" template
            regsub -all {<FINAL_DESTRUCTOR>} $template "" template
            regsub -all {<VIRTUAL>} $template "virtual " template
            regsub -all {<OVERRIDE_OR_FINAL>}  $template "override" template
            regsub -all {<DISABLE_OBJECT_FACTORY>} $template "#if 0 // This class cannot be instantiated" template
            regsub -all {<END_DISABLE_OBJECT_FACTORY>} $template "#endif" template
        } else {
            regsub -all {<FINAL_CLASS>} $template "final" template
            regsub -all {<FINAL_DESTRUCTOR>} $template "final" template
            regsub -all {<VIRTUAL>} $template "virtual " template
            regsub -all {<OVERRIDE_OR_FINAL>}  $template "final" template
            regsub -all {<DISABLE_OBJECT_FACTORY>} $template "" template
            regsub -all {<END_DISABLE_OBJECT_FACTORY>} $template "" template
        }
        set Classname [string toupper $classname 0 0]
        regsub -all  {_} $Classname "" Classname

        append headers "#include \"headers/$classname.h\"\n"

        if {$modeltype == "group_def"} {
            foreach {id define} [defineType 1 uhdm${classname} ""] {}
            if {$define != ""} {
                append defines "  $define\n"
            }
            generate_group_checker $model
            continue
        }

        log "Generating headers/$classname.h"
        if {$modeltype != "class_def"} {
            append factories "    ${classname}Factory ${classname}Maker;\n"
            append factories_methods "    ${classname}* Make[string toupper ${classname} 0 0] () { ${classname}* tmp = ${classname}Maker.Make(); tmp->SetSerializer(this); tmp->UhdmId(objId_++); return tmp;}\n"
        }
        append factories "    VectorOf${classname}Factory ${classname}VectMaker;\n"
        append factories_methods "    std::vector<${classname}*>* Make[string toupper ${classname} 0 0]Vec () { return ${classname}VectMaker.Make();}\n"
        if {$modeltype != "class_def"} {
            append factory_object_type_map "  case uhdm${classname}: return ${classname}Maker.objects_\[index\];\n"
        }
        lappend classes $classname

        regsub -all {<CLASSNAME>} $template $classname template
        regsub -all {<UPPER_CLASSNAME>} $template [string toupper $classname] template
        foreach {id define} [defineType 1 uhdm${classname} ""] {}
        if {$define != ""} {
            append defines "  $define\n"
        }
        printClassListener $classname
        printVpiListener $classname $classname $classname 0


        if {$modeltype == "class_def"} {
            # DeepClone() not implemented for class_def; just declare to narrow the covariant return type.
            append methods($classname) "\n  ${classname}* DeepClone(Serializer* serializer, ElaboratorListener* elab_listener, BaseClass* parent) const override = 0;\n"
        } else {
            # Builtin properties do not need to be specified in each models
            # Builtins: "vpiParent, Parent type, vpiFile, Id" method and field
            append methods($classname) [printMethods $classname BaseClass vpiParent 1]
            append members($classname) [printMembers BaseClass vpiParent 1]
            append methods($classname) [printMethods $classname "unsigned int" uhdmParentType 1]
            append members($classname) [printMembers "unsigned int" uhdmParentType 1]
            append methods($classname) [printMethods $classname string vpiFile 1]
            append members($classname) [printMembers string vpiFile 1]
            lappend vpi_get_str_body_inst($classname) [list $classname string vpiFile 1]
            append methods($classname) [printMethods $classname "unsigned int" uhdmId 1]
            append members($classname) [printMembers "unsigned int" uhdmId 1]
            if [regexp {_call} ${classname}] {
                append methods($classname) "\n  tf_call* DeepClone(Serializer* serializer, ElaboratorListener* elab_listener, BaseClass* parent) const override;\n"
            } else {
                append methods($classname) "\n  ${classname}* DeepClone(Serializer* serializer, ElaboratorListener* elab_listener, BaseClass* parent) const override;\n"
            }
            append vpi_handle_body($classname) [printGetHandleBody $classname BaseClass vpiParent vpiParent 1]
            lappend capnp_schema($classname) [list vpiParent UInt64]
            lappend capnp_schema($classname) [list uhdmParentType UInt64]
            lappend capnp_schema($classname) [list vpiFile UInt64]
            lappend capnp_schema($classname) [list vpiLineNo UInt32]
            lappend capnp_schema($classname) [list vpiColumnNo UInt16]
            lappend capnp_schema($classname) [list vpiEndLineNo UInt32]
            lappend capnp_schema($classname) [list vpiEndColumnNo UInt16]
            lappend capnp_schema($classname) [list uhdmId UInt64]
            append capnp_root_schema "  factory${Classname} @${capnpRootSchemaIndex} :List($Classname);\n"
            incr capnpRootSchemaIndex
            append SAVE($classname) "    ${Classname}s\[index\].setVpiParent(GetId(obj->VpiParent()));\n"
            append SAVE($classname) "    ${Classname}s\[index\].setUhdmParentType(obj->UhdmParentType());\n"
            append SAVE($classname) "    ${Classname}s\[index\].setVpiFile(obj->GetSerializer()->symbolMaker.Make(obj->VpiFile()));\n"
            append SAVE($classname) "    ${Classname}s\[index\].setVpiLineNo(obj->VpiLineNo());\n"
            append SAVE($classname) "    ${Classname}s\[index\].setVpiColumnNo(obj->VpiColumnNo());\n"
            append SAVE($classname) "    ${Classname}s\[index\].setVpiEndLineNo(obj->VpiEndLineNo());\n"
            append SAVE($classname) "    ${Classname}s\[index\].setVpiEndColumnNo(obj->VpiEndColumnNo());\n"
            append SAVE($classname) "    ${Classname}s\[index\].setUhdmId(obj->UhdmId());\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->UhdmParentType(obj.getUhdmParentType());\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->VpiParent(GetObject(obj.getUhdmParentType(),obj.getVpiParent()-1));\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->VpiFile(symbolMaker.GetSymbol(obj.getVpiFile()));\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->VpiLineNo(obj.getVpiLineNo());\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->VpiColumnNo(obj.getVpiColumnNo());\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->VpiEndLineNo(obj.getVpiEndLineNo());\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->VpiEndColumnNo(obj.getVpiEndColumnNo());\n"
            append RESTORE($classname) "   ${classname}Maker.objects_\[index\]->UhdmId(obj.getUhdmId());\n"
        }

        set indTmp 0
        set type_specified 0
        set ordered [dict get $data "ordered"]
        if [info exist TREATED] {
            unset TREATED
        }
        foreach ord $ordered {
        dict for {key val} $data {
            if {$key == "properties"} {
                dict for {prop conf} $val {
                    set name [dict get $conf name]
                    if {$prop != $ord} {
                        continue
                    }
                    if [info exist TREATED($prop)] {
                        continue
                    }
                    set TREATED($prop) 1
                    set vpi  [dict get $conf vpi]
                    set type [dict get $conf type]
                    set card [dict get $conf card]
                    if {$prop == "type"} {
                        set type_specified 1
                        append methods($classname) "\n    $type [string toupper ${vpi} 0 0]() const final { return $name; }\n"
                        lappend vpi_get_body_inst($classname) [list $classname $type $vpi $card]
                        continue
                    }

                    if {$type != "any"} {
                        append containers [printTypeDefs $type $card]
                    }

                    # properties are already defined in vpi_user.h, no need to redefine them
                    append methods($classname) [printMethods $classname $type $vpi $card]
                    append members($classname) [printMembers $type $vpi $card]
                    lappend vpi_get_body_inst($classname) [list $classname $type $vpi $card]
                    lappend vpi_get_str_body_inst($classname) [list $classname $type $vpi $card]
                    lappend capnp_schema($classname) [printCapnpSchema $type $vpi $card]

                    set Vpi [string toupper $vpi 0 0]
                    regsub -all  {_} $Vpi "" Vpi
                    if {$type == "string" || $type == "value" || $type == "delay"} {
                        append SAVE($classname) "    ${Classname}s\[index\].set${Vpi}(obj->GetSerializer()->symbolMaker.Make(obj->[string toupper ${vpi} 0 0]()));\n"
                        append RESTORE($classname) "    ${classname}Maker.objects_\[index\]->[string toupper ${vpi} 0 0](symbolMaker.GetSymbol(obj.get${Vpi}()));\n"
                    } else {
                        append SAVE($classname) "    ${Classname}s\[index\].set${Vpi}(obj->[string toupper ${vpi} 0 0]());\n"
                        append RESTORE($classname) "    ${classname}Maker.objects_\[index\]->[string toupper ${vpi} 0 0](obj.get${Vpi}());\n"
                    }
                }

            }
            if {$key == "extends"} {
                dict for {base_type baseclass} $val {
                    regsub -all {<EXTENDS>} $template $baseclass template
                }
            }
            if {($key == "class") || ($key == "obj_ref") || ($key == "class_ref") || ($key == "group_ref")} {
                dict for {iter content} $val {
                    set name $iter
                    if {$name != $ord} {
                        continue
                    }
                    if [info exist TREATED($name)] {
                        continue
                    }
                    set TREATED($name) 1
                    set vpi  [dict get $content vpi]
                    set type [dict get $content type]
                    set card [dict get $content card]
                    set id   [dict get $content id]
                    set plural ""
                    if {$card == "any"} {
                        if ![regexp {s$} $name] {
                            set plural "s"
                        }
                    }
                    set name "${name}${plural}"

                    set real_type $type
                    if {$key == "group_ref"} {
                        set type "any"
                    }

                    if {$type != "any" && $card == 1} {
                        append forward_declare "class $type;\n"
                    }

                    append containers [printTypeDefs $type $card]
                    # define access properties (allModules...)
                    foreach {id define} [defineType 1 uhdm${name} ""] {}
                    if {$define != ""} {
                        append defines "  $define\n"
                    }
                    append methods($classname) [printMethods $classname $type $name $card $real_type]
                    printVpiListener $classname $vpi $type $card
                    append members($classname) [printMembers $type $name $card]
                    append vpi_iterate_body($classname) [printIterateBody $name $classname $vpi $card]
                    append vpi_handle_by_name_body($classname) [printGetHandleByNameBody $name $classname $vpi $card]
                    append vpi_iterator($classname) "[list $vpi $type $card] "
                    append vpi_scan_body [printScanBody $name $classname $type $card]
                    append vpi_handle_body($classname) [printGetHandleBody $classname uhdm${type} $vpi $name $card]

                    set Type [string toupper $type 0 0]
                    regsub -all  {_} $Type "" Type
                    set Name $name
                    regsub -all  {_} $name "" Name

                    if {$key == "class_ref" || $key == "group_ref"} {
                        set obj_key ObjIndexType
                    } else {
                        set obj_key UInt64
                    }
                    lappend capnp_schema($classname) [printCapnpSchema $obj_key $Name $card]
                    if {$card == 1} {
                        if {$key == "class_ref" || $key == "group_ref"} {
                            append SAVE($classname) "  if (obj->[string toupper ${name} 0 0]()) {\n"
                            append SAVE($classname) "    ::ObjIndexType::Builder tmp$indTmp = ${Classname}s\[index\].get[string toupper ${Name} 0 0]();\n"
                            append SAVE($classname) "    tmp${indTmp}.setIndex(GetId(((BaseClass*) obj->[string toupper ${name} 0 0]())));\n"
                            append SAVE($classname) "    tmp${indTmp}.setType(((BaseClass*)obj->[string toupper ${name} 0 0]())->UhdmType());\n  }"
                            append RESTORE($classname) "     ${classname}Maker.objects_\[index\]->[string toupper ${name} 0 0]((${type}*)GetObject(obj.get[string toupper ${Name} 0 0]().getType(),obj.get[string toupper ${Name} 0 0]().getIndex()-1));\n"
                            incr indTmp
                        } else {
                            append SAVE($classname) "    ${Classname}s\[index\].set[string toupper ${Name} 0 0](GetId(obj->[string toupper ${name} 0 0]()));\n"
                            append RESTORE($classname) "    if (obj.get[string toupper ${Name} 0 0]())
      ${classname}Maker.objects_\[index\]->[string toupper ${name} 0 0](${type}Maker.objects_\[obj.get[string toupper ${Name} 0 0]()-1\]);\n"
                        }
                    } else {

                        if {$key == "class_ref" || $key == "group_ref"} {
                            set obj_key ::ObjIndexType
                        } else {
                            set obj_key ::uint64_t
                        }
                        append SAVE($classname) "
    if (obj->[string toupper ${name} 0 0]()) {
      ::capnp::List<$obj_key>::Builder [string toupper ${Name} 0 0]s = ${Classname}s\[index\].init[string toupper ${Name} 0 0](obj->[string toupper ${name} 0 0]()->size());
      for (unsigned int ind = 0; ind < obj->[string toupper ${name} 0 0]()->size(); ind++) {\n"
                        if {$key == "class_ref" || $key == "group_ref"} {
                            append SAVE($classname) "        ::ObjIndexType::Builder tmp = [string toupper ${Name} 0 0]s\[ind\];\n"
                            append SAVE($classname) "        tmp.setIndex(GetId(((BaseClass*) (*obj->[string toupper ${name} 0 0]())\[ind\])));\n"
                            append SAVE($classname) "        tmp.setType(((BaseClass*)((*obj->[string toupper ${name} 0 0]())\[ind\]))->UhdmType());"
                        } else {
                            append SAVE($classname) "        [string toupper ${Name} 0 0]s.set(ind, GetId((*obj->[string toupper ${name} 0 0]())\[ind\]));"
                        }
                        append SAVE($classname) "\n      }
    }
"
                        append RESTORE($classname) "
    if (obj.get[string toupper ${Name} 0 0]().size()) {
      std::vector<${type}*>* vect = ${type}VectMaker.Make();
      for (unsigned int ind = 0; ind < obj.get[string toupper ${Name} 0 0]().size(); ind++) {\n"
                        if {$key == "class_ref" || $key == "group_ref"} {
                            append RESTORE($classname) "        vect->push_back((${type}*)GetObject(obj.get[string toupper ${Name} 0 0]()\[ind\].getType(),obj.get[string toupper ${Name} 0 0]()\[ind\].getIndex()-1));\n"
                        } else {
                            append RESTORE($classname) "        vect->push_back(${type}Maker.objects_\[obj.get[string toupper ${Name} 0 0]()\[ind\]-1\]);\n"
                        }
                        append RESTORE($classname) "      }
      ${classname}Maker.objects_\[index\]->[string toupper ${name} 0 0](vect);
    }
"
                    }
                }
            }
        }
        }

        if {($type_specified == 0) && ($modeltype == "obj_def")} {
            set vpiclasstype [makeVpiName $classname]
            append methods($classname) "\n  unsigned int VpiType() const final { return $vpiclasstype; }\n"
            lappend vpi_get_body_inst($classname) [list $classname "unsigned int" vpiType 1]

        }
        regsub -all {<METHODS>} $template $methods($classname) template
        regsub -all {<MEMBERS>} $template $members($classname) template
        regsub -all {<EXTENDS>} $template BaseClass template
        regsub -all {<GROUP_HEADER_DEPENDENCY>} $template $group_headers template
        regsub -all {<TYPE_FORWARD_DECLARE>} $template $forward_declare template


        set_content_if_change "[codegen_base]/headers/$classname.h" $template

        # VPI
        update_vpi_inst $classname $classname 1

        set capnpIndex 0
        if {($modeltype != "class_def") && ($modeltype != "group_def")} {
            append capnp_schema_all "struct $Classname \{\n"
            foreach member $capnp_schema($classname) {
                foreach {name type} $member {}
                append capnp_schema_all "$name @$capnpIndex :$type;\n"
                incr capnpIndex
            }
        }

        if [info exist vpi_iterate_body($classname)] {
            append vpi_iterate_body_all $vpi_iterate_body($classname)
        }
        if [info exist vpi_handle_body($classname)] {
            append vpi_handle_body_all $vpi_handle_body($classname)
        }
        if [info exist vpi_handle_by_name_body($classname)] {
            append vpi_handle_by_name_body_all $vpi_handle_by_name_body($classname)
        }

        # process baseclass recursively
        set capnpIndex [process_baseclass $baseclass $classname $modeltype $capnpIndex]

        if {$modeltype != "class_def"} {
            append capnp_schema_all "\}\n"
        }

        closeVpiListener $classname

    } ; #foreach model

    # uhdm.h
    write_uhdm_h $headers

    # uhdm_types.h
    write_uhdm_types_h $defines

    # containers.h
    write_containers_h $containers

    # vpi_user.cpp
    set fid [open "[project_path]/templates/vpi_user.cpp" ]
    set vpi_user [read $fid]
    close $fid
    regsub {<HEADERS>} $vpi_user $headers vpi_user
    regsub {<VPI_HANDLE_BY_NAME_BODY>} $vpi_user $vpi_handle_by_name_body_all vpi_user
    regsub {<VPI_ITERATE_BODY>} $vpi_user $vpi_iterate_body_all vpi_user
    regsub {<VPI_SCAN_BODY>} $vpi_user $vpi_scan_body vpi_user
    regsub {<VPI_HANDLE_BODY>} $vpi_user $vpi_handle_body_all vpi_user
    regsub -all {<VPI_GET_BODY>} $vpi_user $vpi_get_body vpi_user
    regsub -all {<VPI_GET_VALUE_BODY>} $vpi_user $vpi_get_value_body vpi_user
    regsub -all {<VPI_GET_DELAY_BODY>} $vpi_user $vpi_get_delay_body vpi_user
    regsub {<VPI_GET_STR_BODY>} $vpi_user $vpi_get_str_body vpi_user

    set_content_if_change "[codegen_base]/src/vpi_user.cpp" $vpi_user

    # UHDM.capnp
    if {[write_capnp $capnp_schema_all $capnp_root_schema]
        || ![file exists "[codegen_base]/src/UHDM.capnp.h"]} {
        log "Generating Capnp schema..."
        file delete -force [codegen_base]/src/UHDM.capnp.*
        set capnp_path [find_file $working_dir "capnpc-c++$exeext"]
        puts "capnp_path = $capnp_path"
        set capnp_path [file dirname $capnp_path]

        if { ($tcl_platform(platform) == "windows") && (![info exists ::env(MSYSTEM)]) } {
            exec -ignorestderr cmd /c "set PATH=$capnp_path;%PATH%; && cd /d [codegen_base]/src && $capnp_path/capnp.exe compile -oc++ UHDM.capnp"
        } else {
            if { $tcl_platform(platform) == "windows" } {
                exec -ignorestderr sh -c "export PATH=\$(cygpath -u -a $capnp_path):\$PATH; $capnp_path/capnp compile -oc++:. [codegen_base]/src/UHDM.capnp"
            } else {
                exec -ignorestderr sh -c "export PATH=$capnp_path:\$PATH; $capnp_path/capnp compile -oc++:. [codegen_base]/src/UHDM.capnp"
            }
        }
    }

    # BaseClass.h
    file_copy_if_change "[project_path]/templates/BaseClass.h" "[codegen_base]/headers/BaseClass.h"

    # uhdm_vpi_user
    file_copy_if_change "[project_path]/templates/uhdm_vpi_user.h" "[codegen_base]/headers/uhdm_vpi_user.h"

    # SymbolFactory.h
    file_copy_if_change "[project_path]/templates/SymbolFactory.h" "[codegen_base]/headers/SymbolFactory.h"

    # SymbolFactory.cpp
    file_copy_if_change "[project_path]/templates/SymbolFactory.cpp" "[codegen_base]/src/SymbolFactory.cpp"

    # Serializer.cpp
    set files "Serializer_save.cpp Serializer_restore.cpp vpi_uhdm.h Serializer.h"
    foreach file $files {
        set capnp_init_factories ""
        set capnp_restore_factories ""
        set capnp_save ""
        set capnp_id ""
        set factory_purge ""
        set factory_stats ""

        set fid [open "[project_path]/templates/$file"]
        set serializer_content [read $fid]
        close $fid
        foreach class $classes {
            if {($MODEL_TYPE($class) == "class_def") || ($MODEL_TYPE($class) == "group_def")} {
                continue
            }
            set Class [string toupper $class 0 0]
            regsub -all  {_} $Class "" Class
            if {$SAVE($class) != ""} {
                append capnp_save "
 ::capnp::List<$Class>::Builder ${Class}s = cap_root.initFactory${Class}(${class}Maker.objects_.size());
 index = 0;
 for (auto obj : ${class}Maker.objects_) {
$SAVE($class)
   index++;
 }"
                append capnp_init_factories "
 ::capnp::List<$Class>::Reader ${Class}s = cap_root.getFactory${Class}();
 for (unsigned ind = 0; ind < ${Class}s.size(); ind++) {
   SetId(Make[string toupper ${class} 0 0](), ind);
 }
"
                append capnp_restore_factories "
 index = 0;
 for (${Class}::Reader obj : ${Class}s) {
$RESTORE($class)
   index++;
 }
"
            }
        }

        set capnp_id ""
        foreach class $classes {
            if {$MODEL_TYPE($class) == "class_def"} {
                continue
            }
            append capnp_id "
  index = 1;
  for (auto obj : ${class}Maker.objects_) {
    SetId(obj, index);
    index++;
  }"

            append factory_purge "
  for (auto obj : ${class}Maker.objects_) {
    delete obj;
  }
  ${class}Maker.objects_.clear();
"

            append factory_stats "
   stats.insert(std::make_pair(\"$class\", ${class}Maker.objects_.size()));
"
        }


        regsub {<FACTORIES>} $serializer_content $factories serializer_content
        regsub {<FACTORIES_METHODS>} $serializer_content $factories_methods serializer_content
        regsub {<METHODS_CPP>} $serializer_content $methods_cpp serializer_content

        regsub {<UHDM_NAME_MAP>} $serializer_content $uhdm_name_map serializer_content
        regsub {<FACTORY_PURGE>} $serializer_content $factory_purge serializer_content
        regsub {<FACTORY_STATS>} $serializer_content $factory_stats serializer_content
        regsub {<FACTORY_OBJECT_TYPE_MAP>} $serializer_content $factory_object_type_map serializer_content
        regsub {<CAPNP_ID>} $serializer_content $capnp_id serializer_content
        regsub {<CAPNP_SAVE>} $serializer_content $capnp_save serializer_content
        regsub {<CAPNP_INIT_FACTORIES>} $serializer_content $capnp_init_factories serializer_content
        regsub {<CAPNP_RESTORE_FACTORIES>} $serializer_content $capnp_restore_factories serializer_content
        if {$file == "vpi_uhdm.h" || $file == "Serializer.h"} {
            set_content_if_change "[codegen_base]/headers/$file" $serializer_content
        } else {
            set_content_if_change "[codegen_base]/src/$file" $serializer_content
        }
    }

    # vpi_visitor.h
    file_copy_if_change "[project_path]/templates/vpi_visitor.h" "[codegen_base]/headers/vpi_visitor.h"

    # vpi_visitor.cpp
    write_vpi_visitor_cpp

    # VpiListener.h
    write_VpiListener_h

    # vpi_listener.h
    write_vpi_listener_h

    # vpi_listener.cpp
    write_vpi_listener_cpp

    # uhdm_forward_decl.h
    write_uhdm_forward_decl

    # ExprEval
    file_copy_if_change "[project_path]/templates/ExprEval.h" "[codegen_base]/headers/ExprEval.h"
    file_copy_if_change "[project_path]/templates/ExprEval.cpp" "[codegen_base]/src/ExprEval.cpp"

}

proc debug_models { models } {
    # Model printout
    foreach model $models {
        log "=========="
        global $model
        pdict $model
    }
}

parse_vpi_user_defines

set models [parse_model $model_files]

debug_models $models

generate_code $models

generate_elaborator $models

puts "UHDM MODEL GENERATION DONE."
