// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <stdlib.h>
#include <uhdm/constant.h>
#include <uhdm/uhdm.h>
#include <uhdm/uhdm_types.h>  // for uhdmconstant
#include <uhdm/vhpi_user.h>   // vpi_user functions.
#include <uhdm/vpi_uhdm.h>    // struct uhdm_handle

#include "gtest/gtest.h"

TEST(VpiGetTest, WriteReadRoundtrip) {
  UHDM::Serializer serializer;

  // Let's choose a type that is multiple levels deep to see levels
  // base-class <- expr <- constant

  UHDM::constant *value = serializer.MakeConstant();
  // Base class values
  EXPECT_TRUE(value->VpiFile("hello.v"));
  EXPECT_TRUE(value->VpiLineNo(42));

  // expr values
  EXPECT_TRUE(value->VpiSize(12345));
  EXPECT_TRUE(value->VpiDecompile("decompile"));

  uhdm_handle uhdm_handle(UHDM::uhdmconstant, value);
  vpiHandle vpi_handle = (vpiHandle)&uhdm_handle;

  // Request all the properties set above via vpi
  EXPECT_EQ(vpi_get_str(vpiFile, vpi_handle), std::string("hello.v"));
  EXPECT_EQ(vpi_get(vpiLineNo, vpi_handle), 42);

  EXPECT_EQ(vpi_get(vpiSize, vpi_handle), 12345);
  EXPECT_EQ(vpi_get_str(vpiDecompile, vpi_handle), std::string("decompile"));
}
