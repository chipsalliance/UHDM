import config
import file_utils


def _generate_module_listeners(models, classname):
    listeners = []
    while classname:
        model = models[classname]

        for key, value in model.allitems():
            if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')

                cast = 'any' if key == 'group_ref' else type
                Cast = cast[:1].upper() + cast[1:]
                method = name[:1].upper() + name[1:]

                if (card == 'any') and not method.endswith('s'):
                    method += 's'

                if card == '1':
                    listeners.append(f'if (auto obj = defMod->{method}()) {{')
                    listeners.append( '  auto* stmt = obj->DeepClone(serializer_, this, defMod);')
                    listeners.append( '  stmt->VpiParent(inst);')
                    listeners.append(f'  inst->{method}(stmt);')
                    listeners.append( '}')

                elif method in ['Task_funcs']:
                    # We want to deep clone existing instance tasks and funcs
                    listeners.append(f'if (auto vec = inst->{method}()) {{')
                    listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                    listeners.append(f'  inst->{method}(clone_vec);')
                    listeners.append( '  for (auto obj : *vec) {')
                    listeners.append( '    enterTask_func(obj, nullptr);')
                    listeners.append( '    auto* tf = obj->DeepClone(serializer_, this, inst);')
                    listeners.append( '    ComponentMap& funcMap = std::get<2>(instStack_.at(instStack_.size()-2).second);')
                    listeners.append( '    auto it = funcMap.find(tf->VpiName());')
                    listeners.append( '    if (it != funcMap.end()) funcMap.erase(it);')
                    listeners.append( '    funcMap.insert(ComponentMap::value_type(tf->VpiName(), tf));')
                    listeners.append( '    leaveTask_func(obj, nullptr);')
                    listeners.append( '    tf->VpiParent(inst);')
                    listeners.append( '    clone_vec->push_back(tf);')
                    listeners.append( '  }')
                    listeners.append( '}')
                elif method in ['Cont_assigns', 'Primitives', 'Primitive_arrays']:
                    # We want to deep clone existing instance cont assign to perform binding
                    listeners.append(f'if (auto vec = inst->{method}()) {{')
                    listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                    listeners.append(f'  inst->{method}(clone_vec);')
                    listeners.append( '  for (auto obj : *vec) {')
                    listeners.append( '    auto* stmt = obj->DeepClone(serializer_, this, inst);')
                    listeners.append( '    stmt->VpiParent(inst);')
                    listeners.append( '    clone_vec->push_back(stmt);')
                    listeners.append( '  }')
                    listeners.append( '}')

                elif method in ['Gen_scope_arrays']:
                    # We want to deep clone existing instance cont assign to perform binding
                    listeners.append(f'if (auto vec = inst->{method}()) {{')
                    listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                    listeners.append(f'  inst->{method}(clone_vec);')
                    listeners.append( '  for (auto obj : *vec) {')
                    listeners.append( '    auto* stmt = obj->DeepClone(serializer_, this, inst);')
                    listeners.append( '    stmt->VpiParent(inst);')
                    listeners.append( '    clone_vec->push_back(stmt);')
                    listeners.append( '  }')
                    listeners.append( '}')
                    # We also want to clone the module cont assign
                    listeners.append(f'if (auto vec = defMod->{method}()) {{')
                    listeners.append(f'  if (inst->{method}() == nullptr) {{')
                    listeners.append(f'    auto clone_vec = serializer_->Make{Cast}Vec();')
                    listeners.append(f'    inst->{method}(clone_vec);')
                    listeners.append( '  }')
                    listeners.append(f'  auto clone_vec = inst->{method}();')
                    listeners.append( '  for (auto obj : *vec) {')
                    listeners.append( '    auto* stmt = obj->DeepClone(serializer_, this, inst);')
                    listeners.append( '    stmt->VpiParent(inst);')
                    listeners.append( '    clone_vec->push_back(stmt);')
                    listeners.append( '  }')
                    listeners.append( '}')

                elif method in ['Typespecs']:
                    # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
                    listeners.append(f'if (auto vec = defMod->{method}()) {{')
                    listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                    listeners.append(f'  inst->{method}(clone_vec);')
                    listeners.append( '  for (auto obj : *vec) {')
                    listeners.append( '    if (uniquifyTypespec()) {')
                    listeners.append( '      auto* stmt = obj->DeepClone(serializer_, this, inst);')
                    listeners.append( '      stmt->VpiParent(inst);')
                    listeners.append( '      clone_vec->push_back(stmt);')
                    listeners.append( '    } else {')
                    listeners.append( '      auto* stmt = obj;')
                    listeners.append( '      clone_vec->push_back(stmt);')
                    listeners.append( '    }')
                    listeners.append( '  }')
                    listeners.append( '}')

                elif method in ['Ref_modules']:
                    # No elab
                    listeners.append( '')

                elif method not in ['Ports', 'Nets', 'Parameters', 'Param_assigns', 'Interface_arrays', 'Module_arrays']:
                    # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
                    listeners.append(f'if (auto vec = defMod->{method}()) {{')
                    listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                    listeners.append(f'  inst->{method}(clone_vec);')
                    listeners.append( '  for (auto obj : *vec) {')
                    listeners.append( '    auto* stmt = obj->DeepClone(serializer_, this, inst);')
                    listeners.append( '    stmt->VpiParent(inst);')
                    listeners.append( '    clone_vec->push_back(stmt);')
                    listeners.append( '  }')
                    listeners.append( '}')

                elif method in ['Ports']:
                    listeners.append(f'if (auto vec = inst->{method}()) {{')
                    listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                    listeners.append(f'  inst->{method}(clone_vec);')
                    listeners.append( '  for (auto obj : *vec) {')
                    listeners.append( '    auto* stmt = obj->DeepClone(serializer_, this, inst);')
                    listeners.append( '    stmt->VpiParent(inst);')
                    listeners.append( '    clone_vec->push_back(stmt);')
                    listeners.append( '  }')
                    listeners.append( '}')

        classname = models[classname]['extends']

    return listeners


