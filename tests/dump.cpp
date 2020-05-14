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

static bool ReadIntoString(const std::string &filename, std::string *content) {
  std::ifstream fs;
  fs.open(std::string(filename).c_str());
  if (!fs.good())
    return false;
  content->assign((std::istreambuf_iterator<char>(fs)),
                  std::istreambuf_iterator<char>());
  return true;
}

static bool CompareContentWithFile(const std::string &content,
                                   const std::string &filename,
                                   bool verbose) {
  std::string expected;
  if (!ReadIntoString(filename, &expected)) {
    std::cerr << "Couldn't read '" << filename << "'" << std::endl;
    return false;
  }
  if (content != expected) {
    std::cerr << "Dump does not match content of '"
              << filename << "'" << std::endl;
    return false;
  } else if (verbose) {
    std::cerr << "Dump matches '" << filename << "'" << std::endl;
  }
  return true;
}

static int usage(const char *progname) {
  fprintf(stderr, "Usage:\n%s [options] <uhdm-file> [<golden-file-to-compare>]\n", progname);
  fprintf(stderr, "Reads UHDM binary representation and prints tree. If --elab is given, the\n"
          "tree is elaborated first.\n");
  fprintf(stderr, "Options:\n"
          "\t--elab          : Elaborate the restored design.\n"
          "\t--verbose       : print diagnostic messages.\n"
          "\nIf golden file is given to compare, exit code represent if output matches.\n");
  return 1;
}

int main (int argc, char** argv) {
  bool elab = false;
  bool verbose = false;
  std::string uhdmFile;
  std::string goldenFile;

  // Simple option parsing that works on all platforms.
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    // Also supporting legacy long option with single dash
    if (arg == "-elab" || arg == "--elab") elab = true;
    else if (arg == "--verbose") verbose = true;
    else if (uhdmFile.empty()) uhdmFile = arg;
    else if (goldenFile.empty()) goldenFile = arg;
    else return usage(argv[0]);
  }

  if (uhdmFile.empty()) {
    uhdmFile = "surelog.uhdm";
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

  if (elab) {
    ElaboratorListener* listener = new ElaboratorListener(&serializer, false);
    listen_designs(restoredDesigns, listener);
    if (verbose) std::cerr << uhdmFile << ": Restored design Post-Elab: " << std::endl;
  } else {
    if (verbose) std::cerr << uhdmFile << ": Restored design Pre-Elab: " << std::endl;
  }

  const std::string restored = visit_designs(restoredDesigns);
  std::cout << restored;

  if (!goldenFile.empty()
      && !CompareContentWithFile(restored, goldenFile, verbose)) {
    return 2;
  }

  return 0;
};
