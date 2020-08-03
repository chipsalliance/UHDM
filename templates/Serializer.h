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
#include <iostream>
#include <functional>
#include "uhdm.h"

namespace UHDM {

  typedef std::function<void(const std::string&)> ErrorHandler;

  static void DefaultErrorHandler(const std::string& errorMsg) { std::cout << errorMsg << std::endl; }
  
  class Serializer {
  public:
    Serializer() : incrId_(0), objId_(0), errorHandler(DefaultErrorHandler) {symbolMaker.Make("");}
    void Save(const std::string& file);
    void Purge();
    void SetErrorHandler(ErrorHandler handler) { errorHandler = handler; }
    ErrorHandler GetErrorHandler() { return errorHandler; }
    const std::vector<vpiHandle> Restore(const std::string& file);

<FACTORIES_METHODS>
    std::vector<any*>* MakeAnyVec() { return anyVectMaker.Make(); }
    vpiHandle MakeUhdmHandle(UHDM_OBJECT_TYPE type, const void* object) { return uhdm_handleMaker.Make(type, object); }

    VectorOfanyFactory anyVectMaker;
    SymbolFactory symbolMaker;
    uhdm_handleFactory uhdm_handleMaker;
<FACTORIES>

  std::unordered_map<const BaseClass*, unsigned long>& AllObjects() { return allIds_; }
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
