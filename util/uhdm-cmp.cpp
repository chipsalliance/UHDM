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

#include <uhdm/uhdm.h>
#include <uhdm/uhdm-version.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

static int32_t usage(const char *progName) {
  std::cerr << "Usage:" << std::endl
            << "  " << progName << " <uhdm-file> <uhdm-file>" << std::endl
            << std::endl
            << "Reads input uhdm binary representations of two files and "
               "compares them topographically. (Version: "
            << UHDM_VERSION_MAJOR << "." << UHDM_VERSION_MINOR << ") "
            << std::endl
            << std::endl
            << "Exits with code" << std::endl
            << "  = 0, if input files are equal" << std::endl
            << "  < 0, if input files are not equal" << std::endl
            << "  > 0, for any failures" << std::endl;
  return 1;
}

int32_t main(int32_t argc, char **argv) {
  if (argc != 3) {
    return usage(argv[0]);
  }

  fs::path fileA = argv[1];
  fs::path fileB = argv[2];

  std::error_code ec;
  if (!fs::is_regular_file(fileA, ec) || ec) {
    std::cerr << fileA << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  if (!fs::is_regular_file(fileB, ec) || ec) {
    std::cerr << fileB << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  std::unique_ptr<UHDM::Serializer> serializerA(new UHDM::Serializer);
  std::vector<vpiHandle> handlesA = serializerA->Restore(fileA);

  std::unique_ptr<UHDM::Serializer> serializerB(new UHDM::Serializer);
  std::vector<vpiHandle> handlesB = serializerB->Restore(fileB);

  if (handlesA.empty()) {
    std::cerr << fileA << ": Failed to load." << std::endl;
    return 1;
  }

  if (handlesB.empty()) {
    std::cerr << fileB << ": Failed to load." << std::endl;
    return 1;
  }

  if (handlesA.size() != handlesB.size()) {
    std::cerr << "Number of designs mismatch." << std::endl;
    return -1;
  }

  std::function<const UHDM::design *(vpiHandle handle)> to_design =
      [](vpiHandle handle) {
        return (const UHDM::design *)((const uhdm_handle *)handle)->object;
      };

  std::vector<const UHDM::design *> designsA;
  designsA.reserve(handlesA.size());
  std::transform(handlesA.begin(), handlesA.end(), std::back_inserter(designsA),
                 to_design);

  std::vector<const UHDM::design *> designsB;
  designsB.reserve(handlesB.size());
  std::transform(handlesB.begin(), handlesB.end(), std::back_inserter(designsB),
                 to_design);

  for (size_t i = 0, n = designsA.size(); i < n; ++i) {
    UHDM::AnySet visited;
    if (designsA[i]->Compare(designsB[i], visited) != 0) {
      return -1;
    }
  }

  return 0;
}
