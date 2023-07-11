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

#ifndef UHDM_SERIALIZER_H
#define UHDM_SERIALIZER_H


#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <uhdm/containers.h>
#include <uhdm/vpi_uhdm.h>
#include <uhdm/SymbolFactory.h>

#define UHDM_MAX_BIT_WIDTH (1024*1024)

namespace UHDM {
enum ErrorType {
  UHDM_UNSUPPORTED_EXPR = 700,
  UHDM_UNSUPPORTED_STMT = 701,
  UHDM_WRONG_OBJECT_TYPE = 703,
  UHDM_UNDEFINED_PATTERN_KEY = 712,
  UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN = 713,
  UHDM_REAL_TYPE_AS_SELECT = 714,
  UHDM_RETURN_VALUE_VOID_FUNCTION = 715,
  UHDM_ILLEGAL_DEFAULT_VALUE = 716,
  UHDM_MULTIPLE_CONT_ASSIGN = 717,
  UHDM_ILLEGAL_WIRE_LHS = 718,
  UHDM_ILLEGAL_PACKED_DIMENSION = 719,
  UHDM_NON_SYNTHESIZABLE = 720,
  UHDM_ENUM_CONST_SIZE_MISMATCH = 721,
  UHDM_DIVIDE_BY_ZERO = 722,
  UHDM_INTERNAL_ERROR_OUT_OF_BOUND = 723,
  UHDM_UNDEFINED_USER_FUNCTION = 724,
  UHDM_UNRESOLVED_HIER_PATH = 725,
  UHDM_UNDEFINED_VARIABLE = 726,
  UHDM_INVALID_CASE_STMT_VALUE = 727,
  UHDM_UNSUPPORTED_TYPESPEC = 728
};

#ifndef SWIG
typedef std::function<void(ErrorType errType, const std::string&,
                           const any* object1, const any* object2)>
    ErrorHandler;

void DefaultErrorHandler(ErrorType errType, const std::string& errorMsg,
                         const any* object1, const any* object2);

template <typename T>
class FactoryT;
#endif

class Serializer final {
 public:
  using IdMap = std::map<const BaseClass*, uint32_t>;

  Serializer() : incrId_(0), objId_(0), errorHandler(DefaultErrorHandler) {}
  ~Serializer();

#ifndef SWIG
  void Save(const std::filesystem::path& filepath);
  void Save(const std::string& filepath);
  void Purge();
  void GarbageCollect();
  void SetErrorHandler(ErrorHandler handler) { errorHandler = handler; }
  ErrorHandler GetErrorHandler() { return errorHandler; }
  void MarkKeeper(const any* object) { keepers_.insert(object); }
#endif

  const std::vector<vpiHandle> Restore(const std::filesystem::path& filepath);
  const std::vector<vpiHandle> Restore(const std::string& filepath);
  std::map<std::string, uint32_t, std::less<>> ObjectStats() const;
  void PrintStats(std::ostream& strm, std::string_view infoText) const;

#ifndef SWIG
 private:
  template <typename T>
  T* Make(FactoryT<T>* const factory);

  template <typename T>
  std::vector<T*>* Make(FactoryT<std::vector<T*>>* const factory);

 public:
<FACTORY_FUNCTION_DECLARATIONS>
  std::vector<any*>* MakeAnyVec() { return anyVectMaker.Make(); }

  vpiHandle MakeUhdmHandle(UHDM_OBJECT_TYPE type, const void* object) {
    return uhdm_handleMaker.Make(type, object);
  }

  VectorOfanyFactory anyVectMaker;
  SymbolFactory symbolMaker;
  uhdm_handleFactory uhdm_handleMaker;
<FACTORY_DATA_MEMBERS>

  const IdMap& AllObjects() const {
    return allIds_;
  }

 private:
  template <typename T, typename = typename std::enable_if<
                            std::is_base_of<BaseClass, T>::value>::type>
  void SetSaveId_(FactoryT<T>* const factory);

  template <typename T, typename = typename std::enable_if<
                            std::is_base_of<BaseClass, T>::value>::type>
  void SetRestoreId_(FactoryT<T>* const factory, uint32_t count);

  template <typename T, typename = typename std::enable_if<
                            std::is_base_of<BaseClass, T>::value>::type>
  void GC_(FactoryT<T>* const factory);

  struct SaveAdapter;
  friend struct SaveAdapter;

  struct RestoreAdapter;
  friend struct RestoreAdapter;

 private:
  BaseClass* GetObject(uint32_t objectType, uint32_t index);
  void SetId(const BaseClass* p, uint32_t id);
  uint32_t GetId(const BaseClass* p);

  static const uint64_t kVersion;

  uint64_t version_ = 0;
  IdMap allIds_;
  uint32_t incrId_;  // Capnp id
  uint32_t objId_;   // ID for property annotations
  std::set<const any*> keepers_;
  ErrorHandler errorHandler;
#endif
};
};  // namespace UHDM

#endif
