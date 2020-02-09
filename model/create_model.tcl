#!/usr/bin/tclsh

set modelName [lindex $argv 0]
set modelType [lindex $argv 1]
set extends   [lindex $argv 2]
if {$modelType == ""} {
    set modelType "obj_def"
}
if {$extends != ""} {
    set extends "  - extends: $extends"
}
set fid [open "blank_model.yaml"]
set content [read $fid]
close $fid

regsub -all {<MODEL_NAME>} $content $modelName content
regsub -all {<MODEL_TYPE>} $content $modelType content
regsub -all {<EXTENDS>} $content $extends content

if [file exist "$modelName.yaml"] {
    puts "ERROR: File $modelName.yaml already exists!"
    exit 1
}

set oid [open "$modelName.yaml" "w"]
puts $oid $content
close $oid


