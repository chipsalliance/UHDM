import os
from collections import OrderedDict

import config
import file_utils
import uhdm_types_h


def _print_methods(classname, type, vpi, card, real_type=''):
    content = []
    if card != '1':
        return content

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
    if type == 'any':
        check = f'if (!{real_type}GroupCompliant(data)) return false;'

    pointer = ''
    const = ''
    if type not in ['unsigned int', 'int', 'bool', 'std::string']:
        pointer = '*'
        const = 'const '

    Vpi_ = vpi[:1].upper() + vpi[1:]

    if type == 'std::string':
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
            content.append( '      if ((!name.empty()) && (!skip_name))')
            content.append( '        names.push_back(name);')
            content.append( '      child = parent;')
            content.append( '      parent = parent->VpiParent();')
            content.append( '    }')
            content.append( '    std::string fullName;')
            content.append( '    if (!names.empty()) {')
            content.append( '      unsigned int index = names.size() - 1;')
            content.append( '      while(1) {')
            content.append( '        fullName += names[index];')
            content.append( '        if (index > 0) fullName += column ? "::" : ".";')
            content.append( '        if (index == 0) break;')
            content.append( '        index--;')
            content.append( '      }')
            content.append( '    }')
            content.append( '    if (!fullName.empty()) {')
            content.append(f'      (({classname}*)this)->VpiFullName(fullName);')
            content.append( '    }')
            content.append(f'    return serializer_->symbolMaker.GetSymbol({vpi}_);')
            content.append( '  }')
            content.append( '}')
        else:
            content.append(f'const {type}{pointer}& {classname}::{Vpi_}() const {{ return serializer_->symbolMaker.GetSymbol({vpi}_); }}')
        content.append('')
        content.append(f'bool {classname}::{Vpi_}(const {type}{pointer}& data) {{ {vpi}_ = serializer_->symbolMaker.Make(data); return true; }}')
        content.append('')

    return content


