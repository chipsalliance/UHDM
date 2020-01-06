#!/usr/bin/tclsh

# Copyright 2019 Alain Dargelas
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

set model_files $argv

# proc copied from: https://wiki.tcl-lang.org/page/pdict%3A+Pretty+print+a+dict
proc pdict { d {i 0} {p "  "} {s " -> "} } {
    global $d
    set fRepExist [expr {0 < [llength\
            [info commands tcl::unsupported::representation]]}]
    if { (![string is list $d] || [llength $d] == 1)
            && [uplevel 1 [list info exists $d]] } {
        set dictName $d
        unset d
        upvar 1 $dictName d
        puts "dict $dictName"
    }
    if { ! [string is list $d] || [llength $d] % 2 != 0 } {
        return -code error  "error: pdict - argument is not a dict"
    }
    set prefix [string repeat $p $i]
    set max 0
    foreach key [dict keys $d] {
        if { [string length $key] > $max } {
            set max [string length $key]
        }
    }
    dict for {key val} ${d} {
        puts -nonewline "${prefix}[format "%-${max}s" $key]$s"
        if {    $fRepExist && [string match "value is a dict*"\
                    [tcl::unsupported::representation $val]]
                || ! $fRepExist && [string is list $val]
                    && [llength $val] % 2 == 0 } {
            puts ""
            pdict $val [expr {$i+1}] $p $s
        } else {
            puts "'${val}'"
        }
    }
    return
}

