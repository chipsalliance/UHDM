/*

 Copyright 2019-2021 Alain Dargelas

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
 * File:   ExprEval.cpp
 * Author: alaindargelas
 *
 * Created on July 3, 2021, 8:03 PM
 */

#include <uhdm/ExprEval.h>
#include <uhdm/uhdm.h>
#include <string.h>

using namespace UHDM;

bool ExprEval::isFullySpecified(const UHDM::typespec* tps) {
  VectorOfrange* ranges = nullptr;
  UHDM_OBJECT_TYPE type = tps->UhdmType();
  switch (type) {
    case uhdmlogic_typespec: {
      logic_typespec* ltps = (logic_typespec*)tps;
      ranges = ltps->Ranges();
      break;
    }
    case uhdmarray_typespec: {
      array_typespec* ltps = (array_typespec*)tps;
      const typespec* elem = ltps->Elem_typespec();
      if (!isFullySpecified(elem))
        return false;
      ranges = ltps->Ranges();
      break;
    }
    case uhdmbit_typespec: {
      bit_typespec* ltps = (bit_typespec*)tps;
      ranges = ltps->Ranges();
      break;
    }
    case uhdmenum_typespec: {
      enum_typespec* ltps = (enum_typespec*) tps;
      const typespec* base = ltps->Base_typespec();
      if (base && (!isFullySpecified(base)))
        return false;
      break;
    }
    case uhdmstruct_typespec: {
      struct_typespec* ltps = (struct_typespec*) tps;
      for (typespec_member* member : *ltps->Members()) {
        if (!isFullySpecified(member->Typespec()))
          return false;
      } 
      break;
    }
    case uhdmunion_typespec: {
      union_typespec* ltps = (union_typespec*) tps;
      for (typespec_member* member : *ltps->Members()) {
        if (!isFullySpecified(member->Typespec()))
          return false;
      } 
      break;
    }
    case uhdmpacked_array_typespec: {
      packed_array_typespec* ltps = (packed_array_typespec*)tps;
      const typespec* elem = (const typespec*) ltps->Elem_typespec();
      if (!isFullySpecified(elem))
        return false;
      const typespec* ttps = ltps->Typespec();
      if (ttps && (!isFullySpecified(ttps)))
        return false;
      ranges = ltps->Ranges();
      break;
    }
    default:
      break;
  }
  if (ranges) {
    for (auto range : *ranges) {
      if (range->Left_expr()->UhdmType() != uhdmconstant) return false;
      if (range->Right_expr()->UhdmType() != uhdmconstant) return false;
    }
  }
  return true;
}

uint64_t getValue(const UHDM::expr* expr) {
  uint64_t result = 0;
  if (expr && expr->UhdmType() == UHDM::uhdmconstant) {
    UHDM::constant* c = (UHDM::constant*)expr;
    const std::string& v = c->VpiValue();
    int type = c->VpiConstType();
    switch (type) {
      case vpiBinaryConst: {
        result = std::strtoll(v.c_str() + strlen("BIN:"), 0, 2);
        break;
      }
      case vpiDecConst: {
        result = std::strtoll(v.c_str() + strlen("DEC:"), 0, 10);
        break;
      }
      case vpiHexConst: {
        result = std::strtoll(v.c_str() + strlen("HEX:"), 0, 16);
        break;
      }
      case vpiOctConst: {
        result = std::strtoll(v.c_str() + strlen("OCT:"), 0, 8);
        break;
      }
      case vpiIntConst: {
        result = std::strtoll(v.c_str() + strlen("INT:"), 0, 10);
        break;
      }
      case vpiUIntConst: {
        result = std::strtoull(v.c_str() + strlen("UINT:"), 0, 10);
        break;
      }
      default: {
        if (strstr(v.c_str(), "UINT:")) {
          result = std::strtoull(v.c_str() + strlen("UINT:"), 0, 10);
        } else {
          result = std::strtoll(v.c_str() + strlen("INT:"), 0, 10);
        }
        break;
      }
    }
  }
  return result;
}

