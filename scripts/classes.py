import os

import config
import file_utils


def _get_group_headers(type, real_type):
    return [ f'#include "{real_type}.h"' ] if type == 'any' else []


def _get_declarations(type, vpi, card, real_type=''):
    content = []
    if type in ['string', 'value', 'delay']:
        type = 'std::string'
    if vpi == 'uhdmType':
        type = 'UHDM_OBJECT_TYPE'

    final = ''
    virtual = ''
    if vpi in ['uhdmType', 'vpiName', 'vpiDefName']:
        final = ' final'
        virtual = 'virtual '

    check = ''
    if type == 'any':
        check = f'if (!{real_type}GroupCompliant(data)) return false;\n    '

    Vpi_ = vpi[:1].upper() + vpi[1:]

    if card == '1':
        if type == 'std::string':
            content.append(f'  {virtual}bool {Vpi_}(std::string_view data){final};')
            content.append(f'  {virtual}std::string_view {Vpi_}() const{final};')
        elif type in ['uint32_t', 'int32_t', 'bool']:
            content.append(f'  {virtual}{type} {Vpi_}() const{final} {{ return {vpi}_; }}')
            content.append(f'  {virtual}bool {Vpi_}({type} data){final} {{\n    {check}{vpi}_ = data;\n    return true;\n  }}')
        else:
            content.append(f'  {virtual}{type}* {Vpi_}() {final} {{ return {vpi}_; }}')
            content.append(f'  {virtual}const {type}* {Vpi_}() const{final} {{ return {vpi}_; }}')
            content.append( '  template <typename T>')
            content.append(f'  T* {Vpi_}() {{')
            content.append(f'    return ({vpi}_ == nullptr) ? nullptr : any_cast<T*>({vpi}_);')
            content.append( '  }')
            content.append( '  template <typename T>')
            content.append(f'  const T* {Vpi_}() const {{')
            content.append(f'    return ({vpi}_ == nullptr) ? nullptr : any_cast<const T*>({vpi}_);')
            content.append( '  }')
            content.append(f'  {virtual}bool {Vpi_}({type}* data){final} {{\n    {check}{vpi}_ = data;\n    return true;\n  }}')
    elif card == 'any':
        content.append(f'  VectorOf{type}* {Vpi_}() const {{ return {vpi}_; }}')
        content.append(f'  bool {Vpi_}(VectorOf{type}* data) {{\n    {check}{vpi}_ = data;\n    return true;\n  }}')

    return '\n'.join(content)


