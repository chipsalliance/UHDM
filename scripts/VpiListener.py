import config
import file_utils


def _get_listeners(classname, vpi, type, card):
    listeners = []

    if card == '1':
        # upward vpiModuleInst, vpiInterfaceInst relation (when card == 1, pointing to the parent object) creates loops in visitors
        if vpi in ['vpiParent', 'vpiInstance', 'vpiModuleInst', 'vpiInterfaceInst', 'vpiUse', 'vpiProgram', 'vpiClassDefn', 'vpiPackage', 'vpiUdp']:
            return listeners

        if 'func_call' in classname and vpi == 'vpiFunction':
          # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
          return listeners

        if 'task_call' in classname and vpi == 'vpiTask':
          # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
          return listeners

        listeners.append(f'  if (vpiHandle itr = vpi_handle({vpi}, handle)) {{')
        listeners.append(f'    listenAny(itr);')
        listeners.append( '    vpi_free_object(itr);')
        listeners.append( '  }')

    else:
        if 'uhdmall' in vpi:
          listeners.append(f'  uhdmAllIterator = true;')    
        listeners.append(f'  if (vpiHandle itr = vpi_iterate({vpi}, handle)) {{')
        listeners.append( '    while (vpiHandle obj = vpi_scan(itr)) {')
        listeners.append(f'      listenAny(obj);')
        listeners.append( '      vpi_free_object(obj);')
        listeners.append( '    }')
        listeners.append( '    vpi_free_object(itr);')
        listeners.append( '  }')
        if 'uhdmall' in vpi:
          listeners.append(f'  uhdmAllIterator = false;')
          listeners.append(f'  visited.clear();')
    return listeners


def generate(models):
    private_declarations = []
    private_implementations = []
    public_implementations = []
    classnames = set()

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]

        baseclass = model.get('extends')

        if model.get('subclasses') or modeltype == 'obj_def':
            private_declarations.append(f'  void listen{Classname_}_(vpiHandle handle);')
            private_implementations.append(f'void VpiListener::listen{Classname_}_(vpiHandle handle) {{')
            if baseclass:
                Baseclass_ = baseclass[:1].upper() + baseclass[1:]
                private_implementations.append(f'  listen{Baseclass_}_(handle);')

            for key, value in model.allitems():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    vpi  = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')

                    if key == 'group_ref':
                        type = 'any'

                    private_implementations.extend(_get_listeners(classname, vpi, type, card))

            private_implementations.append( '}')
            private_implementations.append( '')

        if modeltype != 'class_def':
            classnames.add(classname)

            public_implementations.append(f'void VpiListener::listen{Classname_}(vpiHandle handle) {{')
            public_implementations.append(f'  const {classname}* object = (const {classname}*) ((const uhdm_handle*)handle)->object;')
            public_implementations.append(f'  callstack.push_back(object);')
            public_implementations.append(f'  enter{Classname_}(object, handle);')
            public_implementations.append( '  if (visited.insert(object).second) {')
            public_implementations.append(f'    listen{Classname_}_(handle);')
            public_implementations.append( '  }')
            public_implementations.append(f'  leave{Classname_}(object, handle);')
            public_implementations.append(f'  callstack.pop_back();')
            public_implementations.append(f'}}')
            public_implementations.append( '')

    any_implementation = []
    enter_leave_declarations = []
    public_declarations = []
    for classname in sorted(classnames):
        Classname_ = classname[:1].upper() + classname[1:]

        any_implementation.append(f'    case uhdm{classname}: listen{Classname_}(handle); break;')

        enter_leave_declarations.append(f'  virtual void enter{Classname_}(const {classname}* object, vpiHandle handle) {{}}')
        enter_leave_declarations.append(f'  virtual void leave{Classname_}(const {classname}* object, vpiHandle handle) {{}}')
        enter_leave_declarations.append( '')

        public_declarations.append(f'  void listen{Classname_}(vpiHandle handle);')

   # VpiListener.h
    with open(config.get_template_filepath('VpiListener.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_PUBLIC_LISTEN_DECLARATIONS>', '\n'.join(public_declarations))
    file_content = file_content.replace('<VPI_PRIVATE_LISTEN_DECLARATIONS>', '\n'.join(private_declarations))
    file_content = file_content.replace('<VPI_ENTER_LEAVE_DECLARATIONS>', '\n'.join(enter_leave_declarations))
    file_utils.set_content_if_changed(config.get_output_header_filepath('VpiListener.h'), file_content)

    # VpiListener.cpp
    with open(config.get_template_filepath('VpiListener.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_PRIVATE_LISTEN_IMPLEMENTATIONS>', '\n'.join(private_implementations))
    file_content = file_content.replace('<VPI_PUBLIC_LISTEN_IMPLEMENTATIONS>', '\n'.join(public_implementations))
    file_content = file_content.replace('<VPI_LISTENANY_IMPLEMENTATION>', '\n'.join(any_implementation))
    file_utils.set_content_if_changed(config.get_output_source_filepath('VpiListener.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
