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

# Set content of filename, but only if the file doesn't exist or the content
# is different than before.
# Return if content was overwritten.
proc set_content_if_change { filename content } {
    if {[file exists $filename]} {
        set fid [open $filename]
        set orig_content [read $fid]
        close $fid
        if {$orig_content == $content} {
            return 0
        }
    }
    set outfd [open $filename "w"]
    puts -nonewline $outfd $content
    close $outfd
    return 1
}

# Copy file from source to destination if destination does not exist or
# its content is different.
proc file_copy_if_change { source dest } {
    if {[file exists $dest]} {
        set fid [open $source]
        set source_content [read $fid]
        close $fid

        set fid [open $dest]
        set dest_content [read $fid]
        close $fid
        if {$source_content == $dest_content} {
            return 0
        }
    }
    file copy -force -- $source $dest
    return 1
}

proc find_file { baseDir filename } {
    set filepath [ file join $baseDir $filename ]
    if { [file exists $filepath] } { return $filepath; }

    set dirs [ glob -nocomplain -type d [ file join $baseDir * ] ]
    foreach dir $dirs {
        set filepath [ find_file $dir $filename ]
        if { [file exists $filepath] } { return $filepath; }
    }
}
