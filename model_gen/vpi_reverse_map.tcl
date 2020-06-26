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


proc parse_define_file { file } {
    global MAP
    set fid [open $file]
    set content [read $fid]
    foreach line [split $content "\n"] {
        if [regexp {define[ ]+([a-zA-Z0-9]*)[ ]+([0-9]+)} $line tmp name value] {
            if ![info exist MAP($value)] {
                set MAP($value) $name
            }
        }
    }
    close $fid
}


proc write_enum { file } {
    global MAP
    set fid [open $file "w"]
    puts $fid "std::string vpiTypeName(vpiHandle h) {"
    puts $fid "  int type = vpi_get(vpiType, h);"
    puts $fid "  switch (type) {"
    foreach value [array name MAP] {
        puts $fid "    case $value: return \"$MAP($value)\";"
    }
    puts $fid "  }"
    puts $fid "}"

    close $fid
}


parse_define_file "../include/vpi_user.h"
parse_define_file "../include/sv_vpi_user.h"
write_enum "vpi_map.txt"
