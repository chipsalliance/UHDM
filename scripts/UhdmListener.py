import config
import file_utils


def _get_listen_implementation(classname, name, vpi, type, card):
    listeners = []

    Name_ = name[:1].upper() + name[1:]
    Classname_ = classname[:1].upper() + classname[1:]

    if vpi in [ 'vpiParent', 'vpiInstance', 'vpiExtends' ]:
        return listeners # To prevent infinite loops in visitors as these relations are pointing upward in the tree
    
    elif card == '1':
        # upward vpiModule, vpiInterface relation (when card == 1, pointing to the parent object) creates loops in visitors
        if vpi in [ 'vpiModule', 'vpiInterface' ]:
            return listeners

        if 'func_call' in classname and vpi == 'vpiFunction':
          # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
          return listeners

        if 'task_call' in classname and vpi == 'vpiTask':
          # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
          return listeners

        listeners.append(f'  if (const any *const {name}_ = object->{Name_}()) {{')
        listeners.append(f'    listenAny({name}_);')
        listeners.append( '  }')

    else:
        listeners.append(f'  if (const VectorOf{type} *const {name}_ = object->{Name_}()) {{')
        listeners.append(f'    enter{Name_}(object, *{name}_);')
        listeners.append(f'    for (VectorOf{type}::const_reference element : *{name}_) {{')
        listeners.append(f'      listenAny(element);')
        listeners.append( '    }')
        listeners.append(f'    leave{Name_}(object, *{name}_);')
        listeners.append( '  }')

    return listeners


def generate(models):
    private_declarations = []
    private_implementations = []
    public_implementations = []
    vector_enters_leaves = set()
    classnames = set()

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]

        baseclass = model.get('extends')

        if model.get('subclasses') or modeltype == 'obj_def':
            private_declarations.append(f'  void listen{Classname_}_(const {classname}* const object);')
            private_implementations.append(f'void UhdmListener::listen{Classname_}_(const {classname}* const object) {{')
            if baseclass:
                Baseclass_ = baseclass[:1].upper() + baseclass[1:]
                private_implementations.append(f'  listen{Baseclass_}_(object);')

            for key, value in model.allitems():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    name = value.get('name')
                    vpi  = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')

                    if (card == 'any') and not name.endswith('s'):
                        name += 's'

                    if key == 'group_ref':
                        type = 'any'

                    listen_implementation = _get_listen_implementation(classname, name, vpi, type, card)
                    private_implementations.extend(listen_implementation)

                    if card == 'any' and listen_implementation:
                        vector_enters_leaves.add((name, type))

            private_implementations.append( '}')
            private_implementations.append( '')

        if modeltype != 'class_def':
            classnames.add(classname)

            public_implementations.append(f'void UhdmListener::listen{Classname_}(const {classname}* const object) {{')
            public_implementations.append(f'  enter{Classname_}(object);')
            public_implementations.append( '  if (visited.insert(object).second) {')
            public_implementations.append( '    callstack.push_back(object);')
            public_implementations.append(f'    listen{Classname_}_(object);')
            public_implementations.append( '    callstack.pop_back();')
            public_implementations.append( '  }')
            public_implementations.append(f'  leave{Classname_}(object);')
            public_implementations.append(f'}}')
            public_implementations.append( '')

    any_implementation = []
    uhdm_enter_leave_declarations = []
    public_declarations = []
    for classname in sorted(classnames):
        Classname_ = classname[:1].upper() + classname[1:]

        any_implementation.append(f'  case uhdm{classname}: listen{Classname_}(static_cast<const {classname} *>(object)); break;')

        uhdm_enter_leave_declarations.append(f'  virtual void enter{Classname_}(const {classname}* const object) {{}}')
        uhdm_enter_leave_declarations.append(f'  virtual void leave{Classname_}(const {classname}* const object) {{}}')
        uhdm_enter_leave_declarations.append( '')

        public_declarations.append(f'  void listen{Classname_}(const {classname} *const object);')

    enter_leave_vector_declarations = []
    for name, type in sorted(vector_enters_leaves):
        Name_ = name[:1].upper() + name[1:]

        enter_leave_vector_declarations.append(f'  virtual void enter{Name_}(const any* const object, const VectorOf{type}& objects) {{}}')
        enter_leave_vector_declarations.append(f'  virtual void leave{Name_}(const any* const object, const VectorOf{type}& objects) {{}}')
        enter_leave_vector_declarations.append( '')

    private_declarations = sorted(private_declarations)

   # UhdmListener.h
    with open(config.get_template_filepath('UhdmListener.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<UHDM_PUBLIC_LISTEN_DECLARATIONS>', '\n'.join(public_declarations))
    file_content = file_content.replace('<UHDM_PRIVATE_LISTEN_DECLARATIONS>', '\n'.join(private_declarations))
    file_content = file_content.replace('<UHDM_ENTER_LEAVE_DECLARATIONS>', '\n'.join(uhdm_enter_leave_declarations))
    file_content = file_content.replace('<UHDM_ENTER_LEAVE_VECTOR_DECLARATIONS>', '\n'.join(enter_leave_vector_declarations))
    file_utils.set_content_if_changed(config.get_output_header_filepath('UhdmListener.h'), file_content)

    # UhdmListener.cpp
    with open(config.get_template_filepath('UhdmListener.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<UHDM_PRIVATE_LISTEN_IMPLEMENTATIONS>', '\n'.join(private_implementations))
    file_content = file_content.replace('<UHDM_PUBLIC_LISTEN_IMPLEMENTATIONS>', '\n'.join(public_implementations))
    file_content = file_content.replace('<UHDM_LISTENANY_IMPLEMENTATION>', '\n'.join(any_implementation))
    file_utils.set_content_if_changed(config.get_output_source_filepath('UhdmListener.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
