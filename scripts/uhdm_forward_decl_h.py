import config
import file_utils


def generate(models):
    classnames = [ model['name'] for model in models.values() if model['type'] != 'group_def' ]
    declarations = '\n'.join([f'class {classname};' for classname in sorted(classnames) if classname != 'BaseClass'])

    with open(config.get_template_filepath('uhdm_forward_decl.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<UHDM_FORWARD_DECL>', declarations)
    file_utils.set_content_if_changed(config.get_output_header_filepath('uhdm_forward_decl.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
