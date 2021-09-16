import config
import file_utils
from collections import defaultdict


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

        listeners.append(f'    itr = vpi_handle({vpi}, object);')
        listeners.append( '    if (itr) {')
        listeners.append(f'      listen_any(itr, listener, visited);')
        listeners.append( '      vpi_free_object(itr);')
        listeners.append( '    }')

    else:
        listeners.append(f'    itr = vpi_iterate({vpi}, object);')
        listeners.append( '    if (itr) {')
        listeners.append( '      while (vpiHandle obj = vpi_scan(itr)) {')
        listeners.append(f'        listen_any(obj, listener, visited);')
        listeners.append( '        vpi_free_object(obj);')
        listeners.append( '      }')
        listeners.append( '      vpi_free_object(itr);')
        listeners.append( '    }')

    return listeners


def generate(models):
    declarations = []
    iterators = {}
    implementations = []
    classnames = set()

    decl_appended = False
    def _append_iterators(iterators):
        nonlocal decl_appended
        nonlocal implementations
        if iterators:
            if not decl_appended:
                implementations.append( '    vpiHandle itr;')
                decl_appended = True
            implementations.extend(iterators)

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]

        iterators[classname] = []
        declarations.append(f'void listen_{classname}(vpiHandle object, UHDM::VpiListener* listener, UHDM::VisitedContainer* visited);')

        implementations.append(f'void UHDM::listen_{classname}(vpiHandle object, VpiListener* listener, UHDM::VisitedContainer* visited) {{')
        implementations.append(f'  {classname}* d = ({classname}*) ((const uhdm_handle*)object)->object;')
        implementations.append( '  const bool alreadyVisited = (visited->find(d) != visited->end());')
        implementations.append( '  visited->insert(d);')
        implementations.append( '  const BaseClass* parent = d->VpiParent();')
        implementations.append( '  vpiHandle parent_h = parent ? NewVpiHandle(parent) : 0;')
        implementations.append(f'  listener->enter{Classname_}(d, parent, object, parent_h);')
        implementations.append( '  if (!alreadyVisited) {')

        if modeltype != 'class_def':
            classnames.add(classname)

        decl_appended = False

        for key, value in model.allitems():
            if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                vpi  = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                if key == 'group_ref':
                    type = 'any'

                _append_iterators(_get_listeners(classname, vpi, type, card))
                iterators[classname].append((vpi, type, card))

        # process baseclass recursively
        baseclass = model['extends']
        while baseclass:
            for vpi, type, card in iterators[baseclass]:
                _append_iterators(_get_listeners(classname, vpi, type, card))

            baseclass = models[baseclass]['extends']

        implementations.append( '  }')
        implementations.append(f'  listener->leave{Classname_}(d, parent, object, parent_h);')
        implementations.append( '  vpi_release_handle(parent_h);')
        implementations.append( '}')
        implementations.append( '')


   # vpi_listener.h
    with open(config.get_template_filepath('vpi_listener.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENERS_HEADER>', '\n'.join(declarations))
    file_utils.set_content_if_changed(config.get_output_header_filepath('vpi_listener.h'), file_content)

    any_implementation = []
    for classname in sorted(classnames):
        any_implementation.append(f'  case uhdm{classname}:')
        any_implementation.append(f'    listen_{classname}(object, listener, visited);')
        any_implementation.append( '    break;')

    # vpi_listener.cpp
    with open(config.get_template_filepath('vpi_listener.cpp'), 'r+t') as strm:
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
