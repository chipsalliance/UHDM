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

#ifndef UHDM_BASE_CLASS_H
#define UHDM_BASE_CLASS_H

#include <uhdm/RTTI.h>
#include <uhdm/SymbolFactory.h>
#include <uhdm/uhdm_types.h>

#include <deque>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace UHDM {
class BaseClass;
class Serializer;
static inline constexpr std::string_view kEmpty("");

#ifdef STANDARD_VPI
typedef std::set<vpiHandle> VisitedContainer;
#else
typedef std::set<const BaseClass*> VisitedContainer;
#endif
typedef std::set<const BaseClass*> AnySet;

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

class CompareContext : public RTTI {
  UHDM_IMPLEMENT_RTTI(CompareContext, RTTI)

 public:
  CompareContext() = default;
  virtual ~CompareContext() = default;

  AnySet m_visited;
  const BaseClass* m_failedLhs = nullptr;
  const BaseClass* m_failedRhs = nullptr;

 private:
  CompareContext(const CompareContext& rhs) = delete;
  CompareContext& operator=(const CompareContext& rhs) = delete;
};

class BaseClass : public RTTI {
  UHDM_IMPLEMENT_RTTI(BaseClass, RTTI)
  friend Serializer;

 public:
  // Use implicit constructor to initialize all members
  // BaseClass()

  virtual ~BaseClass() = default;

  Serializer* GetSerializer() const { return serializer_; }

  uint32_t UhdmId() const { return uhdmId_; }
  bool UhdmId(uint32_t data) {
    uhdmId_ = data;
    return true;
  }

  BaseClass* VpiParent() { return vpiParent_; }
  const BaseClass* VpiParent() const { return vpiParent_; }
  template <typename T>
  T* VpiParent() {
    return (vpiParent_ == nullptr) ? nullptr : vpiParent_->template Cast<T*>();
  }
  template <typename T>
  const T* VpiParent() const {
    return (vpiParent_ == nullptr) ? nullptr
                                   : vpiParent_->template Cast<const T*>();
  }
  bool VpiParent(BaseClass* data) {
    vpiParent_ = data;
    return true;
  }

  std::string_view VpiFile() const;
  bool VpiFile(std::string_view data);

  uint32_t VpiLineNo() const { return vpiLineNo_; }
  bool VpiLineNo(uint32_t data) {
    vpiLineNo_ = data;
    return true;
  }

  uint16_t VpiColumnNo() const { return vpiColumnNo_; }
  bool VpiColumnNo(uint16_t data) {
    vpiColumnNo_ = data;
    return true;
  }

  uint32_t VpiEndLineNo() const { return vpiEndLineNo_; }
  bool VpiEndLineNo(uint32_t data) {
    vpiEndLineNo_ = data;
    return true;
  }

  uint16_t VpiEndColumnNo() const { return vpiEndColumnNo_; }
  bool VpiEndColumnNo(uint16_t data) {
    vpiEndColumnNo_ = data;
    return true;
  }

  virtual std::string_view VpiName() const { return kEmpty; }
  virtual std::string_view VpiDefName() const { return kEmpty; }

  virtual uint32_t VpiType() const = 0;
  virtual UHDM_OBJECT_TYPE UhdmType() const = 0;

  ClientData* Data() { return clientData_; }
  const ClientData* Data() const { return clientData_; }
  void Data(ClientData* data) { clientData_ = data; }

  // TODO: Make the next three functions pure-virtual after transition to pygen.
  virtual const BaseClass* GetByVpiName(std::string_view name) const;

  virtual std::tuple<const BaseClass*, UHDM_OBJECT_TYPE,
                     const std::vector<const BaseClass*>*>
  GetByVpiType(int32_t type) const;

  typedef std::variant<int64_t, const char*> vpi_property_value_t;
  virtual vpi_property_value_t GetVpiPropertyValue(int32_t property) const;

  // Create a deep copy of this object.
  virtual BaseClass* DeepClone(BaseClass* parent,
                               CloneContext* context) const = 0;

  virtual int32_t Compare(const BaseClass* other,
                          CompareContext* context) const;

