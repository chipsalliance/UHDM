// -*- c++ -*-

/*

 Copyright 2019-2022 Alain Dargelas

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
 * File:   SynthSubset.cpp
 * Author: alaindargelas
 *
 * Created on Feb 16, 2022, 9:03 PM
 */
#include <uhdm/ElaboratorListener.h>
#include <uhdm/ExprEval.h>
#include <uhdm/Serializer.h>
#include <uhdm/SynthSubset.h>
#include <uhdm/Utils.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>

#include <algorithm>
#include <cstring>

namespace uhdm {

SynthSubset::SynthSubset(Serializer* serializer,
                         std::set<const Any*>& nonSynthesizableObjects,
                         Design* des, bool reportErrors, bool allowFormal)
    : m_serializer(serializer),
      m_nonSynthesizableObjects(nonSynthesizableObjects),
      m_design(des),
      m_reportErrors(reportErrors),
      m_allowFormal(allowFormal) {
  constexpr std::string_view kDollar("$");
  for (auto s :
       {// "display",
        "write", "strobe", "monitor", "monitoron", "monitoroff", "displayb",
        "writeb", "strobeb", "monitorb", "displayo", "writeo", "strobeo",
        "monitoro", "displayh", "writeh", "strobeh", "monitorh", "fopen",
        "fclose", "frewind", "fflush", "fseek", "ftell", "fdisplay", "fwrite",
        "swrite", "fstrobe", "fmonitor", "fread", "fscanf", "fdisplayb",
        "fwriteb", "swriteb", "fstrobeb", "fmonitorb", "fdisplayo", "fwriteo",
        "swriteo", "fstrobeo", "fmonitoro", "fdisplayh", "fwriteh", "swriteh",
        "fstrobeh", "fmonitorh", "sscanf", "sdf_annotate", "sformat",
        // "cast",
        "assertkill", "assertoff", "asserton",
        // "bits",
        // "bitstoshortreal",
        "countones", "coverage_control", "coverage_merge", "coverage_save",
        // "dimensions",
        // "error",
        "exit",
        // "fatal",
        "fell", "get_coverage", "coverage_get", "coverage_get_max",
        // "high",
        //"increment",
        "info", "isunbounded", "isunknown",
        // "left",
        "load_coverage_db",
        // "low",
        "onehot", "past",
        // "readmemb",
        // "readmemh",
        //"right",
        "root", "rose", "sampled", "set_coverage_db_name",
        // "shortrealtobits",
        // "size",
        "stable",
        // "typename",
        // "typeof",
        "unit", "urandom", "srandom", "urandom_range", "set_randstate",
        "get_randstate", "dist_uniform", "dist_normal", "dist_exponential",
        "dist_poisson", "dist_chi_square", "dist_t", "dist_erlang",
        // "warning",
        // "writememb",
        // "writememh",
        "value$plusargs"}) {
    m_nonSynthSysCalls.emplace(std::move(std::string(kDollar).append(s)));
  }
}

void SynthSubset::report(std::ostream& out) {
  for (auto object : m_nonSynthesizableObjects) {
    VpiVisitor::visited_t visitedObjects;

    vpiHandle dh =
        object->getSerializer()->makeUhdmHandle(object->getUhdmType(), object);

    visit_object(dh, out, true);
    vpi_release_handle(dh);
  }
}

void SynthSubset::reportError(const Any* object) {
  const Any* tmp = object;
  while (tmp && tmp->getFile().empty()) {
    tmp = tmp->getParent();
  }
  if (tmp) object = tmp;
  if (m_reportErrors && !reportedParent(object)) {
    if (!object->getFile().empty()) {
      const std::string errMsg(object->getName());
      m_serializer->getErrorHandler()(ErrorType::UHDM_NON_SYNTHESIZABLE, errMsg,
                                      object, nullptr);
    }
  }
  mark(object);
}

void SynthSubset::leaveAny(const Any* object, vpiHandle handle) {
  switch (object->getUhdmType()) {
    case UhdmType::FinalStmt:
    case UhdmType::DelayControl:
    case UhdmType::DelayTerm:
    case UhdmType::Thread:
    case UhdmType::WaitStmt:
    case UhdmType::WaitFork:
    case UhdmType::OrderedWait:
    case UhdmType::Disable:
    case UhdmType::DisableFork:
    case UhdmType::Force:
    case UhdmType::Deassign:
    case UhdmType::Release:
    case UhdmType::SequenceInst:
    case UhdmType::SeqFormalDecl:
    case UhdmType::SequenceDecl:
    case UhdmType::PropFormalDecl:
    case UhdmType::PropertyInst:
    case UhdmType::PropertySpec:
    case UhdmType::PropertyDecl:
    case UhdmType::ClockedProperty:
    case UhdmType::CasePropertyItem:
    case UhdmType::CaseProperty:
    case UhdmType::MulticlockSequenceExpr:
    case UhdmType::ClockedSeq:
    case UhdmType::Variable:
    case UhdmType::CheckerPort:
    case UhdmType::CheckerInstPort:
    case UhdmType::SwitchTran:
    case UhdmType::Udp:
    case UhdmType::ModPath:
    case UhdmType::Tchk:
    case UhdmType::UdpDefn:
    case UhdmType::TableEntry:
    case UhdmType::ClockingBlock:
    case UhdmType::ClockingIODecl:
    case UhdmType::ProgramArray:
    case UhdmType::SwitchArray:
    case UhdmType::UdpArray:
    case UhdmType::TchkTerm:
    case UhdmType::NamedEvent:
    case UhdmType::Extends:
    case UhdmType::ClassDefn:
    case UhdmType::ClassObj:
    case UhdmType::Program:
    case UhdmType::CheckerDecl:
    case UhdmType::CheckerInst:
    case UhdmType::ShortRealTypespec:
    case UhdmType::RealTypespec:
    case UhdmType::TimeTypespec:
    case UhdmType::ChandleTypespec:
    case UhdmType::SequenceTypespec:
    case UhdmType::PropertyTypespec:
    case UhdmType::UserSystf:
    case UhdmType::MethodFuncCall:
    case UhdmType::MethodTaskCall:
    case UhdmType::ConstraintOrdering:
    case UhdmType::Constraint:
    case UhdmType::Distribution:
    case UhdmType::DistItem:
    case UhdmType::Implication:
    case UhdmType::ConstrIf:
    case UhdmType::ConstrIfElse:
    case UhdmType::ConstrForeach:
    case UhdmType::SoftDisable:
    case UhdmType::ForkStmt:
    case UhdmType::EventStmt:
    case UhdmType::EventTypespec:
      reportError(object);
      break;
    case UhdmType::ExpectStmt:
    case UhdmType::Cover:
    case UhdmType::Assume:
    case UhdmType::Restrict:
    case UhdmType::ImmediateAssume:
    case UhdmType::ImmediateCover:
      if (!m_allowFormal) reportError(object);
      break;
    case UhdmType::Net:
      if (getTypespec<TimeTypespec>(object) != nullptr) {
        if (!m_allowFormal) reportError(object);
      }
      break;
    default:
      break;
  }
}

void SynthSubset::leaveTask(const Task* topobject, vpiHandle handle) {
  // Give specific error for non-synthesizable tasks
  std::function<void(const Any*, const Any*)> inst_visit =
      [&inst_visit, this](const Any* stmt, const Any* top) {
        UhdmType type = stmt->getUhdmType();
        AnyCollection* stmts = nullptr;
        if (type == UhdmType::Begin) {
          Begin* b = (Begin*)stmt;
          stmts = b->getStmts();
        }
        if (stmts) {
          for (auto st : *stmts) {
            UhdmType sttype = st->getUhdmType();
            switch (sttype) {
              case UhdmType::WaitStmt:
              case UhdmType::WaitFork:
              case UhdmType::OrderedWait:
              case UhdmType::Disable:
              case UhdmType::DisableFork:
              case UhdmType::Force:
              case UhdmType::Deassign:
              case UhdmType::Release:
              case UhdmType::SoftDisable:
              case UhdmType::ForkStmt:
              case UhdmType::EventStmt: {
                reportError(top);
                break;
              }
              default: {
              }
            }
            inst_visit(st, top);
          }
        }
      };

  if (const Any* stmt = topobject->getStmt()) {
    inst_visit(stmt, topobject);
  }
}

void SynthSubset::leaveSysTaskCall(const SysTaskCall* object,
                                   vpiHandle handle) {
  const std::string_view name = object->getName();
  if (m_nonSynthSysCalls.find(name) != m_nonSynthSysCalls.end()) {
    reportError(object);
  }
}

SysFuncCall* SynthSubset::makeStubDisplayStmt(const Any* object) {
  SysFuncCall* display = m_serializer->make<SysFuncCall>();
  display->setName("$display");
  AnyCollection* arguments = m_serializer->makeCollection<Any>();
  Constant* c = m_serializer->make<Constant>();
  c->setConstType(vpiStringVal);
  std::string text = "Stub for non-synthesizable stmt";
  c->setValue("STRING:" + text);
  c->setDecompile(text);
  c->setSize(text.size());
  arguments->push_back(c);
  display->setArguments(arguments);
  return display;
}

bool objectIsInitialBlock(const Any* object) {
  bool inInitialBlock = false;
  const Any* parent = object->getParent();
  while (parent) {
    if (parent->getUhdmType() == UhdmType::Initial) {
      inInitialBlock = true;
      break;
    }
    parent = parent->getParent();
  }
  return inInitialBlock;
}

void SynthSubset::removeFromVector(AnyCollection* vec, const Any* object) {
  AnyCollection::iterator itr = vec->begin();
  for (auto s : *vec) {
    if (s == object) {
      vec->erase(itr);
      if (vec->empty()) {
        const std::string_view name = object->getName();
        if (name == "$error" || name == "$finish" || name == "$display") {
          bool inInitialBLock = objectIsInitialBlock(object);
          if (!inInitialBLock) vec->push_back(makeStubDisplayStmt(object));
        } else {
          vec->push_back(makeStubDisplayStmt(object));
        }
      }
      break;
    }
    itr++;
  }
}

void SynthSubset::removeFromStmt(Any* parent, const Any* object) {
  if (parent->getUhdmType() == UhdmType::ForStmt) {
    ForStmt* st = (ForStmt*)parent;
    st->setStmt(makeStubDisplayStmt(object));
  } else if (parent->getUhdmType() == UhdmType::IfStmt) {
    IfStmt* st = (IfStmt*)parent;
    st->setStmt(makeStubDisplayStmt(object));
  } else if (parent->getUhdmType() == UhdmType::IfElse) {
    IfElse* st = (IfElse*)parent;
    if (st->getStmt() && (st->getStmt() == object))
      st->setStmt(makeStubDisplayStmt(object));
    else if (st->getElseStmt() && (st->getElseStmt() == object))
      st->setElseStmt(makeStubDisplayStmt(object));
  } else if (parent->getUhdmType() == UhdmType::Initial) {
    Initial* st = (Initial*)parent;
    const std::string_view name = object->getName();
    if (name == "$error" || name == "$finish") {
      st->setStmt(makeStubDisplayStmt(object));
    } else if (name == "$display") {
      // No better alternative than to keep the statement
    } else {
      st->setStmt(makeStubDisplayStmt(object));
    }
  }
}

void SynthSubset::filterNonSynthesizable() {
  for (auto p : m_scheduledFilteredObjectsInVector) {
    removeFromVector(p.first, p.second);
  }
  for (auto p : m_scheduledFilteredObjectsInStmt) {
    removeFromStmt(p.first, p.second);
  }
}

void SynthSubset::leaveSysFuncCall(const SysFuncCall* object,
                                   vpiHandle handle) {
  const std::string_view name = object->getName();
  if (m_nonSynthSysCalls.find(name) != m_nonSynthSysCalls.end()) {
    reportError(object);
    const Any* parent = object->getParent();
    if (parent->getUhdmType() == UhdmType::Begin) {
      Begin* st = (Begin*)parent;
      if (st->getStmts()) {
        m_scheduledFilteredObjectsInVector.emplace_back(st->getStmts(), object);
      }
    } else if (parent->getUhdmType() == UhdmType::ForStmt) {
      ForStmt* st = (ForStmt*)parent;
      if (st->getStmt()) {
        m_scheduledFilteredObjectsInStmt.emplace_back(st, object);
      }
    } else if (parent->getUhdmType() == UhdmType::IfStmt) {
      IfStmt* st = (IfStmt*)parent;
      if (st->getStmt()) {
        m_scheduledFilteredObjectsInStmt.emplace_back(st, object);
      }
    } else if (parent->getUhdmType() == UhdmType::IfElse) {
      IfElse* st = (IfElse*)parent;
      if (st->getStmt() && (st->getStmt() == object)) {
        m_scheduledFilteredObjectsInStmt.emplace_back(st, object);
      } else if (st->getElseStmt() && (st->getElseStmt() == object)) {
        m_scheduledFilteredObjectsInStmt.emplace_back(st, object);
      }
    } else if (parent->getUhdmType() == UhdmType::Initial) {
      Initial* st = (Initial*)parent;
      if (st->getStmt()) {
        m_scheduledFilteredObjectsInStmt.emplace_back(st, object);
      }
    }
  }
  // Filter out sys func calls stmt from initial block
  if (name == "$error" || name == "$finish" || name == "$display") {
    bool inInitialBlock = objectIsInitialBlock(object);
    if (inInitialBlock) {
      const Any* parent = object->getParent();
      if (parent->getUhdmType() == UhdmType::Begin) {
        Begin* st = (Begin*)parent;
        if (st->getStmts()) {
          m_scheduledFilteredObjectsInVector.emplace_back(st->getStmts(),
                                                          object);
        }
      } else if (parent->getUhdmType() == UhdmType::Initial) {
        Initial* st = (Initial*)parent;
        if (st->getStmt()) {
          m_scheduledFilteredObjectsInStmt.emplace_back(st, object);
        }
      }
    }
  }
}

void SynthSubset::leaveClassTypespec(const ClassTypespec* object,
                                     vpiHandle handle) {
  if (const Any* def = object->getClassDefn())
    reportError(def);
  else
    reportError(object);
}

void SynthSubset::leaveVariable(const Variable* object, vpiHandle handle) {
  if (const RefTypespec* rt = object->getTypespec()) {
    if (const ClassTypespec* spec = rt->getActual<ClassTypespec>()) {
      if (const ClassDefn* def = spec->getClassDefn()) {
        if (reportedParent(def)) {
          mark(object);
          return;
        }
      }
    }
  }
  reportError(object);
}

void SynthSubset::mark(const Any* object) {
  m_nonSynthesizableObjects.emplace(object);
}

bool SynthSubset::reportedParent(const Any* object) {
  if (object->getUhdmType() == UhdmType::Package) {
    if (object->getName() == "builtin") return true;
  } else if (object->getUhdmType() == UhdmType::ClassDefn) {
    if (object->getName() == "work@semaphore" ||
        object->getName() == "work@process" ||
        object->getName() == "work@mailbox")
      return true;
  }
  if (m_nonSynthesizableObjects.find(object) !=
      m_nonSynthesizableObjects.end()) {
    return true;
  }
  if (const Any* parent = object->getParent()) {
    return reportedParent(parent);
  }
  return false;
}

// Apply some rewrite rule for Synlig limitations, namely Synlig handles aliased
// typespec incorrectly.
void SynthSubset::leaveRefTypespec(const RefTypespec* object,
                                   vpiHandle handle) {
  if (const TypedefTypespec* actual = object->getActual<TypedefTypespec>()) {
    if (const RefTypespec* ref_alias = actual->getTypedefAlias()) {
      // Make the typespec point to the aliased typespec if they are of the same
      // type:
      //   typedef lc_tx_e lc_tx_t;
      // When extra dimensions are added using a packed_array_typespec like in:
      //  typedef lc_tx_e [1:0] lc_tx_t;
      //  We will need to uniquify and create a new typespec instance
      if ((ref_alias->getActual()->getUhdmType() == actual->getUhdmType()) &&
          !ref_alias->getActual()->getName().empty()) {
        ((RefTypespec*)object)->setActual((Typespec*)ref_alias->getActual());
      }
    }
  }
}

void SynthSubset::leaveForStmt(const ForStmt* object, vpiHandle handle) {
  if (const Expr* cond = object->getCondition()) {
    if (cond->getUhdmType() == UhdmType::Operation) {
      Operation* topOp = (Operation*)cond;
      AnyCollection* operands = topOp->getOperands();
      const Any* parent = object->getParent();
      if (topOp->getOpType() == vpiLogAndOp) {
        // Rewrite rule for Yosys (Cannot handle non-constant expression in for
        // loop condition besides loop var)
        // Transforms:
        //  for (int i=0; i<32 && found==0; i++) begin
        //  end
        // Into:
        //  for (int i=0; i<32; i++) begin
        //    if (found==0) break;
        //  end
        //
        // Assumes lhs is comparator over loop var
        // rhs is Any expression
        Any* lhs = operands->at(0);
        Any* rhs = operands->at(1);
        ((ForStmt*)object)->setCondition((Expr*)lhs);
        AnyCollection* stlist = nullptr;
        if (const Any* stmt = object->getStmt()) {
          if (stmt->getUhdmType() == UhdmType::Begin) {
            Begin* st = (Begin*)stmt;
            stlist = st->getStmts();
          }
          if (stlist) {
            IfStmt* ifstmt = m_serializer->make<IfStmt>();
            stlist->insert(stlist->begin(), ifstmt);
            ifstmt->setCondition((Expr*)rhs);
            BreakStmt* brk = m_serializer->make<BreakStmt>();
            ifstmt->setStmt(brk);
          }
        }
      } else {
        if (isInUhdmAllIterator()) return;
        // Rewrite rule for Yosys (Cannot handle non-constant expression as a
        // bound for loop var) Transforms:
        //   logic [1:0] bound;
        //   for(j=0;j<bound;j=j+1) Q = 1'b1;
        // Into:
        //   case (i)
        //     0 :
        //       for(j=0;j<0;j=j+1) Q = 1'b1;
        //     1 :
        //       for(j=0;j<1;j=j+1) Q = 1'b1;
        //   endcase
        bool needsTransform = false;
        Net* var = nullptr;
        if (operands->size() == 2) {
          Any* op = operands->at(1);
          if (op->getUhdmType() == UhdmType::RefObj) {
            RefObj* ref = (RefObj*)op;
            if (Net* actual = ref->getActual<Net>()) {
              if (getTypespec<LogicTypespec>(actual) != nullptr) {
                needsTransform = true;
                var = actual;
              }
            }
          }
        }
        if (needsTransform) {
          // Check that we are in an always stmt
          needsTransform = false;
          const Any* tmp = parent;
          while (tmp) {
            if (tmp->getUhdmType() == UhdmType::Always) {
              needsTransform = true;
              break;
            }
            tmp = tmp->getParent();
          }
        }
        if (needsTransform) {
          ExprEval eval;
          bool invalidValue = false;
          uint32_t size = eval.size(var, invalidValue, parent->getParent(),
                                    parent, true, true);
          CaseStmt* case_st = m_serializer->make<CaseStmt>();
          case_st->setCaseType(vpiCaseExact);
          case_st->setParent((Any*)parent);
          AnyCollection* stmts = nullptr;
          if (parent->getUhdmType() == UhdmType::Begin) {
            stmts = any_cast<Begin>(parent)->getStmts();
          }
          if (stmts) {
            // Substitute the for loop with a case stmt
            for (AnyCollection::iterator itr = stmts->begin();
                 itr != stmts->end(); itr++) {
              if ((*itr) == object) {
                stmts->insert(itr, case_st);
                break;
              }
            }
            for (AnyCollection::iterator itr = stmts->begin();
                 itr != stmts->end(); itr++) {
              if ((*itr) == object) {
                stmts->erase(itr);
                break;
              }
            }
          }
          // Construct the case stmt
          RefObj* ref = m_serializer->make<RefObj>();
          ref->setName(var->getName());
          ref->setActual(var);
          ref->setParent(case_st);
          case_st->setCondition(ref);
          CaseItemCollection* items = m_serializer->makeCollection<CaseItem>();
          case_st->setCaseItems(items);
          for (uint32_t i = 0; i < size; i++) {
            CaseItem* item = m_serializer->make<CaseItem>();
            item->setParent(case_st);
            Constant* c = m_serializer->make<Constant>();
            c->setConstType(vpiUIntConst);
            c->setValue("UINT:" + std::to_string(i));
            c->setDecompile(std::to_string(i));
            c->setParent(item);
            AnyCollection* exprs = m_serializer->makeCollection<Any>();
            exprs->push_back(c);
            item->setExprs(exprs);
            items->push_back(item);
            ElaboratorContext elaboratorContext(m_serializer);
            ForStmt* clone = (ForStmt*)clone_tree(object, &elaboratorContext);
            clone->setParent(item);
            Operation* cond_op = any_cast<Operation>(clone->getCondition());
            AnyCollection* operands = cond_op->getOperands();
            for (uint32_t ot = 0; ot < operands->size(); ot++) {
              if (operands->at(ot)->getName() == var->getName()) {
                operands->at(ot) = c;
                break;
              }
            }
            item->setStmt(clone);
          }
        }
      }
    }
  }
}

void SynthSubset::leavePort(const Port* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  bool signedLowConn = false;
  if (const Any* lc = object->getLowConn()) {
    if (const RefObj* ref = any_cast<RefObj>(lc)) {
      if (const Variable* const actual = ref->getActual<Variable>()) {
        if (const LogicTypespec* const typespec =
                getTypespec<LogicTypespec>(actual)) {
          if (typespec->getSigned()) signedLowConn = true;
        }
      } else if (const Net* const actual = ref->getActual<Net>()) {
        if (const LogicTypespec* const typespec =
                getTypespec<LogicTypespec>(actual)) {
          if (typespec->getSigned()) signedLowConn = true;
        }
      }
    }
  }
  if (signedLowConn) return;

  std::string highConnSignal;
  const Any* reportObject = object;
  if (const Any* hc = object->getHighConn()) {
    if (const RefObj* ref = any_cast<RefObj>(hc)) {
      reportObject = ref;
      if (const Variable* actual = ref->getActual<Variable>()) {
        if (const LogicTypespec* const lt =
                getTypespec<LogicTypespec>(actual)) {
          if (lt->getSigned()) {
            highConnSignal = actual->getName();
            const_cast<LogicTypespec*>(lt)->setSigned(false);
          }
        }
      }
    } else if (const Net* const actual = ref->getActual<Net>()) {
      if (const LogicTypespec* const lt = getTypespec<LogicTypespec>(actual)) {
        if (lt->getSigned()) {
          highConnSignal = actual->getName();
          const_cast<LogicTypespec*>(lt)->setSigned(false);
        }
      }
    }
  }
  if (!highConnSignal.empty()) {
    const std::string errMsg(highConnSignal);
    m_serializer->getErrorHandler()(ErrorType::UHDM_FORCING_UNSIGNED_TYPE,
                                    errMsg, reportObject, nullptr);
  }
}

void SynthSubset::leaveAlways(const Always* object, vpiHandle handle) {
  sensitivityListRewrite(object, handle);
  blockingToNonBlockingRewrite(object, handle);
}

// Transform 3 vars sensitivity list into 2 vars sensitivity list because of a
// Yosys limitation
void SynthSubset::sensitivityListRewrite(const Always* object,
                                         vpiHandle handle) {
  // Transform: always @ (posedge clk or posedge rst or posedge start)
  //              if (rst | start) ...
  // Into:
  //            wire \synlig_tmp = rst | start;
  //            always @ (posedge clk or posedge \synlig_tmp )
  //               if (\synlig_tmp ) ...
  if (const Any* stmt = object->getStmt()) {
    if (const EventControl* ec = any_cast<EventControl>(stmt)) {
      if (const Operation* cond_op = any_cast<Operation>(ec->getCondition())) {
        AnyCollection* operands_top = cond_op->getOperands();
        AnyCollection* operands_op0 = nullptr;
        AnyCollection* operands_op1 = nullptr;
        Any* opLast = nullptr;
        int totalOperands = 0;
        if (operands_top->size() > 1) {
          if (operands_top->at(0)->getUhdmType() == UhdmType::Operation) {
            Operation* op = (Operation*)operands_top->at(0);
            operands_op0 = op->getOperands();
            totalOperands += operands_op0->size();
          }
          if (operands_top->at(1)->getUhdmType() == UhdmType::Operation) {
            Operation* op = (Operation*)operands_top->at(1);
            opLast = op;
            operands_op1 = op->getOperands();
            totalOperands += operands_op1->size();
          }
        }
        if (totalOperands != 3) {
          return;
        }
        Any* opMiddle = operands_op0->at(1);
        if (opMiddle->getUhdmType() == UhdmType::Operation &&
            opLast->getUhdmType() == UhdmType::Operation) {
          Operation* opM = (Operation*)opMiddle;
          Operation* opL = (Operation*)opLast;
          Any* midVar = opM->getOperands()->at(0);
          std::string_view var2Name = midVar->getName();
          std::string_view var3Name = opL->getOperands()->at(0)->getName();
          if (opM->getOpType() == opL->getOpType()) {
            AnyCollection* stmts = nullptr;
            if (const Scope* st = any_cast<Scope>(ec->getStmt())) {
              if (st->getUhdmType() == UhdmType::Begin) {
                stmts = any_cast<Begin>(st)->getStmts();
              }
            } else if (const Any* st = any_cast<Any>(ec->getStmt())) {
              stmts = m_serializer->makeCollection<Any>();
              stmts->push_back((Any*)st);
            }
            if (!stmts) return;
            for (auto stmt : *stmts) {
              Expr* cond = nullptr;
              if (stmt->getUhdmType() == UhdmType::IfElse) {
                cond = any_cast<IfElse>(stmt)->getCondition();
              } else if (stmt->getUhdmType() == UhdmType::IfStmt) {
                cond = any_cast<IfStmt>(stmt)->getCondition();
              } else if (stmt->getUhdmType() == UhdmType::CaseStmt) {
                cond = any_cast<CaseStmt>(stmt)->getCondition();
              }
              if (cond->getUhdmType() == UhdmType::Operation) {
                Operation* op = (Operation*)cond;
                // Check that the sensitivity vars are used as a or-ed
                // condition
                if (op->getOpType() == vpiBitOrOp) {
                  AnyCollection* operands = op->getOperands();
                  if (operands->at(0)->getName() == var2Name &&
                      operands->at(1)->getName() == var3Name) {
                    // All conditions are met, perform the transformation

                    // Remove: "posedge rst" from that part of the tree
                    operands_op0->pop_back();

                    // Create expression: rst | start
                    Operation* orOp = m_serializer->make<Operation>();
                    orOp->setOpType(vpiBitOrOp);
                    orOp->setOperands(m_serializer->makeCollection<Any>());
                    orOp->getOperands()->push_back(midVar);
                    orOp->getOperands()->push_back(opL->getOperands()->at(0));

                    // Move up the tree: posedge clk
                    operands_top->at(0) = operands_op0->at(0);

                    // Create: wire \synlig_tmp = rst | start;
                    ContAssign* ass = m_serializer->make<ContAssign>();
                    Net* lhs = m_serializer->make<Net>();
                    std::string tmpName = std::string("synlig_tmp_") +
                                          std::string(var2Name) + "_or_" +
                                          std::string(var3Name);
                    lhs->setName(tmpName);
                    ass->setLhs(lhs);
                    RefObj* ref = m_serializer->make<RefObj>();
                    ref->setName(tmpName);
                    ref->setActual(lhs);
                    ass->setRhs(orOp);
                    const Any* instance = object->getParent();
                    if (instance->getUhdmType() == UhdmType::Module) {
                      Module* mod = (Module*)instance;
                      if (mod->getContAssigns() == nullptr) {
                        mod->setContAssigns(
                            m_serializer->makeCollection<ContAssign>());
                      }
                      bool found = false;
                      for (ContAssign* ca : *mod->getContAssigns()) {
                        if (ca->getLhs()->getName() == tmpName) {
                          found = true;
                          break;
                        }
                      }
                      if (!found) mod->getContAssigns()->push_back(ass);
                    }

                    // Redirect condition to: if (\synlig_tmp ) ...
                    if (stmt->getUhdmType() == UhdmType::IfElse) {
                      any_cast<IfElse>(stmt)->setCondition(ref);
                    } else if (stmt->getUhdmType() == UhdmType::IfStmt) {
                      any_cast<IfStmt>(stmt)->setCondition(ref);
                    } else if (stmt->getUhdmType() == UhdmType::CaseStmt) {
                      any_cast<CaseStmt>(stmt)->setCondition(ref);
                    }

                    // Redirect 2nd sensitivity list signal to: posedge
                    // \synlig_tmp
                    opL->getOperands()->at(0) = ref;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void collectAssignmentStmt(
    const Any* stmt, std::vector<const Assignment*>& blocking_assigns,
    std::vector<const Assignment*>& nonblocking_assigns) {
  if (stmt == nullptr) return;
  UhdmType stmt_type = stmt->getUhdmType();
  switch (stmt_type) {
    case UhdmType::Begin: {
      AnyCollection* stmts = any_cast<Begin*>(stmt)->getStmts();
      if (stmts)
        for (auto stmt : *stmts) {
          collectAssignmentStmt(stmt, blocking_assigns, nonblocking_assigns);
        }
      break;
    }
    case UhdmType::IfElse: {
      const Any* the_stmt = any_cast<IfElse*>(stmt)->getStmt();
      collectAssignmentStmt(the_stmt, blocking_assigns, nonblocking_assigns);
      const Any* else_stmt = any_cast<IfElse*>(stmt)->getElseStmt();
      collectAssignmentStmt(else_stmt, blocking_assigns, nonblocking_assigns);
      break;
    }
    case UhdmType::IfStmt: {
      const Any* the_stmt = any_cast<IfStmt*>(stmt)->getStmt();
      collectAssignmentStmt(the_stmt, blocking_assigns, nonblocking_assigns);
      break;
    }
    case UhdmType::CaseStmt: {
      // VectorOfcase_item* items = any_cast<case_stmt*>(stmt)->Case_items();
      //  TODO
      break;
    }
    case UhdmType::Assignment: {
      const Assignment* as = any_cast<Assignment*>(stmt);
      if (as->getBlocking()) {
        blocking_assigns.push_back(as);
      } else {
        nonblocking_assigns.push_back(as);
      }
      break;
    }
    default:
      break;
  }
}

// Transforms the following to enable RAM inference:
//    if (we)
//      RAM[addr] = di;
//    read = RAM[addr];
// Into:
//    if (we)
//      RAM[addr] <= di;
//    read <= RAM[addr];
void SynthSubset::blockingToNonBlockingRewrite(const Always* object,
                                               vpiHandle handle) {
  if (const Any* stmt = object->getStmt()) {
    if (const EventControl* ec = any_cast<EventControl*>(stmt)) {
      // Collect all the blocking and non blocking assignments
      std::vector<const Assignment*> blocking_assigns;
      std::vector<const Assignment*> nonblocking_assigns;
      collectAssignmentStmt(ec->getStmt(), blocking_assigns,
                            nonblocking_assigns);
      // Identify a potential RAM in the LHSs
      std::string ram_name;
      // 1) It has to be a LHS of a blocking assignment to be a candidate
      // for (const Assignment* stmt : blocking_assigns) {
      //   const Expr* lhs = stmt->getLhs();
      //   // LHS assigns to a bit select
      //   // RAM[addr] = ...
      //   if (lhs->getUhdmType() == UhdmType::BitSelect) {
      //     // The actual has to be an array_net with 2 dimensions (packed and
      //     // unpacked):
      //     const BitSelect* bs = any_cast<BitSelect*>(lhs);
      //     const Any* actual = bs->getActual();
      //     if (actual && (actual->getUhdmType() == UhdmType::ArrayNet)) {
      //       const ArrayNet* arr_net = any_cast<ArrayNet*>(actual);
      //       if (arr_net->getRanges()) {  // Unpacked dimension
      //         if (NetCollection* nets = arr_net->getNets()) {
      //           if (nets->size()) {
      //             Net* n = nets->at(0);
      //             RefTypespec* reft = n->getTypespec();
      //             Typespec* tps = reft->getActual();
      //             bool has_packed_dimm = false;  // Packed dimension
      //             if (tps->getUhdmType() == UhdmType::LogicTypespec) {
      //               LogicTypespec* ltps = any_cast<LogicTypespec>(tps);
      //               if (ltps->getRanges()) {
      //                 has_packed_dimm = true;
      //               }
      //             }
      //             if (has_packed_dimm) {
      //               ram_name = lhs->getName();
      //             }
      //           }
      //         }
      //       }
      //     }
      //   }
      // }
      // 2) It cannot be a LHS of a non blocking assignment
      for (const Assignment* stmt : nonblocking_assigns) {
        const Expr* lhs = stmt->getLhs();
        if (lhs->getName() == ram_name) {
          // Invalidate the candidate
          ram_name = "";
        }
      }
      // 3) Check that it is referenced in RHS of blocking assignments exactly
      // once, and assigned exactly once
      int countAssignments = 0;
      int countUsages = 0;
      if (!ram_name.empty()) {
        for (const Assignment* stmt : blocking_assigns) {
          const Expr* lhs = stmt->getLhs();
          const Any* rhs = stmt->getRhs();
          if (lhs && lhs->getName() == ram_name) {
            countAssignments++;
          }
          if (rhs && rhs->getName() == ram_name) {
            countUsages++;
          }
        }
      }
      if ((countUsages == 1) && (countAssignments == 1)) {
        // Match all the criteria: Convert all blocking assignments writing or
        // reading the ram to non blocking
        for (const Assignment* stmt : blocking_assigns) {
          const Expr* lhs = stmt->getLhs();
          const Any* rhs = stmt->getRhs();
          if ((lhs && lhs->getName() == ram_name) ||
              (rhs && rhs->getName() == ram_name)) {
            ((Assignment*)stmt)->setBlocking(false);
          }
        }
      }
    }
  }
}

void SynthSubset::leaveNet(const Net* object, vpiHandle handle) {
  if (!isInUhdmAllIterator()) return;
  if (getTypespec<LogicTypespec>(object) != nullptr) {
    const_cast<Net*>(object)->setTypespec(nullptr);
  }
}

}  // namespace uhdm
