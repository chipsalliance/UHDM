import os

import config
import file_utils

_collector_class_types = {
    'Scope': set([
        ( 'assert_stmt', 'InstanceItems' ),
        ( 'assume', 'InstanceItems' ),
        ( 'checker_inst', 'InstanceItems' ),
        ( 'class_defn', 'InstanceItems' ),
        ( 'concurrent_assertions', 'ConcurrentAssertions' ),
        ( 'cover', 'InstanceItems' ),
        ( 'immediate_assert', 'InstanceItems' ),
        ( 'immediate_assume', 'InstanceItems' ),
        ( 'immediate_cover', 'InstanceItems' ),
        ( 'let_decl', 'LetDecls' ),
        ( 'named_event', 'InstanceItems' ),
        ( 'named_event', 'NamedEvents' ),
        ( 'named_event_array', 'InstanceItems' ),
        ( 'named_event_array', 'NamedEventArrays' ),
        ( 'net', 'InstanceItems' ),
        ( 'param_assign', 'ParamAssigns' ),
        ( 'parameter', 'Parameters' ),
        ( 'program', 'InstanceItems' ),
        ( 'program_array', 'InstanceItems' ),
        ( 'property_decl', 'PropertyDecls' ),
        ( 'property_inst', 'InstanceItems' ),
        ( 'restrict', 'InstanceItems' ),
        ( 'scope', 'InternalScopes' ),
        ( 'sequence_decl', 'SequenceDecls' ),
        ( 'sequence_inst', 'InstanceItems' ),
        ( 'spec_param', 'InstanceItems' ),
        ( 'task_func', 'InstanceItems' ),
        ( 'type_parameter', 'Parameters' ),
        ( 'typespec', 'InstanceItems' ),
        ( 'typespec', 'Typespecs' ),
        ( 'variable', 'InstanceItems' ),
        ( 'variable', 'Variables' ),
    ]),
    'UdpDefn': set([
        ( 'io_decl', 'IODecls' ),
        ( 'table_entry', 'TableEntries' ),
    ]),
    'Design': set([
        ( 'class_defn', 'AllClasses' ),
        ( 'interface', 'AllInterfaces' ),
        ( 'let_decl', 'LetDecls' ),
        ( 'module', 'AllModules' ),
        ( 'package', 'AllPackages' ),
        ( 'param_assign', 'ParamAssigns' ),
        ( 'parameter', 'Parameters' ),
        ( 'program', 'AllPrograms' ),
        ( 'task_func', 'TaskFuncs' ),
        ( 'typespec', 'Typespecs' ),
        ( 'udp_defn', 'AllUdps' ),
        ( 'variable', 'Variables' ),
    ]),
    'ClassDefn': set([
        ( 'class_defn', 'DerivedClasses' ),
        ( 'class_typespec', 'ClassTypespecs' ),
        ( 'constraint', 'Constraints' ),
        ( 'task_func', 'Methods' ),
        ( 'task_func_decl', 'TaskFuncDecls' ),
    ]),
    'ClassObj': set([
        ( 'constraint', 'Constraints' ),
        ( 'expr', 'Messages' ),
        ( 'task_func', 'TaskFuncs' ),
        ( 'thread_obj', 'Threads' ),
    ]),
    'Instance': set([
        ( 'class_defn', 'ClassDefns' ),
        ( 'net', 'Nets' ),
        ( 'program', 'Programs' ),
        ( 'program_array', 'ProgramArrays' ),
        ( 'spec_param', 'SpecParams' ),
        ( 'task_func', 'TaskFuncs' ),
        ( 'task_func_decl', 'TaskFuncDecls' ),
    ]),
    'CheckerDecl': set([
        ( 'checker_port', 'Ports' ),
        ( 'cont_assign', 'ContAssigns' ),
        ( 'process_stmt', 'Processes' ),
    ]),
    'CheckerInst': set([
        ( 'checker_inst_port', 'Ports' )
    ]),
    'GenScope': set([
        ( 'net', 'Nets' )
    ]),
    'Interface': set([
        ( 'clocking_block', 'ClockingBlocks' ),
        ( 'cont_assign', 'ContAssigns' ),
        ( 'gen_scope_array', 'GenScopeArrays' ),
        ( 'gen_stmt', 'GenStmts' ),
        ( 'interface_array', 'InterfaceArrays' ),
        ( 'interface', 'Interfaces' ),
        ( 'interface_tf_decl', 'InterfaceTFDecls' ),
        ( 'mod_path', 'ModPaths' ),
        ( 'modport', 'Modports' ),
        ( 'port', 'Ports' ),
        ( 'process_stmt', 'Processes' ),
        ( 'tf_call', 'SysTaskCalls' ),
    ]),
    'Modport': set([
      ( 'io_decl', 'IODecls' ),
    ]),
    'Module': set([
        ( 'alias_stmt', 'Aliases' ),
        ( 'clocking_block', 'ClockingBlocks' ),
        ( 'cont_assign', 'ContAssigns' ),
        ( 'def_param', 'DefParams' ),
        ( 'gen_scope_array', 'GenScopeArrays' ),
        ( 'gen_stmt', 'GenStmts' ),
        ( 'interface_array', 'InterfaceArrays' ),
        ( 'interface', 'Interfaces' ),
        ( 'io_decl', 'IODecls' ),
        ( 'mod_path', 'ModPaths' ),
        ( 'module_array', 'ModuleArrays' ),
        ( 'module', 'Modules' ),
        ( 'port', 'Ports' ),
        ( 'primitive', 'Primitives' ),
        ( 'primitive_array', 'PrimitiveArrays' ),
        ( 'process_stmt', 'Processes' ),
        ( 'ref_module', 'RefModules' ),
        ( 'tchk', 'Tchks' ),
        ( 'tf_call', 'SysTaskCalls' ),
    ]),
    'MulticlockSequenceExpr': set([
        ( 'clocked_seq', 'ClockedSeqs' ),
    ]),
    'Program': set([
        ( 'clocking_block', 'ClockingBlocks' ),
        ( 'cont_assign', 'ContAssigns' ),
        ( 'gen_scope_array', 'GenScopeArrays' ),
        ( 'interface_array', 'InterfaceArrays' ),
        ( 'interface', 'Interfaces' ),
        ( 'port', 'Ports' ),
        ( 'process_stmt', 'Processes' ),
    ]),
}

