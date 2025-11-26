import config
import file_utils


def _get_listeners(classname, vpi, type, card):
  listeners = []

  if card == '1':
    # upward vpiModule, vpiInterface relation (when card == 1, pointing to the parent object) creates loops in visitors
    if vpi in ['vpiParent', 'vpiInstance', 'vpiModule', 'vpiInterface', 'vpiUse', 'vpiProgram', 'vpiClassDefn', 'vpiPackage', 'vpiUdp']:
        return listeners

    if 'func_call' in classname and vpi == 'vpiFunction':
      # Prevent stepping inside functions while processing calls (func_call, method_func_call) to them
      return listeners

    if 'task_call' in classname and vpi == 'vpiTask':
      # Prevent stepping inside tasks while processing calls (task_call, method_task_call) to them
      return listeners

    listeners.append(f'  if (vpiHandle itr = vpi_handle({vpi}, handle)) {{')
    listeners.append(f'    listenAny(itr);')
    listeners.append( '    vpi_free_object(itr);')
    listeners.append( '  }')

  else:
    if 'vpiAll' in vpi:
      listeners.append(f'  uhdmAllIterator = true;')

    listeners.append(f'  if (vpiHandle itr = vpi_iterate({vpi}, handle)) {{')
    listeners.append( '    while (vpiHandle obj = vpi_scan(itr)) {')
    listeners.append(f'      listenAny(obj);')
    listeners.append( '      vpi_free_object(obj);')
    listeners.append( '    }')
    listeners.append( '    vpi_free_object(itr);')
    listeners.append( '  }')

    if 'vpiAll' in vpi:
      listeners.append(f'  uhdmAllIterator = false;')
      listeners.append(f'  m_visited.clear();')
      listeners.append(f'  m_visited.emplace((const Any*)((const uhdm_handle*)handle)->object);')

  return listeners


def _get_trace_listeners(models):
    methods = []
    for model in models.values():
        if model['type'] not in ['class_def', 'group_def']:
            classname = model['name']
            ClassName = config.make_class_name(classname)

            methods.append(f'    void enter{ClassName}(const {ClassName}* object, vpiHandle handle) final {{ TRACE_ENTER; }}')
            methods.append(f'    void leave{ClassName}(const {ClassName}* object, vpiHandle handle) final {{ TRACE_LEAVE; }}')
            methods.append('')

    return methods


def generate(models):
  private_declarations = []
  private_implementations = []
  public_implementations = []
  classnames = set()

  for model in models.values():
    modeltype = model['type']
    if modeltype == 'group_def':
      continue

    classname = model['name']
    ClassName = config.make_class_name(classname)
    baseclass = model.get('extends') or 'BaseClass'

    if model.get('subclasses') or modeltype == 'obj_def':
      private_declarations.append(f'  void listen{ClassName}_(vpiHandle handle);')
      private_implementations.append(f'void VpiListener::listen{ClassName}_(vpiHandle handle) {{')
      if baseclass:
        BaseClass = config.make_class_name(baseclass)
        private_implementations.append(f'  listen{BaseClass}_(handle);')

      for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
          vpi  = value.get('vpi')
          type = value.get('type')
          card = value.get('card')

          if key == 'group_ref':
            type = 'any'

          private_implementations.extend(_get_listeners(ClassName, vpi, type, card))

      private_implementations.append( '}')
      private_implementations.append( '')

    if modeltype != 'class_def':
      classnames.add(ClassName)

      public_implementations.append(f'void VpiListener::listen{ClassName}(vpiHandle handle) {{')
      public_implementations.append(f'  const {ClassName}* object = (const {ClassName}*) ((const uhdm_handle*)handle)->object;')
      public_implementations.append(f'  enter{ClassName}(object, handle);')
      public_implementations.append( '  if (m_visited.insert(object).second) {')
      public_implementations.append( '    m_callstack.emplace_back(object);')
      public_implementations.append(f'    listen{ClassName}_(handle);')
      public_implementations.append( '    m_callstack.pop_back();')
      public_implementations.append( '  }')
      public_implementations.append(f'  leave{ClassName}(object, handle);')
      public_implementations.append(f'}}')
      public_implementations.append( '')

  any_implementation = []
  enter_leave_declarations = []
  public_declarations = []
  for classname in sorted(classnames):
    any_implementation.append(f'    case UhdmType::{classname}: listen{classname}(handle); break;')

    enter_leave_declarations.append(f'  virtual void enter{classname}(const {classname}* object, vpiHandle handle) {{}}')
    enter_leave_declarations.append(f'  virtual void leave{classname}(const {classname}* object, vpiHandle handle) {{}}')
    enter_leave_declarations.append( '')

    public_declarations.append(f'  void listen{classname}(vpiHandle handle);')

  trace_listeners = _get_trace_listeners(models)

  # VpiListener.h
  with open(config.get_template_filepath('VpiListener.h'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<VPI_PUBLIC_LISTEN_DECLARATIONS>', '\n'.join(public_declarations))
  file_content = file_content.replace('//<VPI_PRIVATE_LISTEN_DECLARATIONS>', '\n'.join(private_declarations))
  file_content = file_content.replace('//<VPI_ENTER_LEAVE_DECLARATIONS>', '\n'.join(enter_leave_declarations))
  file_content = file_content.replace('//<VPI_LISTENER_TRACER_METHODS>', '\n'.join(trace_listeners))
  file_utils.set_content_if_changed(config.get_output_header_filepath('VpiListener.h'), file_content)

  # VpiListener.cpp
  with open(config.get_template_filepath('VpiListener.cpp'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<VPI_PRIVATE_LISTEN_IMPLEMENTATIONS>', '\n'.join(private_implementations))
  file_content = file_content.replace('//<VPI_PUBLIC_LISTEN_IMPLEMENTATIONS>', '\n'.join(public_implementations))
  file_content = file_content.replace('//<VPI_LISTENANY_IMPLEMENTATION>', '\n'.join(any_implementation))
  file_utils.set_content_if_changed(config.get_output_source_filepath('VpiListener.cpp'), file_content)

  return True


def _main():
  import loader

  config.configure()

  models = loader.load_models()
  return generate(models)


if __name__ == '__main__':
  import sys
  sys.exit(0 if _main() else 1)
