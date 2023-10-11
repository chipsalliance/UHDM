/*
 * Copyright 2023 Alain Dargelas
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
#include <uhdm/UhdmLint.h>
#include <uhdm/VpiListener.h>
#include <uhdm/SynthSubset.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

static int32_t usage(const char *progName) {
  std::cerr << "Usage:" << std::endl
            << "  " << progName << " <uhdm-file>" << std::endl
            << std::endl
            << "Reads input uhdm binary representation and lints the db (Version: "
            << UHDM_VERSION_MAJOR << "." << UHDM_VERSION_MINOR << ") "
            << std::endl
            << std::endl
            << "Exits with code" << std::endl;
  return 0;
}

class MyLinter : public UHDM::VpiListener {
 public:
  explicit MyLinter(UHDM::Serializer* serializer) : serializer_(serializer) {}
  ~MyLinter() override = default;

  void leaveAny(const UHDM::any* object, vpiHandle handle) final {
    if (any_cast<const UHDM::typespec*>(object) != nullptr) {
    }
  }

  void leaveUnsupported_expr(const UHDM::unsupported_expr* object,
                             vpiHandle handle) final {
    if (isInUhdmAllIterator()) return;
    serializer_->GetErrorHandler()(UHDM::ErrorType::UHDM_UNSUPPORTED_EXPR,
                                   std::string(object->VpiName()), object,
                                   nullptr);
  }

  void leaveUnsupported_stmt(const UHDM::unsupported_stmt* object,
                             vpiHandle handle) final {
    if (isInUhdmAllIterator()) return;
    serializer_->GetErrorHandler()(UHDM::ErrorType::UHDM_UNSUPPORTED_STMT,
                                   std::string(object->VpiName()), object,
                                   nullptr);
  }

  void leaveUnsupported_typespec(const UHDM::unsupported_typespec* object,
                                 vpiHandle handle) final {
    if (isInUhdmAllIterator()) return;
    serializer_->GetErrorHandler()(UHDM::ErrorType::UHDM_UNSUPPORTED_TYPESPEC,
                                   std::string(object->VpiName()), object,
                                   object->VpiParent());
  }

 private:
  UHDM::Serializer* serializer_;
};

int32_t main(int32_t argc, char **argv) {
  if (argc != 2) {
    return usage(argv[0]);
  }

  fs::path file = argv[1];
  
  std::error_code ec;
  if (!fs::is_regular_file(file, ec) || ec) {
    std::cerr << file << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  std::unique_ptr<UHDM::Serializer> serializer(new UHDM::Serializer);
  std::vector<vpiHandle> designs = serializer->Restore(file);

  if (designs.empty()) {
    std::cerr << file << ": Failed to load." << std::endl;
    return 1;
  }

  UHDM::ErrorHandler errHandler =
      [=](UHDM::ErrorType errType, std::string_view msg,
          const UHDM::any* object1, const UHDM::any* object2) {
        std::string errmsg;
        switch (errType) {
          case UHDM::UHDM_UNSUPPORTED_EXPR:
            errmsg = "Unsupported expression";
            break;
          case UHDM::UHDM_UNSUPPORTED_STMT:
            errmsg = "Unsupported stmt";
            break;
          case UHDM::UHDM_WRONG_OBJECT_TYPE:
            errmsg = "Wrong object type";
            break;
          case UHDM::UHDM_UNDEFINED_PATTERN_KEY:
            errmsg = "Undefined pattern key";
            break;
          case UHDM::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN:
            errmsg = "Unmatched field in pattern assign";
            break;
          case UHDM::UHDM_REAL_TYPE_AS_SELECT:
            errmsg = "Real type used as select";
            break;
          case UHDM::UHDM_RETURN_VALUE_VOID_FUNCTION:
            errmsg = "Return value void function";
            break;
          case UHDM::UHDM_ILLEGAL_DEFAULT_VALUE:
            errmsg = "Illegal default value";
            break;
          case UHDM::UHDM_MULTIPLE_CONT_ASSIGN:
            errmsg = "Multiple cont assign";
            break;
          case UHDM::UHDM_ILLEGAL_WIRE_LHS:
            errmsg = "Illegal wire LHS";
            break;
          case UHDM::UHDM_ILLEGAL_PACKED_DIMENSION:
            errmsg = "Illegal Packed dimension";
            break;
          case UHDM::UHDM_NON_SYNTHESIZABLE:
            errmsg = "Non synthesizable construct";
            break;
          case UHDM::UHDM_ENUM_CONST_SIZE_MISMATCH:
            errmsg = "Enum const size mismatch";
            break;
          case UHDM::UHDM_DIVIDE_BY_ZERO:
            errmsg = "Division by zero";
            break;
          case UHDM::UHDM_INTERNAL_ERROR_OUT_OF_BOUND:
            errmsg = "Internal error out of bound";
            break;
          case UHDM::UHDM_UNDEFINED_USER_FUNCTION:
            errmsg = "Undefined user function";
            break;
          case UHDM::UHDM_UNRESOLVED_HIER_PATH:
            errmsg = "Unresolved hierarchical path";
            break;
          case UHDM::UHDM_UNDEFINED_VARIABLE:
            errmsg = "Undefined variable";
            break;
          case UHDM::UHDM_INVALID_CASE_STMT_VALUE:
            errmsg = "Invalid case stmt value";
            break;
          case UHDM::UHDM_UNSUPPORTED_TYPESPEC:
            errmsg = "Unsupported typespec";
            break;
          case UHDM::UHDM_UNRESOLVED_PROPERTY:
            errmsg = "Unresolved property";
            break;
          case UHDM::UHDM_NON_TEMPORAL_SEQUENCE_USE:
            errmsg = "Sequence used in non-temporal context";
            break;
        }

        if (object1) {
          std::cout << object1->VpiFile() << ":" << object1->VpiLineNo() << ":"
                    << object1->VpiColumnNo() << ": ";
          std::cout << errmsg << ", " << msg << std::endl;
        } else {
          std::cout << errmsg << ", " << msg << std::endl;
        }
        if (object2) {
          std::cout << "  \\_ " << object2->VpiFile() << ":"
                    << object2->VpiLineNo() << ":" << object2->VpiColumnNo() << ":" << std::endl;
        }
      };
  serializer.get()->SetErrorHandler(errHandler);

  vpiHandle designH = designs.at(0);
  UHDM::design* design = UhdmDesignFromVpiHandle(designH);

  UHDM::UhdmLint* linter = new UHDM::UhdmLint(serializer.get(), design);
  linter->listenDesigns(designs);
  delete linter;

  std::set<const UHDM::any*> nonSynthesizableObjects;
  UHDM::SynthSubset* annotate = new UHDM::SynthSubset(
      serializer.get(), nonSynthesizableObjects, true, true);
  annotate->listenDesigns(designs);
  delete annotate;

  MyLinter* mylinter = new MyLinter(serializer.get());
  mylinter->listenDesigns(designs);
  delete mylinter;

  return 0;
}
