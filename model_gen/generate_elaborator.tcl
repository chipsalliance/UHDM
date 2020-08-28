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
    set clone_implementations ""

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

        set clone_impl "\n${classname}* ${classname}::DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const \{\n"

        if [regexp {Net} $vpiName] {
            append clone_impl "  $classname* clone = dynamic_cast<$classname*>(elaborator->bindNet(VpiName()));
  if (clone == nullptr) {
    clone = serializer->Make${Classname}();
  }
"
        } elseif [regexp {Parameter} $vpiName] {
            append clone_impl "  $classname* clone = dynamic_cast<$classname*>(elaborator->bindParam(VpiName()));
  if (clone == nullptr) {
    clone = serializer->Make${Classname}();
  }
"
        } else {
            append clone_impl "  $classname* const clone = serializer->Make${Classname}();
"
        }

        append clone_impl "  const unsigned long id = clone->UhdmId();
  *clone = *this;
  clone->UhdmId(id);
"
        if {$classname != "part_select"} {            
            append clone_impl "  clone->VpiParent(parent);
"
        }
        
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

                            if {$classname == "func_call"} {
                                if {$method == "Function"} {
                                    append clone_impl "  if (auto obj = ${method}()) clone->${method}((function*) obj);
"
                                }
                            } elseif {$classname == "task_call"} {
                                if {$method == "Task"} {
                                    append clone_impl "  if (auto obj = ${method}()) clone->${method}((task*) obj);
"
                                }
                            } elseif {($classname == "ref_obj") && ($method == "Actual_group")} {
                                append clone_impl "  clone->${method}(elaborator->bindAny(VpiName()));
"
                            } else {
                                 append clone_impl "  if (auto obj = ${method}()) clone->${method}(obj->DeepClone(serializer, elaborator, clone));
"
                            }
                            
                            if {$rootclassname == "module"} {
                                append vpi_listener "          if (auto obj = defMod->${method}()) {
            inst->${method}(obj->DeepClone(serializer_, this, defMod));
          }
"
                            }
                                                          
                        } else {
                            append clone_impl "  if (auto vec = ${method}()) {
    auto clone_vec = serializer->Make${Cast}Vec();
    clone->${method}(clone_vec);
    for (auto obj : *vec) {
      clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));
    }
  }
"

                             if {($rootclassname == "module") && ($method == "Task_funcs")} {
                                append vpi_listener "          if (auto vec = defMod->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              clone_vec->push_back((task_func*) obj);
            }
          }
"

                            } elseif {($rootclassname == "module") && ($method != "Ports") && ($method != "Nets")} {
                                # We don't want to override the elaborated instance ports by the module def ports
                                append vpi_listener "          if (auto vec = defMod->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              clone_vec->push_back(obj->DeepClone(serializer_, this, defMod));
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

        append clone_impl "\n  return clone;\n\}\n"

        append clone_implementations $clone_impl
    }


    set fid [open "[project_path]/templates/ElaboratorListener.h"]
    set listener_content [read $fid]
    close $fid

    regsub {<ELABORATOR_LISTENER>} $listener_content $vpi_listener listener_content

    set_content_if_change "[project_path]/headers/ElaboratorListener.h" $listener_content

    set fid [open "[project_path]/templates/clone_tree.h"]
    set clone_content [read $fid]
    close $fid

    set_content_if_change "[project_path]/headers/clone_tree.h" $clone_content

    set fid [open "[project_path]/templates/clone_tree.cpp"]
    set clone_content [read $fid]
    close $fid

    regsub {<CLONE_IMPLEMENTATIONS>} $clone_content $clone_implementations clone_content

    set_content_if_change "[project_path]/src/clone_tree.cpp" $clone_content
}
