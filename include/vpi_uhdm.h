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
  typedef std::vector<std::string> Id2SymbolMap;
  typedef std::unordered_map<std::string, unsigned long> Symbol2IdMap;
  typedef void any;
  class BaseClass {
  public: 
    //BaseClass(){}
    virtual unsigned int UhdmType() = 0;   
    virtual ~BaseClass(){}
  };
  
  class Serializer {
  public:
    static void Save(std::string file);
    static void Purge();
    static const std::vector<vpiHandle> Restore(std::string file);
  private:
    static BaseClass* GetObject(unsigned int objectType, unsigned int index);
    static void SetId(BaseClass* p, unsigned long id);
    static unsigned long GetId(BaseClass* p) ;
    static std::unordered_map<BaseClass*, unsigned long> allIds_;
  };

  class SymbolFactory {
    friend Serializer;
  public:
    static unsigned int Make(const std::string& symbol);
    static const std::string& GetSymbol(unsigned int id);
    static unsigned int GetId(const std::string& symbol);
  private:
    static unsigned int idCounter_;
    static std::string bad_symbol_;
    static Id2SymbolMap id2SymbolMap_;
    static Symbol2IdMap symbol2IdMap_;

  };

  class VectorOfanyFactory {
  friend Serializer;
  public:
    static std::vector<UHDM::any*>* Make() {
      std::vector<UHDM::any*>* obj = new std::vector<UHDM::any*>();
    objects_.push_back(obj);
    return obj;
  }
  private:
    static std::vector<std::vector<UHDM::any*>*> objects_;
  };
  
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
  static vpiHandle Make(unsigned int type, const void* object) {
    uhdm_handle* obj = new uhdm_handle(type, object);
    objects_.push_back(obj);
    return (vpiHandle) obj;
  }
  private:
    static std::vector<uhdm_handle*> objects_;
  };


#endif
