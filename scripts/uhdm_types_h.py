from collections import OrderedDict

import config
import file_utils


_next_objectid = 2001 # above sv_vpi_user.h and vhpi_user.h ranges.
_typename_map = OrderedDict()


def get_type_map(models):
    global _next_objectid
    global _typename_map

    if _typename_map:
      return _typename_map

    typenames = set()
    for model in models.values():
        classname = model['name']
        typenames.add(f'uhdm{classname}')

        for key, value in model.allitems():
            if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                card = value.get('card')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'

                typenames.add(f'uhdm{name}')

    for typename in sorted(typenames):
        _typename_map[typename] = _next_objectid
        _next_objectid += 1

    return _typename_map


def generate(models):
    types = '\n'.join([f'  {name} = {id},' for name, id in get_type_map(models).items()])

    with open(config.get_template_filepath('uhdm_types.h'), 'rt') as strm:
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
