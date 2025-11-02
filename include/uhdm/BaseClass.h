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
 * File:   BaseClass.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_BASECLASS_H
#define UHDM_BASECLASS_H

#include <uhdm/RTTI.h>
#include <uhdm/SymbolFactory.h>
#include <uhdm/uhdm_types.h>

#include <algorithm>
#include <map>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace uhdm {
class BaseClass;
class Comment;
class Serializer;
class UhdmComparer;

#ifndef SWIG
static inline constexpr std::string_view kEmpty("");
#endif

using AnySet = std::set<const BaseClass*>;
using CommentCollection = std::vector<Comment*>;

class ClientData {
 public:
  virtual ~ClientData() = default;
};

class CloneContext : public RTTI {
  UHDM_IMPLEMENT_RTTI(CloneContext, RTTI)

 public:
  CloneContext(Serializer* serializer) : m_serializer(serializer) {}
  virtual ~CloneContext() = default;

  Serializer* const m_serializer = nullptr;

 private:
  CloneContext(const CloneContext& rhs) = delete;
  CloneContext& operator=(const CloneContext& rhs) = delete;
};

class BaseClass : public RTTI {
  UHDM_IMPLEMENT_RTTI(BaseClass, RTTI)
  friend Serializer;

 public:
  static constexpr UhdmType kUhdmType = UhdmType::BaseClass;

  // Use implicit constructor to initialize all members
  // BaseClass()

  virtual ~BaseClass() = default;

  Serializer* getSerializer() const { return m_serializer; }

  uint32_t getUhdmId() const { return m_uhdmId; }
  bool setUhdmId(uint32_t data) {
    m_uhdmId = data;
    return true;
  }

  BaseClass* getParent() { return m_parent; }
  const BaseClass* getParent() const { return m_parent; }
  template <typename T>
  T* getParent() {
    return (m_parent == nullptr) ? nullptr : m_parent->template Cast<T>();
  }
  template <typename T>
  const T* getParent() const {
    return (m_parent == nullptr) ? nullptr : m_parent->template Cast<T>();
  }
  virtual bool setParent(BaseClass* data, bool force = false);

  std::string_view getFile() const;
  bool setFile(std::string_view data);

  uint32_t getStartLine() const { return m_startLine; }
  bool setStartLine(uint32_t data) {
    m_startLine = data;
    return true;
  }

  uint16_t getStartColumn() const { return m_startColumn; }
  bool setStartColumn(uint16_t data) {
    m_startColumn = data;
    return true;
  }

  uint32_t getEndLine() const { return m_endLine; }
  bool setEndLine(uint32_t data) {
    m_endLine = data;
    return true;
  }

  uint16_t getEndColumn() const { return m_endColumn; }
  bool setEndColumn(uint16_t data) {
    m_endColumn = data;
    return true;
  }

  virtual std::string_view getName() const { return kEmpty; }
  virtual std::string_view getDefName() const { return kEmpty; }

  virtual uint32_t getVpiType() const = 0;
  virtual UhdmType getUhdmType() const = 0;

  ClientData* getClientData() { return m_clientData; }
  const ClientData* getClientData() const { return m_clientData; }
  void setClientData(ClientData* data) { m_clientData = data; }

  // TODO: Make the next three functions pure-virtual after transition to pygen.
  virtual const BaseClass* getByVpiName(std::string_view name) const;

  using get_by_vpi_type_return_t =
      std::tuple<UhdmType, const BaseClass*,
                 const std::vector<const BaseClass*>*>;
  virtual get_by_vpi_type_return_t getByVpiType(int32_t type) const;

  using vpi_property_value_t = std::variant<int64_t, const char*>;
  virtual vpi_property_value_t getVpiPropertyValue(int32_t property) const;

  // Create a deep copy of this object.
  virtual BaseClass* deepClone(BaseClass* parent,
                               CloneContext* context) const = 0;

  virtual int32_t compare(const BaseClass* other,
                          UhdmComparer* comparer) const;

 protected:
  void deepCopy(BaseClass* clone, BaseClass* parent,
                CloneContext* context) const;

  std::string computeFullName() const;

  void setSerializer(Serializer* serializer) { m_serializer = serializer; }

  virtual void swap(const BaseClass* what, BaseClass* with);
  virtual void swap(const std::map<const BaseClass*, BaseClass*>& replacements);

  template <typename T>
  static void swapT(std::vector<T*>& collection, const BaseClass* what,
                    BaseClass* with) {
    auto it = std::find(collection.begin(), collection.end(), what);
    if (it != collection.end()) {
      if (with == nullptr) {
        collection.erase(it);
      } else if (T* const withT = with->template Cast<T>()) {
        *it = withT;
      } else {
        collection.erase(it);
      }
    }
  }

  template <typename T>
  static void swapT(
      std::vector<T*>& collection,
      const std::map<const BaseClass*, BaseClass*>& replacements) {
    if (!std::any_of(collection.cbegin(), collection.cend(),
                     [&replacements](const BaseClass* const any) {
                       return replacements.find(any) != replacements.cend();
                     })) {
      return;
    }

    AnySet unique;
    const std::vector<T*> ordered = std::move(collection);
    for (auto whatT : ordered) {
      if (auto it = replacements.find(whatT); it != replacements.cend()) {
        if (it->second != nullptr) {
          if (T* const withT = it->second->template Cast<T>()) {
            if (unique.emplace(it->second).second) {
              collection.emplace_back(withT);
            }
          } else {
            if (unique.emplace(whatT).second) collection.emplace_back(whatT);
          }
        }
      } else {
        if (unique.emplace(whatT).second) collection.emplace_back(whatT);
      }
    }
  }

  virtual void onChildAdded(BaseClass* child) {}
  virtual void onChildRemoved(BaseClass* child) {}

 protected:
  Serializer* m_serializer = nullptr;
  ClientData* m_clientData = nullptr;
  CommentCollection* m_comments = nullptr;

  uint32_t m_uhdmId = 0;
  BaseClass* m_parent = nullptr;
  SymbolId m_fileId = BadSymbolId;

  uint32_t m_startLine = 0;
  uint32_t m_endLine = 0;
  uint16_t m_startColumn = 0;
  uint16_t m_endColumn = 0;
};

using Any = BaseClass;
}  // namespace uhdm

UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(clonecontext_cast, uhdm::CloneContext)
UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(any_cast, uhdm::BaseClass)

#endif  // UHDM_BASECLASS_H