def _get_implementations(classname, type, vpi, card):
    includes = set()
    content = []
    if card != '1':
        return content, includes

    if type in ['string', 'value', 'delay']:
        type = 'std::string'

    if type != 'std::string':
        return content, includes

    if vpi == 'uhdmType':
        type = 'UHDM_OBJECT_TYPE'

    Vpi_ = vpi[:1].upper() + vpi[1:]

    if vpi == 'vpiFullName':
        content.append(f'std::string_view {classname}::{Vpi_}() const {{')
        content.append(f'  if ({vpi}_) {{')
        content.append(f'    return serializer_->symbolMaker.GetSymbol({vpi}_);')
        content.append( '  } else {')
        content.append( '    std::vector<std::string_view> names;')
        content.append( '    const BaseClass* parent = this;')
        content.append( '    const BaseClass* child = nullptr;')
        content.append( '    bool column = false;')
        content.append( '    while (parent != nullptr) {')
        content.append( '      const BaseClass* actual_parent = parent->VpiParent();')
        content.append( '      if (parent->UhdmType() == uhdmdesign) break;')
        content.append( '      if ((parent->UhdmType() == uhdmpackage) || (parent->UhdmType() == uhdmclass_defn)) column = true;')
        content.append( '      std::string_view name = parent->VpiName().empty() ? parent->VpiDefName() : parent->VpiName();')
        content.append( '      UHDM_OBJECT_TYPE parent_type = (parent != nullptr) ? parent->UhdmType() : uhdmunsupported_stmt;')
        content.append( '      UHDM_OBJECT_TYPE actual_parent_type = (actual_parent != nullptr) ? actual_parent->UhdmType() : uhdmunsupported_stmt;')
        content.append( '      bool skip_name = (actual_parent_type == uhdmref_obj) || (parent_type == uhdmmethod_func_call) ||')
        content.append( '                       (parent_type == uhdmmethod_task_call) || (parent_type == uhdmfunc_call) ||')
        content.append( '                       (parent_type == uhdmtask_call) || (parent_type == uhdmsys_func_call) ||')
        content.append( '                       (parent_type == uhdmsys_task_call);')
        content.append( '      if (child != nullptr) {')
        content.append( '        UHDM_OBJECT_TYPE child_type = child->UhdmType();')
        content.append( '        if ((child_type == uhdmbit_select) && (parent_type == uhdmport)) {')
        content.append( '          skip_name = true;')
        content.append( '        }')
        content.append( '        if ((child_type == uhdmref_obj) && (parent_type == uhdmbit_select)) {')
        content.append( '          skip_name = true;')
        content.append( '        }')
        content.append( '        if ((child_type == uhdmref_obj) && (parent_type == uhdmindexed_part_select)) {')
        content.append( '          skip_name = true;')
        content.append( '        }')
        content.append( '        if ((child_type == uhdmref_obj) && (parent_type == uhdmhier_path)) {')
        content.append( '          skip_name = true;')
        content.append( '        }')
        content.append( '      }')
        content.append( '      if ((!name.empty()) && (!skip_name)) {')
        content.append( '        names.emplace_back(name);')
        content.append( '      }')
        content.append( '      child = parent;')
        content.append( '      parent = parent->VpiParent();')
        content.append( '    }')
        content.append( '    std::string fullName;')
        content.append( '    if (!names.empty()) {')
        content.append( '      size_t index = names.size() - 1;')
        content.append( '      while(1) {')
        content.append( '        fullName += names[index];')
        content.append( '        if (index > 0) fullName += column ? "::" : ".";')
        content.append( '        if (index == 0) break;')
        content.append( '        index--;')
        content.append( '      }')
        content.append( '    }')
        content.append( '    if (!fullName.empty()) {')
        content.append(f'      const_cast<{classname}*>(this)->VpiFullName(fullName);')
        content.append( '    }')
        content.append(f'    return serializer_->symbolMaker.GetSymbol({vpi}_);')
        content.append( '  }')
        content.append( '}')
    else:
        content.append(f'std::string_view {classname}::{Vpi_}() const {{')
        content.append(f'  return {vpi}_ ? serializer_->symbolMaker.GetSymbol({vpi}_) : kEmpty;')
        content.append(f'}}')

    content.append('')
    content.append(f'bool {classname}::{Vpi_}(std::string_view data) {{')
    content.append(f'  {vpi}_ = serializer_->symbolMaker.Make(data);')
    content.append(f'  return true;')
    content.append(f'}}')
    content.append('')

    return content, includes


def _get_data_member(type, vpi, card):
    content = []

    if type in ['string', 'value', 'delay']:
        type = 'std::string'

    if card == '1':
        pointer = ''
        default_assignment = 'false' if type == 'bool' else '0'
        if type not in ['uint32_t', 'int32_t', 'bool', 'std::string']:
            pointer = '*'
            default_assignment = 'nullptr'

        if type == 'std::string':
            content.append(f'  SymbolId {vpi}_;')
        else:
            content.append(f'  {type}{pointer} {vpi}_ = {default_assignment};')

    elif card == 'any':
        content.append(f'  VectorOf{type}* {vpi}_ = nullptr;')

    return content


