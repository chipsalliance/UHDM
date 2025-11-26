import config
import file_utils
import uhdm_types_h


def _get_listen_implementation(classname, name, vpi, type, card):
    listeners = []

    FuncName = config.make_func_name(name, card)
    TypeName = config.make_class_name(type)

    if card == '1':
        suffix = 'Obj 'if vpi in ['vpiName'] else ''
        listeners.append(f'  if (const Any *const any = object->get{FuncName}{suffix}()) {{')
        listeners.append(f'    listenAny(any, {vpi});')
        listeners.append( '  }')
    else:
        listeners.append(f'  if (const {TypeName}Collection *const collection = object->get{FuncName}()) {{')
        listeners.append(f'    enter{TypeName}Collection(object, *collection, {vpi});')
        listeners.append(f'    for ({TypeName}Collection::const_reference any : *collection) {{')
        listeners.append(f'      listenAny(any, {vpi});')
        listeners.append( '    }')
        listeners.append(f'    leave{TypeName}Collection(object, *collection, {vpi});')
        listeners.append( '  }')

    return listeners


def _get_trace_listeners(models):
    object_methods = []
    for model in models.values():
        if model['type'] not in ['class_def', 'group_def']:
            classname = model['name']
            ClassName = config.make_class_name(classname)

            object_methods.append(f'    void enter{ClassName}(const {ClassName}* const object, uint32_t vpiRelation = 0) final {{ TRACE_ENTER; }}')
            object_methods.append(f'    void leave{ClassName}(const {ClassName}* const object, uint32_t vpiRelation = 0) final {{ TRACE_LEAVE; }}')
            object_methods.append('')

    collection_methods = []
    for TypeName in sorted(uhdm_types_h.get_type_map(models).keys()):
        if TypeName != 'BaseClass':
            collection_methods.append(f'    void enter{TypeName}Collection(const Any* const object, const {TypeName}Collection& objects, uint32_t vpiRelation = 0) final {{ TRACE_ENTER; }}')
            collection_methods.append(f'    void leave{TypeName}Collection(const Any* const object, const {TypeName}Collection& objects, uint32_t vpiRelation = 0) final {{ TRACE_LEAVE; }}')
            collection_methods.append('')

    return object_methods, collection_methods


def generate(models):
    private_declarations = []
    private_implementations = []
    public_declarations = []
    public_implementations = []
    ClassNames = set()

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        ClassName = config.make_class_name(classname)

        basename = model.get('extends') or 'Any'
        BaseName = config.make_class_name(basename)

        if model.get('subclasses') or modeltype == 'obj_def':
            private_declarations.append(f'  void listen{ClassName}_(const {ClassName}* object);')
            private_implementations.append(f'void UhdmListener::listen{ClassName}_(const {ClassName}* object) {{')
            private_implementations.append(f'  listen{BaseName}_(object);')

            for key, value in model.allitems():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    name = value.get('name')
                    vpi  = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')

                    if key == 'group_ref':
                        type = 'any'

                    private_implementations.extend(_get_listen_implementation(classname, name, vpi, type, card))

            private_implementations.append( '}')
            private_implementations.append( '')

        if modeltype != 'class_def':
            ClassNames.add(ClassName)

            public_declarations.append(f'  void listen{ClassName}(const {ClassName}* object, uint32_t vpiRelation = 0);')

            public_implementations.append(f'void UhdmListener::listen{ClassName}(const {ClassName}* object, uint32_t vpiRelation /* = 0 */) {{')
            public_implementations.append(f'  enter{ClassName}(object, vpiRelation);')
            public_implementations.append( '  if (m_visited.emplace(object).second) {')
            public_implementations.append( '    m_callstack.emplace_back(object);')
            public_implementations.append(f'    listen{ClassName}_(object);')
            public_implementations.append( '    m_callstack.pop_back();')
            public_implementations.append( '  }')
            public_implementations.append(f'  leave{ClassName}(object, vpiRelation);')
            public_implementations.append( '}')
            public_implementations.append( '')

    any_implementation = []
    enter_leave_declarations = []
    for ClassName in sorted(ClassNames):
        any_implementation.append(f'    case UhdmType::{ClassName}: listen{ClassName}(static_cast<const {ClassName} *>(object), vpiRelation); break;')

        enter_leave_declarations.append(f'  virtual void enter{ClassName}(const {ClassName}* object, uint32_t vpiRelation) {{}}')
        enter_leave_declarations.append(f'  virtual void leave{ClassName}(const {ClassName}* object, uint32_t vpiRelation) {{}}')
        enter_leave_declarations.append( '')

    enter_leave_collection_declarations = []
    for TypeName in sorted(uhdm_types_h.get_type_map(models).keys()):
        if TypeName != 'BaseClass':
            enter_leave_collection_declarations.append(f'  virtual void enter{TypeName}Collection(const Any* object, const {TypeName}Collection& objects, uint32_t vpiRelation) {{}}')
            enter_leave_collection_declarations.append(f'  virtual void leave{TypeName}Collection(const Any* object, const {TypeName}Collection& objects, uint32_t vpiRelation) {{}}')
            enter_leave_collection_declarations.append( '')

    private_declarations = sorted(private_declarations)

    object_methods, collection_methods = _get_trace_listeners(models)

   # UhdmListener.h
    with open(config.get_template_filepath('UhdmListener.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('//<UHDM_PUBLIC_LISTEN_DECLARATIONS>', '\n'.join(sorted(public_declarations)))
    file_content = file_content.replace('//<UHDM_PRIVATE_LISTEN_DECLARATIONS>', '\n'.join(private_declarations))
    file_content = file_content.replace('//<UHDM_ENTER_LEAVE_DECLARATIONS>', '\n'.join(enter_leave_declarations))
    file_content = file_content.replace('//<UHDM_ENTER_LEAVE_COLLECTION_DECLARATIONS>', '\n'.join(enter_leave_collection_declarations))
    file_content = file_content.replace('//<UHDM_LISTENER_OBJECT_TRACER_METHODS>', '\n'.join(object_methods))
    file_content = file_content.replace('//<UHDM_LISTENER_COLLECTION_TRACER_METHODS>', '\n'.join(collection_methods))
    file_utils.set_content_if_changed(config.get_output_header_filepath('UhdmListener.h'), file_content)

    # UhdmListener.cpp
    with open(config.get_template_filepath('UhdmListener.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('//<UHDM_PRIVATE_LISTEN_IMPLEMENTATIONS>', '\n'.join(private_implementations))
    file_content = file_content.replace('//<UHDM_PUBLIC_LISTEN_IMPLEMENTATIONS>', '\n'.join(public_implementations))
    file_content = file_content.replace('//<UHDM_LISTENANY_IMPLEMENTATION>', '\n'.join(any_implementation))
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
