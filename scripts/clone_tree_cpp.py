import config
import file_utils


def generate(models):
    implementations = []

    for model in models.values():
        modeltype = model.get('type')
        if modeltype != 'obj_def':
            continue

        classname = model.get('name')
        if '_call' in classname or classname in [ 'function', 'task', 'constant', 'tagged_pattern', 'gen_scope_array', 'hier_path', 'cont_assign' ]:
            continue  # Use hardcoded implementations

        Classname = classname[0].upper() + classname[1:]
        vpi_name = config.make_vpi_name(classname)

        implementations.append(f'{classname}* {classname}::DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const {{')
        if 'Net' in vpi_name:
            implementations.append(f'  {classname}* clone = dynamic_cast<{classname}*>(elaborator->bindNet(VpiName()));')
            implementations.append( '  if (clone != nullptr) {')
            implementations.append(f'    return clone;')
            implementations.append( '  }')
            implementations.append(f'  clone = serializer->Make{Classname}();')

        elif 'Parameter' in vpi_name:
            implementations.append(f'  {classname}* clone = dynamic_cast<{classname}*>(elaborator->bindParam(VpiName()));')
            implementations.append( '  if (clone == nullptr) {')
            implementations.append(f'    clone = serializer->Make{Classname}();')
            implementations.append( '  }')

        else:
            implementations.append(f'  {classname}* const clone = serializer->Make{Classname}();')

        implementations.append('  const unsigned long id = clone->UhdmId();')
        implementations.append('  *clone = *this;')
        implementations.append('  clone->UhdmId(id);')

        if classname in ['part_select', 'bit_select', 'indexed_part_select']:
            implementations.append('  if (const any* parent = VpiParent()) {')
            implementations.append('    ref_obj* ref = serializer->MakeRef_obj();')
            implementations.append('    clone->VpiParent(ref);')
            implementations.append('    ref->VpiName(parent->VpiName());')
            implementations.append('    if (parent->UhdmType() == uhdmref_obj) {')
            implementations.append('      ref->VpiFullName(((ref_obj*) VpiParent())->VpiFullName());')
            implementations.append('    }')
            implementations.append('    ref->VpiParent((any*) parent);')
            implementations.append('    ref->Actual_group(elaborator->bindAny(ref->VpiName()));')
            implementations.append('    if (!ref->Actual_group())')
            implementations.append('      if (parent->UhdmType() == uhdmref_obj) {')
            implementations.append('        ref->Actual_group((any*) ((ref_obj*) VpiParent())->Actual_group());')
            implementations.append('      }')
            implementations.append('  }')
        else:
            implementations.append('  clone->VpiParent(parent);')

        if 'BitSelect' in vpi_name:
            implementations.append('  if (any* n = elaborator->bindNet(VpiName())) {')
            implementations.append('    if (net* nn = dynamic_cast<net*>(n))')
            implementations.append('      clone->VpiFullName(nn->VpiFullName());')
            implementations.append('  }')

        baseclass = classname
        while baseclass:
            model = models[baseclass]
            classname = model.get('name')

            for key, value in model.allitems():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    name = value.get('name')
                    vpi = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')

                    cast = 'any' if key == 'group_ref' else type
                    Cast = cast[:1].upper() + cast[1:]
                    method = name[:1].upper() + name[1:]

                    if (card == 'any') and not method.endswith('s'):
                        method += 's'

                    # Unary relations
                    if card == '1':
                        if (classname in ['ref_obj', 'ref_var']) and (method == 'Actual_group'):
                            implementations.append(f'  clone->{method}(elaborator->bindAny(VpiName()));')
                            implementations.append(f'  if (!clone->{method}()) clone->{method}((any*) this->{method}());')

                        elif method in ['Task', 'Function']:
                            prefix = ''
                            if 'method_' in classname:
                                implementations.append(f'  const ref_obj* ref = dynamic_cast<const ref_obj*> (clone->Prefix());')
                                implementations.append( '  const class_var* prefix = nullptr;')
                                implementations.append( '  if (ref) prefix = dynamic_cast<const class_var*> (ref->Actual_group());')
                                prefix = ', prefix'
                            implementations.append(f'  if ({method.lower()}* t = dynamic_cast<{method.lower()}*> (elaborator->bindTaskFunc(VpiName(){prefix}))) {{')
                            implementations.append(f'    clone->{method}(t);')
                            implementations.append( '  } else {')
                            implementations.append( '    elaborator->scheduleTaskFuncBinding(clone);')
                            implementations.append( '  }')

                        elif classname == 'disable' and method == 'VpiExpr':
                            implementations.append(f'  if (auto obj = {method}()) clone->{method}((expr*) obj);')

                        elif classname == 'function' and method == 'Return':
                            implementations.append(f'  if (auto obj = {method}()) clone->{method}((variables*) obj);')

                        elif classname == 'class_typespec' and method == 'Class_defn':
                            implementations.append(f'  if (auto obj = {method}()) clone->{method}((class_defn*) obj);')

                        elif method == 'Instance':
                            implementations.append(f'  if (auto obj = {method}()) clone->{method}((instance*) obj);')
                            implementations.append( '  if (instance* inst = dynamic_cast<instance*>(parent))')
                            implementations.append( '    clone->Instance(inst);')

                        elif method == 'Module':
                            implementations.append(f'  if (auto obj = {method}()) clone->{method}((module*) obj);')

                        elif method == 'Typespec':
                            implementations.append( '  if (elaborator->uniquifyTypespec()) {')
                            implementations.append(f'    if (auto obj = {method}()) clone->{method}(obj->DeepClone(serializer, elaborator, clone));')
                            implementations.append( '  } else {')
                            implementations.append(f'    if (auto obj = {method}()) clone->{method}((typespec*) obj);')
                            implementations.append( '  }')

                        else:
                            implementations.append(f'  if (auto obj = {method}()) clone->{method}(obj->DeepClone(serializer, elaborator, clone));')

                    # N-ary relations
                    elif method == 'Typespecs':
                        implementations.append(f'  if (auto vec = {method}()) {{')
                        implementations.append(f'    auto clone_vec = serializer->Make{Cast}Vec();')
                        implementations.append(f'    clone->{method}(clone_vec);')
                        implementations.append( '    for (auto obj : *vec) {')
                        implementations.append( '      if (elaborator->uniquifyTypespec()) {')
                        implementations.append( '        clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));')
                        implementations.append( '      } else {')
                        implementations.append( '        clone_vec->push_back(obj);')
                        implementations.append( '      }')
                        implementations.append( '    }')
                        implementations.append( '  }')

                    elif classname == 'class_defn' and method == 'Deriveds':
                        # Don't deep clone
                        implementations.append(f'  if (auto vec = {method}()) {{')
                        implementations.append(f'    auto clone_vec = serializer->Make{Cast}Vec();')
                        implementations.append(f'    clone->{method}(clone_vec);')
                        implementations.append( '    for (auto obj : *vec) {')
                        implementations.append( '      clone_vec->push_back(obj);')
                        implementations.append( '    }')
                        implementations.append( '  }')

                    else:
                        implementations.append(f'  if (auto vec = {method}()) {{')
                        implementations.append(f'    auto clone_vec = serializer->Make{Cast}Vec();')
                        implementations.append(f'    clone->{method}(clone_vec);')
                        implementations.append( '    for (auto obj : *vec) {')
                        implementations.append( '      clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));')
                        implementations.append( '    }')
                        implementations.append( '  }')

            baseclass = models[baseclass]['extends']

        implementations.append('  return clone;')
        implementations.append('}')
        implementations.append('')

    with open(config.get_template_filepath('clone_tree.cpp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CLONE_IMPLEMENTATIONS>', '\n'.join(implementations))
    file_utils.set_content_if_changed(config.get_output_source_filepath('clone_tree.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