def _get_DeepClone_implementation(model, models):
    classname = model.get('name')
    modeltype = model.get('type')
    Classname = classname[0].upper() + classname[1:]
    includes = set(['ElaboratorListener'])
    content = []
    content.append(f'void {classname}::DeepCopy({classname}* clone, BaseClass* parent, CloneContext* context) const {{')
    content.append( '  [[maybe_unused]] ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext*>(context);')
    if modeltype != 'class_def':
        content.append(f'  elaboratorContext->m_elaborator.enter{Classname}(clone, nullptr);')

    if classname in ['bit_select']:
        includes.add('ExprEval')
        content.append(f'  ExprEval eval;')
        content.append(f'  bool invalidValue = false;')
        content.append( '  if (any* val = eval.reduceExpr(VpiIndex(), invalidValue, parent, parent, true)) {')
        content.append( '    if (!invalidValue) {')
        content.append(f'      std::string indexName = eval.prettyPrint(val);')
        content.append( '      if (any* indexVal = elaboratorContext->m_elaborator.bindAny(indexName)) {')
        content.append(f'        val = eval.reduceExpr(indexVal, invalidValue, parent, parent, true);')
        content.append(f'        if (!invalidValue) indexName = eval.prettyPrint(val);')
        content.append( '      }')
        content.append( '      const std::string_view name(VpiName());')
        content.append( '      std::string fullIndexName(name);')
        content.append( '      fullIndexName.append("[").append(indexName).append("]");')
        content.append(f'      clone->Actual_group(elaboratorContext->m_elaborator.bindAny(fullIndexName));')
        content.append(f'      if (!clone->Actual_group()) clone->Actual_group(elaboratorContext->m_elaborator.bindAny(name));')
        content.append(f'      if (!clone->Actual_group()) clone->Actual_group((any*) Actual_group());')
        content.append( '    }')
        content.append( '  }')

    content.append(f'  basetype_t::DeepCopy(clone, parent, context);')
    vpi_name = config.make_vpi_name(classname)

    if 'Select' in vpi_name:
        includes.add('net')
        content.append('  if (any* n = elaboratorContext->m_elaborator.bindNet(VpiName())) {')
        content.append('    if (net* nn = any_cast<net*>(n))')
        content.append('      clone->VpiFullName(nn->VpiFullName());')
        content.append('  }')

    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            type = value.get('type')
            card = value.get('card')

            cast = 'any' if key == 'group_ref' else type
            Cast = cast[:1].upper() + cast[1:]
            method = name[:1].upper() + name[1:]

            if (card == 'any') and not method.endswith('s'):
                method += 's'

            # Unary relations
            if card == '1':
                if (classname in ['ref_obj', 'ref_var']) and (method == 'Actual_group'):
                    includes.add('ElaboratorListener')
                    content.append(f'  if (!clone->{method}()) clone->{method}(elaboratorContext->m_elaborator.bindAny(VpiName()));')
                    content.append(f'  if (!clone->{method}()) clone->{method}((any*) {method}());')

                elif classname == 'udp' and method == 'Udp_defn':
                    includes.add('ElaboratorListener')
                    content.append(f'  if (!clone->{method}()) clone->{method}((udp_defn*) elaboratorContext->m_elaborator.bindAny(VpiDefName()));')
                    content.append(f'  if (!clone->{method}()) clone->{method}((udp_defn*) {method}());')

                elif method in ['Task', 'Function']:
                    prefix = 'nullptr'
                    includes.add(method.lower())
                    if 'method_' in classname:
                        includes.add('ref_obj')
                        includes.add('class_var')
                        includes.add(method.lower())
                        content.append(f'  const ref_obj* ref = any_cast<const ref_obj*> (clone->Prefix());')
                        content.append( '  const class_var* prefix = nullptr;')
                        content.append( '  if (ref) prefix = any_cast<const class_var*>(ref->Actual_group());')
                        prefix = 'prefix'
                    content.append(f'  elaboratorContext->m_elaborator.scheduleTaskFuncBinding(clone, {prefix});')

                elif classname == 'disable' and method == 'VpiExpr':
                    includes.add('expr')
                    content.append(f'  if (auto obj = {method}()) clone->{method}((expr*) obj);')

                elif classname == 'ports' and method == 'High_conn':
                    content.append(f'  if (auto obj = {method}())')
                    content.append('    { elaboratorContext->m_elaborator.ignoreLastInstance(true); ')
                    content.append(f'      clone->{method}(obj->DeepClone(clone, context));')
                    content.append('      elaboratorContext->m_elaborator.ignoreLastInstance(false); }')

                elif classname == 'int_typespec' and method == 'Cast_to_expr':
                    includes.add('variables')
                    content.append(f'  if (auto obj = {method}()) clone->{method}((variables*) obj);')

                elif classname == 'function' and method == 'Return':
                    includes.add('variables')
                    content.append(f'  if (auto obj = {method}()) clone->{method}((variables*) obj);')

                elif classname == 'class_typespec' and method == 'Class_defn':
                    includes.add('class_defn')
                    content.append(f'  if (auto obj = {method}()) clone->{method}((class_defn*) obj);')

                elif method == 'Instance':
                    includes.add('instance')
                    content.append(f'  if (auto obj = {method}()) clone->{method}((instance*) obj);')
                    content.append( '  if (instance* inst = any_cast<instance*>(parent))')
                    content.append( '    clone->Instance(inst);')

                elif method == 'Module_inst':
                    includes.add('module_inst')
                    content.append(f'  if (auto obj = {method}()) clone->{method}((module_inst*) obj);')

                elif method == 'Typespec':
                    includes.add('typespec')
                    content.append( '  if (elaboratorContext->m_elaborator.uniquifyTypespec()) {')
                    content.append(f'    if (auto obj = {method}()) clone->{method}(obj->DeepClone(clone, context));')
                    content.append( '  } else {')
                    content.append(f'    if (auto obj = {method}()) clone->{method}((typespec*) obj);')
                    content.append( '  }')

                else:
                    content.append(f'  if (auto obj = {method}()) clone->{method}(obj->DeepClone(clone, context));')

            # N-ary relations
            elif method == 'Typespecs':
                content.append(f'  if (auto vec = {method}()) {{')
                content.append(f'    auto clone_vec = context->m_serializer->Make{Cast}Vec();')
                content.append(f'    clone->{method}(clone_vec);')
                content.append( '    for (auto obj : *vec) {')
                content.append( '      if (elaboratorContext->m_elaborator.uniquifyTypespec()) {')
                content.append( '        clone_vec->push_back(obj->DeepClone(clone, context));')
                content.append( '      } else {')
                content.append( '        clone_vec->push_back(obj);')
                content.append( '      }')
                content.append( '    }')
                content.append( '  }')

            elif classname == 'class_defn' and method == 'Deriveds':
                # Don't deep clone
                content.append(f'  if (auto vec = {method}()) {{')
                content.append(f'    auto clone_vec = context->m_serializer->Make{Cast}Vec();')
                content.append(f'    clone->{method}(clone_vec);')
                content.append( '    for (auto obj : *vec) {')
                content.append( '      clone_vec->push_back(obj);')
                content.append( '    }')
                content.append( '  }')

            elif classname == 'module_inst' and method == 'Ref_modules':
                pass # No cloning

            else:
                content.append(f'  if (auto vec = {method}()) {{')
                content.append(f'    auto clone_vec = context->m_serializer->Make{Cast}Vec();')
                content.append(f'    clone->{method}(clone_vec);')
                content.append( '    for (auto obj : *vec) {')
                content.append( '      clone_vec->push_back(obj->DeepClone(clone, context));')
                content.append( '    }')
                content.append( '  }')
    if modeltype != 'class_def':
          content.append(f'  elaboratorContext->m_elaborator.leave{Classname}(clone,nullptr);')
    content.append('}')
    content.append('')

    if '_call' in classname or classname in [ 'function', 'task', 'constant', 'tagged_pattern', 'gen_scope_array', 'hier_path', 'cont_assign' ]:
        return content, includes  # Use hardcoded implementations of DeepClone

    if modeltype == 'obj_def':
        # DeepClone() not implemented for class_def; just declare to narrow the covariant return type.
        content.append(f'{classname}* {classname}::DeepClone(BaseClass* parent, CloneContext* context) const {{')

        if classname in ['begin', 'named_begin', 'fork', 'named_fork']:
            content.append( '  ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext*>(context);')
            content.append(f'  elaboratorContext->m_elaborator.enter{Classname}(this, nullptr);')

        if 'Net' in vpi_name:
            includes.add('ElaboratorListener')
            content.append( '  ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext*>(context);')
            content.append(f'  {classname}* clone = any_cast<{classname}*>(elaboratorContext->m_elaborator.bindNet(VpiName()));')
            content.append( '  if (clone != nullptr) {')
            content.append(f'    return clone;')
            content.append( '  }')
            content.append(f'  clone = context->m_serializer->Make{Classname}();')

        elif 'Parameter' in vpi_name:
            includes.add('ElaboratorListener')
            content.append( '  ElaboratorContext* const elaboratorContext = clonecontext_cast<ElaboratorContext*>(context);')
            content.append(f'  {classname}* clone = any_cast<{classname}*>(elaboratorContext->m_elaborator.bindParam(VpiName()));')
            content.append( '  if (clone == nullptr) {')
            content.append(f'    clone = context->m_serializer->Make{Classname}();')
            content.append( '  }')

        else:
            content.append(f'  {classname}* const clone = context->m_serializer->Make{Classname}();')

        content.append('  const uint32_t id = clone->UhdmId();')
        content.append('  *clone = *this;')
        content.append('  clone->UhdmId(id);')
        content.append('  DeepCopy(clone, parent, context);')

        if classname in ['begin', 'named_begin', 'fork', 'named_fork']:
            content.append(f'  elaboratorContext->m_elaborator.leave{Classname}(this, nullptr);')

        content.append('  return clone;')
        content.append('}')
        content.append('')

    return content, includes


