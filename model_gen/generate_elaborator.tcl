# -*- mode: Tcl; c-basic-offset: 4; indent-tabs-mode: nil; -*-
#
# Copyright 2019-2021 Alain Dargelas
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
    set module_vpi_listener ""
    set class_vpi_listener ""
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
        if {[regexp {_call} ${classname}] || ($classname == "function") || ($classname == "task") || ($classname == "constant") || ($classname == "tagged_pattern") || ($classname == "gen_scope_array") || ($classname == "hier_path")} {
            # Use hardcoded implementations
            continue
        } else {
            set clone_impl "\n${classname}* ${classname}::DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const \{\n"
        }

        if {[regexp {Net} $vpiName]} {
            append clone_impl "  $classname* clone = dynamic_cast<$classname*>(elaborator->bindNet(VpiName()));
  if (clone != nullptr) {
    return clone;
  }
  clone = serializer->Make${Classname}();
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
        if {($classname != "part_select") && ($classname != "bit_select") && ($classname != "indexed_part_select")} {
            append clone_impl "  clone->VpiParent(parent);
"
        } else {
            append clone_impl "
  if (const any* parent = VpiParent()) {
    ref_obj* ref = serializer->MakeRef_obj();
    clone->VpiParent(ref);
    ref->VpiName(parent->VpiName());
    if (parent->UhdmType() == uhdmref_obj) {
      ref->VpiFullName(((ref_obj*)VpiParent())->VpiFullName());
    }
    ref->VpiParent((any*) parent);
    ref->Actual_group(elaborator->bindAny(ref->VpiName()));
    if (!ref->Actual_group())
      if (parent->UhdmType() == uhdmref_obj) {
        ref->Actual_group((any*) ((ref_obj*)VpiParent())->Actual_group());
      }
  }
"
        }
        if {[regexp {BitSelect} $vpiName]} {
            append clone_impl "  if (any* n = elaborator->bindNet(VpiName())) {
     if (net* nn = dynamic_cast<net*> (n))
       clone->VpiFullName(nn->VpiFullName());
  }
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
                        # Unary relations
                        if {$card == 1} {

                            if {(($classname == "ref_obj") || ($classname == "ref_var")) && ($method == "Actual_group")} {
                                append clone_impl "  clone->${method}(elaborator->bindAny(VpiName()));
  if (!clone->${method}()) clone->${method}((any*) this->${method}());
"
                            } elseif {($method == "Task")} {
                                set prefix ""
                                if [regexp {method_} $classname] {
                                    append clone_impl "  const ref_obj* ref = dynamic_cast<const ref_obj*> (clone->Prefix());\n"
                                    append clone_impl "  const class_var* prefix = nullptr;\n"
                                    append clone_impl "  if (ref) prefix = dynamic_cast<const class_var*> (ref->Actual_group());\n"
                                    set prefix ", prefix"
                                }
                                append clone_impl "  if (task* t = dynamic_cast<task*> (elaborator->bindTaskFunc(VpiName()$prefix))) {
    clone->${method}(t);
  } else {
    elaborator->scheduleTaskFuncBinding(clone);
  }
"
                            } elseif {($method == "Function")} {
                                set prefix ""
                                if [regexp {method_} $classname] {
                                    append clone_impl "  const ref_obj* ref = dynamic_cast<const ref_obj*> (clone->Prefix());\n"
                                    append clone_impl "  const class_var* prefix = nullptr;\n"
                                    append clone_impl "  if (ref) prefix = dynamic_cast<const class_var*> (ref->Actual_group());\n"
                                    set prefix ", prefix"
                                }
                                append clone_impl "  if (function* f = dynamic_cast<function*> (elaborator->bindTaskFunc(VpiName()$prefix))) {
    clone->${method}(f);
  } else {
    elaborator->scheduleTaskFuncBinding(clone);
  }
"
                            } elseif {($rootclassname == "function") && ($method == "Return")} {
                                append clone_impl "  if (auto obj = ${method}()) clone->${method}((variables*)obj);
"
                            } elseif {($rootclassname == "class_typespec") && ($method == "Class_defn")} {
                                append clone_impl "  if (auto obj = ${method}()) clone->${method}((class_defn*)obj);
"
                            } elseif {$method == "Instance"} {
                                append clone_impl "  if (auto obj = ${method}()) clone->${method}((instance*)obj);
  if (instance* inst = dynamic_cast<instance*>(parent))
    clone->Instance(inst);
"
                            } elseif {$method == "Module"} {
                                append clone_impl "  if (auto obj = ${method}()) clone->${method}((module*)obj);
"
                            } elseif {$method == "Typespec"} {

                                append clone_impl "  if (elaborator->uniquifyTypespec()) {
    if (auto obj = ${method}()) clone->${method}(obj->DeepClone(serializer, elaborator, clone));
  } else {
    if (auto obj = ${method}()) clone->${method}((typespec*)obj);
  }
"
                            } else {
                                append clone_impl "  if (auto obj = ${method}()) clone->${method}(obj->DeepClone(serializer, elaborator, clone));
"
                            }

                            if {$rootclassname == "module"} {
                                append module_vpi_listener "          if (auto obj = defMod->${method}()) {
            auto* stmt = obj->DeepClone(serializer_, this, defMod);
            stmt->VpiParent(inst);
            inst->${method}(stmt);
          }
"
                            }
                            if {$rootclassname == "class_defn"} {
                                append class_vpi_listener "          if (auto obj = cl->${method}()) {
            auto* stmt = obj->DeepClone(serializer_, this, cl);
            stmt->VpiParent(cl);
            cl->${method}(stmt);
          }
