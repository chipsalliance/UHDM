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
 * File:   SymbolFactory.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_SYMBOL_FACTORY_H
#define UHDM_SYMBOL_FACTORY_H

#include <string>
#include <vector>
#include <unordered_map>

#include "uhdm_types.h"

namespace UHDM {
  class Serializer;
  
  class SymbolFactory {
    friend Serializer;
  public:

    typedef unsigned int ID;
    typedef std::vector<std::string> Id2SymbolMap;
    typedef std::unordered_map<std::string, ID> Symbol2IdMap;

    ID Make(const std::string& symbol);

    const std::string& GetSymbol(ID id);

    ID GetId(const std::string& symbol);

  private:
    ID idCounter_ = 0;
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
