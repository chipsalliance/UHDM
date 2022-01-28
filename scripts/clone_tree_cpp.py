import config
import file_utils


def generate(models):
    with open(config.get_template_filepath('clone_tree.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CLONE_IMPLEMENTATIONS>', '')
    file_utils.set_content_if_changed(config.get_output_source_filepath('clone_tree.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
