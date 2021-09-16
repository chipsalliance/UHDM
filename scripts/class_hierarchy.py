import config
import file_utils
from collections import defaultdict
from collections import OrderedDict
from io import StringIO


def generate(models):
    relationships = defaultdict(set)
    for model in models.values():
        if model['type'] not in [ 'group_def' ]:
            thisclass = model['name']
            baseclass = model.get('extends') or 'BaseClass'
            relationships[baseclass].add(thisclass)

    file_content = StringIO()
    stack = [ ('BaseClass', 0) ]
    while stack:
        thisclass, indent = stack.pop()

        file_content.write(' ' * indent)
        file_content.write(thisclass)
        file_content.write('\n')
        stack.extend([ (subclass, indent + 2) for subclass in sorted(relationships.get(thisclass, set()), reverse=True) ])

    file_utils.set_content_if_changed(config.get_output_header_filepath('class_hierarchy.txt'), file_content.getvalue())
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
