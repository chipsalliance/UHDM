import config
import file_utils
import VpiListener_h

def generate(models):
    methods = VpiListener_h.get_methods(models, ' TRACE_ENTER; ', ' TRACE_LEAVE; ')

    with open(config.get_template_filepath('VpiListenerTracer.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENER_TRACER_METHODS>', '\n'.join(methods))
    file_utils.set_content_if_changed(config.get_output_header_filepath('VpiListenerTracer.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
