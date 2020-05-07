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




proc generate_elaborator { } {
    global VPI_LISTENERS
    
    set fid [open "[project_path]/templates/ElaboratorListener.h"]
    set listener_content [read $fid]
    close $fid
    set vpi_listener ""
    
    regsub {<ELABORATOR_LISTENER>} $listener_content $vpi_listener listener_content

    set listenerId [open "[project_path]/headers/ElaboratorListener.h" "w"]
    puts $listenerId $listener_content
    close $listenerId

    
    set fid [open "[project_path]/templates/clone_tree.h"]
    set clone_content [read $fid]
    close $fid
    
    set cloneId [open "[project_path]/headers/clone_tree.h" "w"]
    puts $cloneId $clone_content
    close $cloneId

    set fid [open "[project_path]/templates/clone_tree.cpp"]
    set clone_content [read $fid]
    close $fid

    set clone_cases ""
    
    regsub {<CLONE_CASES>} $clone_content $clone_cases clone_content

    set cloneId [open "[project_path]/src/clone_tree.cpp" "w"]
    puts $cloneId $clone_content
    close $cloneId
    
}