def generate(models):
    methods = []
    saves = {}
    restores = {}

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]
        Classname = Classname_.replace('_', '')

        saves[classname] = []
        restores[classname] = []

        if modeltype != 'class_def':
            methods.extend(_print_methods(classname, 'BaseClass', 'vpiParent', '1'))
            methods.extend(_print_methods(classname, 'unsigned int', 'uhdmParentType', '1'))
            methods.extend(_print_methods(classname, 'string','vpiFile', '1'))
            methods.extend(_print_methods(classname, 'unsigned int', 'uhdmId', '1'))

            saves[classname].append(f'    {Classname}s[index].setVpiParent(GetId(obj->VpiParent()));')
            saves[classname].append(f'    {Classname}s[index].setUhdmParentType(obj->UhdmParentType());')
            saves[classname].append(f'    {Classname}s[index].setVpiFile(obj->GetSerializer()->symbolMaker.Make(obj->VpiFile()));')
            saves[classname].append(f'    {Classname}s[index].setVpiLineNo(obj->VpiLineNo());')
            saves[classname].append(f'    {Classname}s[index].setVpiColumnNo(obj->VpiColumnNo());')
            saves[classname].append(f'    {Classname}s[index].setVpiEndLineNo(obj->VpiEndLineNo());')
            saves[classname].append(f'    {Classname}s[index].setVpiEndColumnNo(obj->VpiEndColumnNo());')
            saves[classname].append(f'    {Classname}s[index].setUhdmId(obj->UhdmId());')

            restores[classname].append(f'    {classname}Maker.objects_[index]->UhdmParentType(obj.getUhdmParentType());')
            restores[classname].append(f'    {classname}Maker.objects_[index]->VpiParent(GetObject(obj.getUhdmParentType(), obj.getVpiParent()-1));')
            restores[classname].append(f'    {classname}Maker.objects_[index]->VpiFile(symbolMaker.GetSymbol(obj.getVpiFile()));')
            restores[classname].append(f'    {classname}Maker.objects_[index]->VpiLineNo(obj.getVpiLineNo());')
            restores[classname].append(f'    {classname}Maker.objects_[index]->VpiColumnNo(obj.getVpiColumnNo());')
            restores[classname].append(f'    {classname}Maker.objects_[index]->VpiEndLineNo(obj.getVpiEndLineNo());')
            restores[classname].append(f'    {classname}Maker.objects_[index]->VpiEndColumnNo(obj.getVpiEndColumnNo());')
            restores[classname].append(f'    {classname}Maker.objects_[index]->UhdmId(obj.getUhdmId());')

        indTmp = 0
        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                if name == 'type':
                    continue

                methods.extend(_print_methods(classname, type, vpi, card))

                Vpi_ = vpi[:1].upper() + vpi[1:]
                Vpi = Vpi_.replace('_', '')

                if type in ['string', 'value', 'delay']:
                    saves[classname].append(f'    {Classname}s[index].set{Vpi}(obj->GetSerializer()->symbolMaker.Make(obj->{Vpi_}()));')
                    restores[classname].append(f'    {classname}Maker.objects_[index]->{Vpi_}(symbolMaker.GetSymbol(obj.get{Vpi}()));')
                else:
                    saves[classname].append(f'    {Classname}s[index].set{Vpi}(obj->{Vpi_}());')
                    restores[classname].append(f'    {classname}Maker.objects_[index]->{Vpi_}(obj.get{Vpi}());')
            
            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')
                id = value.get('id')

                Type_ = type[:1].upper() + type[1:]
                Type = Type_.replace('_', '')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'

                Name_ = name[:1].upper() + name[1:]
                Name = Name_.replace('_', '')

                real_type = type
                if key == 'group_ref':
                    type = 'any'

                methods.extend(_print_methods(classname, type, name, card, real_type))

                if card == '1':
                    if key in ['class_ref', 'group_ref']:
                        saves[classname].append(f'    if (obj->{Name_}()) {{')
                        saves[classname].append(f'      ::ObjIndexType::Builder tmp{indTmp} = {Classname}s[index].get{Name}();')
                        saves[classname].append(f'      tmp{indTmp}.setIndex(GetId(((BaseClass*) obj->{Name_}())));')
                        saves[classname].append(f'      tmp{indTmp}.setType(((BaseClass*)obj->{Name_}())->UhdmType());')
                        saves[classname].append( '    }')
                        restores[classname].append(f'    {classname}Maker.objects_[index]->{Name_}(({type}*)GetObject(obj.get{Name}().getType(), obj.get{Name}().getIndex()-1));')
                        indTmp += 1
                    else:
                        saves[classname].append(f'    {Classname}s[index].set{Name}(GetId(obj->{Name_}()));')
                        restores[classname].append(f'    if (obj.get{Name}()) {{')
                        restores[classname].append(f'      {classname}Maker.objects_[index]->{Name_}({type}Maker.objects_[obj.get{Name}()-1]);')
                        restores[classname].append( '    }')

                else:
                    obj_key = '::ObjIndexType' if key in ['class_ref', 'group_ref'] else '::uint64_t'

                    saves[classname].append(f'    if (obj->{Name_}()) {{')
                    saves[classname].append(f'      ::capnp::List<{obj_key}>::Builder {Name}s = {Classname}s[index].init{Name}(obj->{Name_}()->size());')
                    saves[classname].append(f'      for (unsigned int ind = 0; ind < obj->{Name_}()->size(); ind++) {{')
                    restores[classname].append(f'    if (obj.get{Name}().size()) {{')
                    restores[classname].append(f'      std::vector<{type}*>* vect = {type}VectMaker.Make();')
                    restores[classname].append(f'      for (unsigned int ind = 0; ind < obj.get{Name}().size(); ind++) {{')

                    if key in ['class_ref', 'group_ref']:
                        saves[classname].append(f'        ::ObjIndexType::Builder tmp = {Name}s[ind];')
                        saves[classname].append(f'        tmp.setIndex(GetId(((BaseClass*) (*obj->{Name_}())[ind])));')
                        saves[classname].append(f'        tmp.setType(((BaseClass*)((*obj->{Name_}())[ind]))->UhdmType());')
                        restores[classname].append(f'        vect->push_back(({type}*)GetObject(obj.get{Name}()[ind].getType(), obj.get{Name}()[ind].getIndex()-1));')
                    else:
                        saves[classname].append(f'        {Name}s.set(ind, GetId((*obj->{Name_}())[ind]));')
                        restores[classname].append(f'        vect->push_back({type}Maker.objects_[obj.get{Name}()[ind]-1]);')

                    saves[classname].append('      }')
                    saves[classname].append('    }')
                    restores[classname].append( '      }')
                    restores[classname].append(f'      {classname}Maker.objects_[index]->{Name_}(vect);')
                    restores[classname].append( '    }')

        baseclass = model['extends']
        while baseclass:
            Baseclass = (baseclass[:1].upper() + baseclass[1:]).replace('_', '')

            saves[classname].extend([ line.replace(f' {Baseclass}s', f' {Classname}s') for line in saves[baseclass] ])
            restores[classname].extend([ line.replace(f' {baseclass}Maker', f' {classname}Maker') for line in restores[baseclass] ])

            # Parent class
            baseclass = models[baseclass]['extends']

    uhdm_name_map = [
        'std::string UhdmName(UHDM_OBJECT_TYPE type) {',
        '  switch (type) {'
    ]
    uhdm_name_map.extend([ f'  case {name} /* = {id} */: return "{name[4:]}";' for name, id in uhdm_types_h.get_type_map(models).items() ])
    uhdm_name_map.append('  default: return "NO TYPE";')
    uhdm_name_map.append('}')
    uhdm_name_map.append('}')
    uhdm_name_map.append('')

    factories = []
    factories_methods = []

    factory_purge = []
    factory_stats = []
    factory_object_type_map = []
    capnp_id = []
    capnp_save = []

    capnp_init_factories = []
    capnp_restore_factories = []

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]
        Classname = Classname_.replace('_', '')

        if modeltype != 'class_def':
            factories.append(f'    {classname}Factory {classname}Maker;')
            factories_methods.append(f'    {classname}* Make{Classname_}() {{ {classname}* tmp = {classname}Maker.Make(); tmp->SetSerializer(this); tmp->UhdmId(objId_++); return tmp; }}')
            factory_object_type_map.append(f'  case uhdm{classname}: return {classname}Maker.objects_[index];')

        factories.append(f'    VectorOf{classname}Factory {classname}VectMaker;')
        factories_methods.append(f'    std::vector<{classname}*>* Make{Classname_}Vec() {{ return {classname}VectMaker.Make();}}')

        if modeltype == 'class_def':
            continue

        capnp_id.append( '  index = 1;')
        capnp_id.append(f'  for (auto obj : {classname}Maker.objects_) {{')
        capnp_id.append( '    SetId(obj, index);')
        capnp_id.append( '    index++;')
        capnp_id.append( '  }')

        capnp_save.append(f'  ::capnp::List<{Classname}>::Builder {Classname}s = cap_root.initFactory{Classname}({classname}Maker.objects_.size());')
        capnp_save.append( '  index = 0;')
        capnp_save.append(f'  for (auto obj : {classname}Maker.objects_) {{')
        capnp_save.extend(saves[classname])
        capnp_save.append( '    index++;')
        capnp_save.append( '  }')

        capnp_init_factories.append(f'  ::capnp::List<{Classname}>::Reader {Classname}s = cap_root.getFactory{Classname}();')
        capnp_init_factories.append(f'  for (unsigned ind = 0; ind < {Classname}s.size(); ind++) {{')
        capnp_init_factories.append(f'    SetId(Make{Classname_}(), ind);')
        capnp_init_factories.append( '  }')
        capnp_init_factories.append( '')

        capnp_restore_factories.append( '  index = 0;')
        capnp_restore_factories.append(f'  for ({Classname}::Reader obj : {Classname}s) {{')
        capnp_restore_factories.extend(restores[classname])
        capnp_restore_factories.append( '    index++;')
        capnp_restore_factories.append( '  }')
        capnp_restore_factories.append( '')

        factory_purge.append(f'  for (auto obj : {classname}Maker.objects_) {{')
        factory_purge.append( '    delete obj;')
        factory_purge.append( '  }')
        factory_purge.append(f'  {classname}Maker.objects_.clear();')
        factory_purge.append('')

        factory_stats.append(f'  stats.insert(std::make_pair("{classname}", {classname}Maker.objects_.size()));')


    # Serializer.h
    with open(config.get_template_filepath('Serializer.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<FACTORIES>', '\n'.join(factories))
    file_content = file_content.replace('<FACTORIES_METHODS>', '\n'.join(factories_methods))
    file_utils.set_content_if_changed(config.get_output_header_filepath('Serializer.h'), file_content)

    # Serializer_save.cpp
    with open(config.get_template_filepath('Serializer_save.cpp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<METHODS_CPP>', '\n'.join(methods))
    file_content = file_content.replace('<UHDM_NAME_MAP>', '\n'.join(uhdm_name_map))
    file_content = file_content.replace('<FACTORY_PURGE>', '\n'.join(factory_purge))
    file_content = file_content.replace('<FACTORY_STATS>', '\n'.join(factory_stats))
    file_content = file_content.replace('<FACTORY_OBJECT_TYPE_MAP>', '\n'.join(factory_object_type_map))
    file_content = file_content.replace('<CAPNP_ID>', '\n'.join(capnp_id))
    file_content = file_content.replace('<CAPNP_SAVE>', '\n'.join(capnp_save))
    file_utils.set_content_if_changed(config.get_output_source_filepath('Serializer_save.cpp'), file_content)

    # Serializer_restore.cpp
    with open(config.get_template_filepath('Serializer_restore.cpp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_INIT_FACTORIES>', '\n'.join(capnp_init_factories))
    file_content = file_content.replace('<CAPNP_RESTORE_FACTORIES>', '\n'.join(capnp_restore_factories))
    file_utils.set_content_if_changed(config.get_output_source_filepath('Serializer_restore.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