static void recursiveFlattening(Serializer& s, VectorOfany* flattened, const VectorOfany* ordered, std::vector<const typespec*> fieldTypes) {
  // Flattening
  int index = 0;
  for (any* op : *ordered) {
    if (op->UhdmType() == uhdmtagged_pattern) {
      tagged_pattern* tp = (tagged_pattern*)op;
      const typespec* ttp = tp->Typespec();
      UHDM_OBJECT_TYPE ttpt = ttp->UhdmType();
      switch (ttpt) {
        case uhdmint_typespec: {
          any* sop = (any*)tp->Pattern();
          flattened->push_back(sop);
          break;
        }
        case uhdmstring_typespec: {
          any* sop = (any*)tp->Pattern();
          UHDM_OBJECT_TYPE sopt = sop->UhdmType();
          if (sopt == uhdmoperation) {
            VectorOfany* operands = ((operation*)sop)->Operands();
            for (auto op1 : *operands) {
              bool substituted = false;
              if (op1->UhdmType() == uhdmtagged_pattern) {
                tagged_pattern* tp1 = (tagged_pattern*)op1;
                const typespec* ttp1 = tp1->Typespec();
                UHDM_OBJECT_TYPE ttpt1 = ttp1->UhdmType();
                if (ttpt1 == uhdmstring_typespec) {
                  if (ttp1->VpiName() == "default") {
                    const any* patt = tp1->Pattern();
                    const typespec* mold = fieldTypes[index];
                    operation* subst = s.MakeOperation();
                    VectorOfany* sops = s.MakeAnyVec();
                    subst->Operands(sops);
                    subst->VpiOpType(vpiConcatOp);
                    flattened->push_back(subst);
                    UHDM_OBJECT_TYPE moldtype = mold->UhdmType();
                    if (moldtype == uhdmstruct_typespec) {
                      struct_typespec* molds = (struct_typespec*)mold;
                      for (auto mem : *molds->Members()) {
                        if (mem) sops->push_back((any*)patt);
                      }
                    } else if (moldtype == uhdmlogic_typespec) {
                      logic_typespec* molds = (logic_typespec*)mold;
                      VectorOfrange* ranges = molds->Ranges();
                      for (range* r : *ranges) {
                        uint64_t from = getValue(r->Left_expr());
                        uint64_t to = getValue(r->Right_expr());
                        if (from > to) {
                          std::swap(from,to);
                        }
                        for (uint64_t i = from; i <= to; i++) {
                          sops->push_back((any*)patt);
                        }
                        // TODO: Multidimension
                        break;
                      }
                    }
                    substituted = true;
                    break;
                  }
                }
              } else if (op1->UhdmType() == uhdmoperation) {
//                recursiveFlattening(s, flattened, ((operation*)op1)->Operands(), fieldTypes);
//                substituted = true;
              }
              if (!substituted) {
                flattened->push_back(sop);
                break;
              }
            }
          } else {
            flattened->push_back(sop);
          }
          break;
        }
        default:
          flattened->push_back(op);
          break;
      }
    } else {
      flattened->push_back(op);
    }
    index++;
  }
}

