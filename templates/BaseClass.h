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

#include <set>
#include <vector>

#include <uhdm/uhdm_types.h>
#include <uhdm/RTTI.h>

namespace UHDM {
  class Serializer;
  class ElaboratorListener;
  static std::string nonamebaseclass ("");

  class ClientData {
  public:
    virtual ~ClientData() = default;
  };

  class BaseClass : public RTTI {
    UHDM_IMPLEMENT_RTTI(BaseClass, RTTI)
    friend Serializer;

  public:
    // Use implicit constructor to initialize all members
    // BaseClass()

    virtual ~BaseClass() = default;

    Serializer* GetSerializer() const { return serializer_; }

    virtual UHDM_OBJECT_TYPE UhdmType() const = 0;

    virtual const BaseClass* VpiParent() const = 0;

    virtual bool VpiParent(BaseClass* data) = 0;

    virtual unsigned int UhdmParentType() const = 0;

    virtual bool UhdmParentType(unsigned int data) = 0;

    virtual const std::string& VpiFile() const = 0;

    virtual bool VpiFile(const std::string& data) = 0;

    virtual int VpiLineNo() const final { return vpiLineNo_; }

    virtual bool VpiLineNo(int data) final { vpiLineNo_ = data; return true; }

    virtual short int VpiColumnNo() const final { return vpiColumnNo_; }

    virtual bool VpiColumnNo(short int data) final { vpiColumnNo_ = data; return true; }

    virtual int VpiEndLineNo() const final { return vpiEndLineNo_; }

    virtual bool VpiEndLineNo(int data) final { vpiEndLineNo_ = data; return true; }

    virtual short int VpiEndColumnNo() const final { return vpiEndColumnNo_; }

    virtual bool VpiEndColumnNo(short int data) final { vpiEndColumnNo_ = data; return true; }

    virtual const std::string& VpiName() const { return nonamebaseclass; }

    virtual const std::string& VpiDefName() const { return nonamebaseclass; }

    virtual unsigned int VpiType() const = 0;

    ClientData* Data() const { return clientData_; }

    void Data(ClientData* data) { clientData_ = data; }

    virtual unsigned int UhdmId() const = 0;

    virtual bool UhdmId(unsigned int id) = 0;

    // Create a deep copy of this object.
    virtual BaseClass* DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const = 0;

  protected:
    void DeepCopy(BaseClass* clone, Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const;

    void SetSerializer(Serializer* serial) { serializer_ = serial; }

    Serializer* serializer_ = nullptr;

    ClientData* clientData_ = nullptr;

  private:
    int vpiLineNo_ = 0;
    int vpiEndLineNo_ = 0;
    short int vpiColumnNo_ = 0;
    short int vpiEndColumnNo_ = 0;

  };

  inline BaseClass* BaseClass::DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const {}
  inline void BaseClass::DeepCopy(BaseClass* clone, Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const {}

#ifdef STANDARD_VPI
  typedef std::set<vpiHandle> VisitedContainer;
#else
  typedef  std::set<const BaseClass*> VisitedContainer;
#endif

  template<typename T>
  class FactoryT final {
    friend Serializer;
    typedef std::vector<T *> objects_t;

    public:
      T* Make() {
        T* obj = new T;
        objects_.push_back(obj);
        return obj;
      }

      bool Erase(T* tps) {
        for (typename objects_t::const_iterator itr = objects_.begin();
             itr != objects_.end(); ++itr) {
          if ((*itr) == tps) {
            objects_.erase(itr);
            return true;
          }
        }
        return false;
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

}  // namespace UHDM

UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(any_cast, UHDM::BaseClass)

#endif
