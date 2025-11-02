/*
 Copyright 2020 Alain Dargelas

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
 * File:   Utils.h
 * Author: hs
 *
 * Created on Oct 25, 2025, 02:00 AM
 */

#ifndef UHDM_UTILS_H
#define UHDM_UTILS_H
#pragma once

#include <uhdm/Serializer.h>
#include <uhdm/uhdm.h>

#include <type_traits>

namespace uhdm {
template <typename R = Any, typename T = Any>
inline auto getActual(T* object) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (object == nullptr) return nullptr;

  if (auto* const ro = any_cast<RefObj>(object)) {
    return ro->template getActual<R>();
  } else if (auto* const cb = any_cast<ClockingBlock>(object)) {
    return cb->template getActual<R>();
  } else if (auto* const hp = any_cast<HierPath>(object)) {
    if (auto* const pe = hp->getPathElems()) {
      if (!pe->empty()) return getActual<R>(pe->back());
    }
  } else if (auto* const rm = any_cast<RefModule>(object)) {
    return rm->template getActual<R>();
  } else if (auto* const rt = any_cast<RefTypespec>(object)) {
    return rt->template getActual<R>();
  }

  return nullptr;
}

template <typename T>
inline bool setActual(uhdm::Any* object, T* actual) {
  if (object == nullptr) return false;

  if (RefObj* const ro = any_cast<RefObj>(object)) {
    return ro->setActual(actual);
  } else if (ClockingBlock* const cb = any_cast<ClockingBlock>(object)) {
    return cb->setActual(any_cast<ClockingBlock>(actual));
  } else if (RefModule* const rm = any_cast<RefModule>(object)) {
    return rm->setActual(actual);
  } else if (RefTypespec* const rt = any_cast<RefTypespec>(object)) {
    return rt->setActual(any_cast<Typespec>(actual));
  }

  return false;
}

