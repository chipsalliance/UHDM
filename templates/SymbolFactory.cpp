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
 * File:   SymbolFactory.cpp
 * Author: alain
 *
 * Created on March 6, 2017, 11:10 PM
 */

#include <uhdm/SymbolFactory.h>

#include <cassert>

namespace UHDM {

SymbolFactory::SymbolFactory() : m_parent(nullptr), m_idOffset(0) {
  registerSymbol(getBadSymbol());
}

const std::string& SymbolFactory::getBadSymbol() {
  static const std::string k_badSymbol(BadRawSymbol);
  return k_badSymbol;
}

const std::string& SymbolFactory::getEmptyMacroMarker() {
  static const std::string k_emptyMacroMarker("@@EMPTY_MACRO@@");
  return k_emptyMacroMarker;
}

std::pair<SymbolId, std::string_view> SymbolFactory::add(
    std::string_view symbol) {
  if (m_parent) {
    if (auto [id, normalized] = m_parent->get(symbol);
        (id != getBadId() || symbol == getBadSymbol()) &&
        (RawSymbolId)id < m_idOffset) {
      return {id, normalized};
    }
  }

  auto found = m_symbol2IdMap.find(symbol);
  if (found != m_symbol2IdMap.end()) {
    // Do NOT use m_id2SymbolMap[found->second] to build the returned pair.
    // MS cl compiler seems to have some bug that ends up actually creating
    // the string_view from a clone of the string and thus invalid by the time
    // the caller of this function receives it. Use the already pinned
    // string_view from the map instead.
    return {SymbolId(found->second + m_idOffset, found->first), found->first};
  }
  const std::string& normalized = m_id2SymbolMap.emplace_back(symbol);
  const auto inserted = m_symbol2IdMap.insert({normalized, m_idCounter});
  assert(inserted.second);  // This new insert must succeed.
  m_idCounter++;
  // Read the comment above about using the string from m_id2SymbolMap
  // to build the returned pair.
  return {SymbolId(inserted.first->second + m_idOffset, inserted.first->first),
          inserted.first->first};
}

std::pair<SymbolId, std::string_view> SymbolFactory::get(
    std::string_view symbol) const {
  if (m_parent) {
    if (auto [id, normalized] = m_parent->get(symbol);
        id != getBadId() && (RawSymbolId)id < m_idOffset) {
      return {id, normalized};
    }
  }

  // Read the comment in SymbolFactory::add above about using the
  // string from m_id2SymbolMap to build the returned pair.
  auto found = m_symbol2IdMap.find(symbol);
  return (found == m_symbol2IdMap.end())
             ? std::make_pair(getBadId(), getBadSymbol())
             : std::make_pair(
                   SymbolId(found->second + m_idOffset, found->first),
                   found->first);
}

const std::string& SymbolFactory::getSymbol(SymbolId id) const {
  RawSymbolId rid = (RawSymbolId)id;
  if (rid < m_idOffset) {
    assert(m_parent);  // If we have a non-0 idOffset, we must have parent
    return m_parent->getSymbol(id);
  }
  rid -= m_idOffset;
  if (rid >= m_id2SymbolMap.size()) return getBadSymbol();
  return m_id2SymbolMap[rid];
}

SymbolId SymbolFactory::copyFrom(SymbolId id, const SymbolFactory* rhs) {
  return id ? registerSymbol(rhs->getSymbol(id)) : BadSymbolId;
}

void SymbolFactory::AppendSymbols(int64_t up_to,
                                std::vector<std::string_view>* dest) const {
  if (m_parent) m_parent->AppendSymbols(m_idOffset, dest);
  up_to -= m_idOffset;
  assert(up_to >= 0);
  for (const auto& s : m_id2SymbolMap) {
    if (up_to-- <= 0) return;
    dest->push_back(s);
  }
}

std::vector<std::string_view> SymbolFactory::getSymbols() const {
  std::vector<std::string_view> result;
  result.reserve(m_idOffset + m_id2SymbolMap.size());
  AppendSymbols(m_idOffset + m_id2SymbolMap.size(), &result);
  return result;
}

void SymbolFactory::Purge() {
  Symbol2IdMap().swap(m_symbol2IdMap);
  Id2SymbolMap().swap(m_id2SymbolMap);
  m_idCounter = 0;
  registerSymbol(getBadSymbol());
}

SymbolId SymbolFactory::Make(std::string_view symbol) {
  // IMPORTANT NOTE:
  // This is UHDM specific API and to be used within UHDM only.
  // An important distinction between these and the above -
  // For invalid id, these will return an empty string
  // unlike the above which return bad symbol.
  return (symbol.empty() || (symbol == getBadSymbol()))
             ? getBadId()
             : registerSymbol(symbol);
}

const std::string& SymbolFactory::GetSymbol(SymbolId id) const {
  // IMPORTANT NOTE:
  // This is UHDM specific API and to be used within UHDM only.
  // An important distinction between these and the above -
  // For invalid id, these will return an empty string
  // unlike the above which return bad symbol.
  static const std::string k_empty;
  return id ? getSymbol(id) : k_empty;
}

SymbolId SymbolFactory::GetId(std::string_view symbol) const {
  // IMPORTANT NOTE:
  // This is UHDM specific API and to be used within UHDM only.
  // An important distinction between these and the above -
  // For invalid id, these will return an empty string
  // unlike the above which return bad symbol.
  return (symbol.empty() || (symbol == getBadSymbol())) ? getBadId()
                                                        : getId(symbol);
}

}  // namespace UHDM
