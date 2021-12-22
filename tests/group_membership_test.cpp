/*

 Copyright 2021 Alain Dargelas

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
 * File:   group_membership_test
 * Author: alain
 *
 * Created on Dec 22, 2021, 10:03 PM
 */

#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "uhdm/ElaboratorListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_listener.h"
#include "uhdm/vpi_visitor.h"

using namespace UHDM;

//-------------------------------------------

TEST(GroupTest, Membership) {
  Serializer s;
  sequence_inst* inst = s.MakeSequence_inst();
  VectorOfany* exprs = s.MakeAnyVec();
  constant* legal = s.MakeConstant();
  exprs->push_back(legal);
  module* illegal = s.MakeModule();
  inst->Named_event_sequence_expr_groups(exprs);
  VectorOfany* all_legal = inst->Named_event_sequence_expr_groups();
  EXPECT_EQ(all_legal->size(), 1);

  sequence_inst* inst2 = s.MakeSequence_inst();
  VectorOfany* exprs2 = s.MakeAnyVec();
  exprs2->push_back(illegal);
  inst2->Named_event_sequence_expr_groups(exprs2);
  VectorOfany* all_illegal = inst2->Named_event_sequence_expr_groups();
  EXPECT_EQ(all_illegal, nullptr);
}