 protected:
  void DeepCopy(BaseClass* clone, BaseClass* parent,
                CloneContext* context) const;

  std::string ComputeFullName() const;

  void SetSerializer(Serializer* serial) { serializer_ = serial; }

  static int32_t SafeCompare(const BaseClass* lhs, const BaseClass* rhs,
                             CompareContext* context) {
    if ((lhs != nullptr) && (rhs != nullptr)) {
      return lhs->Compare(rhs, context);
    } else if ((lhs != nullptr) && (rhs == nullptr)) {
      context->m_failedLhs = lhs;
      return 1;
    } else if ((lhs == nullptr) && (rhs != nullptr)) {
      context->m_failedRhs = rhs;
      return -1;
    }
    return 0;
  }

  template <typename T>
  static int32_t SafeCompare(const BaseClass* lhs_obj,
                             const std::vector<T*>* lhs,
                             const BaseClass* rhs_obj,
                             const std::vector<T*>* rhs,
                             CompareContext* context) {
    if ((lhs != nullptr) && (rhs != nullptr)) {
      int32_t r = 0;
      if ((r = static_cast<int32_t>(lhs->size() - rhs->size())) != 0) {
        context->m_failedLhs = lhs_obj;
        context->m_failedRhs = rhs_obj;
        return 1;
      }
      for (size_t i = 0, n = lhs->size(); i < n; ++i) {
        if ((r = SafeCompare(lhs->at(i), rhs->at(i), context)) != 0) return r;
      }
    } else if ((lhs != nullptr) && !lhs->empty() && (rhs == nullptr)) {
      context->m_failedLhs = lhs_obj;
      return 1;
    } else if ((lhs == nullptr) && (rhs != nullptr) && !rhs->empty()) {
      context->m_failedRhs = rhs_obj;
      return -1;
    }
    return 0;
  }

 protected:
  Serializer* serializer_ = nullptr;
  ClientData* clientData_ = nullptr;

  uint32_t uhdmId_ = 0;
  BaseClass* vpiParent_ = nullptr;
  SymbolId vpiFile_;

  uint32_t vpiLineNo_ = 0;
  uint32_t vpiEndLineNo_ = 0;
  uint16_t vpiColumnNo_ = 0;
  uint16_t vpiEndColumnNo_ = 0;
};

template <typename T>
class FactoryT final {
  friend Serializer;
  // TODO: make this an arena: iinstead of pointers, store the
  // objects directly with placement-new'ed object using emplace_back()
  // One less indirection and a lot less overhead.
  // (dqque guarantees pointer stability; however, we'd need to
  // do something special in Erase() - can't re-arrange).
  typedef std::deque<T*> objects_t;

 public:
  T* Make() {
    T* obj = new T;
    objects_.push_back(obj);
    return obj;
  }

  bool Erase(const T* obj) {
    for (typename objects_t::const_iterator itr = objects_.begin();
         itr != objects_.end(); ++itr) {
      if ((*itr) == obj) {
        delete obj;
        objects_.erase(itr);
        return true;
      }
    }
    return false;
  }

  void EraseIfNotIn(const AnySet &container) {
    objects_t keepers;
    for (typename objects_t::reference obj : objects_) {
      if (container.find(obj) == container.end()) {
        delete obj;
      } else {
        keepers.emplace_back(obj);
      }
    }
    keepers.swap(objects_);
  }

  void MapToIndex(std::map<const BaseClass*, uint32_t>& table, uint32_t index = 1) const {
    for (typename objects_t::const_reference obj : objects_) {
      table.emplace(obj, index++);
    }
  }

  void Purge() {
    for (typename objects_t::reference obj : objects_) {
      delete obj;
    }
    objects_.clear();
  }

 private:
  objects_t objects_;
};

typedef FactoryT<std::vector<BaseClass*>> VectorOfBaseClassFactory;
typedef FactoryT<std::vector<BaseClass*>> VectorOfanyFactory;

}  // namespace UHDM

UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(clonecontext_cast, UHDM::CloneContext)
UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(comparecontext_cast, UHDM::CompareContext)
UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(any_cast, UHDM::BaseClass)

#endif