"
                            }
                            # N-ary relations
                        } else {

                            if {($method == "Typespecs")} {
                                append clone_impl "  if (auto vec = ${method}()) {
    auto clone_vec = serializer->Make${Cast}Vec();
    clone->${method}(clone_vec);
    for (auto obj : *vec) {
      if (elaborator->uniquifyTypespec()) {
        clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));
      } else {
        clone_vec->push_back(obj);
      }
    }
  }
"
                            } elseif {($rootclassname == "class_defn") && ($method == "Deriveds")} {
                                # Don't deep clone
                                append clone_impl "  if (auto vec = ${method}()) {
    auto clone_vec = serializer->Make${Cast}Vec();
    clone->${method}(clone_vec);
    for (auto obj : *vec) {
      clone_vec->push_back(obj);
    }
  }
"
                            } else {
                                append clone_impl "  if (auto vec = ${method}()) {
    auto clone_vec = serializer->Make${Cast}Vec();
    clone->${method}(clone_vec);
    for (auto obj : *vec) {
      clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));
    }
  }
"
                            }

                            if {($rootclassname == "module") && ($method == "Task_funcs")} {
                                append module_vpi_listener "          if (auto vec = defMod->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              enterTask_func(obj, defMod, nullptr, nullptr);
              auto* tf = obj->DeepClone(serializer_, this, inst);
              ComponentMap\\& funcMap = std::get<2>((instStack_.at(instStack_.size()-2)).second);
              funcMap.insert(std::make_pair(tf->VpiName(), tf));
              leaveTask_func(obj, defMod, nullptr, nullptr);
              tf->VpiParent(inst);
              clone_vec->push_back(tf);
            }
          }
"
                            } elseif {($rootclassname == "class_defn")} {
                                if {$method == "Deriveds"} {
                                    # Don't deep clone
                                    append class_vpi_listener "          if (auto vec = cl->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            cl->${method}(clone_vec);
            for (auto obj : *vec) {
              auto* stmt = obj;
              clone_vec->push_back(stmt);
            }
          }
"
                                } else {

                                    append class_vpi_listener "          if (auto vec = cl->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            cl->${method}(clone_vec);
            for (auto obj : *vec) {
              auto* stmt = obj->DeepClone(serializer_, this, cl);
              stmt->VpiParent(cl);
              clone_vec->push_back(stmt);
            }
          }
"
                                }

                            } elseif {($rootclassname == "module") && (($method == "Cont_assigns") || ($method == "Gen_scope_arrays"))} {
                                # We want to deep clone existing instance cont assign to perform binding
                                append module_vpi_listener "          if (auto vec = inst->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              auto* stmt = obj->DeepClone(serializer_, this, inst);
              stmt->VpiParent(inst);
              clone_vec->push_back(stmt);
            }
         }
"
                                # We also want to clone the module cont assign
                                append module_vpi_listener "          if (auto vec = defMod->${method}()) {
            if (inst->${method}() == nullptr) {
              auto clone_vec = serializer_->Make${Cast}Vec();
              inst->${method}(clone_vec);
            }
            auto clone_vec = inst->${method}();
            for (auto obj : *vec) {
              auto* stmt = obj->DeepClone(serializer_, this, inst);
              stmt->VpiParent(inst);
              clone_vec->push_back(stmt);
            }
          }
"

                            } elseif {($rootclassname == "module") && ($method == "Typespecs")} {
                                # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
                                append module_vpi_listener "          if (auto vec = defMod->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              if (uniquifyTypespec()) {
                auto* stmt = obj->DeepClone(serializer_, this, inst);
                stmt->VpiParent(inst);
                clone_vec->push_back(stmt);
              } else {
                auto* stmt = obj;
                clone_vec->push_back(stmt);
              }
            }
          }
"
                            } elseif {($rootclassname == "module") && ($method != "Ports") && ($method != "Nets") && ($method != "Parameters") && ($method != "Param_assigns")} {
                                # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
                                append module_vpi_listener "          if (auto vec = defMod->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              auto* stmt = obj->DeepClone(serializer_, this, inst);
              stmt->VpiParent(inst);
              clone_vec->push_back(stmt);
            }
          }
"

                            } elseif {($rootclassname == "module") && ($method == "Ports")} {
                                 append module_vpi_listener "          if (auto vec = inst->${method}()) {
            auto clone_vec = serializer_->Make${Cast}Vec();
            inst->${method}(clone_vec);
            for (auto obj : *vec) {
              auto* stmt = obj->DeepClone(serializer_, this, inst);
              stmt->VpiParent(inst);
              clone_vec->push_back(stmt);
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

    regsub {<MODULE_ELABORATOR_LISTENER>} $listener_content $module_vpi_listener listener_content
    regsub {<CLASS_ELABORATOR_LISTENER>} $listener_content $class_vpi_listener listener_content

    set_content_if_change "[gen_header_dir]/ElaboratorListener.h" $listener_content

    set fid [open "[project_path]/templates/clone_tree.h"]
    set clone_content [read $fid]
    close $fid

    set_content_if_change "[gen_header_dir]/clone_tree.h" $clone_content

    set fid [open "[project_path]/templates/clone_tree.cpp"]
    set clone_content [read $fid]
    close $fid

    regsub {<CLONE_IMPLEMENTATIONS>} $clone_content $clone_implementations clone_content

    set_content_if_change "[codegen_base]/src/clone_tree.cpp" $clone_content
}
