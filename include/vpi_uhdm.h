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

namespace UHDM {

  class BaseClass {
  public: 
    //BaseClass(){}
    virtual unsigned int getUhdmType() { return 0; }   
    virtual ~BaseClass(){}
  };

  class Serializer {
  public:
    static void save(std::string file);
    static void purge();
    static const std::vector<vpiHandle> restore(std::string file);
  private:
    static BaseClass* getObject(unsigned int objectType, unsigned int index);
  };

  class SymbolFactory {
  public:
    static unsigned int make(const std::string& symbol);
    static const std::string& getSymbol(unsigned int id);
    static unsigned int getId(const std::string& symbol);
  private:
    static unsigned int idCounter_;
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
  static vpiHandle make(unsigned int type, const void* object) {
    uhdm_handle* obj = new uhdm_handle(type, object);
    objects_.push_back(obj);
    return (vpiHandle) obj;
  }
  private:
    static std::vector<uhdm_handle*> objects_;
  };


#endif
