// -*- c++ -*-

/*

 Copyright 2019 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   Serializer.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef SERIALIZER_UHDM_H
#define SERIALIZER_UHDM_H

#include <string>
#include <vector>
#include <map>

#include <iostream>
#include <functional>
#include <uhdm/uhdm.h>

namespace UHDM {
  enum ErrorType {
    UHDM_WRONG_OBJECT_TYPE = 703,
    UHDM_UNDEFINED_PATTERN_KEY = 712,
    UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN = 713
  };

  typedef std::function<void(ErrorType errType, const std::string&, any* object)> ErrorHandler;

  void DefaultErrorHandler(ErrorType errType, const std::string& errorMsg, any* object);

  template<typename T>
  class FactoryT;

  class Serializer {
  public:
    Serializer() : incrId_(0), objId_(0), errorHandler(DefaultErrorHandler) {symbolMaker.Make("");}
    void Save(const std::string& file);
    void Purge();
    void SetErrorHandler(ErrorHandler handler) { errorHandler = handler; }
    ErrorHandler GetErrorHandler() { return errorHandler; }
    const std::vector<vpiHandle> Restore(const std::string& file);
    std::map<std::string_view, unsigned long> ObjectStats() const;

  private:
    template<typename T, typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    T *Make(FactoryT<T> *const factory) {
      T* const obj = factory->Make();
      obj->SetSerializer(this);
      obj->UhdmId(objId_++);
      return obj;
    }

    template<typename T, typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    std::vector<T*>* Make(FactoryT<std::vector<T*>> *const factory) {
      return factory->Make();
    }

  public:
<FACTORIES_METHODS>
    std::vector<any*>* MakeAnyVec() { return anyVectMaker.Make(); }
    vpiHandle MakeUhdmHandle(UHDM_OBJECT_TYPE type, const void* object) { return uhdm_handleMaker.Make(type, object); }

    VectorOfanyFactory anyVectMaker;
    SymbolFactory symbolMaker;
    uhdm_handleFactory uhdm_handleMaker;
<FACTORIES>

    std::unordered_map<const BaseClass*, unsigned long>& AllObjects() { return allIds_; }

  private:
    template<typename T, typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    void SetSaveId_(FactoryT<T>* const factory);

    template<typename T, typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    void SetRestoreId_(FactoryT<T>* const factory, unsigned long count);

    template<
      typename T, typename U,
      typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    struct AnySaveAdapter {};
    template<typename, typename, typename> friend struct AnySaveAdapter;

    template<
      typename T, typename U,
      typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    struct VectorOfanySaveAdapter {};
    template<typename, typename, typename> friend struct VectorOfanySaveAdapter;

    template<
      typename T, typename U,
      typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    struct AnyRestoreAdapter {};
    template<typename, typename, typename> friend struct AnyRestoreAdapter;

    template<
      typename T, typename U,
      typename = typename std::enable_if<std::is_base_of<BaseClass, T>::value>::type>
    struct VectorOfanyRestoreAdapter {};
    template<typename, typename, typename> friend struct VectorOfanyRestoreAdapter;

  private:
    BaseClass* GetObject(unsigned int objectType, unsigned int index);
    void SetId(const BaseClass* p, unsigned long id);
    unsigned long GetId(const BaseClass* p) ;
    std::unordered_map<const BaseClass*, unsigned long> allIds_;
    unsigned long incrId_; // Capnp id
    unsigned long objId_;  // ID for property annotations

    ErrorHandler errorHandler;
  };
};

#endif
