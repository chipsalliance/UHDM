from collections import OrderedDict

import config
import file_utils


def _get_implementation(classname, vpi, card):
    content = []

    if card == '1':
        shallow_visit = 'false'

        if vpi in ['vpiParent', 'vpiInstance', 'vpiModule', 'vpiInterface', 'vpiUse', 'vpiProgram', 'vpiClassDefn', 'vpiPackage', 'vpiUdp']:
            # Prevent walking upwards and makes the UHDM output cleaner
            # Prevent loop in Standard VPI
            shallow_visit = 'true'

        if 'func_call' in classname and vpi == 'vpiFunction':
            # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
            shallow_visit = 'true'

        if 'task_call' in classname and vpi == 'vpiTask':
            # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
            shallow_visit = 'true'

        if classname in ['ref_obj']:
            # Ref_obj are always printed shallow
            shallow_visit = 'true'

        content.append(f'  if (vpiHandle itr = vpi_handle({vpi}, obj_h)) {{')
        content.append(f'    visit_object(itr, indent + kLevelIndent, "{vpi}", visited, out, {shallow_visit});')
        content.append( '    release_handle(itr);')
        content.append( '  }')
    else:
        shallow_visit = 'false'
        if 'module_inst' in classname and vpi == 'vpiRefModule':
            # Prevent stepping inside ref_modules while processing module_inst
            shallow_visit = 'true'

        content.append(f'  if (vpiHandle itr = vpi_iterate({vpi}, obj_h)) {{')
        content.append( '    while (vpiHandle obj = vpi_scan(itr)) {')
        content.append(f'      visit_object(obj, indent + kLevelIndent, "{vpi}", visited, out, {shallow_visit});')
        content.append( '      release_handle(obj);')
        content.append( '    }')
        content.append( '    release_handle(itr);')
        content.append( '  }')

    return content


def _get_vpi_xxx_visitor(type, vpi, card):
    content = []
    if vpi == 'vpiValue':
        content.append('  s_vpi_value value;')
        content.append('  vpi_get_value(obj_h, &value);')
        content.append('  if (value.format) {')
        content.append('    std::string val = visit_value(&value);')
        content.append('    if (!val.empty()) {')
        content.append('      stream_indent(out, indent) << val;')
        content.append('    }')
        content.append('  }')
    elif vpi == 'vpiDelay':
        content.append('  s_vpi_delay delay;')
        content.append('  vpi_get_delays(obj_h, &delay);')
        content.append('  if (delay.da != nullptr) {')
        content.append('    stream_indent(out, indent) << visit_delays(&delay);')
        content.append('  }')
    elif (card == '1') and (vpi not in ['vpiType', 'vpiFile', 'vpiLineNo', 'vpiColumnNo', 'vpiEndLineNo', 'vpiEndColumnNo']):
        if type == 'string':
            content.append(f'  if (const char* s = vpi_get_str({vpi}, obj_h))')
            content.append(f'    stream_indent(out, indent) << "|{vpi}:" << s << "\\n";') # no std::endl, avoid flush
        else:
            content.append(f'  if (const int n = vpi_get({vpi}, obj_h))')
            content.append(f'    stream_indent(out, indent) << "|{vpi}:" << n << "\\n";') # no std::endl, avoid flush
    return content


def generate(models):
    visit_object_body = []
    private_visitor_bodies = []

    ignored_objects = set(['vpiNet'])

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        if classname in ['net_drivers', 'net_loads']:
            continue

        vpi_name = config.make_vpi_name(classname)
        if vpi_name not in ignored_objects:
            visit_object_body.append(f'    case {vpi_name}: visit_{classname}(obj_h, indent, relation, visited, out, shallowVisit); break;')

        private_visitor_bodies.append(f'static void visit_{classname}(vpiHandle obj_h, int indent, const char *relation, VisitedContainer* visited, std::ostream& out, bool shallowVisit) {{')

        # Make sure vpiParent is called before the base class visit.
        if modeltype != 'class_def':
            private_visitor_bodies.extend(_get_implementation(classname, 'vpiParent', '1'))

        baseclass = model.get('extends', None)
        if baseclass:
            private_visitor_bodies.append(f'  visit_{baseclass}(obj_h, indent, relation, visited, out, shallowVisit);')

        if modeltype != 'class_def':
            private_visitor_bodies.extend(_get_vpi_xxx_visitor('string', 'vpiFile', '1'))
        
        type_specified = False
        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                type_specified = name == 'type' or type_specified
                private_visitor_bodies.extend(_get_vpi_xxx_visitor(type, vpi, card))

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                vpi = value.get('vpi')
                card = value.get('card')

                private_visitor_bodies.extend(_get_implementation(classname, vpi, card))

        if not type_specified and (modeltype == 'obj_def'):
            private_visitor_bodies.extend(_get_vpi_xxx_visitor('unsigned int', 'vpiType', '1'))

        private_visitor_bodies.append(f'}}')
        private_visitor_bodies.append('')

    visitors = [ f'  switch (objectType) {{' ] + sorted(visit_object_body) + [ f'  }}' ]

    # vpi_visitor.cpp
    with open(config.get_template_filepath('vpi_visitor.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<OBJECT_VISITORS>', '\n'.join(visitors))
    file_content = file_content.replace('<PRIVATE_OBJECT_VISITORS>', '\n'.join(private_visitor_bodies))
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