_special_parenting_types = set([
    'Alias',
    'Assertion',
    'CheckerInst',
    'CheckerInstPort',
    'ClassDefn',
    'ConcurrentAssertions',
    'ContAssign',
    'DefParam',
    'GenScopeArray',
    'GenStmt',
    'ImmediateAssert',
    'ImmediateAssume',
    'ImmediateCover',
    'InterfaceArray',
    'InterfaceInst',
    'InterfaceTFDecl',
    'IODecl',
    'LetDecl',
    'Modport',
    'Module_array',
    'Module',
    'NamedEvent',
    'NamedEventArray',
    'Nets',
    'ParamAssign',
    'Parameter',
    'Primitive',
    'PrimitiveArray',
    'Process',
    'Program',
    'ProgramArray',
    'PropertyDecl',
    'PropertyInst',
    'RefModule',
    'Scope',
    'SequenceDecl',
    'SequenceInst',
    'SpecParam',
    'TableEntry',
    'TaskFunc',
    'Tchk',
    'Thread',
    'Typespec',
    'Variable',
])


def _get_group_headers(type, real_type):
    return [real_type] if type == 'any' and real_type != 'any' else []


def _get_declarations(name, type, vpi, card, real_type=''):
    content = []
    if type in ['string', 'value', 'delay']:
        type = 'std::string'
    if vpi == 'uhdmType':
        type = 'UhdmType'

    final = ' final' if vpi in ['uhdmType', 'vpiType', 'vpiDefName'] else ''
    check = f'if (!{real_type}GroupCompliant(data)) return false;\n    ' if type == 'any' and real_type != 'any' else ''

    varName = config.make_var_name(name, card)
    FuncName = config.make_func_name(name, card)

    if card == '1':
        if type == 'std::string':
            content.append(f'  std::string_view get{FuncName}() const{final};')
            content.append(f'  bool set{FuncName}(std::string_view data);')

        elif type in ['int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'bool']:
            content.append(f'  {type} get{FuncName}() const{final} {{ return m_{varName}; }}')
            content.append(f'  bool set{FuncName}({type} data) {{\n    m_{varName} = data;\n    return true;\n  }}')

        else:
            Type = config.make_class_name(type)
            suffix = ''
            if vpi in ['vpiName']:
                suffix = 'Obj'
                content.append('  std::string_view getName() const final;')
                content.append('  bool setName(std::string_view name);')

            content.append(f'  {Type}* get{FuncName}{suffix}(){final} {{ return m_{varName}; }}')
            content.append(f'  const {Type}* get{FuncName}{suffix}() const{final} {{ return m_{varName}; }}')
            content.append(f'  template <typename T> T* get{FuncName}{suffix}() {{ return any_cast<T>(m_{varName}); }}')
            content.append(f'  template <typename T> const T* get{FuncName}{suffix}() const {{ return any_cast<T>(m_{varName}); }}')
            content.append(f'  bool set{FuncName}{suffix}({Type}* data) {{\n    {check}m_{varName} = data;\n    return true;\n  }}')

            # if type == 'ref_typespec':
            #     content.append(f'  template <typename T> T* get{FuncName}Actual() {{ return (m_{varName} != nullptr) ? m_{varName}->template getActual<T>() : nullptr; }}')
            #     content.append(f'  template <typename T> const T* get{FuncName}Actual() const {{ return (m_{varName} != nullptr) ? m_{varName}->template getActual<T>() : nullptr; }}')
            #     content.append(f'  RefTypespec* set{FuncName}Actual(Typespec* data);')

    elif card == 'any':
        TypeName = config.make_class_name(type)
        content.append(f'  {TypeName}Collection* get{FuncName}() const {{ return m_{varName}; }}')
        content.append(f'  template<typename T> {TypeName}Collection* get{FuncName}(T) = delete;')
        content.append(f'  {TypeName}Collection* get{FuncName}(bool createIfNull);')
        content.append(f'  bool set{FuncName}({TypeName}Collection* data) {{\n    {check}if ((m_{varName} == nullptr) || (data == nullptr)) {{\n      m_{varName} = data;\n      return true;\n    }}\n    return false;\n  }}')

    return '\n'.join(content)


def _get_implementations(classname, name, type, vpi, card):
    content = []

    ClassName = config.make_class_name(classname)
    varName = config.make_var_name(name, card)
    FuncName = config.make_func_name(name, card)
    TypeName = config.make_class_name(type)

    if vpi in ['vpiName'] and type == 'identifier':
        content.append(f'std::string_view {ClassName}::getName() const {{')
        content.append(f'  return (m_{varName} != nullptr) ? m_{varName}->getName() : kEmpty;')
        content.append( '}')
        content.append( '')
        content.append(f'bool {ClassName}::setName(std::string_view name) {{')
        content.append( '  if (m_name == nullptr) {')
        content.append( '    m_name = m_serializer->make<Identifier>();')
        content.append( '    m_name->setParent(this);')
        content.append( '  }')
        content.append( '  m_name->setName(name);')
        content.append( '  return true;')
        content.append( '}')

    if card == '1':
        if type in ['string', 'value', 'delay']:
            type = 'std::string'

        if type == 'std::string':
            if vpi == 'uhdmType':
                type = 'UhdmType'

            if vpi == 'vpiFullName':
                content.append(f'std::string_view {ClassName}::get{FuncName}() const {{')
                content.append(f'  if (!m_{varName}) {{')
                content.append( '    const std::string fullName = computeFullName();')
                content.append( '    if (!fullName.empty()) {')
                content.append(f'      const_cast<{ClassName}*>(this)->setFullName(fullName);')
                content.append( '    }')
                content.append( '  }')
                content.append(f'  return m_{varName} ? m_serializer->getSymbol(m_{varName}) : kEmpty;')
                content.append( '}')
            else:
                content.append(f'std::string_view {ClassName}::get{FuncName}() const {{')
                content.append(f'  return m_{varName} ? m_serializer->getSymbol(m_{varName}) : kEmpty;')
                content.append(f'}}')

            content.append('')
            content.append(f'bool {ClassName}::set{FuncName}(std::string_view data) {{')
            content.append(f'  m_{varName} = m_serializer->makeSymbol(data);')
            content.append(f'  return true;')
            content.append(f'}}')

        # elif type == 'ref_typespec':
        #     content.append(f'RefTypespec* {ClassName}::set{FuncName}Actual(Typespec* data) {{')
        #     content.append(f'  if (m_{varName} == nullptr) {{')
        #     content.append(f'    m_{varName} = m_serializer->make<RefTypespec>();')
        #     content.append(f'    m_{varName}->setParent(this);')
        #     content.append(f'  }}')
        #     content.append(f'  m_{varName}->setActual(data);')
        #     content.append(f'  return m_{varName};')
        #     content.append(f'}}')

    elif card == 'any':
        content.append(f'{TypeName}Collection* {ClassName}::get{FuncName}(bool createIfNull) {{')
        content.append(f'  if (m_{varName} == nullptr) m_{varName} = m_serializer->makeCollection<{TypeName}>();')
        content.append(f'  return m_{varName};')
        content.append( '}')

    if content:
      content.append('')

    return content


def _get_data_member(name, type, vpi, card):
    content = []

    if type in ['string', 'value', 'delay']:
        type = 'std::string'

    varName = config.make_var_name(name, card)
    if vpi in ['vpiName'] and type == 'identifier':
        content.append(f'  Identifier* m_{varName} = nullptr;')

    elif card == '1':
        pointer = ''
        default_assignment = 'false' if type == 'bool' else '0'
        if type in ['int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'bool']:
            content.append(f'  {type} m_{varName} = {default_assignment};')

        elif type in ['std::string', 'symbol']:
            content.append(f'  SymbolId m_{varName} = BadSymbolId;')

        else:
            type = config.make_class_name(type)
            content.append(f'  {type}* m_{varName} = nullptr;')

    elif card == 'any':
        type = config.make_class_name(type)
        content.append(f'  {type}Collection* m_{varName} = nullptr;')

    return content


def _get_deepClone_implementation(model, models):
    classname = model.get('name')
    ClassName = config.make_class_name(classname)
    modeltype = model.get('type')

    includes = set()
    content = []
    content.append(f'void {ClassName}::deepCopy({ClassName}* clone, BaseClass* parent, CloneContext* context) const {{')
    content.append( '  [[maybe_unused]] ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext>(context);')
    if modeltype != 'class_def':
        content.append(f'  elaboratorContext->m_elaborator.enter{ClassName}(clone, nullptr);')

    if classname in ['bit_select']:
        includes.add('ExprEval')
        content.append(f'  ExprEval eval;')
        content.append(f'  bool invalidValue = false;')
        content.append( '  if (Any* val = eval.reduceExpr(getIndex(), invalidValue, parent, parent, true)) {')
        content.append( '    if (!invalidValue) {')
        content.append(f'      std::string indexName = eval.prettyPrint(val);')
        content.append( '      if (Any *const indexVal = elaboratorContext->m_elaborator.bindAny(indexName)) {')
        content.append(f'        val = eval.reduceExpr(indexVal, invalidValue, parent, parent, true);')
        content.append(f'        if (!invalidValue) indexName = eval.prettyPrint(val);')
        content.append( '      }')
        content.append( '      const std::string_view name = getName();')
        content.append( '      std::string fullIndexName(name);')
        content.append( '      fullIndexName.append("[").append(indexName).append("]");')
        content.append(f'      clone->setActual(elaboratorContext->m_elaborator.bindAny(fullIndexName));')
        content.append(f'      if (!clone->getActual()) clone->setActual(elaboratorContext->m_elaborator.bindAny(name));')
        content.append(f'      if (!clone->getActual()) clone->setActual((Any*) getActual());')
        content.append( '    }')
        content.append( '  }')

    content.append(f'  basetype_t::deepCopy(clone, parent, context);')
    vpi_name = config.make_vpi_name(classname)

    if 'Select' in vpi_name:
        includes.add('net')
        content.append('  if (Any *const n = elaboratorContext->m_elaborator.bindNet(getName())) {')
        content.append('    if (Net *const nn = n->Cast<Net>())')
        content.append('      clone->setFullName(nn->getFullName());')
        content.append('  }')

    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            type = value.get('type')
            card = value.get('card')

            TypeName = config.make_class_name('any' if key == 'group_ref' else type)
            FuncName = config.make_func_name(name, card)
            varName = config.make_var_name(name, card)

            # Unary relations
            if card == '1':
                if (ClassName in ['RefObj', 'RefVar']) and (varName == 'actual'):
                    includes.add('ElaboratorListener')
                    content.append(f'  if (!clone->m_{varName}) clone->m_{varName} = elaboratorContext->m_elaborator.bindAny(getName());')
                    content.append(f'  if (!clone->m_{varName}) clone->m_{varName} = (Any*) m_{varName};')

                elif (ClassName in ['RefTypespec']) and (varName == 'actual'):
                    includes.add('ElaboratorListener')
                    includes.add('typespec')
                    content.append( '  if (elaboratorContext->m_elaborator.uniquifyTypespec()) {')
                    content.append(f'    if (auto obj = m_{varName}) clone->m_{varName} = obj->deepClone(clone, context);')
                    content.append( '  } else {')
                    content.append(f'    if (auto obj = m_{varName}) clone->m_{varName} = (Typespec*) obj;')
                    content.append( '  }')

                elif (ClassName == 'Udp') and (varName == 'udpDefn'):
                    includes.add('ElaboratorListener')
                    content.append(f'  if (!clone->m_{varName}) clone->m_{varName} = (UdpDefn*) elaboratorContext->m_elaborator.bindAny(getDefName());')
                    content.append(f'  if (!clone->m_{varName}) clone->m_{varName} = (UdpDefn*) m_{varName};')

                elif name in ['Task', 'Function']:
                    prefix = 'nullptr'
                    includes.add(name.lower())
                    if ClassName.startswith('Method'):
                        includes.add('ref_obj')
                        includes.add('class_var')
                        includes.add(name.lower())
                        content.append( '  const ClassVar* prefix = nullptr;')
                        content.append(f'  if (const RefObj *const ref = clone->getPrefix<RefObj>()) {{')
                        content.append( '    prefix = ref->getActual<ClassVar>();')
                        content.append( '  }')
                        prefix = 'prefix'
                    content.append(f'  elaboratorContext->m_elaborator.scheduleTaskFuncBinding(clone, {prefix});')

                elif (ClassName == 'Disable') and (varName == 'vpiExpr'):
                    includes.add('expr')
                    content.append(f'  clone->m_{varName} = m_{varName};')

                elif (ClassName == 'ports') and (varName == 'highConn'):
                    content.append(f'  if (m_{varName} != nullptr) {{')
                    content.append( '    elaboratorContext->m_elaborator.ignoreLastInstance(true); ')
                    content.append(f'    clone->m_{varName} = m_{varName}->deepClone(clone, context);')
                    content.append( '    elaboratorContext->m_elaborator.ignoreLastInstance(false);')
                    content.append( '  }')

                elif (ClassName == 'IntTypespec') and (varName == 'castToExpr'):
                    includes.add('variable')
                    content.append(f'  clone->m_{varName} = m_{varName};')

                elif (ClassName == 'Function') and (varName == 'return'):
                    includes.add('variable')
                    content.append(f'  clone->m_{varName} = m_{varName};')

                elif (ClassName == 'ClassTypespec') and (varName == 'Class_defn'):
                    includes.add('class_defn')
                    content.append(f'  clone->m_{varName} = m_{varName};')

                elif varName == 'instance':
                    includes.add('instance')
                    content.append(f'  clone->m_{varName} = m_{varName};')
                    content.append( '  if (Instance *const inst = parent->Cast<Instance>())')
                    content.append( '    clone->setInstance(inst);')

                elif varName == 'module':
                    includes.add('module')
                    content.append(f'  clone->m_{varName} = m_{varName};')

                elif varName == 'interface':
                    includes.add('interface')
                    content.append(f'  clone->m_{varName} = m_{varName};')

                else:
                    content.append(f'  if (m_{varName} != nullptr) clone->m_{varName} = m_{varName}->deepClone(clone, context);')

            elif (ClassName == 'Module') and (varName == 'refModules'):
                pass # No cloning

            elif varName == 'typespecs':
                # Don't deep clone
                content.append(f'  if (m_{varName} != nullptr) {{')
                content.append(f'    auto clone_vec = context->m_serializer->makeCollection<{TypeName}>();')
                content.append(f'    clone->set{FuncName}(clone_vec);')
                content.append(f'    clone_vec->insert(clone_vec->cend(), m_{varName}->cbegin(), m_{varName}->cend());')
                content.append( '  }')

            elif (ClassName == 'ClassDefn') and (varName == 'derivedClasses'):
                # Don't deep clone
                content.append(f'  if (m_{varName} != nullptr) {{')
                content.append(f'    auto clone_vec = context->m_serializer->makeCollection<{TypeName}>();')
                content.append(f'    clone->set{FuncName}(clone_vec);')
                content.append(f'    clone_vec->insert(clone_vec->cend(), m_{varName}->cbegin(), m_{varName}->cend());')
                content.append( '  }')

            else:
                # N-ary relations
                content.append(f'  if (auto vec = m_{varName}) {{')
                content.append(f'    auto clone_vec = context->m_serializer->makeCollection<{TypeName}>();')
                content.append(f'    clone->set{FuncName}(clone_vec);')
                content.append(f'    for (auto obj : *vec) {{')
                content.append( '      clone_vec->emplace_back(obj->deepClone(clone, context));')
                content.append( '    }')
                content.append( '  }')

    if modeltype != 'class_def':
        content.append(f'  elaboratorContext->m_elaborator.leave{ClassName}(clone, nullptr);')

    content.append('}')
    content.append('')

    if ClassName.endswith('Call') or ClassName in [ 'Function', 'Task', 'Constant', 'TaggedPattern', 'GenScopeArray', 'HierPath', 'ContAssign' ]:
        return content, includes  # Use hardcoded implementations of deepClone

    if modeltype == 'obj_def':
        # deepClone() not implemented for class_def; just declare to narrow the covariant return type.
        content.append(f'{ClassName}* {ClassName}::deepClone(BaseClass* parent, CloneContext* context) const {{')

        if ClassName in ['Begin', 'NamedBegin', 'Fork', 'NamedFork']:
            content.append( '  ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext>(context);')
            content.append(f'  elaboratorContext->m_elaborator.enter{ClassName}(this, nullptr);')

        if 'Net' in vpi_name:
            includes.add('ElaboratorListener')
            content.append( '  ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext>(context);')
            content.append(f'  {ClassName}* clone = any_cast<{ClassName}>(elaboratorContext->m_elaborator.bindNet(getName()));')
            content.append(f'  if (clone != nullptr) return clone;')
            content.append(f'  clone = context->m_serializer->make<{ClassName}>();')

        elif 'Parameter' in vpi_name:
            includes.add('ElaboratorListener')
            content.append( '  ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext>(context);')
            content.append(f'  {ClassName}* clone = any_cast<{ClassName}>(elaboratorContext->m_elaborator.bindParam(getName()));')
            content.append(f'  if (clone == nullptr) clone = context->m_serializer->make<{ClassName}>();')

        else:
            content.append(f'  {ClassName}* const clone = context->m_serializer->make<{ClassName}>();')

        content.append('  const uint32_t id = clone->getUhdmId();')
        content.append('  *clone = *this;')
        content.append('  clone->setUhdmId(id);')
        content.append('  deepCopy(clone, parent, context);')

        if ClassName in ['Begin', 'NamedBegin', 'Fork', 'NamedFork']:
            content.append(f'  elaboratorContext->m_elaborator.leave{ClassName}(this, nullptr);')

        content.append('  return clone;')
        content.append('}')
        content.append('')

    return content, includes


def _get_getByVpiName_implementation(model):
    classname = model['name']
    ClassName = config.make_class_name(classname)

    includes = set()
    content = []
    content.append(f'const BaseClass* {ClassName}::getByVpiName(std::string_view name) const {{')

    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            card = value.get('card')

            varName = config.make_var_name(name, card)

            if card == '1':
                content.append(f'  if ((m_{varName} != nullptr) && (m_{varName}->getName().compare(name) == 0)) return m_{varName};')
            else:
                type = value.get('type')
                if key != 'group_ref' and type != 'symbol':
                    includes.add(type)

                content.append(f'  if (m_{varName} != nullptr) {{')
                content.append(f'    for (const BaseClass *ref : *m_{varName}) if (ref->getName().compare(name) == 0) return ref;')
                content.append( '  }')

    content.append(f'  return basetype_t::getByVpiName(name);')
    content.append( '}')
    content.append( '')

    return content, includes


def _get_getByVpiType_implementation(model, models):
    classname = model['name']
    ClassName = config.make_class_name(classname)
    modeltype = model['type']

    case_bodies = {}
    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            card = value.get('card')
            name = value.get('name')
            vpi = value.get('vpi')
            type = value.get('type')

            if key == 'group_ref':
              type = 'BaseClass'

            varName = config.make_var_name(name, card)

            if card == '1':
                case_bodies[vpi] = (type, varName, case_bodies.get(vpi, (type, None, None))[2])

            elif card == 'any':
                case_bodies[vpi] = (type, case_bodies.get(vpi, (type, None, None))[1], varName)

    content = []
    content.append(f'{ClassName}::get_by_vpi_type_return_t {ClassName}::getByVpiType(int32_t type) const {{')

    if (modeltype == 'obj_def') or case_bodies:
        content.append('  switch (type) {')

    if case_bodies:
        for vpi in sorted(case_bodies.keys()):
            type, varName1, varNameAny = case_bodies[vpi]
            TypeName = config.make_class_name(type)

            if varName1 and varNameAny:
                content.append(f'    case {vpi}: return get_by_vpi_type_return_t(UhdmType::{TypeName}, m_{varName1}, (const std::vector<const BaseClass*>*)m_{varNameAny});')
            elif varName1:
                content.append(f'    case {vpi}: return get_by_vpi_type_return_t(UhdmType::{TypeName}, m_{varName1}, nullptr);')
            else:
                content.append(f'    case {vpi}: return get_by_vpi_type_return_t(UhdmType::{TypeName}, nullptr, (const std::vector<const BaseClass*>*)m_{varNameAny});')

    if modeltype == 'obj_def' or case_bodies:
        content.append( '    default: break;')
        content.append( '  }')

    content.append( '  return basetype_t::getByVpiType(type);')
    content.append( '}')
    content.append( '')

    return content


def _get_getVpiPropertyValue_implementation(model, models):
    classname = model['name']
    ClassName = config.make_class_name(classname)
    modeltype = model['type']

    baseclass_name = model.get('extends') or 'BaseClass'
    baseclass_model = models.get(baseclass_name, {})
    baseclass_type = baseclass_model.get('type')

    type_specified = False
    case_bodies = {}
    for key, value in model.allitems():
        if key == 'property':
            name = value.get('name')
            vpi = value.get('vpi')
            type = value.get('type')
            card = value.get('card')

            FuncName = config.make_func_name(name, card)

            if name == 'type':
                type_specified = True

            if (card == '1') and (type not in ['value', 'delay']):
                if type == 'string':
                    case_bodies[vpi] = [ f'    case {vpi}: {{' ]
                    if vpi == 'vpiFullName':
                        case_bodies[vpi].extend([
                            f'      std::string_view fullname = getFullName();',
                            f'      if (!fullname.empty() && (getName() != fullname)) {{',
                            f'        return vpi_property_value_t(fullname.data());',
                            f'      }}'
                          ])
                    else:
                        case_bodies[vpi].extend([
                            f'      std::string_view data = get{FuncName}();',
                            f'      if (!data.empty()) return vpi_property_value_t(data.data());'
                        ])
                    case_bodies[vpi].append(f'    }} break;')
                else:
                    case_bodies[vpi] = [ f'    case {vpi}: return vpi_property_value_t(get{FuncName}());' ]

    if not type_specified and (modeltype == 'obj_def') and (baseclass_type != 'obj_def'):
        case_bodies['vpiType']= [ f'    case vpiType: return vpi_property_value_t(getVpiType());' ]

    content = []
    content.append(f'{ClassName}::vpi_property_value_t {ClassName}::getVpiPropertyValue(int32_t property) const {{')

    if case_bodies:
        content.append(f'  switch (property) {{')
        for vpi in sorted(case_bodies.keys()):
          content.extend(case_bodies[vpi])
        content.append('    default: break;')
        content.append('  }')

    content.append(f'  return basetype_t::getVpiPropertyValue(property);')
    content.append(f'}}')
    content.append(f'')

    return content


def _get_compare_implementation(model):
    classname = model['name']
    ClassName = config.make_class_name(classname)
    modeltype = model['type']

    includes = set(['UhdmComparer'])
    content = [
        f'int32_t {ClassName}::compare(const BaseClass *other, UhdmComparer* comparer) const {{',
         '  int32_t r = 0;',
         '',
         '  if ((r = basetype_t::compare(other, comparer)) != 0) return r;',
         ''
    ]

    var_declared = False
    for key, value in model.allitems():
        if key not in ['property', 'obj_ref', 'class_ref', 'class', 'group_ref']:
            continue

        type = value.get('type')
        if type in ['value', 'delay']:
            continue

        vpi = value.get('vpi')
        name = value.get('name')
        card = value.get('card')

        varName = config.make_var_name(name, card)
        FuncName = config.make_func_name(name, card)

        if not var_declared:
            var_declared = True
            content.append(f'  const thistype_t *const lhs = this;')
            content.append(f'  const thistype_t *const rhs = (const thistype_t *)other;')
            content.append('')

        if card == '1':
            if type == 'string':
                content.append(f'  if ((r = comparer->compare(lhs, get{FuncName}(), rhs, rhs->get{FuncName}(), {vpi}, r)) != 0) return r;')

            elif type in ['int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'bool', 'symbol']:
                content.append(f'  if ((r = comparer->compare(lhs, m_{varName}, rhs, rhs->m_{varName}, {vpi}, r)) != 0) return r;')

            else:
                includes.add(type)
                content.append(f'  if ((r = comparer->compare(lhs, m_{varName}, rhs, rhs->m_{varName}, {vpi}, r)) != 0) return r;')
        else:
            if type != 'symbol':
                includes.add(type)

            content.append(f'  if ((r = comparer->compare(lhs, m_{varName}, rhs, rhs->m_{varName}, {vpi}, r)) != 0) return r;')

    content.extend([
        '  return r;',
        '}',
        ''
    ])

    return content, includes


def _get_swap_implementation(model):
    classname = model['name']
    ClassName = config.make_class_name(classname)
    modeltype = model['type']

    includes = set()
    content_one = [
        f'void {ClassName}::swap(const BaseClass *what, BaseClass* with) {{',
         '  basetype_t::swap(what, with);',
         ''
    ]

    content_many = [
        f'void {ClassName}::swap(const std::map<const BaseClass*, BaseClass*>& replacements) {{',
         '  basetype_t::swap(replacements);',
         ''
    ]

    for key, value in model.allitems():
        if key not in ['property', 'obj_ref', 'class_ref', 'class', 'group_ref']:
            continue

        type = value.get('type')
        if type in ['value', 'delay']:
            continue

        name = value.get('name')
        card = value.get('card')

        if key == 'group_ref':
            type = 'any'

        TypeName = config.make_class_name(type)
        varName = config.make_var_name(name, card)

        if card == '1':
            if type not in ['string', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'bool']:
                if type not in ['any', 'symbol']:
                    includes.add(type)

                content_one.append(f'  if (m_{varName} == what) {{')
                content_one.append(f'    if (with == nullptr) m_{varName} = nullptr;')
                content_one.append(f'    else if ({TypeName}* const withT = with->Cast<{TypeName}>()) m_{varName} = withT;')
                content_one.append( '  }')

                content_many.append(f'  if (m_{varName} != nullptr) {{')
                content_many.append(f'    if (auto it = replacements.find(m_{varName}); it != replacements.cend()) {{')
                content_many.append(f'      if (it->second == nullptr) m_{varName} = nullptr;')
                content_many.append(f'      else if ({TypeName}* const withT = it->second->Cast<{TypeName}>()) m_{varName} = withT;')
                content_many.append( '    }')
                content_many.append( '  }')
        else:
            if type not in ['any', 'symbol']:
                includes.add(type)

            content_one.append(f'  if (m_{varName} != nullptr) swapT(*m_{varName}, what, with);')

            content_many.append(f'  if (m_{varName} != nullptr) swapT(*m_{varName}, replacements);')

    content_one.append('}')
    content_many.append('}')

    content = content_one + [''] + content_many + ['']
    return content, includes


_cached_members = {}
def _get_group_members_recursively(model, models):
    global _cached_members

    groupname = model.get('name')
    if groupname in _cached_members:
        return _cached_members[groupname]

    checktype = set()
    _cached_members[groupname] = set()
    for key, value in model.allitems():
        if key in ['obj_ref', 'class_ref', 'group_ref'] and value:
            name = value.get('name')
            if name not in models:
                name = value.get('type')

            if key == 'group_ref':
                checktype.update(_get_group_members_recursively(models[name], models))
            elif key == 'class_ref':
                checktype.add(name)
                checktype.update(models[name]['subclasses'])
            else:
                checktype.add(name)

    _cached_members[groupname] = checktype
    return checktype


def _generate_group_checker(model, models, templates):
    global _cached_members

    groupname = model.get('name')
    checktype = _get_group_members_recursively(model, models)

    if checktype:
        checktype = (' &&\n' + (' ' * 6)).join([f'(uhdmType != UhdmType::{config.make_class_name(member)})' for member in sorted(checktype)])
    else:
        checktype = 'false'

    files = {
        'group_header.h': config.get_output_header_filepath(f'{groupname}.h'),
        'group_header.cpp': config.get_output_source_filepath(f'{groupname}.cpp'),
    }

    for input, output in files.items():
        file_content = templates[input]
        file_content = file_content.replace('<GROUPNAME>', groupname)
        file_content = file_content.replace('<UPPER_GROUPNAME>', groupname.upper())
        file_content = file_content.replace('<CHECKTYPE>', checktype)
        file_utils.set_content_if_changed(output, file_content)

    return True


def _get_setParent_implementation(model):
    classname = model['name']
    ClassName = config.make_class_name(classname)
    includes = ['scope', 'design']

    content = [
        f'bool {ClassName}::setParent(BaseClass* data, bool force /* = false */) {{',
    ]

    if classname in ['param_assign']:
      includes.append('class_typespec')
      content.append('  if ((data != nullptr) && (data->Cast<Scope>() == nullptr) && (data->Cast<Design>() == nullptr) && (data->Cast<ClassTypespec>() == nullptr)) {')

    elif classname in ['io_decl']:
      includes.append('modport')
      includes.append('task_func_decl')
      content.append('  if ((data != nullptr) && (data->Cast<Scope>() == nullptr) && (data->Cast<Design>() == nullptr) && (data->Cast<Modport>() == nullptr) && (data->Cast<TaskFuncDecl>() == nullptr)) {')

    else:
      content.append('  if ((data != nullptr) && (data->Cast<Scope>() == nullptr) && (data->Cast<Design>() == nullptr)) {')

    content.extend([
         '    if (this != m_serializer->topScope()) if (BaseClass* const topScope = m_serializer->topScope()) data = topScope;',
         '  }',
        '  return basetype_t::setParent(data, force);',
        '}',
        ''
    ])

    return content, includes


def _get_onChildXXX_implementation(model):
    classname = model['name']
    ClassName = config.make_class_name(classname)

    added_contents = [
        f'void {ClassName}::onChildAdded(BaseClass* child) {{',
         '  basetype_t::onChildAdded(child);'
    ]
    removed_contents = [
        f'void {ClassName}::onChildRemoved(BaseClass* child) {{',
         '  basetype_t::onChildRemoved(child);'
    ]

    includes = set()
    for (member, Member) in sorted(_collector_class_types[ClassName]):
        includes.add(member)
        MemberName = config.make_class_name(member)

        added_contents.extend([
            f'  if ({MemberName} *const childT = child->Cast<{MemberName}>()) {{',
            f'    get{Member}(true)->emplace_back(childT);',
              '  }'
        ])

        removed_contents.extend([
            f'  if ({MemberName} *const childT = child->Cast<{MemberName}>()) {{',
            f'    if (auto c = get{Member}(false)) {{',
             '      c->erase(std::remove(c->begin(), c->end(), childT), c->end());',
             '    }',
             '  }'
        ])

    added_contents.append('}')
    removed_contents.append('}')

    return (added_contents + [''] + removed_contents), includes


def _generate_one_class(model, models, templates):
    header_file_content = templates['class_header.h']
    source_file_content = templates['class_source.cpp']
    modeltype = model['type']
    group_headers = set()
    public_declarations = []
    private_declarations = []
    data_members = []
    implementations = []
    forward_declares = set()
    includes = set()
    leaf = (modeltype == 'obj_def') and (len(model['subclasses']) == 0)

    classname = model['name']
    ClassName = config.make_class_name(classname)

    basename = model.get('extends', 'BaseClass') or 'BaseClass'
    BaseName = config.make_class_name(basename)

    if ClassName in _special_parenting_types:
      public_declarations.extend([
           '  using basetype_t::getParent;',
          f'  bool setParent(BaseClass* data, bool force = false) {"final" if leaf else "override"};',
      ])

    type_specified = False
    for key, value in model.allitems():
        if key == 'property':
            name = value.get('name')
            vpi = value.get('vpi')
            type = value.get('type')
            card = value.get('card')

            if name == 'type':
                type_specified = True
                Vpi = vpi[:1].upper() + vpi[1:]
                public_declarations.append(f'  {type} {Vpi}() const {"final" if leaf else "override"} {{ return {value.get("vpiname")}; }}')
            else: # properties are already defined in vpi_user.h, no need to redefine them
                data_members.extend(_get_data_member(name, type, vpi, card))
                public_declarations.append(_get_declarations(name, type, vpi, card))
                implementations.extend(_get_implementations(classname, name, type, vpi, card))

        elif (key == 'extends') and value:
            header_file_content = header_file_content.replace('<EXTENDS_HEADER>', value)
            value = config.make_class_name(value)
            header_file_content = header_file_content.replace('<EXTENDS>', value)
            source_file_content = source_file_content.replace('<EXTENDS>', value)

        elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            vpi = value.get('vpi')
            type = value.get('type')
            card = value.get('card')
            real_type = type

            if key == 'group_ref':
                type = 'any'

            if (type != 'any') and (card == '1'):
                includes.add(type)
                forward_declares.add(config.make_class_name(type))

            group_headers.update(_get_group_headers(type, real_type))
            data_members.extend(_get_data_member(name, type, name, card))
            public_declarations.append(_get_declarations(name, type, vpi, card, real_type))
            implementations.extend(_get_implementations(classname, name, type, vpi, card))

    if not type_specified and (modeltype == 'obj_def'):
        vpiclasstype = config.make_vpi_name(classname)
        public_declarations.append(f'  uint32_t getVpiType() const {"final" if leaf else "override"} {{ return {vpiclasstype}; }}')

    override = 'override' if model['subclasses'] else 'final'
    if modeltype == 'class_def':
        # deepClone() not implemented for class_def; just declare to narrow the covariant return type.
        public_declarations.append(f'  {ClassName}* deepClone(BaseClass* parent, CloneContext* context) const override = 0;')
    else:
        return_type = 'TFCall' if ClassName.endswith('Call') else ClassName
        public_declarations.append(f'  {return_type}* deepClone(BaseClass* parent, CloneContext* context) const {override};')

    public_declarations.append(f'  const BaseClass* getByVpiName(std::string_view name) const {override};')
    public_declarations.append(f'  get_by_vpi_type_return_t getByVpiType(int32_t type) const {override};')
    public_declarations.append(f'  vpi_property_value_t getVpiPropertyValue(int32_t property) const {override};')
    public_declarations.append(f'  int32_t compare(const BaseClass* other, UhdmComparer* comparer) const {override};')
    public_declarations.append(f'  void swap(const BaseClass* what, BaseClass* with) {override};')
    public_declarations.append(f'  void swap(const std::map<const BaseClass*, BaseClass*>& replacements) {override};')

    if ClassName in _special_parenting_types:
        func_body, func_includes = _get_setParent_implementation(model)
        implementations.extend(func_body)
        includes.update(func_includes)

    func_body, func_includes = _get_getByVpiName_implementation(model)
    implementations.extend(func_body)
    includes.update(func_includes)

    implementations.extend(_get_getByVpiType_implementation(model, models))
    implementations.extend(_get_getVpiPropertyValue_implementation(model, models))

    func_body, func_includes = _get_deepClone_implementation(model, models)
    implementations.extend(func_body)
    includes.update(func_includes)

    func_body, func_includes = _get_compare_implementation(model)
    implementations.extend(func_body)
    includes.update(func_includes)

    func_body, func_includes = _get_swap_implementation(model)
    implementations.extend(func_body)
    includes.update(func_includes)

    if ClassName in _collector_class_types:
        private_declarations.append('  void onChildAdded(BaseClass* child) override;')
        private_declarations.append('  void onChildRemoved(BaseClass* child) override;')

        func_body, func_includes = _get_onChildXXX_implementation(model)
        implementations.extend(func_body)
        includes.update(func_includes)

    includes.discard('any')
    group_headers.discard('any')

    is_class_def = modeltype == 'class_def'
    header_file_content = header_file_content.replace('<VIRTUAL>', 'virtual ')
    header_file_content = header_file_content.replace('<FINAL_CLASS>', ' final' if leaf and not is_class_def else '')
    header_file_content = header_file_content.replace('<FINAL_DESTRUCTOR>', ' final' if leaf and not is_class_def else '')
    header_file_content = header_file_content.replace('<OVERRIDE_OR_FINAL>', 'final' if leaf and not is_class_def else 'override')

    header_file_content = header_file_content.replace('<EXTENDS>', BaseName)
    header_file_content = header_file_content.replace('<EXTENDS_HEADER>', basename)
    header_file_content = header_file_content.replace('<CLASSNAME>', ClassName)
    header_file_content = header_file_content.replace('<CLASSNAME_HEADER>', classname)
    header_file_content = header_file_content.replace('<UPPER_CLASSNAME>', classname.upper())
    header_file_content = header_file_content.replace('<PUBLIC_METHODS>', '\n\n'.join(public_declarations))
    header_file_content = header_file_content.replace('<PRIVATE_METHODS>', '\n\n'.join(private_declarations))
    header_file_content = header_file_content.replace('<MEMBERS>', '\n'.join(data_members))
    header_file_content = header_file_content.replace('<GROUP_HEADER_DEPENDENCY>', '\n'.join(f'#include <uhdm/{include}.h>' for include in sorted(group_headers)))

    source_file_content = source_file_content.replace('<CLASSNAME>', ClassName)
    source_file_content = source_file_content.replace('<CLASSNAME_HEADER>', classname)
    source_file_content = source_file_content.replace('<INCLUDES>', '\n'.join(f'#include <uhdm/{include}.h>' for include in sorted(includes)))
    source_file_content = source_file_content.replace('<METHODS>', '\n'.join(implementations))

    file_utils.set_content_if_changed(config.get_output_header_filepath(f'{classname}.h'), header_file_content)
    file_utils.set_content_if_changed(config.get_output_source_filepath(f'{classname}.cpp'), source_file_content)
    return True


def generate(models):
    templates = {}
    for filename in [ 'class_header.h', 'class_source.cpp', 'group_header.h', 'group_header.cpp' ]:
        template_filepath = config.get_template_filepath(filename)
        with open(template_filepath, 'rt') as strm:
            templates[filename] = strm.read()

    content = []
    for model in models.values():
        classname = model['name']
        modeltype = model['type']

        if modeltype == 'group_def':
            _generate_group_checker(model, models, templates)
        else:
            _generate_one_class(model, models, templates)

        content.append(f'#include "{classname}.cpp"')

    file_utils.set_content_if_changed(config.get_output_source_filepath('classes.cpp'), '\n'.join(sorted(content)))
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
