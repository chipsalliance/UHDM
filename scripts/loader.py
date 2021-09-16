import os
import re
from collections import OrderedDict
from multimap import MutableMultiMap
import pprint

import config


def _load_one_model(filepath):
    lineNo = 0
    top_def = None
    cur_def = None
    with open(filepath, 'r+t') as strm:
        for line in strm:
            lineNo += 1

            # Strip out any comment
            pos = line.find('#')
            if pos >= 0:
                line = line[:pos]

            line = line.strip()
            if not line: # empty line, ignore!
                continue

            m = re.match('^[-]*\s*(?P<key>\w+)\s*:\s*(?P<value>.+)$', line)
            if not m:
              print(f'Failed to parse {filepath}:{lineNo}')
              continue  # TODO(HS): Should this be an error?

            key = m.group('key').strip()
            value = m.group('value').strip()
            if key in [ 'obj_def', 'class_def', 'group_def' ]:
                top_def = MutableMultiMap([
                    ('type', key),
                    ('name', value),
                    ('extends', None),
                    ('subclasses', set()),
                ])
                cur_def = top_def

            elif key in ['name']:
                cur_def['vpiname'] = value

            elif key in ['extends']:
                top_def[key] = value

            elif key in ['property', 'class_ref', 'obj_ref', 'group_ref', 'class']:
                cur_def = MutableMultiMap([
                    ('type', key),
                    ('name', value),
                ])
                top_def.append((key, cur_def))

            elif key in ['type', 'card', 'vpi', 'vpi_obj']:
                cur_def[key] = value

            else:
                print(f'Found unknown key "{key}" at {filepath}:{lineNo}')

    return top_def


def load_models():
    list_filepath = config.get_modellist_filepath()
    base_dirpath = os.path.dirname(list_filepath)

    models = OrderedDict()
    with open(list_filepath, 'r+t') as strm:
        for model_filename in strm:
            pos = model_filename.find('#')
            if pos >= 0:
                model_filename = model_filename[:pos]
            model_filename = model_filename.strip()

            if model_filename:
                model_filepath = os.path.join(base_dirpath, model_filename)
                model = _load_one_model(model_filepath)
                models[model['name']] = model

                # if config.verbosity():
                #     print(f"{'=' * 40} {model_filename} {'=' * 40}")
                #     pprint.pprint(model)
                #     print('')

    # Populate the subclass list
    for name, value in models.items():
        baseclass = value.get('extends')
        while baseclass:
            models[baseclass]['subclasses'].add(name)
            baseclass = models[baseclass].get('extends')

    return models


def _main():
  config.configure()
  load_models()
  return True


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
