// -*- c++ -*-

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
 * File:   ExprEval.h
 * Author: alaindargelas
 *
 * Created on July 3, 2021, 8:03 PM
 */

#ifndef UHDM_EXPREVAL_H
#define UHDM_EXPREVAL_H

#include <uhdm/typespec.h>
#include <uhdm/expr.h>
#include <iostream>
#include <sstream>

namespace UHDM {
  class Serializer;
  class ExprEval {
 
  public:
   bool isFullySpecified(const typespec* tps);

   uint64_t size(
       const any* object, bool& invalidValue, const instance* inst,
       const any* pexpr,
       bool full /* false: use only last range size, true: use all ranges */);

   expr* reduceExpr(const any* object, bool& invalidValue, const instance* inst, const any* pexpr);

   uint64_t getValue(const UHDM::expr* expr);

   any* getValue(const std::string& name, const instance* inst, const any* pexpr);

   any* getObject(const std::string& name, const instance* inst, const any* pexpr);

   int64_t get_value(bool& invalidValue, const UHDM::expr* expr);

   uint64_t get_uvalue(bool& invalidValue, const UHDM::expr* expr);

  long double get_double(bool& invalidValue, const UHDM::expr* expr);

   expr* flattenPatternAssignments(Serializer& s, const typespec* tps,
                                   expr* assignExpr);

   void prettyPrint(Serializer& s, const any* tree, uint32_t indent,
                    std::ostream& out);

   std::string prettyPrint(UHDM::any* handle);

  private:
   expr* reduceCompOp(operation* op, bool& invalidValue, const instance* inst,
                      const any* pexpr);

   expr* reduceBitSelect(expr* op, unsigned int index_val, bool& invalidValue,
                         const instance* inst, const any* pexpr);

   void recursiveFlattening(Serializer& s, VectorOfany* flattened,
                            const VectorOfany* ordered,
                            std::vector<const typespec*> fieldTypes);
  };

  std::string vPrint(UHDM::any* handle);

}

#endif
