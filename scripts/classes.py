import os

import config
import file_utils


def _get_group_headers(type, real_type):
    return [ f'#include "{real_type}.h"' ] if type == 'any' else []


def _get_declarations(classname, type, vpi, card, real_type=''):
    content = []
    if type in ['string', 'value', 'delay']:
        type = 'std::string'
    if vpi == 'uhdmType':
        type = 'UHDM_OBJECT_TYPE'

    final = ''
    virtual = ''
    if vpi in ['vpiParent', 'uhdmParentType', 'uhdmType', 'vpiLineNo', 'vpiColumnNo', 'vpiEndLineNo', 'vpiEndColumnNo', 'vpiFile', 'vpiName', 'vpiDefName', 'uhdmId']:
        final = ' final'
        virtual = 'virtual '

    check = ''
    group_headers = []
    if type == 'any':
        check = f'if (!{real_type}GroupCompliant(data)) return false; '

    Vpi_ = vpi[:1].upper() + vpi[1:]

    if card == '1':
        pointer = ''
        const = ''
        if type not in ['unsigned int', 'int', 'bool', 'std::string']:
            pointer = '*'
            const = 'const '

        if vpi in ['vpiFile', 'vpiDefFile']:
            content.append(f'  {virtual}bool {Vpi_}(const std::filesystem::path& data){final};')
            content.append(f'  {virtual}SymbolFactory::ID {Vpi_}Id() const{final} {{ return {vpi}_; }}')
            content.append(f'  {virtual}std::filesystem::path {Vpi_}() const{final};')
        elif type == 'std::string':
            content.append(f'  {virtual}bool {Vpi_}(const std::string& data){final};')
            content.append(f'  {virtual}const std::string& {Vpi_}() const{final};')
        else:
            content.append(f'  {virtual}{const}{type}{pointer} {Vpi_}() const{final} {{ return {vpi}_; }}')
            if vpi == 'vpiParent':
                content.append(f'  virtual bool {Vpi_}({type}{pointer} data) final {{ {check}{vpi}_ = data; uhdmParentType_ = (data != nullptr) ? data->UhdmType() : 0; return true; }}')
            else:
                content.append(f'  {virtual}bool {Vpi_}({type}{pointer} data){final} {{ {check}{vpi}_ = data; return true; }}')
    elif card == 'any':
        content.append(f'  VectorOf{type}* {Vpi_}() const {{ return {vpi}_; }}')
        content.append(f'  bool {Vpi_}(VectorOf{type}* data) {{ {check}{vpi}_ = data; return true; }}')

    return content


def _get_implementations(classname, type, vpi, card, real_type=''):
    content = []
    if card != '1':
        return content

    if type in ['string', 'value', 'delay']:
        type = 'std::string'

    if type != 'std::string':
        return content

    if vpi == 'uhdmType':
        type = 'UHDM_OBJECT_TYPE'

    pointer = ''
    const = ''
    if type not in ['unsigned int', 'int', 'bool', 'std::string']:
        pointer = '*'
        const = 'const '

    Vpi_ = vpi[:1].upper() + vpi[1:]

    if vpi == 'vpiFullName':
        content.append(f'const {type}{pointer}& {classname}::{Vpi_}() const {{')
        content.append(f'  if ({vpi}_) {{')
        content.append(f'    return serializer_->symbolMaker.GetSymbol({vpi}_);')
        content.append( '  } else {')
        content.append( '    std::vector<std::string> names;')
        content.append( '    const BaseClass* parent = this;')
        content.append( '    const BaseClass* child = nullptr;')
        content.append( '    bool column = false;')
        content.append( '    while (parent != nullptr) {')
        content.append( '      const BaseClass* actual_parent = parent->VpiParent();')
        content.append( '      if (parent->UhdmType() == uhdmdesign) break;')
        content.append( '      if ((parent->UhdmType() == uhdmpackage) || (parent->UhdmType() == uhdmclass_defn)) column = true;')
        content.append( '      const std::string& name = parent->VpiName().empty() ? parent->VpiDefName() : parent->VpiName();')
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
        content.append( '        names.push_back(name);')
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
    elif vpi in ['vpiFile', 'vpiDefFile']:
        content.append(f'std::filesystem::path {classname}::{Vpi_}() const {{')
        content.append(f'  const std::string &symbol = serializer_->symbolMaker.GetSymbol({vpi}_);')
        content.append( '  return (symbol.empty() || (symbol == SymbolFactory::getBadSymbol())) ? std::filesystem::path() : std::filesystem::path(symbol);')
        content.append(f'}}')
    else:
        content.append(f'const {type}{pointer}& {classname}::{Vpi_}() const {{ return serializer_->symbolMaker.GetSymbol({vpi}_); }}')

    content.append('')
    if vpi in ['vpiFile', 'vpiDefFile']:
        content.append(f'bool {classname}::{Vpi_}(const std::filesystem::path& data) {{ {vpi}_ = serializer_->symbolMaker.Make(data.string()); return true; }}')
    else:
        content.append(f'bool {classname}::{Vpi_}(const {type}{pointer}& data) {{ {vpi}_ = serializer_->symbolMaker.Make(data); return true; }}')
    content.append('')

    return content


