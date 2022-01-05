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
 * File:   UhdmLint.cpp
 * Author: alaindargelas
 *
 * Created on Jan 3, 2022, 9:03 PM
 */

#include <uhdm/UhdmLint.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

namespace UHDM {

void UhdmLint::leaveBit_select(const bit_select* object,
                               const BaseClass* parent, vpiHandle handle,
                               vpiHandle parentHandle) {
  const expr* index = object->VpiIndex();
  if (index) {
    if (index->UhdmType() == uhdmref_obj) {
      ref_obj* ref = (ref_obj*)index;
      const any* act = ref->Actual_group();
      if (act && act->UhdmType() == uhdmreal_var) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_REAL_TYPE_AS_SELECT,
                                       act->VpiName(), ref);
      }
    }
  }
}

static const any* returnWithValue(const any* stmt) {
  switch (stmt->UhdmType()) {
    case uhdmreturn_stmt: {
      return_stmt* ret = (return_stmt*)stmt;
      if (const any* r = ret->VpiCondition()) return r;
      break;
    }
    case uhdmbegin: {
      begin* st = (begin*)stmt;
      for (auto s : *st->Stmts()) {
        if (const any* r = returnWithValue(s)) return r;
      }
      break;
    }
    case uhdmif_stmt: {
      if_stmt* st = (if_stmt*) stmt;
      if (const any* r = returnWithValue(st->VpiStmt())) return r;
      break;
    }
    case uhdmif_else: {
      if_else* st = (if_else*) stmt;
      if (const any* r = returnWithValue(st->VpiStmt())) return r;
      if (const any* r = returnWithValue(st->VpiElseStmt())) return r;
      break;
    }
    default:
      break;
  }
  return nullptr;
}

void UhdmLint::leaveFunction(const function* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) {
  if (object->Return() == nullptr) {
    if (const any* st = object->Stmt()) {
      const any* ret = returnWithValue(st);
      if (ret) {
        serializer_->GetErrorHandler()(ErrorType::UHDM_RETURN_VALUE_VOID_FUNCTION,
                                       object->VpiName(), ret);
      }
    }
  }

}

}  // namespace UHDM
