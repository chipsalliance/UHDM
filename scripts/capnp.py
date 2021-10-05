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
    individual_model_schemas = {}
    flattened_model_schemas = []
    root_schema_index = 2
    root_schema = []

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        individual_model_schemas[classname] = []

        Classname_ = classname[:1].upper() + classname[1:]
        Classname = Classname_.replace('_', '')

        if modeltype != 'class_def':
            individual_model_schemas[classname].append(('uhdmId', 'UInt64'))
            individual_model_schemas[classname].append(('vpiParent', 'UInt64'))
            individual_model_schemas[classname].append(('uhdmParentType', 'UInt64'))
            individual_model_schemas[classname].append(('vpiFile', 'UInt64'))
            individual_model_schemas[classname].append(('vpiLineNo', 'UInt32'))
            individual_model_schemas[classname].append(('vpiEndLineNo', 'UInt32'))
            individual_model_schemas[classname].append(('vpiColumnNo', 'UInt16'))
            individual_model_schemas[classname].append(('vpiEndColumnNo', 'UInt16'))

            root_schema.append(f'  factory{Classname} @{root_schema_index} :List({Classname});')
            root_schema_index += 1

        for key, value in model.allitems():
            if key == 'property':
                if value.get('name') != 'type':
                    vpi = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')
                    individual_model_schemas[classname].append(_get_schema(type, vpi, card))

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'

                name = name.replace('_', '')

                obj_key = 'ObjIndexType' if key in ['class_ref', 'group_ref'] else 'UInt64'
                individual_model_schemas[classname].append(_get_schema(obj_key, name, card))

        if modeltype != 'class_def':
            # Process the hierarchy tree top-down
            stack = []
            baseclass = classname
            while (baseclass):
                stack.append(baseclass)
                baseclass = models[baseclass]['extends']

            capnpIndex = 0
            flattened_model_schemas.append(f'struct {Classname} {{')

            while stack:
                for name, type in individual_model_schemas[stack.pop()]:
                    if name and type:
                        flattened_model_schemas.append(f'  {name} @{capnpIndex} :{type};')
                        capnpIndex += 1

            flattened_model_schemas.append('}')
            flattened_model_schemas.append('')

    with open(config.get_template_filepath('UHDM.capnp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_SCHEMA>', '\n'.join(flattened_model_schemas))
    file_content = file_content.replace('<CAPNP_ROOT_SCHEMA>', '\n'.join(root_schema))
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