def _get_data_member(type, vpi, card):
    content = []

    if type in ['string', 'value', 'delay']:
        type = 'std::string'

    if card == '1':
        pointer = ''
        default_assignment = '0'
        if type not in ['unsigned int', 'int', 'bool', 'std::string']:
            pointer = '*'
            default_assignment = 'nullptr'

        if type == 'std::string' or vpi in ['vpiFile', 'vpiDefFile']:
            content.append(f'  SymbolFactory::ID {vpi}_ = {default_assignment};')
        else:
            content.append(f'  {type}{pointer} {vpi}_ = {default_assignment};')

    elif card == 'any':
        content.append(f'  VectorOf{type}* {vpi}_ = nullptr;')

    return content


def _get_clone_implementation(model, models):
    classname = model.get('name')

    content = []
    content.append(f'void {classname}::DeepCopy({classname}* clone, Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const {{')
    content.append(f'  basetype_t::DeepCopy(clone, serializer, elaborator, parent);')

    modeltype = model.get('type')
    basename = model.get('extends', 'BaseClass')
    Classname = classname[0].upper() + classname[1:]
    vpi_name = config.make_vpi_name(classname)

    if classname in ['part_select', 'bit_select', 'indexed_part_select']:
        content.append('  if (const any* parent = VpiParent()) {')
        content.append('    ref_obj* ref = serializer->MakeRef_obj();')
        content.append('    clone->VpiParent(ref);')
        content.append('    ref->VpiName(parent->VpiName());')
        content.append('    if (parent->UhdmType() == uhdmref_obj) {')
        content.append('      ref->VpiFullName(((ref_obj*) VpiParent())->VpiFullName());')
        content.append('    }')
        content.append('    ref->VpiParent((any*) parent);')
        content.append('    ref->Actual_group(elaborator->bindAny(ref->VpiName()));')
        content.append('    if (!ref->Actual_group())')
        content.append('      if (parent->UhdmType() == uhdmref_obj) {')
        content.append('        ref->Actual_group((any*) ((ref_obj*) VpiParent())->Actual_group());')
        content.append('      }')
        content.append('  }')
    elif modeltype == 'obj_def':
        content.append('  clone->VpiParent(parent);')

    if 'BitSelect' in vpi_name:
        content.append('  if (any* n = elaborator->bindNet(VpiName())) {')
        content.append('    if (net* nn = any_cast<net*>(n))')
        content.append('      clone->VpiFullName(nn->VpiFullName());')
        content.append('  }')

    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            vpi = value.get('vpi')
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
                    content.append(f'  clone->{method}(elaborator->bindAny(VpiName()));')
                    content.append(f'  if (!clone->{method}()) clone->{method}((any*) this->{method}());')

                elif method in ['Task', 'Function']:
                    prefix = ''
                    if 'method_' in classname:
                        content.append(f'  const ref_obj* ref = any_cast<const ref_obj*> (clone->Prefix());')
                        content.append( '  const class_var* prefix = nullptr;')
                        content.append( '  if (ref) prefix = any_cast<const class_var*> (ref->Actual_group());')
                        prefix = ', prefix'
                    content.append(f'  if ({method.lower()}* t = any_cast<{method.lower()}*> (elaborator->bindTaskFunc(VpiName(){prefix}))) {{')
                    content.append(f'    clone->{method}(t);')
                    content.append( '  } else {')
                    content.append( '    elaborator->scheduleTaskFuncBinding(clone);')
                    content.append( '  }')

                elif classname == 'disable' and method == 'VpiExpr':
                    content.append(f'  if (auto obj = {method}()) clone->{method}((expr*) obj);')

                elif classname == 'int_typespec' and method == 'Cast_to_expr':
                    content.append(f'  if (auto obj = {method}()) clone->{method}((variables*) obj);')

                elif classname == 'function' and method == 'Return':
                    content.append(f'  if (auto obj = {method}()) clone->{method}((variables*) obj);')

                elif classname == 'class_typespec' and method == 'Class_defn':
                    content.append(f'  if (auto obj = {method}()) clone->{method}((class_defn*) obj);')

                elif method == 'Instance':
                    content.append(f'  if (auto obj = {method}()) clone->{method}((instance*) obj);')
                    content.append( '  if (instance* inst = any_cast<instance*>(parent))')
                    content.append( '    clone->Instance(inst);')

                elif method == 'Module':
                    content.append(f'  if (auto obj = {method}()) clone->{method}((module*) obj);')

                elif method == 'Typespec':
                    content.append( '  if (elaborator->uniquifyTypespec()) {')
                    content.append(f'    if (auto obj = {method}()) clone->{method}(obj->DeepClone(serializer, elaborator, clone));')
                    content.append( '  } else {')
                    content.append(f'    if (auto obj = {method}()) clone->{method}((typespec*) obj);')
                    content.append( '  }')

                else:
                    content.append(f'  if (auto obj = {method}()) clone->{method}(obj->DeepClone(serializer, elaborator, clone));')

            # N-ary relations
            elif method == 'Typespecs':
                content.append(f'  if (auto vec = {method}()) {{')
                content.append(f'    auto clone_vec = serializer->Make{Cast}Vec();')
                content.append(f'    clone->{method}(clone_vec);')
                content.append( '    for (auto obj : *vec) {')
                content.append( '      if (elaborator->uniquifyTypespec()) {')
                content.append( '        clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));')
                content.append( '      } else {')
                content.append( '        clone_vec->push_back(obj);')
                content.append( '      }')
                content.append( '    }')
                content.append( '  }')

            elif classname == 'class_defn' and method == 'Deriveds':
                # Don't deep clone
                content.append(f'  if (auto vec = {method}()) {{')
                content.append(f'    auto clone_vec = serializer->Make{Cast}Vec();')
                content.append(f'    clone->{method}(clone_vec);')
                content.append( '    for (auto obj : *vec) {')
                content.append( '      clone_vec->push_back(obj);')
                content.append( '    }')
                content.append( '  }')

            else:
                content.append(f'  if (auto vec = {method}()) {{')
                content.append(f'    auto clone_vec = serializer->Make{Cast}Vec();')
                content.append(f'    clone->{method}(clone_vec);')
                content.append( '    for (auto obj : *vec) {')
                content.append( '      clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));')
                content.append( '    }')
                content.append( '  }')
    content.append('}')
    content.append('')

    if '_call' in classname or classname in [ 'function', 'task', 'constant', 'tagged_pattern', 'gen_scope_array', 'hier_path', 'cont_assign' ]:
        return content  # Use hardcoded implementations of DeepClone

    if modeltype == 'obj_def':
        # DeepClone() not implemented for class_def; just declare to narrow the covariant return type.
        content.append(f'{classname}* {classname}::DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const {{')
        if 'Net' in vpi_name:
            content.append(f'  {classname}* clone = any_cast<{classname}*>(elaborator->bindNet(VpiName()));')
            content.append( '  if (clone != nullptr) {')
            content.append(f'    return clone;')
            content.append( '  }')
            content.append(f'  clone = serializer->Make{Classname}();')

        elif 'Parameter' in vpi_name:
            content.append(f'  {classname}* clone = any_cast<{classname}*>(elaborator->bindParam(VpiName()));')
            content.append( '  if (clone == nullptr) {')
            content.append(f'    clone = serializer->Make{Classname}();')
            content.append( '  }')

        else:
            content.append(f'  {classname}* const clone = serializer->Make{Classname}();')

        content.append('  const unsigned long id = clone->UhdmId();')
        content.append('  *clone = *this;')
        content.append('  clone->UhdmId(id);')
        content.append('  DeepCopy(clone, serializer, elaborator, parent);')
        content.append('  return clone;')
        content.append('}')
        content.append('')

    return content


