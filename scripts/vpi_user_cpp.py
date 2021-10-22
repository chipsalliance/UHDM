import config
import file_utils


def generate(models):
    vpi_get_value_classes = set()
    vpi_get_delay_classes = set()
    for model in models.values():
        modeltype = model['type']
        if modeltype != 'obj_def':
            continue

        classname = model['name']

        baseclass = classname
        while baseclass:
            for key, value in models[baseclass].allitems():
                if key == 'property' and value.get('card') == '1':
                    type = value.get('type')

                    if type == 'value':
                        vpi_get_value_classes.add(classname)

                    if type == 'delay':
                        vpi_get_delay_classes.add(classname)

            baseclass = models[baseclass]['extends']

    # headers = [ f"#include <uhdm/{model['name']}.h>" for model in models.values() ]
    headers = [ f"#include <uhdm/{name}.h>" for name in sorted(set.union(vpi_get_value_classes, vpi_get_delay_classes)) ]

    vpi_handle_by_name_body = [
        f'  if (object->GetSerializer()->symbolMaker.GetId(name) == static_cast<SymbolFactory::ID>(-1)) {{',
         '    return nullptr;',
         '  }',
         '  const BaseClass *const ref = object->GetByVpiName(std::string_view(name));',
         '  return (ref != nullptr) ? NewVpiHandle(ref) : nullptr;'
    ]

    vpi_handle_body = [
        '  auto [ref, ignored1, ignored2] = object->GetByVpiType(type);',
        '  return (ref != nullptr) ? NewHandle(ref->UhdmType(), ref) : nullptr;'
    ]

    vpi_iterate_body = [
        '  auto [ignored, refType, refVector] = object->GetByVpiType(type);',
        '  return (refVector != nullptr) ? NewHandle(refType, refVector) : nullptr;'
    ]

    vpi_get_body = [
        '  BaseClass::vpi_property_value_t value = obj->GetVpiPropertyValue(property);',
        '  return std::holds_alternative<int64_t>(value) ? std::get<int64_t>(value) : 0;'
    ]

    vpi_get_str_body = [
        '  BaseClass::vpi_property_value_t value = obj->GetVpiPropertyValue(property);',
        '  return std::holds_alternative<const char *>(value) ? const_cast<char *>(std::get<const char *>(value)) : nullptr;'
    ]

    vpi_get_value_body = [
        f'  const s_vpi_value* v = nullptr;',
        f'  switch (handle->type) {{'
    ] + [
        f'    case uhdm{classname}: v = String2VpiValue((({classname}*)obj)->VpiValue()); break;' for classname in sorted(vpi_get_value_classes)
    ] + [
        f'  }}',
        f'  if (v != nullptr) *value_p = *v;'
    ] if vpi_get_value_classes else []

    vpi_get_delay_body = [
        f'  const s_vpi_delay* v = nullptr;',
        f'  switch (handle->type) {{',
    ] + [
        f'    case uhdm{classname}: v = String2VpiDelays((({classname}*)obj)->VpiDelay()); break;' for classname in sorted(vpi_get_delay_classes)
    ] + [
        f'  }}',
        f'  if (v != nullptr) *delay_p = *v;'
    ] if vpi_get_delay_classes else []

    # vpi_user.cpp
    with open(config.get_template_filepath('vpi_user.cpp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<HEADERS>', '\n'.join(headers))
    file_content = file_content.replace('<VPI_HANDLE_BY_NAME_BODY>', '\n'.join(vpi_handle_by_name_body))
    file_content = file_content.replace('<VPI_HANDLE_BODY>', '\n'.join(vpi_handle_body))
    file_content = file_content.replace('<VPI_ITERATE_BODY>', '\n'.join(vpi_iterate_body))
    file_content = file_content.replace('<VPI_SCAN_BODY>', '')
    file_content = file_content.replace('<VPI_GET_BODY>', '\n'.join(vpi_get_body))
    file_content = file_content.replace('<VPI_GET_STR_BODY>', '\n'.join(vpi_get_str_body))
    file_content = file_content.replace('<VPI_GET_VALUE_BODY>', '\n'.join(vpi_get_value_body))
    file_content = file_content.replace('<VPI_GET_DELAY_BODY>', '\n'.join(vpi_get_delay_body))
    file_utils.set_content_if_changed(config.get_output_source_filepath('vpi_user.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
