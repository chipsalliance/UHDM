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

proc parse_model { file } {
    global ID OBJECTID BASECLASS DIRECT_CHILDREN ALL_CHILDREN
    set models {}
    set fid [open "$file"]
    set modellist [read $fid]
    close $fid

    set lines [split $modellist "\n"]
    foreach line $lines {
        if [regexp {^\#} $line] {
            continue
        }
        if {$line != ""} {
            set fid [open "[project_path]/model/$line"]
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

        if [regexp {\- obj_def: ([a-zA-Z0-9_]+)} $line tmp name] {
            set vpiType ""
            set vpiObj  ""
            global obj_def$modelId

            foreach {id define} [defineType 0 $name $vpiType] {}

            set obj_def$modelId [dict create "name" $name "type" obj_def "id" $id "properties" {} "class_ref" {} "obj_ref" {}]
            lappend models obj_def$modelId
            set OBJ(curr) obj_def$modelId
            incr modelId
        }
        if [regexp {\- class_def: ([a-zA-Z0-9_]+)} $line tmp name] {
            set vpiType ""
            set vpiObj  ""
            global obj_def$modelId

            foreach {id define} [defineType 0 $name $vpiType] {}

            set obj_def$modelId [dict create "name" $name "type" class_def "id" $id "properties" {} "class_ref" {} "obj_ref" {}]
            lappend models obj_def$modelId
            set OBJ(curr) obj_def$modelId
            incr modelId
        }
        if [regexp {\- group_def: ([a-zA-Z0-9_]+)} $line tmp name] {
            set vpiType ""
            set vpiObj  ""
            global obj_def$modelId

            foreach {id define} [defineType 0 $name $vpiType] {}

            set obj_def$modelId [dict create "name" $name "type" group_def "id" $id "properties" {} "class_ref" {} "obj_ref" {}]
            lappend models obj_def$modelId
            set OBJ(curr) obj_def$modelId
            incr modelId
        }
        if [regexp {property: ([a-zA-Z0-9_]+)} $line tmp name] {
            dict set $OBJ(curr) "properties" $name {}
            set obj_name $name
            set obj_type "properties"
        }
        if [regexp {class_ref: ([a-zA-Z0-9_]+)} $line tmp name] {
            dict set $OBJ(curr) "class_ref" $name {}
            set obj_name $name
            set obj_type "class_ref"
        }
        if [regexp {extends: ([a-zA-Z0-9_]+)} $line tmp name] {
            dict set $OBJ(curr) "extends" class_def $name
            set obj_name $name
            set obj_type "class_ref"
            set data [subst $$OBJ(curr)]
            set classname [dict get $data name]
            set BASECLASS($classname) $name
        }
        if [regexp {obj_ref: ([a-zA-Z0-9_]+)} $line tmp name] {
            dict set $OBJ(curr) "obj_ref" $name {}
            set obj_name $name
            set obj_type "obj_ref"
        }
        if [regexp {group_ref: ([a-zA-Z0-9_]+)} $line tmp name] {
            dict set $OBJ(curr) "group_ref" $name {}
            set obj_name $name
            set obj_type "group_ref"
        }
        if [regexp {class: ([a-zA-Z0-9_]+)} $line tmp name] {
            dict set $OBJ(curr) "class" $name {}
            set obj_name $name
            set obj_type "class"
        }
        if [regexp {type: (.*)} $line tmp type] {
            dict set $OBJ(curr) $obj_type $obj_name type $type
        }
        if [regexp {vpi: ([a-zA-Z0-9_]+)} $line tmp vpiType] {
            dict set $OBJ(curr) $obj_type $obj_name vpi $vpiType
        }
        if [regexp {vpi_obj: ([a-zA-Z0-9_]+)} $line tmp vpiObj] {
            dict set $OBJ(curr) $obj_type $obj_name vpi $vpiObj
        }
        if [regexp {card: ([a-zA-Z0-9_]+)} $line tmp card] {
            dict set $OBJ(curr) $obj_type $obj_name card $card
            foreach {id define} [defineType 0 $name $vpiType] {}
            if {$define != ""} {
                append defines "$define\n"
            }
            dict set $OBJ(curr) $obj_type $obj_name "id" $id
        }
        if [regexp {name: ([a-zA-Z0-9_]*)} $line tmp name] {
            dict set $OBJ(curr) $obj_type $obj_name name $name
        }
    }

    foreach classname [array names BASECLASS] {
        set baseclass $BASECLASS($classname)
        append DIRECT_CHILDREN($baseclass) "$classname "
        append ALL_CHILDREN($baseclass) "$classname "
        if [info exist BASECLASS($baseclass)] {
            append ALL_CHILDREN($BASECLASS($baseclass)) "$classname "
        }

    }

    return $models
}
