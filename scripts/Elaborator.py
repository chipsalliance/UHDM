import config
import file_utils


def _generate_module_listeners(models, classname):
  listeners = []
  while classname:
    model = models[classname]

    for key, value in model.allitems():
      if key in ['obj_ref', 'class_ref', 'group_ref']:
        name = value.get('name')
        type = value.get('type')
        card = value.get('card')
        vpi = value.get('vpi')

        FuncName = config.make_func_name(name, card)
        varName = config.make_var_name(name, card)
        if varName == 'return':
          varName = 'rtn'

        if card == '1':
          suffix = 'Obj' if type == 'identifier' else ''
          listeners.append(f'if (auto {varName} = defMod->get{FuncName}{suffix}()) inst->set{FuncName}{suffix}(clone({varName}, inst), true);')

          if type == 'string' and vpi not in ['vpiFullName']:
            listeners.append(f'  target->set{FuncName}(source->get{FuncName}());')

        elif FuncName in ['RefModules', 'GenStmts']:
          pass # No elab

        elif FuncName in ['TaskFuncs']:
          # We want to deep clone existing instance tasks and funcs
          listeners.append(f'if (auto {varName} = inst->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = inst->get{FuncName}(true);')
          listeners.append(f'  for (auto obj : *{varName}) {{')
          listeners.append( '    enterTaskFunc(obj, nullptr);')
          listeners.append( '    auto* tf = clone(obj, inst);')
          listeners.append( '    if (!tf->getName().empty()) {')
          listeners.append( '      ComponentMap& funcMap = std::get<3>(m_instStack.at(m_instStack.size() - 2));')
          listeners.append( '      auto it = funcMap.find(tf->getName());')
          listeners.append( '      if (it != funcMap.end()) funcMap.erase(it);')
          listeners.append( '      funcMap.emplace(tf->getName(), tf);')
          listeners.append( '    }')
          listeners.append( '    leaveTaskFunc(obj, nullptr);')
          listeners.append( '    clone_vec->emplace_back(tf);')
          listeners.append( '  }')
          listeners.append( '}')

        elif FuncName in ['ContAssigns', 'Primitives', 'PrimitiveArrays', 'Ports']:
          # We want to deep clone existing instance cont assign to perform binding
          listeners.append(f'if (auto {varName} = inst->get{FuncName}()) inst->set{FuncName}(clone({varName}, inst), true);')

        elif FuncName in ['GenScopeArrays']:
          # We want to deep clone existing instance cont assign to perform binding
          listeners.append(f'if (auto {varName} = inst->get{FuncName}()) inst->set{FuncName}(clone({varName}, inst), true);')
          # We also want to clone the module cont assign
          listeners.append(f'if (auto {varName} = defMod->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = inst->get{FuncName}(true);')
          listeners.append(f'  for (auto obj : *{varName}) {{')
          listeners.append( '    clone_vec->emplace_back(clone(obj, inst));')
          listeners.append( '  }')
          listeners.append( '}')

        elif FuncName in ['Typespecs']:
          # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
          listeners.append(f'if (auto {varName} = defMod->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = inst->get{FuncName}(true);')
          listeners.append(f'  for (auto obj : *{varName}) {{')
          listeners.append( '    if (uniquifyTypespec()) {')
          listeners.append( '      clone_vec->emplace_back(clone(obj, inst));')
          listeners.append( '    } else {')
          listeners.append( '      clone_vec->emplace_back(obj);')
          listeners.append( '    }')
          listeners.append( '  }')
          listeners.append( '}')

        elif FuncName not in ['Nets', 'Parameters', 'ParamAssigns', 'InterfaceArrays', 'ModuleArrays']:
          # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
          listeners.append(f'if (auto {varName} = defMod->get{FuncName}()) inst->set{FuncName}(clone({varName}, inst), true);')

    classname = models[classname]['extends']

  return listeners


def _generate_class_listeners(models):
  listeners = []

  for model in models.values():
    modeltype = model.get('type')
    if modeltype != 'obj_def':
      continue

    classname = model.get('name')
    if classname != 'class_defn':
      continue

    while classname:
      model = models[classname]

      for key, value in model.allitems():
        if key in ['obj_ref', 'class_ref', 'group_ref']:
          name = value.get('name')
          type = value.get('type')
          card = value.get('card')
          vpi = value.get('vpi')

          FuncName = config.make_func_name(name, card)
          varName = config.make_var_name(name, card)
          if varName == 'return':
            varName = 'rtn'

          if card == '1':
            suffix = 'Obj' if type == 'identifier' else ''
            listeners.append(f'if (auto {varName} = cl->get{FuncName}{suffix}()) cl->set{FuncName}{suffix}(clone({varName}, cl), true);')

            if type == 'string' and vpi not in ['vpiFullName']:
              listeners.append(f'  target->set{FuncName}(source->get{FuncName}(), true);')

          elif FuncName == 'DerivedClasses':
            # Don't deep clone
            listeners.append(f'if (auto {varName} = cl->get{FuncName}()) cl->set{FuncName}(cloneT({varName}), true);')

          elif FuncName in ['InternalScopes', 'InstanceItems']:
            # Don't clone
            pass

          else:
            listeners.append(f'if (auto {varName} = cl->get{FuncName}()) cl->set{FuncName}(clone({varName}, cl), true);')

      classname = models[classname]['extends']

  return listeners


