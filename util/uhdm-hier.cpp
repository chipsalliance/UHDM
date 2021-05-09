/*
 * Copyright 2021 Alain Dargelas
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

#include "headers/uhdm.h"
#include "headers/vpi_listener.h"

using namespace UHDM;

static int usage(const char* progname) {
  fprintf(stderr, "Usage:\n%s [options] <uhdm-file>\n", progname);
  fprintf(stderr,
          "Reads UHDM binary representation and prints hierarchy tree.\n");
  return 1;
}

int main(int argc, char** argv) {
  std::string uhdmFile;

  // Simple option parsing that works on all platforms.
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (uhdmFile.empty())
      uhdmFile = arg;
    else
      return usage(argv[0]);
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
  std::vector<vpiHandle> restoredDesigns = serializer.Restore(uhdmFile);

  if (restoredDesigns.empty()) {
    std::cerr << uhdmFile << ": empty design." << std::endl;
    return 1;
  }
  std::string result;
  for (vpiHandle design : restoredDesigns) {
    if (vpi_get(vpiType, design) == vpiDesign) {
      result +=
          "Design name: " + std::string(vpi_get_str(vpiName, design)) + "\n";
      result += "Instance tree:\n";

      vpiHandle instItr = vpi_iterate(UHDM::uhdmtopModules, design);
      while (vpiHandle obj_h = vpi_scan(instItr)) {
        std::function<std::string(vpiHandle, std::string)> inst_visit =
            [&inst_visit](vpiHandle obj_h, std::string path) {
              std::string res;
              std::string objectName;
              if (const char* s = vpi_get_str(vpiName, obj_h)) {
                objectName = s;
              }
              if (objectName.size()) {
                res += path + objectName + "\n";
                path += objectName + ".";
              }
              // Recursive tree traversal
              vpiHandle subItr = vpi_iterate(vpiModule, obj_h);
              while (vpiHandle sub_h = vpi_scan(subItr)) {
                res += inst_visit(sub_h, path);
                vpi_release_handle(sub_h);
              }
              vpi_release_handle(subItr);
              subItr = vpi_iterate(vpiGenScopeArray, obj_h);
              while (vpiHandle sub_h = vpi_scan(subItr)) {
                res += inst_visit(sub_h, path);
                vpi_release_handle(sub_h);
              }
              vpi_release_handle(subItr);
              subItr = vpi_iterate(vpiGenScope, obj_h);
              while (vpiHandle sub_h = vpi_scan(subItr)) {
                res += inst_visit(sub_h, path);
                vpi_release_handle(sub_h);
              }
              vpi_release_handle(subItr);
              return res;
            };
        result += inst_visit(obj_h, "");
        vpi_release_handle(obj_h);
      }
      vpi_release_handle(instItr);
    }
  }
  std::cout << result;
  return 0;
};
