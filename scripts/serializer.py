import os

import config
import file_utils
import uhdm_types_h


def generate(models):
    factory_declarations = []
    factory_methods = []
    factory_purge = []
    factory_stats = []
    factory_object_type_map = []

    save_ids = []
    save_objects = []
    saves_adapters = []

    restore_ids = []
    restore_objects = []
    restore_adapters = []

    type_map = uhdm_types_h.get_type_map(models)

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]
        Classname = Classname_.replace('_', '')

        baseclass = model.get('extends', 'BaseClass') or 'BaseClass'

        if modeltype != 'class_def':
            factory_declarations.append(f'    {classname}Factory {classname}Maker;')
            factory_methods.append(f'    {classname}* Make{Classname_}() {{ return Make<{classname}>(&{classname}Maker); }}')
            factory_object_type_map.append(f'  case uhdm{classname} /* = {type_map["uhdm" + classname]} */: return {classname}Maker.objects_[index];')

            save_ids.append(f'  SetSaveId_(&{classname}Maker);')
            save_objects.append(f'  VectorOfanySaveAdapter<{classname}, {Classname}>()({classname}Maker.objects_, this, cap_root.initFactory{Classname}({classname}Maker.objects_.size()));')

            restore_ids.append(f'  SetRestoreId_(&{classname}Maker, cap_root.getFactory{Classname}().size());')
            restore_objects.append(f'  VectorOfanyRestoreAdapter<{classname}, {Classname}>()(cap_root.getFactory{Classname}(), this, {classname}Maker.objects_);')

            factory_purge.append(f'  {classname}Maker.Purge();')
            factory_stats.append(f'  stats.insert(std::make_pair("{classname}", {classname}Maker.objects_.size()));')

        factory_declarations.append(f'    VectorOf{classname}Factory {classname}VectMaker;')
        factory_methods.append(f'    std::vector<{classname}*>* Make{Classname_}Vec() {{ return Make<{classname}>(&{classname}VectMaker); }}')

        saves_adapters.append(f'template<typename U>')
        saves_adapters.append(f'struct Serializer::AnySaveAdapter<{classname}, U> {{')
        saves_adapters.append(f'  void operator()(const {classname} *const obj, Serializer *const serializer, U builder) const {{')
        saves_adapters.append(f'    AnySaveAdapter<{baseclass}, U>()(static_cast<const {baseclass}*>(obj), serializer, builder);')

        restore_adapters.append(f'template<typename U>')
        restore_adapters.append(f'struct Serializer::AnyRestoreAdapter<{classname}, U> {{')
        restore_adapters.append(f'  void operator()(U reader, Serializer *const serializer, {classname} *const obj) const {{')
        restore_adapters.append(f'    AnyRestoreAdapter<{baseclass}, U>()(reader, serializer, static_cast<{baseclass}*>(obj));')

        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                if name == 'type':
                    continue

                Vpi_ = vpi[:1].upper() + vpi[1:]
                Vpi = Vpi_.replace('_', '')

                if vpi in ['vpiFile', 'vpiDefFile']:
                    saves_adapters.append(f'    builder.set{Vpi}(serializer->symbolMaker.Make(obj->{Vpi_}().string()));')
                    restore_adapters.append(f'    obj->{Vpi_}(serializer->symbolMaker.GetSymbol(reader.get{Vpi}()));')
                elif type in ['string', 'value', 'delay']:
                    saves_adapters.append(f'    builder.set{Vpi}(serializer->symbolMaker.Make(obj->{Vpi_}()));')
                    restore_adapters.append(f'    obj->{Vpi_}(serializer->symbolMaker.GetSymbol(reader.get{Vpi}()));')
                else:
                    saves_adapters.append(f'    builder.set{Vpi}(obj->{Vpi_}());')
                    restore_adapters.append(f'    obj->{Vpi_}(reader.get{Vpi}());')
            
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

                if card == '1':
                    if key in ['class_ref', 'group_ref']:
                        saves_adapters.append(f'    if (obj->{Name_}() != nullptr) {{')
                        saves_adapters.append(f'      ::ObjIndexType::Builder tmp = builder.get{Name}();')
                        saves_adapters.append(f'      tmp.setIndex(serializer->GetId(obj->{Name_}()));')
                        saves_adapters.append(f'      tmp.setType((obj->{Name_}())->UhdmType());')
                        saves_adapters.append( '    }')

                        restore_adapters.append(f'    obj->{Name_}(({type}*)serializer->GetObject(reader.get{Name}().getType(), reader.get{Name}().getIndex() - 1));')
                    else:
                        saves_adapters.append(f'    builder.set{Name}(serializer->GetId(obj->{Name_}()));')

                        restore_adapters.append(f'    if (reader.get{Name}()) {{')
                        restore_adapters.append(f'      obj->{Name_}(serializer->{type}Maker.objects_[reader.get{Name}() - 1]);')
                        restore_adapters.append( '    }')

                else:
                    obj_key = '::ObjIndexType' if key in ['class_ref', 'group_ref'] else '::uint64_t'

                    saves_adapters.append(f'    if (obj->{Name_}() != nullptr) {{')
                    saves_adapters.append(f'      ::capnp::List<{obj_key}>::Builder {Name}s = builder.init{Name}(obj->{Name_}()->size());')
                    saves_adapters.append(f'      for (unsigned int i = 0, n = obj->{Name_}()->size(); i < n; ++i) {{')

                    restore_adapters.append(f'    if (reader.get{Name}().size() > 0) {{')
                    restore_adapters.append(f'      std::vector<{type}*>* vect = serializer->{type}VectMaker.Make();')
                    restore_adapters.append(f'      for (unsigned int i = 0, n = reader.get{Name}().size(); i < n; ++i) {{')

                    if key in ['class_ref', 'group_ref']:
                        saves_adapters.append(f'        ::ObjIndexType::Builder tmp = {Name}s[i];')
                        saves_adapters.append(f'        tmp.setIndex(serializer->GetId((*obj->{Name_}())[i]));')
                        saves_adapters.append(f'        tmp.setType(((BaseClass*)((*obj->{Name_}())[i]))->UhdmType());')

                        restore_adapters.append(f'        vect->push_back(({type}*)serializer->GetObject(reader.get{Name}()[i].getType(), reader.get{Name}()[i].getIndex() - 1));')
                    else:
                        saves_adapters.append(f'        {Name}s.set(i, serializer->GetId((*obj->{Name_}())[i]));')

                        restore_adapters.append(f'        vect->push_back(serializer->{type}Maker.objects_[reader.get{Name}()[i] - 1]);')

                    saves_adapters.append('      }')
                    saves_adapters.append('    }')

                    restore_adapters.append( '      }')
                    restore_adapters.append(f'      obj->{Name_}(vect);')
                    restore_adapters.append( '    }')

        saves_adapters.append('  }')
        saves_adapters.append('};')
        saves_adapters.append('')

        restore_adapters.append('  }')
        restore_adapters.append('};')
        restore_adapters.append('')

    uhdm_name_map = [
        'std::string UhdmName(UHDM_OBJECT_TYPE type) {',
        '  switch (type) {'
    ]
    uhdm_name_map.extend([ f'  case {name} /* = {id} */: return "{name[4:]}";' for name, id in uhdm_types_h.get_type_map(models).items() ])
    uhdm_name_map.append('  default: return "NO TYPE";')
    uhdm_name_map.append('}')
    uhdm_name_map.append('}')

    # Serializer.h
    with open(config.get_template_filepath('Serializer.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<FACTORIES>', '\n'.join(factory_declarations))
    file_content = file_content.replace('<FACTORIES_METHODS>', '\n'.join(factory_methods))
    file_utils.set_content_if_changed(config.get_output_header_filepath('Serializer.h'), file_content)

    # Serializer.cpp
    with open(config.get_template_filepath('Serializer.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<METHODS_CPP>', '') # Deprecated
    file_content = file_content.replace('<UHDM_NAME_MAP>', '\n'.join(uhdm_name_map))
    file_content = file_content.replace('<FACTORY_PURGE>', '\n'.join(factory_purge))
    file_content = file_content.replace('<FACTORY_STATS>', '\n'.join(factory_stats))
    file_content = file_content.replace('<FACTORY_OBJECT_TYPE_MAP>', '\n'.join(factory_object_type_map))
    file_utils.set_content_if_changed(config.get_output_source_filepath('Serializer.cpp'), file_content)

    # Serializer_save.cpp
    with open(config.get_template_filepath('Serializer_save.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_ID>', '\n'.join(save_ids))
    file_content = file_content.replace('<CAPNP_SAVE>', '\n'.join(save_objects))
    file_content = file_content.replace('<CAPNP_SAVE_ADAPTERS>', '\n'.join(saves_adapters))
    file_utils.set_content_if_changed(config.get_output_source_filepath('Serializer_save.cpp'), file_content)

    # Serializer_restore.cpp
    with open(config.get_template_filepath('Serializer_restore.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_INIT_FACTORIES>', '\n'.join(restore_ids))
    file_content = file_content.replace('<CAPNP_RESTORE_FACTORIES>', '\n'.join(restore_objects))
    file_content = file_content.replace('<CAPNP_RESTORE_ADAPTERS>', '\n'.join(restore_adapters))
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
