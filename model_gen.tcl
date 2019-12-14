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

set model_file "uhdm.yaml"

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

proc parse_model { file } {
    set models {}
    set fid [open $file]
    set content [read $fid]
    close $fid

    set lines [split $content "\n"]
    set OBJ(curr) ""
    set INDENT(curr) 0
    set modelId 0
    set obj_name ""
    set obj_type ""
    foreach line $lines {
	set spaces ""
	regexp {^([ ]*)} $line tmp spaces
	set indent [string length $spaces]
	if {$indent <= $INDENT(curr)} {
	}
	set INDENT(curr) $indent
	
	if [regexp {\- obj_def: (.*)} $line tmp name] {
	    global obj_def$modelId
	    set obj_def$modelId [dict create "name" $name "type" obj_def "properties" {} "class_ref" {} "obj_ref" {}]	   
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
	if [regexp {card: (.*)} $line tmp card] {
	    dict set $OBJ(curr) $obj_type $obj_name card $card
	}
	if [regexp {name: (.*)} $line tmp name] {
	    dict set $OBJ(curr) $obj_type $obj_name name $name
	}
    }
    return $models
}


set models [parse_model $model_file]

foreach model $models {
    puts "=========="
    pdict $model
}

