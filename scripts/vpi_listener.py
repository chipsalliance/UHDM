import config
import file_utils


def _get_listeners(classname, vpi, type, card):
    listeners = []

    if vpi in [ 'vpiParent', 'vpiInstance', 'vpiExtends' ]:
        return listeners # To prevent infinite loops in visitors as these relations are pointing upward in the tree

    elif card == '1':
        # upward vpiModule, vpiInterface relation (when card == 1, pointing to the parent object) creates loops in visitors
        if vpi in ['vpiModule', 'vpiInterface']:
            return listeners

        if 'func_call' in classname and vpi == 'vpiFunction':
          # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
          return listeners

        if 'task_call' in classname and vpi == 'vpiTask':
          # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
          return listeners

        listeners.append(f'  if (vpiHandle itr = vpi_handle({vpi}, handle)) {{')
        listeners.append(f'    listen_any(itr, listener, visited);')
        listeners.append( '    vpi_free_object(itr);')
        listeners.append( '  }')

    else:
        listeners.append(f'  if (vpiHandle itr = vpi_iterate({vpi}, handle)) {{')
        listeners.append( '    while (vpiHandle obj = vpi_scan(itr)) {')
        listeners.append(f'      listen_any(obj, listener, visited);')
        listeners.append( '      vpi_free_object(obj);')
        listeners.append( '    }')
        listeners.append( '    vpi_free_object(itr);')
        listeners.append( '  }')

    return listeners


def generate(models):
    declarations = ['void listen_any(vpiHandle handle, VpiListener* listener);']
    private_implementations = []
    public_implementations = []
    classnames = set()

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]

        if modeltype != 'class_def':
            classnames.add(classname)

        baseclass = model.get('extends')

        declarations.append(f'void listen_{classname}(vpiHandle handle, VpiListener* listener);')
        declarations.append(f'void listen_{classname}(vpiHandle handle, VpiListener* listener, VisitedContainer* visited);')

        private_implementations.append(f'static void listen_{classname}_(const {classname}* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle, VpiListener* listener, VisitedContainer* visited) {{')
        if baseclass:
            private_implementations.append(f'  listen_{baseclass}_(object, parent, handle, parentHandle, listener, visited);')

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

        public_implementations.append(f'void UHDM::listen_{classname}(vpiHandle handle, VpiListener* listener, VisitedContainer* visited) {{')
        public_implementations.append(f'  const {classname}* object = (const {classname}*) ((const uhdm_handle*)handle)->object;')
        public_implementations.append( '  const BaseClass* parent = object->VpiParent();')
        public_implementations.append( '  vpiHandle parentHandle = (parent != nullptr) ? NewVpiHandle(parent) : nullptr;')
        public_implementations.append(f'  listener->enter{Classname_}(object, parent, handle, parentHandle);')
        public_implementations.append( '  if (visited->insert(object).second) {')
        public_implementations.append(f'    listen_{classname}_(object, parent, handle, parentHandle, listener, visited);')
        public_implementations.append( '  }')
        public_implementations.append(f'  listener->leave{Classname_}(object, parent, handle, parentHandle);')
        public_implementations.append( '  vpi_release_handle(parentHandle);')
        public_implementations.append(f'}}')
        public_implementations.append( '')
        public_implementations.append(f'void UHDM::listen_{classname}(vpiHandle handle, VpiListener* listener) {{')
        public_implementations.append( '  VisitedContainer visited;')
        public_implementations.append(f'  listen_{classname}(handle, listener, &visited);')
        public_implementations.append(f'}}')
        public_implementations.append( '')

   # vpi_listener.h
    with open(config.get_template_filepath('vpi_listener.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENERS_HEADER>', '\n'.join(declarations))
    file_utils.set_content_if_changed(config.get_output_header_filepath('vpi_listener.h'), file_content)

    implementations = private_implementations + public_implementations + [
        'void UHDM::listen_any(vpiHandle handle, VpiListener* listener) {',
        '  VisitedContainer visited;',
        '  listen_any(handle, listener, &visited);',
        '}',
        ''
    ]
    any_implementation = [
      f'  case uhdm{classname}: listen_{classname}(handle, listener, visited); break;'
          for classname in sorted(classnames)
    ]

    # vpi_listener.cpp
    with open(config.get_template_filepath('vpi_listener.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENERS>', '\n'.join(implementations))
    file_content = file_content.replace('<VPI_ANY_LISTENERS>', '\n'.join(any_implementation))
    file_utils.set_content_if_changed(config.get_output_source_filepath('vpi_listener.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