def _get_GetByVpiName_implementation(model):
    classname = model['name']

    includes = set()
    content = []
    content.append(f'const BaseClass* {classname}::GetByVpiName(std::string_view name) const {{')

    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            card = value.get('card')

            if (card == 'any') and not name.endswith('s'):
                name += 's'

            if card == '1':
                content.append(f'  if (({name}_ != nullptr) && ({name}_->VpiName().compare(name) == 0)) {{')
                content.append(f'    return {name}_;')
                content.append( '  }')
            else:
                if key != 'group_ref':
                    includes.add(value.get('type'))

                content.append(f'  if ({name}_ != nullptr) {{')
                content.append(f'    for (const BaseClass *ref : *{name}_) {{')
                content.append(f'      if (ref->VpiName().compare(name) == 0) return ref;')
                content.append( '    }')
                content.append( '  }')

    content.append(f'  return basetype_t::GetByVpiName(name);')
    content.append( '}')
    content.append( '')

    return content, includes


def _get_GetByVpiType_implementation(model, models):
    classname = model['name']
    modeltype = model['type']

    case_bodies = {}
    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            card = value.get('card')
            name = value.get('name')
            vpi = value.get('vpi')

            if card == '1':
                case_bodies[vpi] = (name, case_bodies.get(vpi, (None, None))[1])

            elif card == 'any':
                if not name.endswith('s'):
                    name += 's'
                case_bodies[vpi] = (case_bodies.get(vpi, (None, None))[0], name)

    content = []
    content.append(f'std::tuple<const BaseClass*, UHDM_OBJECT_TYPE, const std::vector<const BaseClass*>*> {classname}::GetByVpiType(int32_t type) const {{')

    if (modeltype == 'obj_def') or case_bodies:
        content.append(f'  switch (type) {{')

    if case_bodies:
        for vpi in sorted(case_bodies.keys()):
            name1, name_any = case_bodies[vpi]
            if name1 and name_any:
                content.append(f'    case {vpi}: return std::make_tuple({name1}_, uhdm{name_any}, (const std::vector<const BaseClass*>*){name_any}_);')
            elif name1:
                content.append(f'    case {vpi}: return std::make_tuple({name1}_, static_cast<UHDM_OBJECT_TYPE>(0), nullptr);')
            else:
                content.append(f'    case {vpi}: return std::make_tuple(nullptr, uhdm{name_any}, (const std::vector<const BaseClass*>*){name_any}_);')

    if modeltype == 'obj_def' or case_bodies:
        content.append( '  }')

    content.append( '  return basetype_t::GetByVpiType(type);')
    content.append( '}')
    content.append( '')

    return content


