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

#include <uhdm/uhdm_types.h>

namespace UHDM {
class Serializer;

class SymbolFactory {
  friend Serializer;

public:
  typedef unsigned int ID;
  typedef std::vector<std::string> Id2SymbolMap;
  typedef std::unordered_map<std::string, ID> Symbol2IdMap;

private:
  static constexpr ID kBadId = -1;  // setting all the bits.

public:
  // Register given "symbol" string as a symbol and return its id.
  // If this is an existing symbol, its ID is returned, otherwise a new one
  // is created.
  ID Make(const std::string& symbol);

  // Find id of given "symbol" or return "@@BAD_SYMBOL@@" if it doesn't exist.
  const std::string& GetSymbol(ID id) const;

  // Get symbol string identified by given ID or 0 (zero) if it doesn't exist
  ID GetId(const std::string& symbol) const;

  // Remove all symbols
  void Purge();

  static const std::string& getBadSymbol();
  static ID getBadId() { return kBadId; }

private:
  ID idCounter_ = 0;
  Id2SymbolMap id2SymbolMap_;
  Symbol2IdMap symbol2IdMap_;
};

} // namespace UHDM

#endif
