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

set model_file "uvdm.yaml"

proc parse_model { file } {
    set models {}
    set fid [open $file]
    set content [read $fid]
    close $fid

    set lines [split $content "\n"]
    
    foreach line $lines {
	if [regexp {obj_def} $line] {
	    set obj_def [dict create]
	    lappend models $obj_def
	}
	
	
    }

    
    return $models
}


set models [parse_model $model_file]

puts $models

