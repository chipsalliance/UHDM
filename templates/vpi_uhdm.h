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
 * File:   vpi_uhdm.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef VPI_UHDM_H
#define VPI_UHDM_H

#include <unordered_map>

std::string UhdmName(unsigned int type);

namespace UHDM {
  class Serializer;  
};

struct uhdm_handle {
  uhdm_handle(unsigned int type, const void* object) :
    type(type), object(object), index(0) {}
  const unsigned int type;
  const void* object;
  unsigned int index;
};

class uhdm_handleFactory {
  friend UHDM::Serializer;
  public:
  vpiHandle Make(unsigned int type, const void* object) {
    uhdm_handle* obj = new uhdm_handle(type, object);
    objects_.push_back(obj);
    return (vpiHandle) obj;
  }
  private:
    std::vector<uhdm_handle*> objects_;
  };

namespace UHDM {
  typedef std::vector<std::string> Id2SymbolMap;
  typedef std::unordered_map<std::string, unsigned long> Symbol2IdMap;
  typedef void any;

  class BaseClass {
  public:
    // Use implicit constructor to initialize all members
    // BaseClass()
    
    virtual ~BaseClass(){}

    void SetSerializer(Serializer* serial) { serializer_ = serial; }

    Serializer* GetSerializer() { return serializer_; }

    virtual unsigned int UhdmType() const = 0;

    virtual const BaseClass* VpiParent() const = 0;

    virtual bool VpiParent(BaseClass* data) = 0;

    virtual unsigned int UhdmParentType() const = 0;

    virtual bool UhdmParentType(unsigned int data) = 0;

    virtual const std::string& VpiFile() const = 0;

    virtual bool VpiFile(std::string data) = 0;

    virtual unsigned int VpiLineNo() const = 0;

    virtual bool VpiLineNo(unsigned int data) = 0;

  protected:
    // This base class or any virtual class should not contain actual data fields that need
    // to be serialized. Only leaf classes contain serializable fields.
    // Exception, this runtime-only field that is not actually serialized: 
    Serializer* serializer_;
  };
  
  class SymbolFactory {
    friend Serializer;
  public:
    unsigned int Make(const std::string& symbol);
    const std::string& GetSymbol(unsigned int id);
    unsigned int GetId(const std::string& symbol);
  private:
    unsigned int idCounter_ = 0;
    std::string bad_symbol_ = "@@BAD_SYMBOL@@";
    Id2SymbolMap id2SymbolMap_;
    Symbol2IdMap symbol2IdMap_;
  };

  class VectorOfanyFactory {
  friend Serializer;
  public:
    std::vector<UHDM::any*>* Make() {
      std::vector<UHDM::any*>* obj = new std::vector<UHDM::any*>();
    objects_.push_back(obj);
    return obj;
  }
  private:
    std::vector<std::vector<UHDM::any*>*> objects_;
  };

};



#endif
