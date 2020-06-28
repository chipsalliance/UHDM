// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>
#include <memory>

#include "headers/vpi_uhdm.h"     // struct uhdm_handle
#include "include/vhpi_user.h"    // vpi_user functions.


#include <stdlib.h>

#define EXPECT_TRUE(x) if ((x)) {} else { std::cerr << __LINE__ << ": " << #x << "\n"; abort(); }
#define EXPECT_FALSE(x) if (!(x)) {} else { std::cerr << __LINE__ << ": " << #x << "\n"; abort(); }
#define EXPECT_EQ(x, y) if ((x) == (y)) {} else { std::cerr << __LINE__ << ": " << #x << " == " << #y << "\n"; abort(); }

static void TEST_vpivalue2string() {
  s_vpi_value value;

  value.format = vpiIntVal;
  value.value.integer = 42;
  EXPECT_EQ(VpiValue2String(&value), "INT:42");

  value.format = vpiScalarVal;
  value.value.integer = vpiX;   // value of 3
  // This is currently not properly translated. vpi0, vpi1, vpiZ, vpiX,
  // vpiH, vpiL and vpiDontCare would be expected.
  EXPECT_EQ(VpiValue2String(&value), "SCAL:3");

  value.format = vpiStringVal;
  value.value.str = (PLI_BYTE8*)"helloworld";
  EXPECT_EQ(VpiValue2String(&value), "STRING:helloworld");

  value.format = vpiHexStrVal;
  value.value.str = (PLI_BYTE8*)"FEEDCAFE";
  EXPECT_EQ(VpiValue2String(&value), "HEX:FEEDCAFE");

  value.format = vpiOctStrVal;
  value.value.str = (PLI_BYTE8*)"007";
  EXPECT_EQ(VpiValue2String(&value), "OCT:007");

  value.format = vpiBinStrVal;
  value.value.str = (PLI_BYTE8*)"101010";
  EXPECT_EQ(VpiValue2String(&value), "BIN:101010");

  value.format = vpiRealVal;
  value.value.real = 3.141592;
  EXPECT_EQ(VpiValue2String(&value), "REAL:3.141592");
}

// Test that a string converted to vpi_value and back looks the same.
static bool ParseConvertBackRoundtrip(const std::string &str) {
  std::unique_ptr<s_vpi_value> val(String2VpiValue(str));
  return VpiValue2String(val.get()) == str;
}

static void TEST_roundtrip() {
  EXPECT_TRUE(ParseConvertBackRoundtrip("INT:42"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("SCAL:1"));

  // These don't work yet. Document with EXPECT_FALSE()
  EXPECT_FALSE(ParseConvertBackRoundtrip("SCAL:X"));
  EXPECT_FALSE(ParseConvertBackRoundtrip("SCAL:Z"));
  EXPECT_FALSE(ParseConvertBackRoundtrip("SCAL:H"));
  EXPECT_FALSE(ParseConvertBackRoundtrip("SCAL:L"));

  EXPECT_TRUE(ParseConvertBackRoundtrip("STRING:hello"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("HEX:AFFE"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("OCT:0123"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("BIN:11111"));

  EXPECT_TRUE(ParseConvertBackRoundtrip("REAL:3.141592"));
}

int main() {
  TEST_vpivalue2string();
  TEST_roundtrip();
  return 0;
}
