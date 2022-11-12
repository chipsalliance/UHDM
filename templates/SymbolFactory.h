
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
 * Author: alain
 *
 * Created on March 6, 2017, 11:10 PM
 */

#ifndef UHDM_SYMBOLFACTORY_H
#define UHDM_SYMBOLFACTORY_H
#pragma once

#include <uhdm/SymbolId.h>

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace UHDM {
class Serializer;

class SymbolFactory {
 public:
  SymbolFactory();
  ~SymbolFactory() = default;

  // Must never copy: expensive, and string locations would not be stable.
  // It would also be a programming error.
  SymbolFactory& operator=(const SymbolFactory&) = delete;
  SymbolFactory(SymbolFactory&& s) = delete;
  SymbolFactory& operator=(SymbolFactory&&) = delete;

  // Register given "symbol" string as a symbol and return its id and the
  // canonicalized view. If this is an existing symbol, its existing ID is
  // returned, otherwise a new one is created.
  std::pair<SymbolId, std::string_view> add(std::string_view symbol);

  // Register given "symbol" string as a symbol and return its id.
  // If this is an existing symbol, its ID is returned, otherwise a new one
  // is created.
  SymbolId registerSymbol(std::string_view symbol) { return add(symbol).first; }

  // Find id and canonicalized view of given "symbol" or return bad-ID
  // (see #getBad()) if it doesn't exist.
  std::pair<SymbolId, std::string_view> get(std::string_view symbol) const;

  // Find id of given "symbol" or return bad-ID (see #getBad()) if it doesn't
  // exist.
  SymbolId getId(std::string_view symbol) const { return get(symbol).first; }

  // Get symbol string identified by given ID or BadSymbol if it doesn't exist
  // (see #getBadSymbol()).
  const std::string& getSymbol(SymbolId id) const;

  // Copy input id from corresponding SymbolFactory rhs to this SymbolFactory
  SymbolId copyFrom(SymbolId id, const SymbolFactory* rhs);

  // Get a vector of all symbols. As a special property, the SymbolID can be
  // used as an index into this  vector to get the corresponding text-symbol.
  std::vector<std::string_view> getSymbols() const;

  static const std::string& getBadSymbol();
  static SymbolId getBadId() { return BadSymbolId; }
  static const std::string& getEmptyMacroMarker();

  // IMPORTANT NOTE:
  // These are UHDM specific APIs and to be used within UHDM only.
  // An important distinction between these and the above -
  // For invalid id, these will return an empty string
  // unlike the above which return bad symbol.
  SymbolId Make(std::string_view symbol);
  const std::string& GetSymbol(SymbolId id) const;
  SymbolId GetId(std::string_view symbol) const;

 protected:
  // Create a snapshot of the current symbol table. Private, as this
  // functionality should be explicitly accessed through CreateSnapshot().
  SymbolFactory(const SymbolFactory& parent)
      : m_parent(&parent), m_idOffset(parent.m_idCounter + parent.m_idOffset) {}

 private:
  void Purge();
  void AppendSymbols(int64_t up_to, std::vector<std::string_view>* dest) const;

  typedef std::deque<std::string> Id2SymbolMap;
  typedef std::unordered_map<std::string_view, RawSymbolId> Symbol2IdMap;

  const SymbolFactory *const m_parent;
  const RawSymbolId m_idOffset;

  RawSymbolId m_idCounter = 0;

  // Stable strings whose address doesn't change with reallocations.
  Id2SymbolMap m_id2SymbolMap;

  // The key string_views point to the stable backing buffer provided in
  // m_id2SymbolMap
  Symbol2IdMap m_symbol2IdMap;

  friend Serializer;
};
}  // namespace UHDM

#endif /* UHDM_SYMBOLFACTORY_H */
