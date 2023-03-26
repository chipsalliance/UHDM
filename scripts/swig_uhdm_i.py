import config
import file_utils


def generate(models):
    classnames = [ model['name'] for model in models.values() if model['type'] != 'group_def' ]
    footer = '\n'.join([ f'%include "{classname}.i"' for classname in classnames ])

    with open(config.get_template_filepath('uhdm_swig_type.i'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<INCLUDE_FILES>', footer,1)
    file_utils.set_content_if_changed(config.get_output_header_filepath('uhdm_swig_type.i'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)

