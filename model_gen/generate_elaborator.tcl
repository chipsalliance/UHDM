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


proc generate_elaborator { models } {
    global BASECLASS
    set vpi_listener ""
    set clone_cases ""

    foreach model $models {
        global $model
        set data [subst $$model]
        set classname [dict get $data name]
        set Classname [string toupper $classname 0 0]
        set modeltype [dict get $data type]
        set MODEL_TYPE($classname) $modeltype
        set DATA($classname) $data
        if {$modeltype != "obj_def"} {
            continue
        }
        set vpiName [makeVpiName $classname]

        set baseclass $classname
  
        append clone_cases "  case $vpiName: {
    $classname* clone_obj = s.Make${Classname}();
    unsigned long id = clone_obj->UhdmId();
    *clone_obj =  *(($classname*)root);
    clone_obj->UhdmId(id);
    clone = clone_obj;
"
        set rootclassname $classname
        while {$baseclass != ""} {
            set data $DATA($baseclass)
            set classname [dict get $data name]
            set Classname [string toupper $classname 0 0]
            set modeltype [dict get $data type]
            set MODEL_TYPE($classname) $modeltype
            
            dict for {key val} $data {
                if {$key == "properties"} {
                    dict for {prop conf} $val {
                        set name [dict get $conf name]
                        set vpi  [dict get $conf vpi]
                        set type [dict get $conf type]
                        set card [dict get $conf card]
                    }
                }
                if {($key == "class") || ($key == "obj_ref") || ($key == "class_ref") || ($key == "group_ref")} {
                    dict for {iter content} $val {
                        set name $iter
                        set vpi  [dict get $content vpi]
                        set type [dict get $content type]
                        set card [dict get $content card]
                        set id   [dict get $content id]
                        set cast ${type}
                        if {$key == "group_ref"} {
                            set cast "any"
                        }
                        set Cast [string toupper ${cast} 0 0]

                        set method [string toupper ${name} 0 0]
                        if {$card == "any"} {
                            if ![regexp {s$} $method] {
                                append method "s"
                            }
                        }
                        
                        if {$card == 1} {
                            append clone_cases "    clone_obj->${method}((${cast}*) clone_tree((($classname*)root)->${method}(), s, elaborator));
"
                            if {$classname == "ref_obj"} {
                                if {$method == "Actual_group"} {
                                    append clone_cases "    if (clone_obj->${method}() == nullptr) {
      clone_obj->${method}(elaborator->bindNet((($classname*)root)->VpiName()));
    }
"  
                                } 
                            }
                            
                            if {$rootclassname == "module"} {
                                append vpi_listener "          inst->${method}((${cast}*) clone_tree(defMod->${method}(), *serializer_, this));
"
                            }
                        } else {                          
                            append clone_cases "    if (auto vec = (($classname*)root)->${method}()) {
      auto clone_vec = s.Make${Cast}Vec();
      clone_obj->${method}(clone_vec);
      for (auto obj : *vec) {
        clone_vec->push_back((${cast}*) clone_tree(obj, s, elaborator));
      }
    }
"
                            if {($rootclassname == "module") && ($method != "Ports")} {
                                # We don't want to override the elaborated instance ports by the module def ports
                                append vpi_listener "          if (auto vec = defMod->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              clone_vec->push_back((${cast}*) clone_tree(obj, *serializer_, this));
            }
          }
"
                                
                            }  
                        }
                    }
                }
            }

            # Parent class
            if [info exist BASECLASS($baseclass)] {
                set baseclass $BASECLASS($baseclass)
            } else {
                set baseclass ""
            }
            
            
        }

        append clone_cases "    break;
  }
"
 
   
    }

    
    set fid [open "[project_path]/templates/ElaboratorListener.h"]
    set listener_content [read $fid]
    close $fid
    
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
    
    regsub {<CLONE_CASES>} $clone_content $clone_cases clone_content

    set cloneId [open "[project_path]/src/clone_tree.cpp" "w"]
    puts $cloneId $clone_content
    close $cloneId
    
}
