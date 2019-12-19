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

	    set id [defineType 0 $name $vpiType]

	    set obj_def$modelId [dict create "name" $name "type" obj_def "id" $id "properties" {} "class_ref" {} "obj_ref" {}]	   
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
	    set id [defineType 0 $name $vpiType]
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
	if {($type != "int") && ($type != "bool") && ($type != "std::string")} {
	    set pointer "*"
	}
	append methods "\n    ${type}${pointer} get_${vpi}() const { return ${vpi}_; }\n"
	append methods "\n    void set_${vpi}(${type}${pointer} data) { ${vpi}_ = data; }\n"
    } elseif {$card == "any"} {
	append methods "\n    const VectorOf${type}* get_${vpi}() const { return ${vpi}_; }\n"
	append methods "\n    void set_${vpi}(VectorOf${type}* data) { ${vpi}_ = data; }\n"
    }
    return $methods
}

proc printMembers { type vpi card } {
    if {$type == "string"} {
	set type "std::string"
    }
    if {$card == "1"} {
	set pointer ""
	if {($type != "int") && ($type != "bool") && ($type != "std::string")} {
	    set pointer "*"
	}
	append members "\n    ${type}${pointer} ${vpi}_;\n"
    } elseif {$card == "any"} {
	append members "\n    VectorOf${type}* ${vpi}_;\n"
    }
}

proc printTypeDefs { containerId type card } {
    global CONTAINER
    if {$card == "any"} {
	if ![info exist CONTAINER($type)] {
	    set CONTAINER($type) 1
	    puts $containerId "class $type;"
	    puts $containerId "typedef std::vector<${type}*> VectorOf${type};"
	    puts $containerId "typedef std::vector<${type}*>::iterator VectorOf${type}Itr;"
	}
    }
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
       return (vpiHandle) new uhdm_handle($type, (($classname*)(object))->get_${object}());\n\
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
          uhdm_handle* h = new uhdm_handle(uhdm${type}, the_vec->at(handle->index));\n\
	  handle->index++;\n\
          return (vpiHandle) h;\n\
      }\n\
  }"
    }
    return $vpi_scan_body
}

proc defineType { fid name vpiType } {
    global ID OBJECTID
    if [info exist ID($name)] {
	set id $ID($name)
	if {$fid != 0} {
	    puts $fid "#define $name $id"
	}
    } elseif [info exist ID($vpiType)] {
	set id $ID($vpiType)
    } else {
	set id $OBJECTID
	incr OBJECTID
	set ID($name) $id
	if {$fid != 0} {
	    puts $fid "#define $name $id"
	}
    }    
    return $id
}

proc generate_code { models } {
    global ID
    puts "=========="
    exec sh -c "mkdir -p headers"
    exec sh -c "mkdir -p src"
    set fid [open "templates/class_header.h"]
    set template_content [read $fid]
    close $fid
    
    set mainId [open "headers/uhdm.h" "w"]
    puts $mainId "#include <string>"
    puts $mainId "#include <vector>"
    puts $mainId "#ifndef UHDM_H
#define UHDM_H"
    puts $mainId "#include \"include/sv_vpi_user.h\""
    puts $mainId "#include \"include/vhpi_user.h\""
    puts $mainId "#include \"include/vpi_uhdm.h\""
    
    set containerId [open "headers/containers.h" "w"]
    puts $containerId "#ifndef CONTAINER_H
#define CONTAINER_H
namespace UHDM {"

    set fid [open "templates/vpi_user.cpp" ]
    set vpi_user [read $fid]
    close $fid
    set vpi_userId [open "src/vpi_user.cpp" "w"]
    set vpi_iterate_body ""
    set vpi_scan_body ""
    set vpi_handle_body ""
    set vpi_get_body ""
    set vpi_get_str_body ""
    set headers ""
    foreach model $models {
	global $model
	puts "** $model **"
	set data [subst $$model] 
	set classname [dict get $data name]
	set template $template_content

	puts "Generating headers/$classname.h"
	append headers "#include \"headers/$classname.h\"\n"
	set oid [open "headers/$classname.h" "w"]
	regsub -all {<CLASSNAME>} $template $classname template
	regsub -all {<UPPER_CLASSNAME>} $template [string toupper $classname] template
	set methods ""
	set members ""
	defineType $mainId uhdm${classname} ""

        # Builtin "Parent pointer and Parent type" method and field
        append methods [printMethods BaseClass vpiParent 1] 
	append members [printMembers BaseClass vpiParent 1]
        append methods [printMethods int uhdmParentType 1] 
	append members [printMembers int uhdmParentType 1]
        append vpi_handle_body [printGetHandleBody $classname BaseClass vpiParent vpiParent 1]
	
	dict for {key val} $data {
	    if {$key == "properties"} {
		dict for {prop conf} $val {
		    set name [dict get $conf name]
		    set vpi  [dict get $conf vpi]
		    set type [dict get $conf type]
		    set card [dict get $conf card]
		    printTypeDefs $containerId $type $card
                    # properties are already defined in vpi_user.h, no need to redefine them
		    append methods [printMethods $type $vpi $card] 
		    append members [printMembers $type $vpi $card]
                    append vpi_get_body [printGetBody $classname $type $vpi $card]
                    append vpi_get_str_body [printGetStrBody $classname $type $vpi $card]
		}
	    }
	    if {($key == "class") || ($key == "obj_ref") || ($key == "class_ref")} {
		dict for {iter content} $val {
		    set name $iter
		    set vpi  [dict get $content vpi]
		    set type [dict get $content type]
		    set card [dict get $content card]
		    set id   [dict get $content id]
		    printTypeDefs $containerId $type $card
                    # define access properties (allModules...)
		    defineType $mainId uhdm${name} ""
		    append methods [printMethods $type $name $card] 
		    append members [printMembers $type $name $card]
		    append vpi_iterate_body [printIterateBody $name $classname $vpi $card]
                    append vpi_scan_body [printScanBody $name $classname $type $card]
                    append vpi_handle_body [printGetHandleBody $classname uhdm${type} $vpi $type $card]		    
		}
	    }
	}
	regsub -all {<METHODS>} $template $methods template
	regsub -all {<MEMBERS>} $template $members template
	
	puts $oid $template
	close $oid
	
    }

    # uhdm.h
    puts $mainId "#include \"headers/containers.h\""
    puts $mainId $headers
    puts $mainId "#endif" 
    close $mainId

    # containers.h
    puts $containerId "};
#endif"
    close $containerId

    # vpi_user.cpp
    regsub {<HEADERS>} $vpi_user $headers vpi_user
    regsub {<VPI_ITERATE_BODY>} $vpi_user $vpi_iterate_body vpi_user
    regsub {<VPI_SCAN_BODY>} $vpi_user $vpi_scan_body vpi_user
    regsub {<VPI_HANDLE_BODY>} $vpi_user $vpi_handle_body vpi_user
    regsub -all {<VPI_GET_BODY>} $vpi_user $vpi_get_body vpi_user
    regsub {<VPI_GET_STR_BODY>} $vpi_user $vpi_get_str_body vpi_user

    puts $vpi_userId $vpi_user
    close $vpi_userId
    
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




