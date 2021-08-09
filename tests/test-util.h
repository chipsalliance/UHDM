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
  tmpdir = getenv("TMPDIR");
  if (tmpdir && tmpdir[0]) return tmpdir;
  return "/tmp";
}
#endif // UHDM_TEST_UTIL
