/*
 * Copyright 2019 Alain Dargelas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <limits.h> /* PATH_MAX */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#if !(defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__))
#include <dirent.h>
#include <unistd.h>
#endif

#include <uhdm/ElaboratorListener.h>
#include <uhdm/VpiListener.h>
#include <uhdm/uhdm-version.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>

using namespace UHDM;

static bool ReadIntoString(const std::string &filename, std::string *content) {
  std::ifstream fs;
  fs.open(std::string(filename).c_str());
  if (!fs.good()) return false;
  content->assign((std::istreambuf_iterator<char>(fs)),
                  std::istreambuf_iterator<char>());
  return true;
}

static bool CompareContentWithFile(const std::string &content,
                                   const std::string &filename, bool verbose) {
  std::string expected;
  if (!ReadIntoString(filename, &expected)) {
    std::cerr << "Couldn't read '" << filename << "'" << std::endl;
    return false;
  }
  if (content != expected) {
    std::cerr << "Dump does not match content of '" << filename << "'"
              << std::endl;
    return false;
  } else if (verbose) {
    std::cerr << "Dump matches '" << filename << "'" << std::endl;
  }
  return true;
}

static int usage(const char *progname) {
  fprintf(stderr,
          "Usage:\n%s [options] <uhdm-file> [<golden-file-to-compare>]\n",
          progname);
  fprintf(stderr,
          "Reads UHDM binary representation and prints tree. If --elab is "
          "given, the\ntree is also elaborated.\n");
  fprintf(stderr,
          "Options:\n"
          "\t--elab          : Elaborate the restored design.\n"
          "\t--verbose       : print diagnostic messages.\n"
          "\nIf golden file is given to compare, exit code represent if output "
          "matches.\n"
          "\t--version       : print version and exit.\n");
  return 1;
}

int main(int argc, char **argv) {
  std::ios::sync_with_stdio(false);

  bool elab = false;
  bool verbose = false;
  std::string uhdmFile;
  std::string goldenFile;

  // Simple option parsing that works on all platforms.
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    // Also supporting legacy long option with single dash
    if (arg == "-elab" || arg == "--elab") {
      elab = true;
    } else if (arg == "--verbose") {
      verbose = true;
    } else if (arg == "--version") {
      fprintf(stderr, "%d.%d\n", UHDM_VERSION_MAJOR, UHDM_VERSION_MINOR);
      return 0;
    } else if (uhdmFile.empty()) {
      uhdmFile = arg;
    } else if (goldenFile.empty()) {
      goldenFile = arg;
    } else {
      return usage(argv[0]);
    }
  }

  if (uhdmFile.empty()) {
    return usage(argv[0]);
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

  if (!goldenFile.empty()) {
    std::stringstream restored;
    visit_designs(restoredDesigns, restored);
    if (!CompareContentWithFile(restored.str(), goldenFile, verbose)) {
      return 2;
    }
  }

  if (elab) {
    ElaboratorListener *listener = new ElaboratorListener(&serializer, false);
    listener->listenDesigns(restoredDesigns);
    delete listener;

    std::cout << uhdmFile << ": Restored design Post-Elab: " << std::endl;
    visit_designs(restoredDesigns, std::cout);
  }

  return 0;
};
