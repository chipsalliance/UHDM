/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 Copyright 2022 The UHDM Team.

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

#include <uhdm/SymbolFactory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

namespace UHDM {
namespace {
TEST(SymbolFactoryTest, SymbolFactoryAccess) {
  SymbolFactory table;

  const SymbolFactory::ID foo_id = table.Make("foo");
  EXPECT_NE(foo_id, SymbolFactory::getBadId());

  const SymbolFactory::ID bar_id = table.Make("bar");
  EXPECT_NE(foo_id, bar_id);

  // Attempting to register the same symbol will result in original ID.
  EXPECT_EQ(table.Make("foo"), foo_id);
  EXPECT_EQ(table.Make("bar"), bar_id);

  // Retrieve symbol-ID by text string
  EXPECT_EQ(table.GetId("foo"), foo_id);
  EXPECT_EQ(table.GetId("bar"), bar_id);
  EXPECT_EQ(table.GetId("baz"), SymbolFactory::getBadId());  // no-exist

  // Make sure comparisons don't latch on the address of the string constants
  // (as the linker will make all "foo" constants appear on the same address),
  // but actually do the find by content
  const std::string stringsource("foobar");
  EXPECT_EQ(table.GetId(stringsource.substr(0, 3)), foo_id);
  EXPECT_EQ(table.GetId(stringsource.substr(3, 3)), bar_id);

  // Retrieve text symbol by ID
  EXPECT_EQ(table.GetSymbol(foo_id), "foo");
  EXPECT_EQ(table.GetSymbol(bar_id), "bar");
  EXPECT_EQ(table.GetSymbol(42), SymbolFactory::getBadSymbol());  // no-exist
}

TEST(SymbolFactoryTest, SymbolStringsAreStable) {
  SymbolFactory table;

  const SymbolFactory::ID foo_id = table.Make("foo");

  // Deliberately using .data() here so that API change to GetSymbol() returning
  // std::string_view later will keep this test source-code compatible.
  const char *before_data = table.GetSymbol(foo_id).data();

  // We want to make sure that even after reallocing the underlying
  // data structure, the symbol reference does not change. Let's enforce
  // some reallocs by creating a bunch of symbols.
  for (int i = 0; i < 100000; ++i) {
    table.Make("bar" + std::to_string(i));
  }

  const char *after_data = table.GetSymbol(foo_id).data();

  EXPECT_EQ(before_data, after_data);
}

}  // namespace
}  // namespace UHDM