proc parse_vpi_user_defines { } {
    global ID
    set fid [open "include/vpi_user.h"]
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

proc parse_model { file } {
    global ID OBJECTID
    set models {}
    set fid [open $file]
    set modellist [read $fid]
    close $fid

    set lines [split $modellist "\n"]
    foreach line $lines {
	if [regexp {^\#} $line] {
	    continue
	}
	if {$line != ""} {
	    set fid [open model/$line]
	    set model [read $fid]
	    append content "$model\n"
	    close $fid
	}
    }

    set lines [split $content "\n"]
    set OBJ(curr) ""
    set INDENT(curr) 0
    set modelId 0
    set obj_name ""
    set obj_type ""
    set vpiType ""
    set vpiObj ""
    foreach line $lines {
	if [regexp {^#} $line] {
	    continue
	}
	set spaces ""
	regexp {^([ ]*)} $line tmp spaces
	set indent [string length $spaces]
	if {$indent <= $INDENT(curr)} {
	}
	set INDENT(curr) $indent
	
	if [regexp {\- obj_def: (.*)} $line tmp name] {
	    set vpiType ""
	    set vpiObj  ""
	    global obj_def$modelId

	    foreach {id define} [defineType 0 $name $vpiType] {}

	    set obj_def$modelId [dict create "name" $name "type" obj_def "id" $id "properties" {} "class_ref" {} "obj_ref" {}]	   
	    lappend models obj_def$modelId 
	    set OBJ(curr) obj_def$modelId
	    incr modelId
	}
	if [regexp {\- class_def: (.*)} $line tmp name] {
	    set vpiType ""
	    set vpiObj  ""
	    global obj_def$modelId

	    foreach {id define} [defineType 0 $name $vpiType] {}

	    set obj_def$modelId [dict create "name" $name "type" class_def "id" $id "properties" {} "class_ref" {} "obj_ref" {}]	   
	    lappend models obj_def$modelId 
	    set OBJ(curr) obj_def$modelId
	    incr modelId
	}
	if [regexp {property: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "properties" $name {}
	    set obj_name $name
	    set obj_type "properties"
	}
	if [regexp {class_ref: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "class_ref" $name {}
	    set obj_name $name
	    set obj_type "class_ref"
	}
	if [regexp {extends: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "extends" class_def $name 
	    set obj_name $name
	    set obj_type "class_ref"
	}
	if [regexp {obj_ref: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "obj_ref" $name {}
	    set obj_name $name
	    set obj_type "obj_ref"
	}
	if [regexp {class: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "class" $name {}
	    set obj_name $name
	    set obj_type "class"
	}
	if [regexp {type: (.*)} $line tmp type] {
	    dict set $OBJ(curr) $obj_type $obj_name type $type
	}
	if [regexp {vpi: (.*)} $line tmp vpiType] {
	    dict set $OBJ(curr) $obj_type $obj_name vpi $vpiType	    
	}
	if [regexp {vpi_obj: (.*)} $line tmp vpiObj] {
	    dict set $OBJ(curr) $obj_type $obj_name vpi $vpiObj	    
	}
	if [regexp {card: (.*)} $line tmp card] {
	    dict set $OBJ(curr) $obj_type $obj_name card $card
	    foreach {id define} [defineType 0 $name $vpiType] {}
	    if {$define != ""} {
              append defines "$define\n"
            }
	    dict set $OBJ(curr) $obj_type $obj_name "id" $id
	}
	if [regexp {name: (.*)} $line tmp name] {
	    dict set $OBJ(curr) $obj_type $obj_name name $name
	}
    }
    return $models
}

proc printMethods { type vpi card } {
    set methods ""
    if {$type == "string"} {
	set type "std::string"
    }
    if {$card == "1"} {
	set pointer ""
	if {($type != "unsigned int") && ($type != "int") && ($type != "bool") && ($type != "std::string")} {
	    set pointer "*"
	}
	if {$type == "std::string"} {
	    append methods "\n    ${type}${pointer} get_${vpi}() const { return SymbolFactory::getSymbol(${vpi}_); }\n"
	    append methods "\n    void set_${vpi}(${type}${pointer} data) { ${vpi}_ = SymbolFactory::make(data); }\n"	    
	} else {
	    append methods "\n    ${type}${pointer} get_${vpi}() const { return ${vpi}_; }\n"
	    append methods "\n    void set_${vpi}(${type}${pointer} data) { ${vpi}_ = data; }\n"
	}
    } elseif {$card == "any"} {
	append methods "\n    VectorOf${type}* get_${vpi}() const { return ${vpi}_; }\n"
	append methods "\n    void set_${vpi}(VectorOf${type}* data) { ${vpi}_ = data; }\n"
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
    if {$card == "1"} {
	return [list ${vpi} ${type}]
    } elseif {$card == "any"} {
	return [list ${vpi} List(${type})]
    }
}

proc printMembers { type vpi card } {
    set members ""
    if {$type == "string"} {
	set type "std::string"
    }
    if {$card == "1"} {
	set pointer ""
	if {($type != "unsigned int") && ($type != "int") && ($type != "bool") && ($type != "std::string")} {
	    set pointer "*"
	}
	if {$type == "std::string"} {
	    append members "\n    unsigned int ${vpi}_;\n"
	} else {
	    append members "\n    ${type}${pointer} ${vpi}_;\n"	    
	}
    } elseif {$card == "any"} {
	append members "\n    VectorOf${type}* ${vpi}_;\n"
    }
    return $members
}

proc printTypeDefs { type card } {
    global CONTAINER
    set typedefs ""
    if {$card == "any"} {
	if ![info exist CONTAINER($type)] {
	    set CONTAINER($type) 1
	    append typedefs "class $type;\n"
	    append typedefs "typedef std::vector<${type}*> VectorOf${type};\n"
	    append typedefs "typedef std::vector<${type}*>::iterator VectorOf${type}Itr;\n"
	}
    }
    return $typedefs
}

proc printIterateBody { name classname vpi card } {
    set vpi_iterate_body ""
    if {$card == "any"} {
	append vpi_iterate_body "\n\    
 if (handle->type == uhdm${classname}) {\n\
  if (type == $vpi) {\n\
     if ((($classname*)(object))->get_${name}())\n\
       return (vpiHandle) new uhdm_handle(uhdm${name}, (($classname*)(object))->get_${name}());\n\
     else return 0;
  }\n\
}\n"
    return $vpi_iterate_body
   }
}

proc printGetBody {classname type vpi card} {
    set vpi_get_body ""
    if {($card == 1) && ($type != "string")} {
	append vpi_get_body "\n\
 if (handle->type == uhdm${classname}) {
     if (property == $vpi) {
       return (($classname*)(obj))->get_${vpi}();
     } 
}
"
    }
    return $vpi_get_body
}


proc printGetHandleBody { classname type vpi object card } {
    if {$type == "BaseClass"} {
	set type "(($classname*)(object))->get_uhdmParentType()"
    }
    set vpi_get_handle_body ""
    if {$card == 1} {
	append vpi_get_handle_body "\n\
 if (handle->type == uhdm${classname}) {
     if (type == $vpi) {
       return (vpiHandle) new uhdm_handle((($classname*)(object))->get_${object}()->getUhdmType(), (($classname*)(object))->get_${object}());\n\
     } 
}
"
    }
    return $vpi_get_handle_body
}

proc printGetStrBody {classname type vpi card} {
    set vpi_get_str_body ""
    if {$card == 1 && ($type == "string")} {
	append vpi_get_str_body "\n\
 if (handle->type == uhdm${classname}) {
     if (property == $vpi) {
       return (PLI_BYTE8*) strdup((($classname*)(obj))->get_${vpi}().c_str());
     } 
}
"
    }
    return $vpi_get_str_body
}

proc printScanBody { name classname type card } {
    set vpi_scan_body ""
    if {$card == "any"} {
	append vpi_scan_body "\n
  if (handle->type == uhdm${name}) {\n\
    VectorOf${type}* the_vec = (VectorOf${type}*)vect;\n\
      if (handle->index < the_vec->size()) {\n\
          uhdm_handle* h = new uhdm_handle(the_vec->at(handle->index)->getUhdmType(), the_vec->at(handle->index));\n\
	  handle->index++;\n\
          return (vpiHandle) h;\n\
      }\n\
  }"
    }
    return $vpi_scan_body
}

proc defineType { def name vpiType } {
    global ID OBJECTID
    set id ""
    set define ""
    if [info exist ID($name)] {
	set id $ID($name)
	if {$def != 0} {
	    set define "#define $name $id"
	}
    } elseif [info exist ID($vpiType)] {
	set id $ID($vpiType)
    } else {
	set id $OBJECTID
	incr OBJECTID
	set ID($name) $id
	if {$def != 0} {
	    set define "#define $name $id"
	}
    }
    return [list $id $define]
}

proc generate_code { models } {
    global ID
    puts "=========="
    exec sh -c "mkdir -p headers"
    exec sh -c "mkdir -p src"
    set fid [open "templates/class_header.h"]
    set template_content [read $fid]
    close $fid

    set vpi_iterate_body_all ""
    set vpi_scan_body ""
    set vpi_handle_body ""
    set vpi_get_body ""
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
	puts "** $model **"
	set data [subst $$model]
	set classname [dict get $data name]
	set template $template_content
	set modeltype [dict get $data type]
	set MODEL_TYPE($classname) $modeltype
	set baseclass ""
	set capnp_schema($classname) ""
	if {$modeltype == "class_def"} {
	    regsub -all {<FINAL_DESTRUCTOR>} $template "" template
	}
	set Classname [string toupper $classname 0 0]
	regsub -all  {_} $Classname "" Classname
	
	puts "Generating headers/$classname.h"
	append headers "#include \"headers/$classname.h\"\n"
	if {$modeltype != "class_def"} {
	    append factories "std::vector<${classname}*> ${classname}Factory::objects_;\n"
	}
	append factories "std::vector<std::vector<${classname}*>*> VectorOf${classname}Factory::objects_;\n"
	if {$modeltype != "class_def"} {
	    append factory_object_type_map "  case uhdm${classname}: return ${classname}Factory::objects_\[index\];\n"
	}
        lappend classes $classname
	
	set oid [open "headers/$classname.h" "w"]
	regsub -all {<CLASSNAME>} $template $classname template
	regsub -all {<UPPER_CLASSNAME>} $template [string toupper $classname] template
	set methods($classname) ""
	set members($classname) ""
	foreach {id define} [defineType 1 uhdm${classname} ""] {}
        if {$define != ""} {
          append defines "$define\n"
        }
	append SAVE($classname) ""
	append RESTORE($classname) ""

	if {$modeltype != "class_def"} {
	    # Builtin properties do not need to be specified in each models
	    # Builtins: "vpiParent, Parent type, vpiFile, vpiLineNo" method and field
	    append methods($classname) [printMethods BaseClass vpiParent 1]
	    append members($classname) [printMembers BaseClass vpiParent 1]
	    append methods($classname) [printMethods "unsigned int" uhdmParentType 1] 
	    append members($classname) [printMembers "unsigned int" uhdmParentType 1]
	    append methods($classname) [printMethods string vpiFile 1] 
	    append members($classname) [printMembers string vpiFile 1]
	    lappend vpi_get_str_body_inst($classname) [list $classname string vpiFile 1]
	    append methods($classname) [printMethods "unsigned int" vpiLineNo 1] 
	    append members($classname) [printMembers "unsigned int" vpiLineNo 1]
	    lappend vpi_get_body_inst($classname) [list $classname int vpiLineNo 1]
	    append vpi_handle_body [printGetHandleBody $classname BaseClass vpiParent vpiParent 1]
	    lappend capnp_schema($classname) [list vpiParent UInt64]
	    lappend capnp_schema($classname) [list uhdmParentType UInt64]
	    lappend capnp_schema($classname) [list vpiFile UInt64]
	    lappend capnp_schema($classname) [list vpiLineNo UInt32]
	    append capnp_root_schema "  factory${Classname} @${capnpRootSchemaIndex} :List($Classname);\n"
	    incr capnpRootSchemaIndex
	    append SAVE($classname) "    ${Classname}s\[index\].setVpiParent(getId(obj->get_vpiParent()));\n"
	    append SAVE($classname) "    ${Classname}s\[index\].setUhdmParentType(obj->get_uhdmParentType());\n"
	    append SAVE($classname) "    ${Classname}s\[index\].setVpiFile(SymbolFactory::make(obj->get_vpiFile()));\n"
	    append SAVE($classname) "    ${Classname}s\[index\].setVpiLineNo(obj->get_vpiLineNo());\n"
	    append RESTORE($classname) "   ${classname}Factory::objects_\[index\]->set_uhdmParentType(obj.getUhdmParentType());\n"
	    append RESTORE($classname) "   ${classname}Factory::objects_\[index\]->set_vpiParent(getObject(obj.getUhdmParentType(),obj.getVpiParent()-1));\n"
	    append RESTORE($classname) "   ${classname}Factory::objects_\[index\]->set_vpiFile(SymbolFactory::getSymbol(obj.getVpiFile()));\n"
	    append RESTORE($classname) "   ${classname}Factory::objects_\[index\]->set_vpiLineNo(obj.getVpiLineNo());\n"
	}

	set indTmp 0
	dict for {key val} $data {
	    if {$key == "properties"} {
		dict for {prop conf} $val {
		    set name [dict get $conf name]
		    set vpi  [dict get $conf vpi]
		    set type [dict get $conf type]
		    set card [dict get $conf card]
		    if {$prop == "type"} {
			append methods($classname) "\n    $type get_${vpi}() { return $name; }\n"
			lappend vpi_get_body_inst($classname) [list $classname $type $vpi $card]
			continue
		    }
		    append containers [printTypeDefs $type $card]
                    # properties are already defined in vpi_user.h, no need to redefine them
		    append methods($classname) [printMethods $type $vpi $card] 
		    append members($classname) [printMembers $type $vpi $card]
                    lappend vpi_get_body_inst($classname) [list $classname $type $vpi $card]
                    lappend vpi_get_str_body_inst($classname) [list $classname $type $vpi $card]
		    lappend capnp_schema($classname) [printCapnpSchema $type $vpi $card]
		
		    set Vpi [string toupper $vpi 0 0]
		    regsub -all  {_} $Vpi "" Vpi
		    if {$type == "string"} {
			append SAVE($classname) "    ${Classname}s\[index\].set${Vpi}(SymbolFactory::make(obj->get_${vpi}()));\n"
			append RESTORE($classname) "    ${classname}Factory::objects_\[index\]->set_${vpi}(SymbolFactory::getSymbol(obj.get${Vpi}()));\n"
		    } else {
			append SAVE($classname) "    ${Classname}s\[index\].set${Vpi}(obj->get_${vpi}());\n"
			append RESTORE($classname) "    ${classname}Factory::objects_\[index\]->set_${vpi}(obj.get${Vpi}());\n"
		    }
		}

	    }
	    if {$key == "extends"} {
		dict for {base_type baseclass} $val {
		    regsub -all {<EXTENDS>} $template $baseclass template
		    set BASECLASS($classname) $baseclass
		}		
	    }
	    if {($key == "class") || ($key == "obj_ref") || ($key == "class_ref")} {
		dict for {iter content} $val {
		    set name $iter
		    set vpi  [dict get $content vpi]
		    set type [dict get $content type]
		    set card [dict get $content card]
		    set id   [dict get $content id]
		    append containers [printTypeDefs $type $card]
                    # define access properties (allModules...)
		    foreach {id define} [defineType 1 uhdm${name} ""] {}
                    if {$define != ""} {
                      append defines "$define\n"
                    }
		    append methods($classname) [printMethods $type $name $card] 
		    append members($classname) [printMembers $type $name $card]
		    append vpi_iterate_body($classname) [printIterateBody $name $classname $vpi $card]
                    append vpi_scan_body [printScanBody $name $classname $type $card]
                    append vpi_handle_body [printGetHandleBody $classname uhdm${type} $vpi $name $card]

		    set Type [string toupper $type 0 0]
		    regsub -all  {_} $Type "" Type		    
		    regsub -all  {_} $name "" Name

		    if {$key == "class_ref"} {
			set obj_key ObjIndexType
		    } else {
			set obj_key UInt64 
		    }
		    lappend capnp_schema($classname) [printCapnpSchema $obj_key $Name $card]
		    if {$card == 1} {
			if {$key == "class_ref"} {
			    append SAVE($classname) "  if (obj->get_${name}()) {"
			    append SAVE($classname) "    ::ObjIndexType::Builder tmp$indTmp = ${Classname}s\[index\].get[string toupper ${Name} 0 0]();\n"
			    append SAVE($classname) "    tmp${indTmp}.setIndex(getId((obj->get_${name}())));\n"
			    append SAVE($classname) "    tmp${indTmp}.setType(obj->get_${name}()->getUhdmType());\n  }"			    
			    
			    incr indTmp
			} else {
			    append SAVE($classname) "    ${Classname}s\[index\].set[string toupper ${Name} 0 0](getId(obj->get_${name}()));\n"
			}
			if {$key == "class_ref"} {
			    append RESTORE($classname) "     ${classname}Factory::objects_\[index\]->set_${name}((${type}*)getObject(obj.get[string toupper ${Name} 0 0]().getType(),obj.get[string toupper ${Name} 0 0]().getIndex()-1));\n"
			} else {
			    append RESTORE($classname) "   if (obj.get[string toupper ${Name} 0 0]()) 
     ${classname}Factory::objects_\[index\]->set_${name}(${type}Factory::objects_\[obj.get[string toupper ${Name} 0 0]()-1\]);\n"
			}
		    } else {

			if {$key == "class_ref"} {
			    set obj_key ::ObjIndexType
			} else {
			    set obj_key ::uint64_t
			}
			append SAVE($classname) " 
    if (obj->get_${name}()) {  
      ::capnp::List<$obj_key>::Builder [string toupper ${Name} 0 0]s = ${Classname}s\[index\].init[string toupper ${Name} 0 0](obj->get_${name}()->size());
      for (unsigned int ind = 0; ind < obj->get_${name}()->size(); ind++) {\n"
			if {$key == "class_ref"} {
			    append SAVE($classname) "        ::ObjIndexType::Builder tmp = [string toupper ${Name} 0 0]s\[ind\];\n"
			    append SAVE($classname) "        tmp.setIndex(getId((*obj->get_${name}())\[ind\]));\n"
			    append SAVE($classname) "        tmp.setType(((*obj->get_${name}())\[ind\])->getUhdmType());"
			} else {
			    append SAVE($classname) "[string toupper ${Name} 0 0]s.set(ind, getId((*obj->get_${name}())\[ind\]));"
			}
			append SAVE($classname) "\n      }
    }
"
			append RESTORE($classname) "    
    if (obj.get[string toupper ${Name} 0 0]().size()) { 
      VectorOf${type}* vect = VectorOf${type}Factory::make();
      for (unsigned int ind = 0; ind < obj.get[string toupper ${Name} 0 0]().size(); ind++) {\n"
			if {$key == "class_ref"} {
			    append RESTORE($classname) " 	vect->push_back((${type}*)getObject(obj.get[string toupper ${Name} 0 0]()\[ind\].getType(),obj.get[string toupper ${Name} 0 0]()\[ind\].getIndex()-1));\n"
			} else {
			    append RESTORE($classname) " 	vect->push_back(${type}Factory::objects_\[obj.get[string toupper ${Name} 0 0]()\[ind\]-1\]);\n"
			}			
			append RESTORE($classname) "    }
      ${classname}Factory::objects_\[index\]->set_${name}(vect);
    }
"
		    }
		}
	    
	    }
	}
	regsub -all {<METHODS>} $template $methods($classname) template
	regsub -all {<MEMBERS>} $template $members($classname) template
	regsub -all {<EXTENDS>} $template BaseClass template
	regsub -all {<FINAL_DESTRUCTOR>} $template "final" template
	
	puts $oid $template
	close $oid

	if [info exist vpi_get_str_body_inst($classname)] {
	    foreach inst $vpi_get_str_body_inst($classname) {
		append vpi_get_str_body [printGetStrBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
	    }
	}
	if [info exist vpi_get_body_inst($classname)] {
	    foreach inst $vpi_get_body_inst($classname) {
		append vpi_get_body [printGetBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
	    }
	}

	set capnpIndex 0    
	if {$modeltype != "class_def"} {
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
	while {$baseclass != ""} {
	    if {$modeltype != "class_def"} {
		foreach member $capnp_schema($baseclass) {
		    foreach {name type} $member {}
		    append capnp_schema_all "$name @$capnpIndex :$type;\n"
		    incr capnpIndex
		}
	    }
	    set save ""
	    foreach line [split $SAVE($baseclass) "\n"] {
		set base $baseclass
		regsub -all  {_} $baseclass "" base		
		regsub  [string toupper $base 0 0] $line $Classname tmp
		append save "$tmp\n"
	    }
	    append SAVE($classname) $save
	    set restore $RESTORE($baseclass)
	    regsub -all ${baseclass}Factory $RESTORE($baseclass) ${classname}Factory restore	    
	    append RESTORE($classname) $restore
	    if [info exist vpi_get_str_body_inst($baseclass)] {
		foreach inst $vpi_get_str_body_inst($baseclass) {
		    append vpi_get_str_body [printGetStrBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
		}
	    }
	    if [info exist vpi_get_body_inst($baseclass)] {
		foreach inst $vpi_get_body_inst($baseclass) {
		    append vpi_get_body [printGetBody $classname [lindex $inst 1] [lindex $inst 2] [lindex $inst 3]]
		}
	    }

	    if [info exist vpi_iterate_body($baseclass)] {
		set vpi_iterate $vpi_iterate_body($baseclass)
		regsub uhdm$baseclass $vpi_iterate uhdm$classname vpi_iterate
		append vpi_iterate_body_all $vpi_iterate
	    }
	    	
	    if [info exist BASECLASS($baseclass)] {
		set baseclass $BASECLASS($baseclass)
	    } else {
		set baseclass ""
	    }
	}

	if {$modeltype != "class_def"} {
	    append capnp_schema_all "\}\n"
	}	
    }

    # uhdm.h
    set fid [open "templates/uhdm.h"]
    set uhdm_content [read $fid]
    close $fid 
    set uhdmId [open "headers/uhdm.h" "w"]
    regsub -all {<DEFINES>} $uhdm_content $defines uhdm_content
    regsub -all {<INCLUDE_FILES>} $uhdm_content $headers uhdm_content
    puts $uhdmId $uhdm_content
    close $uhdmId

    # containers.h
    set fid [open "templates/containers.h"]
    set container_content [read $fid]
    close $fid 
    set containerId [open "headers/containers.h" "w"]
    regsub -all {<CONTAINERS>} $container_content $containers container_content
    puts $containerId $container_content
    close $containerId

    # vpi_user.cpp
    set fid [open "templates/vpi_user.cpp" ]
    set vpi_user [read $fid]
    close $fid
    regsub {<HEADERS>} $vpi_user $headers vpi_user
    regsub {<VPI_ITERATE_BODY>} $vpi_user $vpi_iterate_body_all vpi_user
    regsub {<VPI_SCAN_BODY>} $vpi_user $vpi_scan_body vpi_user
    regsub {<VPI_HANDLE_BODY>} $vpi_user $vpi_handle_body vpi_user
    regsub -all {<VPI_GET_BODY>} $vpi_user $vpi_get_body vpi_user
    regsub {<VPI_GET_STR_BODY>} $vpi_user $vpi_get_str_body vpi_user
    
    set vpi_userId [open "src/vpi_user.cpp" "w"]
    puts $vpi_userId $vpi_user
    close $vpi_userId

    # UHDM.capnp
    set fid [open "templates/UHDM.capnp"]
    set capnp_content [read $fid]
    close $fid
    regsub {<CAPNP_SCHEMA>} $capnp_content $capnp_schema_all capnp_content
    regsub {<CAPNP_ROOT_SCHEMA>} $capnp_content $capnp_root_schema capnp_content
    set capnpId [open "src/UHDM.capnp" "w"]
    puts $capnpId $capnp_content
    close $capnpId
    puts "Generating Capnp schema..."
    exec sh -c "rm -rf src/UHDM.capnp.*"
    exec sh -c "capnp compile -oc++:. src/UHDM.capnp"
    
    # Serializer.cpp
    set fid [open "templates/Serializer.cpp" ]
    set serializer_content [read $fid]
    close $fid
    foreach class $classes {
	if {$MODEL_TYPE($class) == "class_def"} {
	    continue
	}
	set Class [string toupper $class 0 0]
	regsub -all  {_} $Class "" Class
	if {$SAVE($class) != ""} {
	    append capnp_save "
 ::capnp::List<$Class>::Builder ${Class}s = cap_root.initFactory${Class}(${class}Factory::objects_.size());
 index = 0;
 for (auto obj : ${class}Factory::objects_) {
$SAVE($class)
   index++;
 }"
	   append capnp_init_factories "
 ::capnp::List<$Class>::Reader ${Class}s = cap_root.getFactory${Class}();
 for (unsigned ind = 0; ind < ${Class}s.size(); ind++) {
   setId(${class}Factory::make(), ind);
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
  for (auto obj : ${class}Factory::objects_) {
    setId(obj, index);
    index++;
  }"

	append factory_purge "
  for (auto obj : ${class}Factory::objects_) {
    delete obj;
  }
  ${class}Factory::objects_.clear();
"
    }
    
    regsub {<FACTORIES>} $serializer_content $factories serializer_content
    regsub {<FACTORY_PURGE>} $serializer_content $factory_purge serializer_content
    regsub {<FACTORY_OBJECT_TYPE_MAP>} $serializer_content $factory_object_type_map serializer_content
    regsub {<CAPNP_ID>} $serializer_content $capnp_id serializer_content
    regsub {<CAPNP_SAVE>} $serializer_content $capnp_save serializer_content
    regsub {<CAPNP_INIT_FACTORIES>} $serializer_content $capnp_init_factories serializer_content
    regsub {<CAPNP_RESTORE_FACTORIES>} $serializer_content $capnp_restore_factories serializer_content
    set serializerId [open "src/Serializer.cpp" "w"]
    puts $serializerId $serializer_content
    close $serializerId
    
}

proc debug_models { models } {
    # Model printout
    foreach model $models {
	puts "=========="
	global $model
	pdict $model
    }
}

parse_vpi_user_defines

set models [parse_model $model_files]

debug_models $models

generate_code $models




