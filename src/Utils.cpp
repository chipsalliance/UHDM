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
 * File:   Utils.cpp
 * Author: hs
 *
 * Created on Oct 25, 2025, 02:00 AM
 */

#include <uhdm/Utils.h>

#include <sstream>

namespace uhdm {
bool setTypespec(Any* object, Typespec* typespec) {
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

bool setElemTypespec(ArrayTypespec* typespec, Typespec* actual) {
  RefTypespec* rt = nullptr;
  if ((rt = typespec->getElemTypespec()) == nullptr) {
    rt = typespec->getSerializer()->make<RefTypespec>();
    typespec->setElemTypespec(rt);
  }
  return (rt != nullptr) && rt->setActual(actual);
}

bool setIndexTypespec(ArrayTypespec* typespec, Typespec* actual) {
  RefTypespec* rt = nullptr;
  if ((rt = typespec->getIndexTypespec()) == nullptr) {
    rt = typespec->getSerializer()->make<RefTypespec>();
    typespec->setIndexTypespec(rt);
  }
  return (rt != nullptr) && rt->setActual(actual);
}

bool getSigned(const Typespec* typespec) {
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

bool setSigned(Typespec* typespec, bool value) {
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

static std::map<int32_t, std::string_view> kUnaryOperatorTokens = {
    {vpiMinusOp, "-"},      {vpiPlusOp, "+"},      {vpiNotOp, "!"},
    {vpiBitNegOp, "~"},     {vpiUnaryAndOp, "&"},  {vpiUnaryNandOp, "~&"},
    {vpiUnaryOrOp, "|"},    {vpiUnaryNorOp, "~|"}, {vpiUnaryXorOp, "^"},
    {vpiUnaryXNorOp, "~^"}, {vpiPreIncOp, "++"},   {vpiPreDecOp, "--"},
};

static std::map<int32_t, std::string_view> kBinaryOperatorTokens = {
    {vpiMinusOp, "-"},
    {vpiPlusOp, "+"},
    {vpiNotOp, "!"},
    {vpiBitNegOp, "~"},
    {vpiUnaryAndOp, "&"},
    {vpiUnaryNandOp, "~&"},
    {vpiUnaryOrOp, "|"},
    {vpiUnaryNorOp, "~|"},
    {vpiUnaryXorOp, "^"},
    {vpiUnaryXNorOp, "~^"},
    {vpiSubOp, "-"},
    {vpiDivOp, "/"},
    {vpiModOp, "%"},
    {vpiEqOp, "=="},
    {vpiNeqOp, "!="},
    {vpiCaseEqOp, "==="},
    {vpiCaseNeqOp, "!=="},
    {vpiGtOp, ">"},
    {vpiGeOp, ">="},
    {vpiLtOp, "<"},
    {vpiLeOp, "<="},
    {vpiLShiftOp, "<<"},
    {vpiRShiftOp, ">>"},
    {vpiAddOp, "+"},
    {vpiMultOp, "*"},
    {vpiLogAndOp, "&&"},
    {vpiLogOrOp, "||"},
    {vpiBitAndOp, "&"},
    {vpiBitOrOp, "|"},
    {vpiBitXorOp, "^"},
    {vpiBitXNorOp, "^~"},
    {vpiArithLShiftOp, "<<<"},
    {vpiArithRShiftOp, ">>>"},
    {vpiPowerOp, "**"},
    {vpiImplyOp, "->"},
    {vpiNonOverlapImplyOp, "|=>"},
    {vpiOverlapImplyOp, "|->"},
};

void prettyPrint(std::ostream& out, const Any* object,
                 size_t indent /* = 0 */) {
  if (object == nullptr) return;

  if (indent > 0) out << std::string(indent, ' ');

  switch (object->getUhdmType()) {
    case UhdmType::Constant:
      out << static_cast<const Constant*>(object)->getDecompile();
      break;

    case UhdmType::Parameter:
      out << ltrim_until(static_cast<const Parameter*>(object)->getValue(),
                         ':');
      break;

    case UhdmType::SysFuncCall: {
      const SysFuncCall* sysFuncCall = static_cast<const SysFuncCall*>(object);
      out << sysFuncCall->getName() << "(";
      if (const AnyCollection* const arguments = sysFuncCall->getArguments()) {
        prettyPrint(out, arguments);
      }
      out << ")";
    } break;

    case UhdmType::EnumConst:
      out << ltrim_until(static_cast<const EnumConst*>(object)->getValue(),
                         ':');
      break;

    case UhdmType::Operation: {
      const Operation* const operation = static_cast<const Operation*>(object);
      const AnyCollection* const operands = operation->getOperands();
      const int32_t opType = operation->getOpType();
      switch (opType) {
        case vpiMinusOp:
        case vpiPlusOp:
        case vpiNotOp:
        case vpiBitNegOp:
        case vpiUnaryAndOp:
        case vpiUnaryNandOp:
        case vpiUnaryOrOp:
        case vpiUnaryNorOp:
        case vpiUnaryXorOp:
        case vpiUnaryXNorOp:
        case vpiPreIncOp:
        case vpiPreDecOp: {
          out << kUnaryOperatorTokens[opType];
          prettyPrint(out, operands->at(0));
        } break;

        case vpiSubOp:
        case vpiDivOp:
        case vpiModOp:
        case vpiEqOp:
        case vpiNeqOp:
        case vpiCaseEqOp:
        case vpiCaseNeqOp:
        case vpiGtOp:
        case vpiGeOp:
        case vpiLtOp:
        case vpiLeOp:
        case vpiLShiftOp:
        case vpiRShiftOp:
        case vpiAddOp:
        case vpiMultOp:
        case vpiLogAndOp:
        case vpiLogOrOp:
        case vpiBitAndOp:
        case vpiBitOrOp:
        case vpiBitXorOp:
        case vpiBitXNorOp:
        case vpiArithLShiftOp:
        case vpiArithRShiftOp:
        case vpiPowerOp:
        case vpiImplyOp:
        case vpiNonOverlapImplyOp:
        case vpiOverlapImplyOp: {
          prettyPrint(out, operands->at(0));
          out << " " << kBinaryOperatorTokens[opType] << " ";
          prettyPrint(out, operands->at(1));
        } break;

        case vpiConditionOp: {
          prettyPrint(out, operands->at(0));
          out << " ? ";
          prettyPrint(out, operands->at(1));
          out << " : ";
          prettyPrint(out, operands->at(2));
        } break;

        case vpiConcatOp:
        case vpiAssignmentPatternOp: {
          out << ((opType == vpiConcatOp) ? "{" : "'{");
          prettyPrint(out, operands);
          out << "}";
        } break;

        case vpiMultiConcatOp: {
          out << "{";
          prettyPrint(out, operands->at(0));
          out << "{";
          prettyPrint(out, operands->at(1));
          out << "}}";
        } break;

        case vpiEventOrOp: {
          prettyPrint(out, operands->at(0));
          out << " or ";
          prettyPrint(out, operands->at(1));
        } break;

        case vpiInsideOp: {
          prettyPrint(out, operands->at(0));
          out << " inside {";
          prettyPrint(out, operands);
          out << "}";
        } break;

        case vpiNullOp:
          break;

        case vpiPosedgeOp: {
          out << "posedge ";
          prettyPrint(out, operands->at(0));
        } break;

        case vpiNegedgeOp: {
          out << "negedge ";
          prettyPrint(out, operands->at(0));
        } break;

        case vpiPostIncOp: {
          prettyPrint(out, operands->at(0));
          out << "++";
        } break;

        case vpiPostDecOp: {
          prettyPrint(out, operands->at(0));
          out << "--";
        } break;

          /*
            { vpiListOp, "," },
            { vpiMinTypMaxOp, ":" },
            { vpiAcceptOnOp, "accept_on" },
            { vpiRejectOnOp, "reject_on" },
            { vpiSyncAcceptOnOp, "sync_accept_on" },
            { vpiSyncRejectOnOp, "sync_reject_on" },
            { vpiOverlapFollowedByOp, "overlapped followed_by" },
            { vpiNonOverlapFollowedByOp, "nonoverlapped followed_by" },
            { vpiNexttimeOp, "nexttime" },
            { vpiAlwaysOp, "always" },
            { vpiEventuallyOp, "eventually" },
            { vpiUntilOp, "until" },
            { vpiUntilWithOp, "until_with" },
            { vpiUnaryCycleDelayOp, "##" },
            { vpiCycleDelayOp, "##" },
            { vpiIntersectOp, "intersection" },
            { vpiFirstMatchOp, "first_match" },
            { vpiThroughoutOp, "throughout" },
            { vpiWithinOp, "within" },
            { vpiRepeatOp, "[=]" },
            { vpiConsecutiveRepeatOp, "[*]" },
            { vpiGotoRepeatOp, "[->]" },
            { vpiMatchOp, "match" },
            { vpiCastOp, "type'" },
            { vpiIffOp, "iff" },
            { vpiWildEqOp, "==?" },
            { vpiWildNeqOp, "!=?" },
            { vpiStreamLROp, "{>>}" },
            { vpiStreamRLOp, "{<<}" },
            { vpiMatchedOp, ".matched" },
            { vpiTriggeredOp, ".triggered" },
            { vpiMultiAssignmentPatternOp, "{n{}}" },
            { vpiIfOp, "if" },
            { vpiIfElseOp, "if–else" },
            { vpiCompAndOp, "and" },
            { vpiCompOrOp, "or" },
            { vpiImpliesOp, "implies" },
            { vpiTypeOp, "type" },
            { vpiAssignmentOp, "=" },
          */

        default:
          break;
      }
    } break;

    case UhdmType::PartSelect: {
      const PartSelect* const ps = static_cast<const PartSelect*>(object);
      prettyPrint(out, ps->getLeftExpr());
      out << ":";
      prettyPrint(out, ps->getRightExpr());
    } break;

    case UhdmType::IndexedPartSelect: {
      const IndexedPartSelect* const ips =
          static_cast<const IndexedPartSelect*>(object);
      prettyPrint(out, ips->getBaseExpr());
      out << ((ips->getIndexedPartSelectType() == vpiPosIndexed) ? "+" : "-")
          << ":";
      prettyPrint(out, ips->getWidthExpr());
    } break;

    case UhdmType::Range: {
      const Range* const r = static_cast<const Range*>(object);
      out << "[";
      if (const Expr* const lhs = r->getLeftExpr()) prettyPrint(out, lhs);
      out << ":";
      if (const Expr* const rhs = r->getRightExpr()) prettyPrint(out, rhs);
      out << "]";
    } break;

    case UhdmType::RefObj: {
      out << object->getName();
    } break;

    case UhdmType::VarSelect: {
      const VarSelect* const vs = static_cast<const VarSelect*>(object);
      out << vs->getName();
      if (const ExprCollection* const indexes = vs->getIndexes()) {
        for (const Expr* const index : *indexes) {
          out << "[";
          prettyPrint(out, index);
          out << "]";
        }
      }
    } break;

    case UhdmType::RefTypespec: {
      if (const Typespec* const typespec =
              static_cast<const RefTypespec*>(object)->getActual()) {
        prettyPrint(out, typespec, indent);
      } else {
        out << object->getName();
      }
    } break;

    case UhdmType::ParamAssign: {
      const ParamAssign* const pa = static_cast<const ParamAssign*>(object);
      if (const Any* const lhs = pa->getLhs()) out << lhs->getName();
      out << "=";
      if (const Any* const rhs = pa->getRhs()) prettyPrint(out, rhs);
    } break;

    case UhdmType::BitTypespec: {
      const BitTypespec* const bt = static_cast<const BitTypespec*>(object);
      out << (bt->getSigned() ? "bit signed" : "bit");
      if (const RangeCollection* const rc = bt->getRanges()) {
        out << " ";
        prettyPrint(out, rc, "");
      }
    } break;

    case UhdmType::ByteTypespec: {
      const ByteTypespec* const bt = static_cast<const ByteTypespec*>(object);
      out << (bt->getSigned() ? "byte" : "byte unsigned");
      if (const RangeCollection* const rc = bt->getRanges()) {
        out << " ";
        prettyPrint(out, rc, "");
      }
    } break;

    case UhdmType::IntTypespec:
      out << (static_cast<const IntTypespec*>(object)->getSigned()
                  ? "int"
                  : "int unsigned");
      break;

    case UhdmType::IntegerTypespec:
      out << (static_cast<const IntegerTypespec*>(object)->getSigned()
                  ? "integer"
                  : "integer unsigned");
      break;

    case UhdmType::LogicTypespec: {
      const LogicTypespec* const lt = static_cast<const LogicTypespec*>(object);
      out << (lt->getSigned() ? "logic signed" : "logic");
      if (const RangeCollection* const rc = lt->getRanges()) {
        out << " ";
        prettyPrint(out, rc, "");
      }
    } break;

    case UhdmType::LongIntTypespec:
      out << (static_cast<const LongIntTypespec*>(object)->getSigned()
                  ? "longint"
                  : "longint unsigned");
      break;

    case UhdmType::ShortIntTypespec:
      out << (static_cast<const ShortIntTypespec*>(object)->getSigned()
                  ? "shortint"
                  : "shortint unsigned");
      break;

    case UhdmType::RealTypespec:
      out << "real";
      break;

    case UhdmType::ShortRealTypespec:
      out << "shortreal";
      break;

    case UhdmType::StringTypespec:
      out << "string";
      break;

    case UhdmType::VoidTypespec:
      out << "void";
      break;

    case UhdmType::TimeTypespec:
      out << "time";
      break;

    case UhdmType::EventTypespec:
      out << "event";
      break;

    case UhdmType::ChandleTypespec:
      out << "chandle";
      break;

    case UhdmType::TypeParameter: {
      if (const RefTypespec* const rt =
              static_cast<const TypeParameter*>(object)->getTypespec()) {
        if (const Typespec* const typespec = rt->getActual()) {
          prettyPrint(out, typespec, indent);
          break;
        }
      }
      out << "type";
    } break;

    case UhdmType::ArrayTypespec: {
      const ArrayTypespec* const at =
          static_cast<const ArrayTypespec*>(object);
      if (const RefTypespec* const ert = at->getElemTypespec()) {
        if (const Typespec* const ets = ert->getActual()) {
          prettyPrint(out, ets);
        }
      }
      if (const RangeCollection* const rc = at->getRanges()) {
        out << " ";
        prettyPrint(out, rc, "");
      }
      if (const Any* const it = at->getIndexTypespec()) {
        out << " [";
        prettyPrint(out, it);
        out << "]";
      }
    } break;

    case UhdmType::ClassTypespec:
    case UhdmType::EnumTypespec:
    case UhdmType::InterfaceTypespec:
    case UhdmType::ModuleTypespec:
    case UhdmType::ProgramTypespec:
    case UhdmType::StructTypespec:
    case UhdmType::TypedefTypespec:
    case UhdmType::UdpDefnTypespec:
    case UhdmType::UnionTypespec:
    case UhdmType::UnsupportedTypespec:
    case UhdmType::PropertyTypespec:
    case UhdmType::SequenceTypespec:
    default:
      out << object->getName();
  }
}

std::string prettyPrint(const Any* object, size_t indent /* = 0 */) {
  std::ostringstream out;
  prettyPrint(out, object, indent);
  return out.str();
}
}  // namespace uhdm
