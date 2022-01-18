import config
import file_utils


def get_methods(models, enter, leave):
    methods = []
    for model in models.values():
        if model['type'] != 'group_def':
            classname = model['name']
            Classname_ = classname[:1].upper() + classname[1:]

            methods.append(f'    virtual void enter{Classname_}(const {classname}* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle) {{{enter}}}')
            methods.append(f'    virtual void leave{Classname_}(const {classname}* object, const BaseClass* parent, vpiHandle handle, vpiHandle parentHandle) {{{leave}}}')
            methods.append('')

    return methods


def generate(models):
    methods = get_methods(models, '', '')

    with open(config.get_template_filepath('VpiListener.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENER_METHODS>', '\n'.join(methods))
    file_utils.set_content_if_changed(config.get_output_header_filepath('VpiListener.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