_copy_overrides = set([
  'begin',
  'bit_select',
  'class_defn',
  'class_typespec',
  'clocking_block',
  'design',
  'disable',
  'foreach_stmt',
  'fork_stmt',
  'func_call',
  'function',
  'gen_scope',
  'indexed_part_select',
  'instance',
  'int_typespec',
  'interface_inst',
  'method_func_call',
  'method_task_call',
  'modport',
  'module_inst',
  'named_begin',
  'named_fork',
  'nets',
  'package',
  'part_select',
  'ports',
  'process_stmt',
  'ref_obj',
  'ref_typespec',
  'ref_var',
  'task',
  'task_call',
  'task_func',
  'tchk',
  'typespec',
  'udp',
  'var_select',
  'variables',
])

def _generate_copy_implementations(models):
  declarations = []
  implementations = []

  for model in models.values():
    modeltype = model.get('type')
    if modeltype == 'group_def':
        continue

    classname = model['name']
    Classname = config.make_class_name(classname)

    if classname not in _copy_overrides:
      continue

    if modeltype != 'group_def':
        declarations.append(f'  void copy(const {Classname}* source, {Classname}* target) override;')

    implementations.append(f'void Elaborator::copy(const {Classname}* source, {Classname}* target) {{')
    if modeltype != 'class_def':
        implementations.append(f'  enter{Classname}(target, nullptr);')

    if classname in ['bit_select']:
        implementations.append(f'  ExprEval eval;')
        implementations.append(f'  bool invalidValue = false;')
        implementations.append( '  const Any* const parent = source->getParent();')
        implementations.append( '  if (Any* val = eval.reduceExpr(source->getIndex(), invalidValue, parent, parent, true)) {')
        implementations.append( '    if (!invalidValue) {')
        implementations.append(f'      std::string indexName = eval.prettyPrint(val);')
        implementations.append( '      if (Any* indexVal = bindAny(indexName)) {')
        implementations.append(f'        val = eval.reduceExpr(indexVal, invalidValue, parent, parent, true);')
        implementations.append(f'        if (!invalidValue) indexName = eval.prettyPrint(val);')
        implementations.append( '      }')
        implementations.append( '      const std::string_view name(source->getName());')
        implementations.append( '      std::string fullIndexName(name);')
        implementations.append( '      fullIndexName.append("[").append(indexName).append("]");')
        implementations.append(f'      target->setActual(bindAny(fullIndexName), true);')
        implementations.append(f'      if (!target->getActual()) target->setActual(bindAny(name), true);')
        implementations.append(f'      if (!target->getActual()) target->setActual((Any*) source->getActual(), true);')
        implementations.append( '    }')
        implementations.append( '  }')

    implementations.append(f'  copy(static_cast<const {Classname}::basetype_t*>(source), static_cast<{Classname}::basetype_t*>(target));')
    vpi_name = config.make_vpi_name(classname)

    if 'Select' in vpi_name:
        implementations.append('  if (Net* const nn = any_cast<Net>(bindNet(source->getName()))) target->setFullName(nn->getFullName());')

    for key, value in model.allitems():
        if key not in ['obj_ref', 'class_ref', 'group_ref', 'property']:
          continue

        name = value.get('name')
        type = value.get('type')
        card = value.get('card')
        vpi = value.get('vpi')

        FuncName = config.make_func_name(name, card)
        varName = config.make_var_name(name, card)
        if varName == 'return':
          varName = 'rtn'

        # Unary relations
        if card == '1':
            if key == 'property':
                if type == 'string' and vpi not in ['vpiFullName']:
                    implementations.append(f'  target->set{FuncName}(source->get{FuncName}());')

            elif (Classname in ['RefObj']) and (FuncName == 'Actual'):
                implementations.append(f'  if (!target->get{FuncName}()) target->set{FuncName}(bindAny(source->getName()), true);')
                implementations.append(f'  if (!target->get{FuncName}()) target->set{FuncName}((Any*) source->get{FuncName}(), true);')

            elif (Classname in ['RefTypespec']) and (FuncName == 'Actual'):
                implementations.append( '  if (uniquifyTypespec()) {')
                implementations.append(f'    if (auto {varName} = source->get{FuncName}()) target->set{FuncName}(clone({varName}, target), true);')
                implementations.append( '  } else {')
                implementations.append(f'    if (auto {varName} = source->get{FuncName}()) target->set{FuncName}((Typespec*) {varName}, true);')
                implementations.append( '  }')

            elif (Classname == 'Udp') and (FuncName == 'UdpDefn'):
                implementations.append(f'  if (!target->get{FuncName}()) target->set{FuncName}((UdpDefn*) bindAny(source->getDefName()), true);')
                implementations.append(f'  if (!target->get{FuncName}()) target->set{FuncName}((UdpDefn*) source->get{FuncName}(), true);')

            elif FuncName in ['Task', 'Function']:
                prefix = 'nullptr'
                if 'method_' in classname:
                    implementations.append(f'  const RefObj* const ref = any_cast<RefObj>(target->getPrefix());')
                    implementations.append( '  const class_var* prefix = nullptr;')
                    implementations.append( '  if (ref) prefix = any_cast<const class_var*>(ref->getActual());')
                    prefix = 'prefix'
                implementations.append(f'  scheduleTaskFuncBinding(target, {prefix});')

            elif (Classname == 'Disable') and (FuncName == 'Expr'):
                implementations.append(f'  if (auto {varName} = source->get{FuncName}()) target->set{FuncName}((Expr*) {varName}, true);')

            elif (Classname == 'Ports') and (FuncName == 'HighConn'):
                implementations.append(f'  if (auto {varName} = source->get{FuncName}()) {{')
                implementations.append( '    ignoreLastInstance(true); ')
                implementations.append(f'    target->set{FuncName}(clone({varName}, target), true);')
                implementations.append( '    ignoreLastInstance(false);')
                implementations.append( '  }')

            elif (Classname == 'IntTypespec') and (FuncName == 'CastToExpr'):
                implementations.append(f'  if (auto {varName} = source->get{FuncName}()) target->set{FuncName}((Variable*) {varName}, true);')

            elif (Classname == 'ClassTypespec') and (FuncName == 'ClassDefn'):
                implementations.append(f'  if (auto {varName} = source->get{FuncName}()) target->set{FuncName}((ClassDefn*) {varName}, true);')

            elif FuncName == 'Instance':
                implementations.append(f'  if (auto {varName} = source->get{FuncName}()) target->set{FuncName}((Instance*) {varName}, true);')
                implementations.append( '  if (Instance* inst = target->getParent<Instance>()) target->setInstance(inst, true);')

            elif FuncName in ['Module', 'Interface']:
                implementations.append(f'  if (auto {varName} = source->get{FuncName}()) target->set{FuncName}(({FuncName}*) {varName}, true);')

            else:
                suffix = 'Obj' if type == 'identifier' else ''
                implementations.append(f'  if (auto {varName} = source->get{FuncName}{suffix}()) target->set{FuncName}{suffix}(clone({varName}, target), true);')

        elif (Classname == 'Module') and (FuncName == 'RefModules'):
            pass # No cloning

        elif (FuncName == 'Typespecs'):
            # Don't deep clone
            implementations.append(f'  if (auto {varName} = source->get{FuncName}()) target->set{FuncName}(cloneT({varName}), true);')

        else:
            # N-ary relations
            implementations.append(f'  if (auto {varName} = source->get{FuncName}()) target->set{FuncName}(clone({varName}, target), true);')

    if modeltype != 'class_def':
        implementations.append(f'  leave{Classname}(target, nullptr);')

    implementations.append('}')
    implementations.append('')

  return declarations, implementations[:-1]