expr* ExprEval::flattenPatternAssignments(Serializer& s, const typespec* tps,
                                          expr* exp) {
  expr* result = exp;
  if ((!exp) || (!tps)) {
    return result;
  }
  // Reordering
  if (exp->UhdmType() == uhdmoperation) {
    operation* op = (operation*)exp;
    if (op->VpiOpType() != vpiAssignmentPatternOp) {
      return result;
    }
    if (tps->UhdmType() != uhdmstruct_typespec) {
      return result;
    }
    if (op->VpiFlattened()) {
      return result;
    }
    struct_typespec* stps = (struct_typespec*)tps;
    std::vector<std::string> fieldNames;
    std::vector<const typespec*> fieldTypes;
    for (typespec_member* memb : *stps->Members()) {
      fieldNames.push_back(memb->VpiName());
      fieldTypes.push_back(memb->Typespec());
    }
    VectorOfany* orig = op->Operands();
    VectorOfany* ordered = s.MakeAnyVec();
    std::vector<any*> tmp(fieldNames.size());
    any* defaultOp = nullptr;
    int index = 0;
    for (auto oper : *orig) {
      if (oper->UhdmType() == uhdmtagged_pattern) {
        op->VpiFlattened(true);
        tagged_pattern* tp = (tagged_pattern*)oper;
        const typespec* ttp = tp->Typespec();
        const std::string& tname = ttp->VpiName();
        bool found = false;
        if (tname == "default") {
          defaultOp = oper;
          found = true;
        }
        for (unsigned int i = 0; i < fieldNames.size(); i++) {
          if (tname == fieldNames[i]) {
            tmp[i] = oper;
            found = true;
            break;
          }
        }
        if (found == false) {
          for (unsigned int i = 0; i < fieldTypes.size(); i++) {
            if (ttp->UhdmType() == fieldTypes[i]->UhdmType()) {
              tmp[i] = oper;
              found = true;
              break;
            }
          }
        }
        if (found == false) {
          s.GetErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY, tname,
                              exp);
          return result;
        }
      } else {
        if (index < (int) tmp.size())
          tmp[index] = oper;
        else 
         s.GetErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY, "Out of bound!",
                              exp); 
      }
      index++;
    }
    index = 0;
    for (auto op : tmp) {
       if (defaultOp) {
        if (op == nullptr) {
          op = defaultOp;
        }
      }
      if (op == nullptr) {
        s.GetErrorHandler()(ErrorType::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN,
                            fieldNames[index], exp);
        return result;
      }
      ordered->push_back(op);
      index++;
    }
    op->Operands(ordered);
    // Flattening
    VectorOfany* flattened = s.MakeAnyVec();
    recursiveFlattening(s, flattened, ordered, fieldTypes);
    op->Operands(flattened);
  }
  return result;
}


void ExprEval::prettyPrint(Serializer& s, const any* object, uint32_t indent, std::ostream &out) {
  if (object == nullptr)
    return;
  UHDM_OBJECT_TYPE type = object->UhdmType();
  for (uint32_t i = 0; i < indent ; i++) {
        out << " ";
  }
  switch (type) {
    case uhdmconstant: {
      constant* c = (constant*) object;
      out << c->VpiDecompile();
      break;
    }
    case uhdmoperation: {
      operation* oper = (operation*) object;
      int opType = oper->VpiOpType();
      switch (opType) {
        case vpiConcatOp: {
          out << "{";
          for (uint32_t i = 0; i < oper->Operands()->size(); i++) {
            prettyPrint(s, oper->Operands()->at(i), 0, out);
            if (i < oper->Operands()->size() - 1) {
              out << ",";
            }
          }
          out << "}";
          break;
        }
        default:
          break;
      }
      break;
    }
    case uhdmref_obj: {
      out << object->VpiName();
      break;
    }
    default: {
      break;
    }
  }

}

namespace UHDM {
  std::string vPrint(UHDM::any* handle) {
    if (handle == nullptr) {
      std::cout << "NULL HANDLE\n";
      return "NULL HANDLE";
    }
    ExprEval eval;
    Serializer* s = handle->GetSerializer();
    std::stringstream out;
    eval.prettyPrint(*s, handle, 0, out);
    std::cout << out.str() << "\n";
    return out.str();
  }
}

std::string ExprEval::prettyPrint(UHDM::any* handle) {
  if (handle == nullptr) {
    std::cout << "NULL HANDLE\n";
    return "NULL HANDLE";
  }
  ExprEval eval;
  Serializer* s = handle->GetSerializer();
  std::stringstream out;
  eval.prettyPrint(*s, handle, 0, out);
  return out.str();  
}