def _get_GetVpiPropertyValue_implementation(model, models):
    classname = model['name']
    modeltype = model['type']

    baseclass_name = model.get('extends', 'BaseClass')
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

            Vpi_ = vpi[:1].upper() + vpi[1:]

            if name == 'type':
                type_specified = True

            if (card == '1') and (type not in ['value', 'delay']):
                if type == 'string':
                    case_bodies[vpi] = [ f'    case {vpi}: {{' ]
                    if vpi == 'vpiFullName':
                        case_bodies[vpi].extend([
                            f'      std::string_view fullname = VpiFullName();',
                            f'      if (!fullname.empty() && (VpiName() != fullname)) {{',
                            f'        return vpi_property_value_t(fullname.data());',
                            f'      }}'
                          ])
                    else:
                        case_bodies[vpi].extend([
                            f'      std::string_view data = {Vpi_}();',
                            f'      if (!data.empty()) return vpi_property_value_t(data.data());'
                        ])
                    case_bodies[vpi].append(f'    }} break;')
                else:
                    case_bodies[vpi] = [ f'    case {vpi}: return vpi_property_value_t({Vpi_}());' ]

    if not type_specified and (modeltype == 'obj_def') and (baseclass_type != 'obj_def'):
        case_bodies['vpiType']= [ f'    case vpiType: return vpi_property_value_t(VpiType());' ]

    content = []
    content.append(f'{classname}::vpi_property_value_t {classname}::GetVpiPropertyValue(int32_t property) const {{')

    if case_bodies:
        content.append(f'  switch (property) {{')
        for vpi in sorted(case_bodies.keys()):
          content.extend(case_bodies[vpi])
        content.append(f'  }}')

    content.append(f'  return basetype_t::GetVpiPropertyValue(property);')
    content.append(f'}}')
    content.append(f'')

    return content