def generate(models):
  module_listeners = _generate_module_listeners(models, 'module')
  interface_listeners = _generate_module_listeners(models, 'interface')
  class_listeners = _generate_class_listeners(models)
  copy_declarations, copy_implementations = _generate_copy_implementations(models)

  with open(config.get_template_filepath('Elaborator.h'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<COPY_DECLARATIONS>', '\n'.join(sorted(copy_declarations)))
  file_utils.set_content_if_changed(config.get_output_header_filepath('Elaborator.h'), file_content)

  with open(config.get_template_filepath('Elaborator.cpp'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<MODULE_ELABORATOR_LISTENER>', (' ' * 6) + ('\n' + (' ' * 6)).join(module_listeners))
  file_content = file_content.replace('//<INTERFACE_ELABORATOR_LISTENER>', (' ' * 10) + ('\n' + (' ' * 10)).join(interface_listeners))
  file_content = file_content.replace('//<CLASS_ELABORATOR_LISTENER>', (' ' * 2) + ('\n' + (' ' * 2)).join(class_listeners))
  file_content = file_content.replace('//<COPY_IMPLEMENTATIONS>', '\n'.join(copy_implementations))
  file_utils.set_content_if_changed(config.get_output_source_filepath('Elaborator.cpp'), file_content)

  return True


def _main():
  import loader

  config.configure()

  models = loader.load_models()
  return generate(models)


if __name__ == '__main__':
  import sys
  sys.exit(0 if _main() else 1)
