import config
import file_utils


def generate(models):
    class_declarations = []
    factory_declarations = []
    container_factory_declarations = []
    for model in models.values():
        model_type = model['type']
        classname = model['name']

        if model_type != 'group_def' and classname != 'BaseClass':
            class_declarations.append(f'class {classname};')
            container_factory_declarations.append(f'typedef FactoryT<std::vector<{classname} *>> VectorOf{classname}Factory;')

            if model_type != 'class_def':
                factory_declarations.append(f'typedef FactoryT<{classname}> {classname}Factory;')

    with open(config.get_template_filepath('uhdm_forward_decl.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<UHDM_CLASSES_FORWARD_DECL>', '\n'.join(sorted(class_declarations)))
    file_content = file_content.replace('<UHDM_FACTORIES_FORWARD_DECL>', '\n'.join(sorted(factory_declarations)))
    file_content = file_content.replace('<UHDM_CONTAINER_FACTORIES_FORWARD_DECL>', '\n'.join(sorted(container_factory_declarations)))
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