template <typename R = Typespec, typename T = Any>
inline auto getTypespec(T* object) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (object == nullptr) return nullptr;

  if (auto* const e = any_cast<Expr>(object)) {
    if (auto* const rt = e->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const iod = any_cast<IODecl>(object)) {
    if (auto* const rt = iod->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const ne = any_cast<NamedEvent>(object)) {
    if (auto* const rt = ne->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const p = any_cast<Ports>(object)) {
    if (auto* const rt = p->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const pfd = any_cast<PropFormalDecl>(object)) {
    if (auto* const rt = pfd->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const sfd = any_cast<SeqFormalDecl>(object)) {
    if (auto* const rt = sfd->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const tp = any_cast<TaggedPattern>(object)) {
    if (auto* const rt = tp->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const tm = any_cast<TypespecMember>(object)) {
    if (auto* const rt = tm->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const tp = any_cast<TypeParameter>(object)) {
    if (auto* const rt = tp->getTypespec()) {
      return rt->template getActual<R>();
    }
  }

  return nullptr;
}

inline bool setTypespec(Any* object, Typespec* typespec) {
  if (object == nullptr) return false;

  Serializer* const serializer = object->getSerializer();
  RefTypespec* rt = nullptr;
  if (Expr* const e = any_cast<Expr>(object)) {
    if ((rt = e->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      e->setTypespec(rt);
    }
  } else if (IODecl* const iod = any_cast<IODecl>(object)) {
    if ((rt = iod->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      iod->setTypespec(rt);
    }
  } else if (NamedEvent* const ne = any_cast<NamedEvent>(object)) {
    if ((rt = ne->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      ne->setTypespec(rt);
    }
  } else if (Ports* const p = any_cast<Ports>(object)) {
    if ((rt = p->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      p->setTypespec(rt);
    }
  } else if (PropFormalDecl* const pfd = any_cast<PropFormalDecl>(object)) {
    if ((rt = pfd->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      pfd->setTypespec(rt);
    }
  } else if (SeqFormalDecl* const sfd = any_cast<SeqFormalDecl>(object)) {
    if ((rt = sfd->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      sfd->setTypespec(rt);
    }
  } else if (TaggedPattern* const tp = any_cast<TaggedPattern>(object)) {
    if ((rt = tp->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      tp->setTypespec(rt);
    }
  } else if (TypespecMember* const tm = any_cast<TypespecMember>(object)) {
    if ((rt = tm->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      tm->setTypespec(rt);
    }
  } else if (TypeParameter* const tp = any_cast<TypeParameter>(object)) {
    if ((rt = tp->getTypespec()) == nullptr) {
      rt = serializer->make<RefTypespec>();
      tp->setTypespec(rt);
    }
  }

  return (rt != nullptr) && rt->setActual(typespec);
}

template <typename R = Typespec, typename T = ArrayTypespec>
inline auto getElemTypespec(T* typespec) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (auto* const at = any_cast<ArrayTypespec>(typespec)) {
    return getActual<R>(at->getElemTypespec());
  }
  return nullptr;
}

inline bool setElemTypespec(ArrayTypespec* typespec, Typespec* actual) {
  RefTypespec* rt = nullptr;
  if ((rt = typespec->getElemTypespec()) == nullptr) {
    rt = typespec->getSerializer()->make<RefTypespec>();
    typespec->setElemTypespec(rt);
  }
  return (rt != nullptr) && rt->setActual(actual);
}

template <typename R = Typespec, typename T = ArrayTypespec>
inline auto getIndexTypespec(T* typespec) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (auto* const at = any_cast<ArrayTypespec>(typespec)) {
    return getActual<R>(at->getIndexTypespec());
  }
  return nullptr;
}

inline bool setIndexTypespec(ArrayTypespec* typespec, Typespec* actual) {
  RefTypespec* rt = nullptr;
  if ((rt = typespec->getIndexTypespec()) == nullptr) {
    rt = typespec->getSerializer()->make<RefTypespec>();
    typespec->setIndexTypespec(rt);
  }
  return (rt != nullptr) && rt->setActual(actual);
}

template <typename R, typename T = Any>
inline auto getParent(T* any) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  auto* p = any_cast<Any>(any);
  while (p != nullptr) {
    if (auto* const pp = any_cast<R>(p)) {
      return pp;
    }
    p = p->getParent();
  }
  return nullptr;
}

inline bool getSigned(const Typespec* typespec) {
  switch (typespec->getUhdmType()) {
    case UhdmType::BitTypespec:
      return static_cast<const BitTypespec*>(typespec)->getSigned();
    case UhdmType::ByteTypespec:
      return static_cast<const ByteTypespec*>(typespec)->getSigned();
    case UhdmType::IntegerTypespec:
      return static_cast<const IntegerTypespec*>(typespec)->getSigned();
    case UhdmType::IntTypespec:
      return static_cast<const IntTypespec*>(typespec)->getSigned();
    case UhdmType::LogicTypespec:
      return static_cast<const LogicTypespec*>(typespec)->getSigned();
    case UhdmType::LongIntTypespec:
      return static_cast<const LongIntTypespec*>(typespec)->getSigned();
    case UhdmType::ShortIntTypespec:
      return static_cast<const ShortIntTypespec*>(typespec)->getSigned();
    default:
      return false;
  }
}

inline bool setSigned(Typespec* typespec, bool value) {
  switch (typespec->getUhdmType()) {
    case UhdmType::BitTypespec:
      return static_cast<const BitTypespec*>(typespec)->getSigned();
    case UhdmType::ByteTypespec:
      return static_cast<ByteTypespec*>(typespec)->setSigned(value);
    case UhdmType::IntegerTypespec:
      return static_cast<IntegerTypespec*>(typespec)->setSigned(value);
    case UhdmType::IntTypespec:
      return static_cast<IntTypespec*>(typespec)->setSigned(value);
    case UhdmType::LogicTypespec:
      return static_cast<const LogicTypespec*>(typespec)->getSigned();
    case UhdmType::LongIntTypespec:
      return static_cast<LongIntTypespec*>(typespec)->setSigned(value);
    case UhdmType::ShortIntTypespec:
      return static_cast<ShortIntTypespec*>(typespec)->setSigned(value);
    default:
      return false;
  }
}
}  // namespace uhdm

#endif  // UHDM_UTILS_H