def _get_GetByVpiName_implementation(model):
    classname = model['name']

    content = []
    content.append(f'const BaseClass* {classname}::GetByVpiName(std::string_view name) const {{')

    for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            name = value.get('name')
            vpi = value.get('vpi')
            card = value.get('card')

            if (card == 'any') and not name.endswith('s'):
                name += 's'

            if card == '1':
                content.append(f'  if (({name}_ != nullptr) && ({name}_->VpiName().compare(name) == 0)) {{')
                content.append(f'    return {name}_;')
                content.append( '  }')
            else:
                content.append(f'  if ({name}_ != nullptr) {{')
                content.append(f'    for (const BaseClass *ref : *{name}_) {{')
                content.append(f'      if (ref->VpiName().compare(name) == 0) return ref;')
                content.append( '    }')
                content.append( '  }')

    content.append(f'  return basetype_t::GetByVpiName(name);')
    content.append( '}')
    content.append( '')

    return content


def _get_GetByVpiType_implementation(model):
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
    content.append(f'std::tuple<const BaseClass*, UHDM_OBJECT_TYPE, const std::vector<const BaseClass*>*> {classname}::GetByVpiType(int type) const {{')

    if modeltype == 'obj_def' or case_bodies:
        content.append(f'  switch (type) {{')

    if modeltype == 'obj_def':
        content.append( '  case vpiParent: return std::make_tuple(vpiParent_, static_cast<UHDM_OBJECT_TYPE>(0), nullptr);')

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


