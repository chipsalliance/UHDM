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

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <uhdm/uhdm_types.h>

namespace UHDM {
class Serializer;

class SymbolFactory {
  friend Serializer;

  static constexpr int kBufferCapacity = 1024 * 1024;
  typedef std::vector<std::unique_ptr<std::string>> buffers_t;

public:
  typedef unsigned int ID;

private:
  static constexpr SymbolFactory::ID kBadId = -1;  // setting all the bits.
  static constexpr std::string_view kBadSymbol{"@@BAD_SYMBOL@@"};

  typedef std::vector<std::string_view> Id2SymbolMap;
  typedef std::unordered_map<std::string_view, ID> Symbol2IdMap;

 public:
  // Register given "symbol" string as a symbol and return its id.
  // If this is an existing symbol, its ID is returned, otherwise a new one
  // is created.
  ID Make(std::string_view symbol);

  // Find id of given "symbol" or return "@@BAD_SYMBOL@@" if it doesn't exist.
  std::string_view GetSymbol(ID id) const;

  // Get symbol string identified by given ID or 0 (zero) if it doesn't exist
  ID GetId(std::string_view symbol) const;

  static std::string_view getBadSymbol() { return kBadSymbol; }
  static ID getBadId() { return kBadId; }

private:
  Id2SymbolMap id2SymbolMap_;
  Symbol2IdMap symbol2IdMap_;
  buffers_t buffers;
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

inline std::string_view SymbolFactory::GetSymbol(ID id) const {
  return (id < id2SymbolMap_.size()) ? id2SymbolMap_[id] : getBadSymbol();
}

inline SymbolFactory::ID SymbolFactory::GetId(std::string_view symbol) const {
  auto found = symbol2IdMap_.find(symbol);
  return (found == symbol2IdMap_.end()) ? getBadId() : found->second;
}

};

#endif
