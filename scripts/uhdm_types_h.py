from collections import OrderedDict

import config
import file_utils


_next_objectid = 2001 # above sv_vpi_user.h and vhpi_user.h ranges.
_object_ids = dict()


def _get_type_id(name):
    global _next_objectid, _object_ids

    id = 0
    if name in _object_ids:
        id = _object_ids[name]

    else:
        id = _next_objectid
        _next_objectid += 1
        _object_ids[name] = id

    return id


def get_type_map(models):
    types = OrderedDict()
    for model in models.values():
        classname = model['name']

        typename = f'uhdm{classname}'
        types[typename] = _get_type_id(typename)

        for key, value in model.allitems():
            if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                card = value.get('card')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'

                typename = f'uhdm{name}'
                types[typename] = _get_type_id(typename)

    return types


def generate(models):
    types = '\n'.join([f'  {name} = {id},' for name, id in get_type_map(models).items()])

    with open(config.get_template_filepath('uhdm_types.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<DEFINES>', types)
    file_utils.set_content_if_changed(config.get_output_header_filepath('uhdm_types.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