def _get_GetVpiPropertyValue_implementation(model):
    classname = model['name']
    modeltype = model['type']

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
                if vpi in ['vpiFile', 'vpiDefFile']:
                    case_bodies[vpi] = [
                        f'    case {vpi}: {{',
                        f'      SymbolFactory::ID fileId = {Vpi_}Id();',
                        f'      if (fileId != SymbolFactory::getBadId()) {{',
                         '        const std::string& file = serializer_->symbolMaker.GetSymbol(fileId);',
                         '        if (!file.empty()) return vpi_property_value_t(file.c_str());',
                        f'      }}',
                        f'    }} break;'
                    ]
                elif type == 'string':
                    case_bodies[vpi] = [ f'    case {vpi}: {{' ]
                    if vpi == 'vpiFullName':
                        case_bodies[vpi].extend([
                            f'      const std::string &fullname = VpiFullName();',
                            f'      if (!fullname.empty() && (VpiName() != fullname)) {{',
                            f'        return vpi_property_value_t(fullname.c_str());',
                            f'      }}'
                          ])
                    else:
                        case_bodies[vpi].extend([
                            f'      const std::string &data = {Vpi_}();',
                            f'      if (!data.empty()) return vpi_property_value_t(data.c_str());'
                        ])
                    case_bodies[vpi].append(f'    }} break;')
                else:
                    case_bodies[vpi] = [ f'    case {vpi}: return vpi_property_value_t({Vpi_}());' ]

    if not type_specified and (modeltype == 'obj_def'):
        case_bodies['vpiType']= [ f'    case vpiType: return vpi_property_value_t(VpiType());' ]

    content = []
    content.append(f'{classname}::vpi_property_value_t {classname}::GetVpiPropertyValue(int property) const {{')

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
    classname = model['name']
    modeltype = model['type']

    content = [
        f'int {classname}::Compare(const BaseClass *const other, AnySet& visited) const {{',
         '  int r = 0;',
         '  if (!visited.insert(this).second) return r;',
         '',
         '  AnySet local;',
         '  if ((r = basetype_t::Compare(other, local)) != 0) return r;',
         '  visited.merge(local);',
         '',
         ''
    ]
    varDeclared = 0
    for key, value in model.allitems():
        if key not in ['property', 'obj_ref', 'class_ref']:
            continue

        vpi = value.get('vpi')
        if vpi in ['vpiParent', 'uhdmParentType', 'uhdmType', 'vpiLineNo', 'vpiColumnNo', 'vpiEndLineNo', 'vpiEndColumnNo', 'vpiFile', 'vpiFullName', 'uhdmId']:
            continue

        type = value.get('type')
        if type in ['value', 'delay']:
            continue

        name = value.get('name')
        card = value.get('card')
        Vpi_ = vpi[:1].upper() + vpi[1:]

        if varDeclared == 0:
            varDeclared = 1
            content.append(f'  const thistype_t *const lhs = this;')
            content.append(f'  const thistype_t *const rhs = (const thistype_t *)other;')
        if card == '1':
            if type == 'string':
                content.append(f'  if ((r = lhs->{Vpi_}().compare(rhs->{Vpi_}())) != 0) return r;')
            elif type in ['unsigned int', 'int']:
                content.append(f'  if ((r = lhs->{Vpi_}() - rhs->{Vpi_}()) != 0) return r;')
            elif type == 'bool':
                content.append(f'  if ((r = (lhs->{Vpi_}() == rhs->{Vpi_}()) ? 0 : (lhs->{Vpi_}() ? 1 : -1)) != 0) return r;')
            else:
                Name = name[:1].upper() + name[1:]
                content.extend([
                   '',
                  f'  auto lhs_{name} = lhs->{Name}();',
                  f'  auto rhs_{name} = rhs->{Name}();',
                  f'  if ((lhs_{name} != nullptr) && (rhs_{name} != nullptr)) {{',
                  f'    if ((r = lhs_{name}->Compare(rhs_{name}, local)) != 0) return r;',
                   '    visited.merge(local);',
                  f'  }} else if ((lhs_{name} != nullptr) && (rhs_{name} == nullptr)) {{',
                   '    return 1;',
                  f'  }} else if ((lhs_{name} == nullptr) && (rhs_{name} != nullptr)) {{',
                   '    return -1;',
                   '  }'
                ])
        else:
            if not name.endswith('s'):
                name += 's'

            Name = name[:1].upper() + name[1:]
            content.extend([
                 '',
                f'  auto lhs_{name} = lhs->{Name}();',
                f'  auto rhs_{name} = rhs->{Name}();',
                f'  if ((lhs_{name} != nullptr) && (rhs_{name} != nullptr)) {{',
                f'    if ((r = static_cast<int>(lhs_{name}->size() - rhs_{name}->size())) != 0) return r;',
                 '',
                f'    for (size_t i = 0, n = lhs_{name}->size(); i < n; ++i) {{',
                f'      if ((r = lhs_{name}->at(i)->Compare(rhs_{name}->at(i), local)) != 0) return r;',
                 '      visited.merge(local);',
                 '    }',
                f'  }} else if ((lhs_{name} != nullptr) && (rhs_{name} == nullptr)) {{',
                 '    return 1;',
                f'  }} else if ((lhs_{name} == nullptr) && (rhs_{name} != nullptr)) {{',
                 '    return -1;',
                 '  }',
            ])

    content.extend([
        '  return r;',
        '}',
        ''
    ])

    return content


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

    Classname_ = classname[:1].upper() + classname[1:]
    Classname = Classname_.replace('_', '')

    if modeltype != 'class_def':
        # Builtin properties do not need to be specified in each models
        # Builtins: "vpiParent, Parent type, vpiFile, Id" method and field
        data_members.extend(_get_data_member('BaseClass', 'vpiParent', '1'))
        declarations.extend(_get_declarations(classname, 'BaseClass', 'vpiParent', '1'))
        implementations.extend(_get_implementations(classname, 'BaseClass', 'vpiParent', '1'))

        data_members.extend(_get_data_member('unsigned int', 'uhdmParentType', '1'))
        declarations.extend(_get_declarations(classname, 'unsigned int', 'uhdmParentType', '1'))
        implementations.extend(_get_implementations(classname, 'unsigned int', 'uhdmParentType', '1'))

        data_members.extend(_get_data_member('string', 'vpiFile', '1'))
        declarations.extend(_get_declarations(classname, 'string','vpiFile', '1'))
        implementations.extend(_get_implementations(classname, 'string','vpiFile', '1'))

        data_members.extend(_get_data_member('unsigned int', 'uhdmId', '1'))
        declarations.extend(_get_declarations(classname, 'unsigned int', 'uhdmId', '1'))
        implementations.extend(_get_implementations(classname, 'unsigned int', 'uhdmId', '1'))

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
                declarations.append(f'  {type} {Vpi}() const final {{ return {value.get("vpiname")}; }}')
            else: # properties are already defined in vpi_user.h, no need to redefine them
                data_members.extend(_get_data_member(type, vpi, card))
                declarations.extend(_get_declarations(classname, type, vpi, card))
                implementations.extend(_get_implementations(classname, type, vpi, card))

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
                forward_declares.add(f'class {type};')

            group_headers.update(_get_group_headers(type, real_type))
            data_members.extend(_get_data_member(type, name, card))
            declarations.extend(_get_declarations(classname, type, name, card, real_type))
            implementations.extend(_get_implementations(classname, type, name, card, real_type))

    if not type_specified and (modeltype == 'obj_def'):
        vpiclasstype = config.make_vpi_name(classname)
        declarations.append(f'  virtual unsigned int VpiType() const final {{ return {vpiclasstype}; }}')

    if modeltype == 'class_def':
        # DeepClone() not implemented for class_def; just declare to narrow the covariant return type.
        declarations.append(f'  virtual {classname}* DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const override = 0;')
    else:
        return_type = 'tf_call' if '_call' in classname else classname
        declarations.append(f'  virtual {return_type}* DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const override;')

    declarations.append('  virtual const BaseClass* GetByVpiName(std::string_view name) const override;')
    declarations.append('  virtual std::tuple<const BaseClass*, UHDM_OBJECT_TYPE, const std::vector<const BaseClass*>*> GetByVpiType(int type) const override;')
    declarations.append('  virtual vpi_property_value_t GetVpiPropertyValue(int property) const override;')
    declarations.append('  virtual int Compare(const BaseClass* const other, AnySet& visited) const override;')

    implementations.extend(_get_clone_implementation(model, models))
    implementations.extend(_get_GetByVpiName_implementation(model))
    implementations.extend(_get_GetByVpiType_implementation(model))
    implementations.extend(_get_GetVpiPropertyValue_implementation(model))
    implementations.extend(_get_Compare_implementation(model))

    if modeltype == 'class_def':
        header_file_content = header_file_content.replace('<FINAL_CLASS>', '')
        header_file_content = header_file_content.replace('<FINAL_DESTRUCTOR>', '')
        header_file_content = header_file_content.replace('<VIRTUAL>', 'virtual ')
        header_file_content = header_file_content.replace('<OVERRIDE_OR_FINAL>', 'override')
        header_file_content = header_file_content.replace('<DISABLE_OBJECT_FACTORY>', '#if 0 // This class cannot be instantiated')
        header_file_content = header_file_content.replace('<END_DISABLE_OBJECT_FACTORY>', '#endif')
    else:
        header_file_content = header_file_content.replace('<FINAL_CLASS>', ' final')
        header_file_content = header_file_content.replace('<FINAL_DESTRUCTOR>', ' final')
        header_file_content = header_file_content.replace('<VIRTUAL>', 'virtual ')
        header_file_content = header_file_content.replace('<OVERRIDE_OR_FINAL>', 'final')
        header_file_content = header_file_content.replace('<DISABLE_OBJECT_FACTORY>', '')
        header_file_content = header_file_content.replace('<END_DISABLE_OBJECT_FACTORY>', '')

    header_file_content = header_file_content.replace('<EXTENDS>', 'BaseClass')
    header_file_content = header_file_content.replace('<CLASSNAME>', classname)
    header_file_content = header_file_content.replace('<UPPER_CLASSNAME>', classname.upper())
    header_file_content = header_file_content.replace('<METHODS>', '\n\n'.join(declarations))
    header_file_content = header_file_content.replace('<MEMBERS>', '\n\n'.join(data_members))
    header_file_content = header_file_content.replace('<GROUP_HEADER_DEPENDENCY>', '\n'.join(sorted(group_headers)))
    header_file_content = header_file_content.replace('<TYPE_FORWARD_DECLARE>', '\n'.join(sorted(forward_declares)))

    source_file_content = source_file_content.replace('<CLASSNAME>', classname)
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

    for model in models.values():
        classname = model['name']
        modeltype = model['type']

        if modeltype == 'group_def':
            _generate_group_checker(model, models, templates)
        else:
            _generate_one_class(model, models, templates)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
