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

#include <uhdm/BaseClass.h>
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

namespace uhdm {
class ScopedScope;

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
  UHDM_NON_POSITIVE_VALUE = 731,
  UHDM_SIGNED_UNSIGNED_PORT_CONN = 732,
  UHDM_FORCING_UNSIGNED_TYPE = 733
};

#ifndef SWIG
using ErrorHandler =
    std::function<void(ErrorType errType, const std::string&,
                       const Any* object1, const Any* object2)>;

void DefaultErrorHandler(ErrorType errType, const std::string& errorMsg,
                         const Any* object1, const Any* object2);
#endif

class Factory final {
  friend Serializer;

  using objects_t = std::vector<Any*>;
  using collections_t = std::vector<objects_t*>;

 public:
  template <typename T, typename... Args>
  T* make(Args&&... args) {
    T* const any = new T(args...);
    m_objects.emplace_back(any);
    return any;
  }

  template <typename T>
  std::vector<T*>* makeCollection() {
    std::vector<T*>* const collection = new std::vector<T*>;
    m_collections.emplace_back((objects_t*)collection);
    return collection;
  }

  bool erase(const Any* any) {
    objects_t::iterator it = std::find(m_objects.begin(), m_objects.end(), any);
    if (it != m_objects.end()) {
      delete any;
      m_objects.erase(it);
      return true;
    }
    return false;
  }

  template <typename T>
  bool erase(const std::vector<T*>* collection) {
    collections_t::iterator it =
        std::find(m_collections.begin(), m_collections.end(),
                  static_cast<const std::vector<Any*>*>(collection));
    if (it != m_collections.end()) {
      delete collection;
      m_collections.erase(it);
      return true;
    }
    return false;
  }

  void eraseIfNotIn(const AnySet& container, AnySet& erased) {
    objects_t keepers;
    for (objects_t::reference any : m_objects) {
      if (container.find(any) == container.cend()) {
        erased.emplace(any);
        delete any;
      } else {
        keepers.emplace_back(any);
      }
    }
    keepers.swap(m_objects);
  }

  void mapToIndex(std::map<const Any*, uint32_t>& table,
                  uint32_t index = 1) const {
    for (objects_t::const_reference any : m_objects) {
      table.emplace(any, index++);
    }
  }

  void purge() {
    for (objects_t::reference any : m_objects) {
      delete any;
    }
    for (collections_t::reference collection : m_collections) {
      delete collection;
    }

    m_objects.clear();
    m_collections.clear();
  }

  const objects_t& getObjects() { return m_objects; }
  const objects_t& getObjects() const { return m_objects; }

  const collections_t& getCollections() { return m_collections; }
  const collections_t& getCollections() const { return m_collections; }

 private:
  objects_t m_objects;
  collections_t m_collections;
};

class Serializer final {
 public:
  using IdMap = std::map<const BaseClass*, uint32_t>;
  static constexpr uint32_t kBadIndex = static_cast<uint32_t>(-1);
  static const uint32_t kVersion;

  Serializer();
  ~Serializer();

#ifndef SWIG
  void save(const std::filesystem::path& filepath);
  void save(const std::string& filepath);
  void purge();

  void setGCEnabled(bool enabled) { m_enableGC = enabled; }
  void collectGarbage();

  void setErrorHandler(ErrorHandler handler) { m_errorHandler = handler; }
  ErrorHandler getErrorHandler() { return m_errorHandler; }

  IdMap getAllObjects() const;

  template <typename T>
  Factory* getFactory() {
    return m_factories[T::kUhdmType];
  }
#endif

  const std::vector<vpiHandle> restore(const std::filesystem::path& filepath);
  const std::vector<vpiHandle> restore(const std::string& filepath);
  std::map<std::string, uint32_t, std::less<>> getObjectStats() const;
  void printStats(std::ostream& strm, std::string_view infoText) const;

  void swap(const Any* what, Any* with);
  void swap(const std::map<const Any*, Any*>& replacements);

#ifndef SWIG
 private:
  template <typename T>
  T* make(Factory* const factory) {
    T *const obj = factory->template make<T>(this);
    obj->setUhdmId(++m_objId);
    return obj;
  }

  template <typename T>
  void make(Factory *const factory, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) make<T>(factory);
  }

  template <typename T>
  std::vector<T *> *makeCollection(Factory *const factory) {
    return factory->template makeCollection<T>();
  }

 public:
  template<typename T>
  T* make() {
    return make<T>(m_factories[T::kUhdmType]);
  }

  template <typename T>
  void make(uint32_t count) {
    make<T>(m_factories[T::kUhdmType], count);
  }

  template <typename T>
  std::vector<T *> *makeCollection() {
    return makeCollection<T>(m_factories[T::kUhdmType]);
  }

  template <typename T>
  T* clone(const T *source) {
    Factory *const factory = m_factories[T::kUhdmType];
    T* const target = factory->template make<T>(this, *source);
    target->setUhdmId(++m_objId);
    return target;
  }

  SymbolId makeSymbol(std::string_view symbol);
  std::string_view getSymbol(SymbolId id) const;
  SymbolId getSymbolId(std::string_view symbol) const;

  SymbolCollection* makeSymbolCollection();

  vpiHandle makeUhdmHandle(UhdmType type, const void* object);

  bool erase(const BaseClass* p);
  template<typename T>
  bool erase(const std::vector<T*>* collection) {
    return m_factories[T::kUhdmType]->template erase<T>(collection);
  }

#ifndef SWIG
  void pushScope(Any* s);
  bool popScope(Any* s);
  Any* topScope() const {
    return m_scopeStack.empty() ? nullptr : m_scopeStack.back();
  }

  Any* topDesign() const {
    for (auto it = m_scopeStack.rbegin(), end = m_scopeStack.rend(); it != end;
         ++it) {
      if ((*it)->getUhdmType() == UhdmType::Design) {
        return *it;
      }
    }
    return nullptr;
  }

  friend class ScopedScope;
#endif

  struct SaveAdapter;
  friend struct SaveAdapter;

  struct RestoreAdapter;
  friend struct RestoreAdapter;

 private:
  template<typename T>
  T* getObject(uint32_t type, uint32_t index) const;

  uint64_t m_version = 0;
  uint32_t m_objId = 0;
  bool m_enableGC = true;
  ErrorHandler m_errorHandler = DefaultErrorHandler;

  SymbolFactory m_symbolFactory;
  UhdmHandleFactory m_uhdmHandleFactory;

  using scope_stack_t = std::vector<Any *>;
  scope_stack_t m_scopeStack;

  using factories_t = std::map<UhdmType, Factory*>;
  factories_t m_factories;
#endif
};

#ifndef SWIG
class ScopedScope final {
 public:
  ScopedScope(Any* s);
  ~ScopedScope();

 private:
  Any* const m_any = nullptr;
};
#endif
} // namespace uhdm

#endif  // UHDM_SERIALIZER_H