def _get_Compare_implementation(model):
    vpi_blacklist = [
        'uhdmId',
        'uhdmType',
        'vpiColumnNo',
        'vpiDefLineNo',
        'vpiEndColumnNo',
        'vpiEndLineNo',
        'vpiFile',
        'vpiFullName',
        'vpiLineNo',
        'vpiRefColumnNo',
        'vpiRefEndColumnNo',
        'vpiRefEndLineNo',
        'vpiRefFile',
        'vpiRefLineNo',
    ]

    classname = model['name']
    modeltype = model['type']

    includes = set()
    content = [
        f'int32_t {classname}::Compare(const BaseClass *other, CompareContext* context) const {{',
         '  int32_t r = 0;'
    ]

    if modeltype == 'obj_def':
        # Mark self visited (and check) only for the most derived sub-class
        content.append('  if (!context->m_visited.insert(this).second) return r;')

    content.extend([
         '  if ((r = basetype_t::Compare(other, context)) != 0) return r;',
         ''
    ])

    var_declared = False
    for key, value in model.allitems():
        if key not in ['property', 'obj_ref', 'class_ref', 'class', 'group_ref']:
            continue

        vpi = value.get('vpi')
        if vpi in vpi_blacklist:
            continue

        type = value.get('type')
        if type in ['value', 'delay']:
            continue

        name = value.get('name')
        card = value.get('card')
        Vpi_ = vpi[:1].upper() + vpi[1:]

        if not var_declared:
            var_declared = True
            content.append(f'  const thistype_t *const lhs = this;')
            content.append(f'  const thistype_t *const rhs = (const thistype_t *)other;')
            content.append('')

        if card == '1':
            if type == 'string':
                content.extend([
                  f'  if ((r = lhs->{Vpi_}().compare(rhs->{Vpi_}())) != 0) {{',
                   '    context->m_failedLhs = lhs;',
                   '    context->m_failedRhs = rhs;',
                   '    return r;',
                   '  }',
                ])
            elif type in ['uint32_t', 'int32_t']:
                content.extend([
                  f'  if ((r = lhs->{Vpi_}() - rhs->{Vpi_}()) != 0) {{',
                   '    context->m_failedLhs = lhs;',
                   '    context->m_failedRhs = rhs;',
                   '    return r;',
                   '  }',
                ])
            elif type == 'bool':
                content.extend([
                  f'  if ((r = (lhs->{Vpi_}() == rhs->{Vpi_}()) ? 0 : (lhs->{Vpi_}() ? 1 : -1)) != 0) {{',
                   '    context->m_failedLhs = lhs;',
                   '    context->m_failedRhs = rhs;',
                   '    return r;',
                   '  }',
                ])
            else:
                Name = name[:1].upper() + name[1:]
                includes.add(type)
                content.extend([
                   '',
                  f'  auto lhs_{name} = lhs->{Name}();',
                  f'  auto rhs_{name} = rhs->{Name}();',
                  f'  if ((lhs_{name} != nullptr) && (rhs_{name} != nullptr)) {{',
                  f'    if ((r = lhs_{name}->Compare(rhs_{name}, context)) != 0) return r;',
                  f'  }} else if ((lhs_{name} != nullptr) && (rhs_{name} == nullptr)) {{',
                   '    context->m_failedLhs = lhs;',
                   '    context->m_failedRhs = rhs;',
                   '    return 1;',
                  f'  }} else if ((lhs_{name} == nullptr) && (rhs_{name} != nullptr)) {{',
                   '    context->m_failedLhs = lhs;',
                   '    context->m_failedRhs = rhs;',
                   '    return -1;',
                   '  }'
                ])
        else:
            if not name.endswith('s'):
                name += 's'

            Name = name[:1].upper() + name[1:]
            includes.add(type)
            content.extend([
                 '',
                f'  auto lhs_{name} = lhs->{Name}();',
                f'  auto rhs_{name} = rhs->{Name}();',
                f'  if ((lhs_{name} != nullptr) && (rhs_{name} != nullptr)) {{',
                f'    if ((r = static_cast<int32_t>(lhs_{name}->size() - rhs_{name}->size())) != 0) {{',
                 '      context->m_failedLhs = lhs;',
                 '      context->m_failedRhs = rhs;',
                 '      return 1;',
                 '    }',
                f'    for (size_t i = 0, n = lhs_{name}->size(); i < n; ++i) {{',
                f'      if ((r = lhs_{name}->at(i)->Compare(rhs_{name}->at(i), context)) != 0) return r;',
                 '    }',
                f'  }} else if ((lhs_{name} != nullptr) && (rhs_{name} == nullptr)) {{',
                 '    context->m_failedLhs = lhs;',
                 '    context->m_failedRhs = rhs;',
                 '    return 1;',
                f'  }} else if ((lhs_{name} == nullptr) && (rhs_{name} != nullptr)) {{',
                 '    context->m_failedLhs = lhs;',
                 '    context->m_failedRhs = rhs;',
                 '    return -1;',
                 '  }',
            ])

    content.extend([
        '  return r;',
        '}',
        ''
    ])

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

            checktype.add(name)
            if key == 'group_ref':
                checktype.update(_get_group_members_recursively(models[name], models))
            elif key == 'class_ref':
                checktype.update(models[name]['subclasses'])

    _cached_members[groupname] = checktype
    return checktype


