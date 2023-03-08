#ifndef UHDM_SYMBOLID_H
#define UHDM_SYMBOLID_H
#pragma once

#include <cstdint>
#include <ostream>
#include <set>
#include <string_view>
#include <unordered_set>
#include <vector>

// TODO(hs-apotell) SYMBOLID_DEBUG_ENABLED should be part of a config.h
// so header and corresponding binary library match up.

namespace UHDM {
/**
 * class SymbolId
 *
 * Used to uniquely represent a string in SymbolFactory. SymbolId can (and
 * should) be resolved only with the SymbolFactory that it was generated with.
 *
 */
typedef uint32_t RawSymbolId;
inline static constexpr RawSymbolId BadRawSymbolId = 0;
inline static constexpr std::string_view BadRawSymbol = "@@BAD_SYMBOL@@";

class SymbolFactory;

class SymbolId final {
 public:
#if SYMBOLID_DEBUG_ENABLED
  SymbolId() : m_id(BadRawSymbolId), m_value(BadRawSymbol) {}
  SymbolId(RawSymbolId id, std::string_view value) : m_id(id), m_value(value) {}
  SymbolId(const SymbolId &rhs) : m_id(rhs.m_id), m_value(rhs.m_value) {}
#else
  SymbolId() : m_id(BadRawSymbolId) {}
  SymbolId(RawSymbolId id, std::string_view value) : m_id(id) {}
  SymbolId(const SymbolId &rhs) : m_id(rhs.m_id) {}
#endif

  SymbolId &operator=(const SymbolId &rhs) {
    if (this != &rhs) {
      m_id = rhs.m_id;
#if SYMBOLID_DEBUG_ENABLED
      m_value = rhs.m_value;
#endif
    }
    return *this;
  }

  explicit operator RawSymbolId() const { return m_id; }
  explicit operator bool() const { return m_id != BadRawSymbolId; }
  explicit operator std::string_view() {
#if SYMBOLID_DEBUG_ENABLED
    return m_value;
#else
    static constexpr std::string_view kEmpty;
    return kEmpty;
#endif
  }

  bool operator==(const SymbolId &rhs) const { return m_id == rhs.m_id; }
  bool operator!=(const SymbolId &rhs) const { return m_id != rhs.m_id; }

 private:
  RawSymbolId m_id;
#if SYMBOLID_DEBUG_ENABLED
  std::string_view m_value;
#endif

  friend std::ostream &operator<<(std::ostream &strm, const SymbolId &symbolId);
};

#if !SYMBOLID_DEBUG_ENABLED
static_assert(sizeof(SymbolId) == sizeof(RawSymbolId), "SymboldId type grew?");
#endif

inline static const SymbolId BadSymbolId(BadRawSymbolId, BadRawSymbol);

inline std::ostream &operator<<(std::ostream &strm, const SymbolId &symbolId) {
  strm << (RawSymbolId)symbolId;
  return strm;
}

struct SymbolIdPP final {  // Pretty Printer
  const SymbolId &m_id;
  const SymbolFactory *const m_symbolFactory;

  SymbolIdPP(const SymbolId &id, const SymbolFactory *const symbolFactory)
      : m_id(id), m_symbolFactory(symbolFactory) {}
};

std::ostream &operator<<(std::ostream &strm, const SymbolIdPP &id);

struct SymbolIdHasher final {
  inline size_t operator()(const SymbolId &value) const {
    return std::hash<RawSymbolId>()((RawSymbolId)value);
  }
};

struct SymbolIdEqualityComparer final {
  inline bool operator()(const SymbolId &lhs, const SymbolId &rhs) const {
    return (lhs == rhs);
  }
};

struct SymbolIdLessThanComparer final {
  inline bool operator()(const SymbolId &lhs, const SymbolId &rhs) const {
    return ((RawSymbolId)lhs < (RawSymbolId)rhs);
  }
};

typedef std::set<SymbolId, SymbolIdLessThanComparer> SymbolIdSet;
typedef std::unordered_set<SymbolId, SymbolIdHasher, SymbolIdEqualityComparer>
    SymbolIdUnorderedSet;
typedef std::vector<SymbolId> SymbolIdVector;

}  // namespace UHDM

#endif  // UHDM_SYMBOLID_H
