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
  value.value.integer = vpiX;
  EXPECT_EQ(VpiValue2String(&value), "SCAL:X");

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

static std::string ParseAndRegenerateString(const std::string &str) {
  std::unique_ptr<s_vpi_value> val(String2VpiValue(str));
  return VpiValue2String(val.get());
}

static bool ParseConvertBackRoundtrip(const std::string &str) {
  return ParseAndRegenerateString(str) == str;
}

static void TEST_ParseValueFindPrefix() {
  EXPECT_EQ(ParseAndRegenerateString("INT:42"), "INT:42");

  // With Whitespace in front.
  EXPECT_EQ(ParseAndRegenerateString("  INT:42"), "INT:42");

  // .. or even other garbage in front.
  EXPECT_EQ(ParseAndRegenerateString("unrelated stuff:43 INT:42"), "INT:42");
}

static void TEST_ParseScalarValue() {
  // Zero and one are represented as integers
  EXPECT_EQ(ParseAndRegenerateString("SCAL:0"), "SCAL:0");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:1"), "SCAL:1");

  // Symbolic scalar values
  EXPECT_EQ(ParseAndRegenerateString("SCAL:Z"), "SCAL:Z");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:X"), "SCAL:X");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:H"), "SCAL:H");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:L"), "SCAL:L");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:W"), "SCAL:DontCare");

  // The longer symbols are case-insensitive
  EXPECT_EQ(ParseAndRegenerateString("SCAL:DontCare"), "SCAL:DontCare");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:dontcare"), "SCAL:DontCare");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:NoChange"), "SCAL:NoChange");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:nochange"), "SCAL:NoChange");

  // Also parse numeric values
  EXPECT_EQ(ParseAndRegenerateString("SCAL:2"), "SCAL:Z");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:3"), "SCAL:X");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:4"), "SCAL:H");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:5"), "SCAL:L");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:6"), "SCAL:DontCare");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:7"), "SCAL:NoChange");

  // (Q: What is the difference between X and DontCare ?)
  EXPECT_EQ(ParseAndRegenerateString("SCAL:6"), "SCAL:DontCare");
  EXPECT_EQ(ParseAndRegenerateString("SCAL:7"), "SCAL:NoChange");
}

// Some smoke testing to see if a string we parse and regenerated is the same
static void TEST_roundtrip() {
  EXPECT_TRUE(ParseConvertBackRoundtrip("INT:42"));

  EXPECT_TRUE(ParseConvertBackRoundtrip("SCAL:1"));

  EXPECT_TRUE(ParseConvertBackRoundtrip("SCAL:X"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("SCAL:Z"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("SCAL:H"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("SCAL:L"));

  EXPECT_TRUE(ParseConvertBackRoundtrip("STRING:hello"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("HEX:AFFE"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("OCT:0123"));
  EXPECT_TRUE(ParseConvertBackRoundtrip("BIN:11111"));

  EXPECT_TRUE(ParseConvertBackRoundtrip("REAL:3.141592"));
}

int main() {
  TEST_vpivalue2string();
  TEST_ParseValueFindPrefix();
  TEST_ParseScalarValue();
  TEST_roundtrip();
  return 0;
}
