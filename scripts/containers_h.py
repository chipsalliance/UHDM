import config
import file_utils
from collections import OrderedDict


def generate(models):
    types = set()
    for model in models.values():
        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')

                if (name != 'type') and (type != 'any') and (card == 'any'):
                    types.add(type)

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                card = value.get('card')

                if card == 'any':
                    type = 'any' if key == 'group_ref' else value.get('type')
                    types.add(type)

    containers = []
    for type in sorted(types):
        containers.append(f'  typedef std::vector<{type}*> VectorOf{type};')
        containers.append(f'  typedef std::vector<{type}*>::iterator VectorOf{type}Itr;')
        containers.append('')
    containers = '\n'.join(containers)

    with open(config.get_template_filepath('containers.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CONTAINERS>', containers)
    file_utils.set_content_if_changed(config.get_output_header_filepath('containers.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