def _generate_class_listeners(models):
    listeners = []

    for model in models.values():
        modeltype = model.get('type')
        if modeltype != 'obj_def':
            continue

        classname = model.get('name')
        if classname != 'class_defn':
            continue

        while classname:
            model = models[classname]

            for key, value in model.allitems():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    name = value.get('name')
                    type = value.get('type')
                    card = value.get('card')

                    cast = 'any' if key == 'group_ref' else type
                    Cast = cast[:1].upper() + cast[1:]
                    method = name[:1].upper() + name[1:]

                    if (card == 'any') and not method.endswith('s'):
                        method += 's'

                    if card == '1':
                        listeners.append(f'if (auto obj = cl->{method}()) {{')
                        listeners.append( '  auto* stmt = obj->DeepClone(serializer_, this, cl);')
                        listeners.append( '  stmt->VpiParent(cl);')
                        listeners.append(f'  cl->{method}(stmt);')
                        listeners.append( '}')

                    elif method == 'Deriveds':
                        # Don't deep clone
                        listeners.append(f'if (auto vec = cl->{method}()) {{')
                        listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                        listeners.append(f'  cl->{method}(clone_vec);')
                        listeners.append( '  for (auto obj : *vec) {')
                        listeners.append( '    auto* stmt = obj;')
                        listeners.append( '    clone_vec->push_back(stmt);')
                        listeners.append( '  }')
                        listeners.append( '}')

                    else:
                        listeners.append(f'if (auto vec = cl->{method}()) {{')
                        listeners.append(f'  auto clone_vec = serializer_->Make{Cast}Vec();')
                        listeners.append(f'  cl->{method}(clone_vec);')
                        listeners.append( '  for (auto obj : *vec) {')
                        listeners.append( '    auto* stmt = obj->DeepClone(serializer_, this, cl);')
                        listeners.append( '    stmt->VpiParent(cl);')
                        listeners.append( '    clone_vec->push_back(stmt);')
                        listeners.append( '  }')
                        listeners.append( '}')

            classname = models[classname]['extends']

    return listeners


def generate(models):
    module_listeners = _generate_module_listeners(models, 'module_inst')
    interface_listeners = _generate_module_listeners(models, 'interface_inst')
    class_listeners = _generate_class_listeners(models)

    with open(config.get_template_filepath('ElaboratorListener.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<MODULE_ELABORATOR_LISTENER>', (' ' * 10) + ('\n' + (' ' * 10)).join(module_listeners))
    file_content = file_content.replace('<INTERFACE_ELABORATOR_LISTENER>', (' ' * 10) + ('\n' + (' ' * 10)).join(interface_listeners))
    file_content = file_content.replace('<CLASS_ELABORATOR_LISTENER>', (' ' * 4) + ('\n' + (' ' * 4)).join(class_listeners))
    file_utils.set_content_if_changed(config.get_output_source_filepath('ElaboratorListener.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
