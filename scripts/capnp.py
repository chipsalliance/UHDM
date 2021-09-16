import ctypes
import os
import platform
import subprocess
import sys

import config
import file_utils


def _get_schema(type, vpi, card):
    mapping = {
      'string': 'UInt64',
      'unsigned int': 'UInt64',
      'int': 'Int64',
      'any': 'Int64',
      'bool': 'Bool',
      'value': 'UInt64',
      'delay': 'UInt64',
    }

    type = mapping.get(type, type)
    if card == '1':
        return (vpi, type)
    elif card == 'any':
        return (vpi, f'List({type})')
    else:
        return (None, None)


def generate(models):
    capnp_schema = {}
    capnp_schema_all = []
    capnpRootSchemaIndex = 2
    capnp_root_schema = []

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        capnp_schema[classname] = []

        Classname_ = classname[:1].upper() + classname[1:]
        Classname = Classname_.replace('_', '')

        if modeltype != 'class_def':
            capnp_schema[classname].append(('vpiParent', 'UInt64'))
            capnp_schema[classname].append(('uhdmParentType', 'UInt64'))
            capnp_schema[classname].append(('vpiFile', 'UInt64'))
            capnp_schema[classname].append(('vpiLineNo', 'UInt32'))
            capnp_schema[classname].append(('vpiColumnNo', 'UInt16'))
            capnp_schema[classname].append(('vpiEndLineNo', 'UInt32'))
            capnp_schema[classname].append(('vpiEndColumnNo', 'UInt16'))
            capnp_schema[classname].append(('uhdmId', 'UInt64'))

            capnp_root_schema.append(f'  factory{Classname} @{capnpRootSchemaIndex} :List({Classname});')
            capnpRootSchemaIndex += 1

        for key, value in model.allitems():
            if key == 'property':
                if value.get('name') != 'type':
                    vpi = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')
                    capnp_schema[classname].append(_get_schema(type, vpi, card))

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'

                name = name.replace('_', '')

                obj_key = 'ObjIndexType' if key in ['class_ref', 'group_ref'] else 'UInt64'
                capnp_schema[classname].append(_get_schema(obj_key, name, card))

        if modeltype != 'class_def':
            capnpIndex = 0
            capnp_schema_all.append(f'struct {Classname} {{')
            for name, type in capnp_schema[classname]:
                if name and type:
                    capnp_schema_all.append(f'  {name} @{capnpIndex} :{type};')
                    capnpIndex += 1

            # process baseclass recursively
            baseclass = model['extends']
            while baseclass:
                for name, type in capnp_schema[baseclass]:
                    if name and type:
                        capnp_schema_all.append(f'  {name} @{capnpIndex} :{type};')
                        capnpIndex += 1

                baseclass = models[baseclass]['extends']

            capnp_schema_all.append('}')
            capnp_schema_all.append('')

    with open(config.get_template_filepath('UHDM.capnp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_SCHEMA>', '\n'.join(capnp_schema_all))
    file_content = file_content.replace('<CAPNP_ROOT_SCHEMA>', '\n'.join(capnp_root_schema))
    file_utils.set_content_if_changed(config.get_output_source_filepath('UHDM.capnp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
