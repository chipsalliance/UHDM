/*
 * simple dump test. Simplified from util/uhdm-dump.cpp
 */
#include <iostream>
#include <sys/stat.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <regex>
#include <fstream>
#include <sstream>

#if !(defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__))
  #include <dirent.h>
  #include <unistd.h>
#endif

#include "headers/uhdm.h"
#include "headers/vpi_listener.h"
#include "headers/vpi_visitor.h"
#include "headers/ElaboratorListener.h"

using namespace UHDM;

static int usage(const char *progname) {
  fprintf(stderr, "Usage %s: see uhdm-dump. This is a reduce test binary\n",
          progname);
  return 1;
}

int main (int argc, char** argv) {
  bool elab = true;
  bool verbose = true;
  std::string uhdmFile;

  // Simple option parsing that works on all platforms.
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    // Also supporting legacy long option with single dash
    if (arg == "-elab" || arg == "--elab") elab = true;
    else if (arg == "--verbose") verbose = true;
    else if (uhdmFile.empty()) uhdmFile = arg;
    else return usage(argv[0]);
  }

  if (uhdmFile.empty()) {
    uhdmFile = "surelog.uhdm";   // used by default in test.
  }

  struct stat buffer;
  if (stat(uhdmFile.c_str(), &buffer) != 0) {
    std::cerr << uhdmFile << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  Serializer serializer;
  if (verbose) std::cerr << uhdmFile << ": restoring from file" << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore(uhdmFile);

  if (restoredDesigns.empty()) {
    std::cerr << uhdmFile << ": empty design." << std::endl;
    return 1;
  }

  std::cout << uhdmFile << ": Restored design Pre-Elab: " << std::endl;
  visit_designs(restoredDesigns, std::cout);

  if (elab) {
    ElaboratorListener* listener = new ElaboratorListener(&serializer, false);
    listen_designs(restoredDesigns, listener);
    std::cout << uhdmFile << ": Restored design Post-Elab: " << std::endl;
    visit_designs(restoredDesigns, std::cout);
  }

  return 0;
};
