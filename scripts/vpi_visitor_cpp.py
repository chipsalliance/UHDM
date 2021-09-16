import config
import file_utils


def _get_implementation(classname, vpi, card):
    shallow_visit = ''
    content = []
    if (vpi == 'vpiParent') and (classname != 'part_select'):
        return content

    if card == '1':
        if 'func_call' in classname and vpi == 'vpiFunction':
            # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
            shallow_visit = ', true'

        if 'task_call' in classname and vpi == 'vpiTask':
            # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
            shallow_visit = ', true'

        # Prevent loop in Standard VPI
        if vpi not in ['vpiModule', 'vpiInterface']:
            content.append(f'    itr = vpi_handle({vpi}, obj_h);')
            content.append(f'    visit_object(itr, subobject_indent, "{vpi}", visited, out{shallow_visit});')
            content.append( '    release_handle(itr);')

    else:
        if classname == 'design':
            content.append('    if (indent == 0) visited->clear();')

        # Prevent loop in Standard VPI
        if vpi != 'vpiUse':
            content.append(f'    itr = vpi_iterate({vpi}, obj_h);')
            content.append( '    while (vpiHandle obj = vpi_scan(itr)) {')
            content.append(f'      visit_object(obj, subobject_indent, "{vpi}", visited, out{shallow_visit});')
            content.append( '      release_handle(obj);')
            content.append( '    }')
            content.append( '    release_handle(itr);')

    return content


def _get_vpi_str_visitor(type, vpi, card):
    content = []
    if (card == '1') and (type == 'string') and (vpi != 'vpiFile'):
        content.append(f'    if (const char* s = vpi_get_str({vpi}, obj_h))')
        content.append(f'      stream_indent(out, indent) << "|{vpi}:" << s << std::endl;')
    return content


def _get_vpi_xxx_visitor(type, vpi, card):
    content = []
    if vpi == 'vpiValue':
        content.append('    s_vpi_value value;')
        content.append('    vpi_get_value(obj_h, &value);')
        content.append('    if (value.format) {')
        content.append('      std::string val = visit_value(&value);')
        content.append('      if (!val.empty()) {')
        content.append('        stream_indent(out, indent) << val;')
        content.append('      }')
        content.append('    }')
    elif vpi == 'vpiDelay':
        content.append('    s_vpi_delay delay;')
        content.append('    vpi_get_delays(obj_h, &delay);')
        content.append('    if (delay.da != nullptr) {')
        content.append('      stream_indent(out, indent) << visit_delays(&delay);')
        content.append('    }')
    elif (card == '1') and (type != 'string') and (vpi not in ['vpiType', 'vpiLineNo', 'vpiColumnNo', 'vpiEndLineNo', 'vpiEndColumnNo']):
        content.append(f'    if (const int n = vpi_get({vpi}, obj_h))')
        content.append(f'      stream_indent(out, indent) << "|{vpi}:" << n << std::endl;')
    return content


def generate(models):
    vpi_iterator = {}
    vpi_get_body = {}
    vpi_get_str_body = {}

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']

        vpi_iterator[classname] = []
        vpi_get_body[classname] = []
        vpi_get_str_body[classname] = []

        if modeltype != 'class_def':
            vpi_iterator[classname].append(('vpiParent', '1'))
            vpi_get_str_body[classname].append(('string', 'vpiFile', '1'))

        type_specified = False
        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                vpi_get_body[classname].append((type, vpi, card))
                if name == 'type':
                    type_specified = True
                else:
                    vpi_get_str_body[classname].append((type, vpi, card))

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                vpi = value.get('vpi')
                card = value.get('card')

                vpi_iterator[classname].append((vpi, card))

        if not type_specified and (modeltype == 'obj_def'):
            vpi_get_body[classname].append(('unsigned int', 'vpiType', '1'))

    visitors = []
    for model in models.values():
        modeltype = model['type']
        if modeltype in ['group_def', 'class_def']:
            continue

        classname = model['name']

        locals = []
        remotes = []
        baseclass = classname
        while baseclass:
            for type, vpi, card in vpi_get_str_body[baseclass]:
                locals.extend(_get_vpi_str_visitor(type, vpi, card))

            for type, vpi, card in vpi_get_body[baseclass]:
                locals.extend(_get_vpi_xxx_visitor(type, vpi, card))

            for vpi, card in vpi_iterator[baseclass]:
                remotes.extend(_get_implementation(classname, vpi, card))

            baseclass = models[baseclass]['extends']

        if locals or remotes:
            vpi_name = config.make_vpi_name(classname)
            visitors.append(f'  if (objectType == {vpi_name}) {{')
            visitors.extend(locals)
            visitors.extend(remotes)
            visitors.append( '    return;')
            visitors.append( '  }')

    # vpi_visitor.cpp
    with open(config.get_template_filepath('vpi_visitor.cpp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<OBJECT_VISITORS>', '\n'.join(visitors))
    file_utils.set_content_if_changed(config.get_output_source_filepath('vpi_visitor.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