def _generate_group_checker(model, models, templates):
    global _cached_members

    groupname = model.get('name')
    checktype = _get_group_members_recursively(model, models)

    if checktype:
        checktype = (' &&\n' + (' ' * 6)).join([f'(uhdmtype != uhdm{member})' for member in sorted(checktype)])
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


def _generate_one_class(model, models, templates):
    header_file_content = templates['class_header.h']
    source_file_content = templates['class_source.cpp']
    classname = model['name']
    modeltype = model['type']
    group_headers = set()
    declarations = []
    data_members = []
    implementations = []
    forward_declares = set()
    includes = set(['Serializer'])
    leaf = (modeltype == 'obj_def') and (len(model['subclasses']) == 0)

    baseclass_name = model.get('extends', 'BaseClass')
    baseclass_model = models.get(baseclass_name, {})
    baseclass_type = baseclass_model.get('type')

    type_specified = False
    for key, value in model.allitems():
        if key == 'property':
            name = value.get('name')
            vpi = value.get('vpi')
            type = value.get('type')
            card = value.get('card')

            Vpi = vpi[:1].upper() + vpi[1:]

            if name == 'type':
                type_specified = True
                declarations.append(f'  {type} {Vpi}() const {"final" if leaf else "override"} {{ return {value.get("vpiname")}; }}')
            else: # properties are already defined in vpi_user.h, no need to redefine them
                data_members.extend(_get_data_member(type, vpi, card))
                declarations.append(_get_declarations(type, vpi, card))
                func_body, func_includes = _get_implementations(classname, type, vpi, card)
                implementations.extend(func_body)
                includes.update(func_includes)

        elif key == 'extends' and value:
            header_file_content = header_file_content.replace('<EXTENDS>', value)
            source_file_content = source_file_content.replace('<EXTENDS>', value)

        elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            vpi = value.get('vpi')
            type = value.get('type')
            card = value.get('card')

            if (card == 'any') and not name.endswith('s'):
                name += 's'
            real_type = type

            if key == 'group_ref':
                type = 'any'

            if type != 'any' and card == '1':
                forward_declares.add(type)

            group_headers.update(_get_group_headers(type, real_type))
            data_members.extend(_get_data_member(type, name, card))
            declarations.append(_get_declarations(type, name, card, real_type))
            func_body, func_includes = _get_implementations(classname, type, name, card)
            implementations.extend(func_body)
            includes.update(func_includes)

    if not type_specified and (modeltype == 'obj_def'):
        vpiclasstype = config.make_vpi_name(classname)
        declarations.append(f'  virtual uint32_t VpiType() const {"final" if leaf else "override"} {{ return {vpiclasstype}; }}')

    if modeltype == 'class_def':
        # DeepClone() not implemented for class_def; just declare to narrow the covariant return type.
        declarations.append(f'  virtual {classname}* DeepClone(BaseClass* parent, CloneContext* context) const override = 0;')
    else:
        return_type = 'tf_call' if '_call' in classname else classname
        declarations.append(f'  virtual {return_type}* DeepClone(BaseClass* parent, CloneContext* context) const override;')

    declarations.append('  virtual const BaseClass* GetByVpiName(std::string_view name) const override;')
    declarations.append('  virtual std::tuple<const BaseClass*, UHDM_OBJECT_TYPE, const std::vector<const BaseClass*>*> GetByVpiType(int32_t type) const override;')
    declarations.append('  virtual vpi_property_value_t GetVpiPropertyValue(int32_t property) const override;')
    declarations.append('  virtual int32_t Compare(const BaseClass* other, CompareContext* context) const override;')

    func_body, func_includes = _get_GetByVpiName_implementation(model)
    implementations.extend(func_body)
    includes.update(func_includes)

    implementations.extend(_get_GetByVpiType_implementation(model, models))
    implementations.extend(_get_GetVpiPropertyValue_implementation(model, models))

    func_body, func_includes = _get_DeepClone_implementation(model, models)
    implementations.extend(func_body)
    includes.update(func_includes)

    func_body, func_includes = _get_Compare_implementation(model)
    implementations.extend(func_body)
    includes.update(func_includes)

    includes.update(forward_declares)

    is_class_def = modeltype == 'class_def'
    header_file_content = header_file_content.replace('<VIRTUAL>', 'virtual ')
    header_file_content = header_file_content.replace('<FINAL_CLASS>', ' final' if leaf and not is_class_def else '')
    header_file_content = header_file_content.replace('<FINAL_DESTRUCTOR>', ' final' if leaf and not is_class_def else '')
    header_file_content = header_file_content.replace('<OVERRIDE_OR_FINAL>', 'final' if leaf and not is_class_def else 'override')
    header_file_content = header_file_content.replace('<DISABLE_OBJECT_FACTORY>', '' if leaf or not is_class_def else '#if 0 // This class cannot be instantiated')
    header_file_content = header_file_content.replace('<END_DISABLE_OBJECT_FACTORY>', '' if leaf or not is_class_def else '#endif')

    header_file_content = header_file_content.replace('<EXTENDS>', 'BaseClass')
    header_file_content = header_file_content.replace('<CLASSNAME>', classname)
    header_file_content = header_file_content.replace('<UPPER_CLASSNAME>', classname.upper())
    header_file_content = header_file_content.replace('<METHODS>', '\n\n'.join(declarations))
    header_file_content = header_file_content.replace('<MEMBERS>', '\n\n'.join(data_members))
    header_file_content = header_file_content.replace('<GROUP_HEADER_DEPENDENCY>', '\n'.join(sorted(group_headers)))
    header_file_content = header_file_content.replace('<TYPE_FORWARD_DECLARE>', '\n'.join([f'class {type};' for type in sorted(forward_declares)]))

    source_file_content = source_file_content.replace('<CLASSNAME>', classname)
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

    file_utils.set_content_if_changed(config.get_output_source_filepath('classes.cpp'), '\n'.join(content))
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
