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

#include <uhdm/SymbolFactory.h>
#include <uhdm/containers.h>
#include <uhdm/vpi_uhdm.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#define UHDM_MAX_BIT_WIDTH (1024 * 1024)

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
  UHDM_UNSUPPORTED_TYPESPEC = 728,
  UHDM_UNRESOLVED_PROPERTY = 729,
  UHDM_NON_TEMPORAL_SEQUENCE_USE = 730,
  UHDM_NON_POSITIVE_VALUE = 731
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
  static constexpr uint32_t kBadIndex = static_cast<uint32_t>(-1);
  static const uint32_t kVersion;

  Serializer() = default;
  ~Serializer();

#ifndef SWIG
  void Save(const std::filesystem::path& filepath);
  void Save(const std::string& filepath);
  void Purge();

  void SetGCEnabled(bool enabled) { m_enableGC = enabled; }
  void GarbageCollect();

  void SetErrorHandler(ErrorHandler handler) { m_errorHandler = handler; }
  ErrorHandler GetErrorHandler() { return m_errorHandler; }

  IdMap AllObjects() const;
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
  void Make(FactoryT<T>* const factory, uint32_t count);

  template <typename T>
  std::vector<T*>* Make(FactoryT<std::vector<T*>>* const factory);

 public:
  <FACTORY_FUNCTION_DECLARATIONS> std::vector<any*>* MakeAnyVec() {
    return anyVectMaker.Make();
  }

  SymbolId MakeSymbol(std::string_view symbol);
  std::string_view GetSymbol(SymbolId id) const;
  SymbolId GetSymbolId(std::string_view symbol) const;

  vpiHandle MakeUhdmHandle(UHDM_OBJECT_TYPE type, const void* object);

  bool Erase(const BaseClass* p);

 private:
  struct SaveAdapter;
  friend struct SaveAdapter;

  struct RestoreAdapter;
  friend struct RestoreAdapter;

 private:
  BaseClass* GetObject(uint32_t objectType, uint32_t index) const;

  uint64_t m_version = 0;
  uint32_t m_objId = 0;
  bool m_enableGC = true;
  ErrorHandler m_errorHandler = DefaultErrorHandler;

  VectorOfanyFactory anyVectMaker;
  SymbolFactory symbolMaker;
  uhdm_handleFactory uhdm_handleMaker;
<FACTORY_DATA_MEMBERS>
#endif
};
};  // namespace UHDM

#endif
