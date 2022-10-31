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
 * File:   ExprEval.h
 * Author: alaindargelas
 *
 * Created on July 3, 2021, 8:03 PM
 */

#ifndef UHDM_EXPREVAL_H
#define UHDM_EXPREVAL_H

#include <uhdm/expr.h>
#include <uhdm/typespec.h>

#include <functional>
#include <iostream>
#include <sstream>

namespace UHDM {
class Serializer;

typedef std::function<any*(const std::string& name, const any* inst,
                           const any* pexpr)>
    GetObjectFunctor;

typedef std::function<task_func*(const std::string& name, const any* inst)>
    GetTaskFuncFunctor;

/* This UHDM extension offers expression reduction and other utilities that can
 be operating either:
 - standalone using UHDM fully elaborated tree
 - as a utility in a greater context, example: Surelog elaboration */

class ExprEval {
 public:
  ExprEval(bool muteError = false) : m_muteError(muteError) {}
  bool isFullySpecified(const typespec* tps);

  /* Computes the size in bits of an object {typespec, var, net, operation...}.
   */
  uint64_t size(
      const any* object, bool& invalidValue, const any* inst, const any* pexpr,
      bool full /* false: use only last range size, true: use all ranges */,
      bool muteError = false);

  /* Tries to reduce any expression into a constant, returns the orignal
     expression if fails. If an invalid value is found in the process,
     invalidValue will be set to true */
  expr* reduceExpr(const any* object, bool& invalidValue, const any* inst,
                   const any* pexpr, bool muteErrors = false);

  uint64_t getValue(const UHDM::expr* expr);

  any* getValue(const std::string& name, const any* inst, const any* pexpr,
                bool muteError = false);

  any* getObject(const std::string& name, const any* inst, const any* pexpr,
                 bool muteError = false);

  int64_t get_value(bool& invalidValue, const UHDM::expr* expr);

  uint64_t get_uvalue(bool& invalidValue, const UHDM::expr* expr);

  long double get_double(bool& invalidValue, const UHDM::expr* expr);

  expr* flattenPatternAssignments(Serializer& s, const typespec* tps,
                                  expr* assignExpr);

  void prettyPrint(Serializer& s, const any* tree, uint32_t indent,
                   std::ostream& out);

  std::string prettyPrint(UHDM::any* handle);

  expr* reduceCompOp(operation* op, bool& invalidValue, const any* inst,
                     const any* pexpr, bool muteError = false);

  expr* reduceBitSelect(expr* op, unsigned int index_val, bool& invalidValue,
                        const any* inst, const any* pexpr,
                        bool muteError = false);

  void recursiveFlattening(Serializer& s, VectorOfany* flattened,
                           const VectorOfany* ordered,
                           std::vector<const typespec*> fieldTypes);

  any* decodeHierPath(hier_path* path, bool& invalidValue, const any* inst,
                      const any* pexpr, bool returnTypespec,
                      bool muteError = false);

  any* hierarchicalSelector(std::vector<std::string>& select_path,
                            unsigned int level, UHDM::any* object,
                            bool& invalidValue, const any* inst,
                            const UHDM::any* pexpr, bool returnTypespec,
                            bool muteError = false);

  typedef std::vector<const instance*> Scopes;

  UHDM::expr* evalFunc(UHDM::function* func, std::vector<UHDM::any*>* args,
                       bool& invalidValue, const any* inst, UHDM::any* pexpr,
                       bool muteError = false);

  void evalStmt(const std::string& funcName, Scopes& scopes, bool& invalidValue,
                bool& continue_flag, bool& break_flag, bool& return_flag,
                const any* inst, const UHDM::any* stmt, bool muteError = false);

  bool setValueInInstance(const std::string& lhs, any* lhsexp, expr* rhsexp,
                          bool& invalidValue, Serializer& s, const any* inst, bool muteError);
  void setDesign(design* des) { m_design = des; }
  /* For Surelog or other UHDM clients to use the UHDM expr evaluator in their
   * context */
  void setGetObjectFunctor(GetObjectFunctor func) { getObjectFunctor = func; }
  void setGetValueFunctor(GetObjectFunctor func) { getValueFunctor = func; }
  void setGetTaskFuncFunctor(GetTaskFuncFunctor func) {
    getTaskFuncFunctor = func;
  }

  UHDM::task_func* getTaskFunc(const std::string& name, const any* inst);

 private:
  GetObjectFunctor getObjectFunctor = nullptr;
  GetObjectFunctor getValueFunctor = nullptr;
  GetTaskFuncFunctor getTaskFuncFunctor = nullptr;
  const UHDM::design* m_design = nullptr;
  bool m_muteError = false;
};

std::string vPrint(UHDM::any* handle);

}  // namespace UHDM

#endif
