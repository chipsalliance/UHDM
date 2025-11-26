import config
import file_utils


def _generate_copy_implementation(model):
  classname = model.get('name')
  Classname = config.make_class_name(classname)

  content = [
    f'void Cloner::copy(const {Classname}* source, {Classname}* target) {{',
    f'  copy(static_cast<const {Classname}::basetype_t*>(source), static_cast<{Classname}::basetype_t*>(target));',
  ]

  for key, value in model.allitems():
    if key in ['obj_ref', 'class_ref', 'group_ref', 'property']:
      name = value.get('name')
      card = value.get('card')
      type = value.get('type')
      vpi = value.get('vpi')

      FuncName = config.make_func_name(name, card)
      varName = config.make_var_name(name, card)
      if varName == 'return':
          varName = 'rtn'

      if key == 'property' and type == 'string':
        if vpi not in ['vpiFullName']:
          content.append(f'  target->set{FuncName}(source->get{FuncName}());')
      elif key != 'property':
          suffix = 'Obj' if type == 'identifier' else ''
          content.append(f'  if (auto {varName} = source->get{FuncName}{suffix}()) target->set{FuncName}{suffix}(clone({varName}, target));')

  content.append('}')
  content.append('')

  return content


def generate(models):
  classnames = set([
    model['name'] for model in models.values() if model['type'] == 'obj_def'
  ])

  copy_declarations = []
  copy_implementations = []

  clone_any_declarations = []
  clone_any_implementations = []
  clone_many_declarations = []
  clone_many_implementations = []
  clone_any_case_statements = []

  passthrough_many_declarations = []
  passthrough_many_implementations = []

  for model in models.values():
    type = model['type']
    classname = model['name']
    Classname = config.make_class_name(classname)

    return_type = Classname
    if classname.endswith('_call') and classname in ['func_call', 'method_func_call', 'method_task_call', 'task_call']:
      return_type = 'TFCall'
    elif (classname.endswith('_typespec') or classname in ['type_parameter']) and classname not in ['ref_typespec']:
      return_type = 'Typespec'

    if type == 'obj_def':
      clone_any_declarations.append(f'  virtual {return_type}* clone(const {Classname}* source, Any* parent);')
      clone_any_implementations.append(f'{return_type}* Cloner::clone(const {Classname}* source, Any* parent) {{ return cloneT<{Classname}>(source, parent); }}')
      clone_any_case_statements.append(f'    case UhdmType::{Classname}: target = clone(static_cast<const {Classname} *>(source), parent); break;')

    if type != 'group_def':
      copy_declarations.append(f'  virtual void copy(const {Classname}* source, {Classname}* target);')
      copy_implementations.extend(_generate_copy_implementation(model))
      clone_many_declarations.append(f'  virtual {Classname}Collection* clone(const {Classname}Collection* source, Any* parent);')
      clone_many_implementations.append(f'{Classname}Collection* Cloner::clone(const {Classname}Collection* source, Any* parent) {{ return cloneT<{Classname}>(source, parent); }}')
      passthrough_many_declarations.append(f'  {Classname}Collection* clone(const {Classname}Collection* source, Any* parent) override;')
      passthrough_many_implementations.append(f'{Classname}Collection* PassThroughCloner::clone(const {Classname}Collection* source, Any* parent) {{ return const_cast<{Classname}Collection*>(source); }}')

  # Cloner.h
  with open(config.get_template_filepath('Cloner.h'), 'rt') as strm:
      file_content = strm.read()

  file_content = file_content.replace('//<COPY_DECLARATIONS>', '\n'.join(sorted(copy_declarations)))
  file_content = file_content.replace('//<CLONE_ANY_DECLARATIONS>', '\n'.join(sorted(clone_any_declarations)))
  file_content = file_content.replace('//<CLONE_MANY_DECLARATIONS>', '\n'.join(sorted(clone_many_declarations)))
  file_content = file_content.replace('//<PASSTHROUGHCLONER_MANY_DECLARATIONS>', '\n'.join(sorted(passthrough_many_declarations)))
  file_utils.set_content_if_changed(config.get_output_header_filepath('Cloner.h'), file_content)

  # Cloner.cpp
  with open(config.get_template_filepath('Cloner.cpp'), 'rt') as strm:
      file_content = strm.read()

  file_content = file_content.replace('//<COPY_IMPLEMENTATIONS>', '\n'.join(copy_implementations[:-1]))
  file_content = file_content.replace('//<CLONE_ANY_IMPLEMENTATIONS>', '\n'.join(sorted(clone_any_implementations)))
  file_content = file_content.replace('//<CLONE_MANY_IMPLEMENTATIONS>', '\n'.join(sorted(clone_many_implementations)))
  file_content = file_content.replace('//<CLONE_ANY_CASE_STATEMENTS>', '\n'.join(sorted(clone_any_case_statements)))
  file_content = file_content.replace('//<PASSTHROUGHCLONER_MANY_IMPLEMENTATIONS>', '\n'.join(sorted(passthrough_many_implementations)))
  file_utils.set_content_if_changed(config.get_output_source_filepath('Cloner.cpp'), file_content)

  return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
