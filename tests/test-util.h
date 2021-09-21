// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Some basic functionality for writing tests in absense of
// googletest.
#ifndef UHDM_TEST_UTIL
#define UHDM_TEST_UTIL

#include <stdlib.h>
#include <string>

namespace uhdm_test {
inline std::string getTmpDir();
}  // namespace uhdm_test

inline std::string uhdm_test::getTmpDir() {
  const char *tmpdir = getenv("TEST_TMPDIR");
  if (tmpdir && tmpdir[0]) return tmpdir;
  tmpdir = getenv("TMPDIR");  // typical Unix env var
  if (tmpdir && tmpdir[0]) return tmpdir;
  tmpdir = getenv("TEMP");  // typical Windows env var
  if (tmpdir && tmpdir[0]) return tmpdir;
  return "/tmp";
}

#define EXPECT_EQ(x, y) if ((x) == (y)) {} else { std::cerr << __LINE__ << ": " << #x << " == " << #y << "\n"; abort(); }
#define EXPECT_TRUE(x) if ((x)) {} else { std::cerr << __LINE__ << ": " << #x << "\n"; abort(); }
#define EXPECT_FALSE(x) if (!(x)) {} else { std::cerr << __LINE__ << ": " << #x << "\n"; abort(); }

#endif // UHDM_TEST_UTIL
