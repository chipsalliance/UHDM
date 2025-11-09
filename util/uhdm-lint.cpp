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

#include <uhdm/SynthSubset.h>
#include <uhdm/UhdmLint.h>
#include <uhdm/VpiListener.h>
#include <uhdm/uhdm-version.h>
#include <uhdm/uhdm.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

static int32_t usage(const char* progName) {
  std::cerr
      << "Usage:" << std::endl
      << "  " << progName << " <uhdm-file>" << std::endl
      << std::endl
      << "Reads input uhdm binary representation and lints the db (Version: "
      << UHDM_VERSION_MAJOR << "." << UHDM_VERSION_MINOR << ") " << std::endl
      << std::endl
      << "Exits with code" << std::endl;
  return 0;
}

class MyLinter : public uhdm::VpiListener {
 public:
  explicit MyLinter(uhdm::Serializer* serializer) : m_serializer(serializer) {}
  ~MyLinter() override = default;

  void leaveAny(const uhdm::Any* object, vpiHandle handle) final {
    if (any_cast<const uhdm::Typespec*>(object) != nullptr) {
    }
  }

  void leaveUnsupportedExpr(const uhdm::UnsupportedExpr* object,
                            vpiHandle handle) final {
    if (isInUhdmAllIterator()) return;
    m_serializer->getErrorHandler()(uhdm::ErrorType::UHDM_UNSUPPORTED_EXPR,
                                    std::string(object->getName()), object,
                                    nullptr);
  }

  void leaveUnsupportedStmt(const uhdm::UnsupportedStmt* object,
                            vpiHandle handle) final {
    if (isInUhdmAllIterator()) return;
    m_serializer->getErrorHandler()(uhdm::ErrorType::UHDM_UNSUPPORTED_STMT,
                                    std::string(object->getName()), object,
                                    nullptr);
  }

  void leaveUnsupportedTypespec(const uhdm::UnsupportedTypespec* object,
                                vpiHandle handle) final {
    if (isInUhdmAllIterator()) return;
    m_serializer->getErrorHandler()(uhdm::ErrorType::UHDM_UNSUPPORTED_TYPESPEC,
                                    std::string(object->getName()), object,
                                    object->getParent());
  }

 private:
  uhdm::Serializer* const m_serializer = nullptr;
};

int32_t main(int32_t argc, char** argv) {
  if (argc != 2) {
    return usage(argv[0]);
  }

  fs::path file = argv[1];

  std::error_code ec;
  if (!fs::is_regular_file(file, ec) || ec) {
    std::cerr << file << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  std::unique_ptr<uhdm::Serializer> serializer(new uhdm::Serializer);
  std::vector<vpiHandle> designs = serializer->restore(file);

  if (designs.empty()) {
    std::cerr << file << ": Failed to load." << std::endl;
    return 1;
  }

  uhdm::ErrorHandler errHandler =
      [=](uhdm::ErrorType errType, std::string_view msg,
          const uhdm::Any* object1, const uhdm::Any* object2) {
        std::string errmsg;
        switch (errType) {
          case uhdm::UHDM_UNSUPPORTED_EXPR:
            errmsg = "Unsupported expression";
            break;
          case uhdm::UHDM_UNSUPPORTED_STMT:
            errmsg = "Unsupported stmt";
            break;
          case uhdm::UHDM_WRONG_OBJECT_TYPE:
            errmsg = "Wrong object type";
            break;
          case uhdm::UHDM_UNDEFINED_PATTERN_KEY:
            errmsg = "Undefined pattern key";
            break;
          case uhdm::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN:
            errmsg = "Unmatched field in pattern assign";
            break;
          case uhdm::UHDM_REAL_TYPE_AS_SELECT:
            errmsg = "Real type used as select";
            break;
          case uhdm::UHDM_RETURN_VALUE_VOID_FUNCTION:
            errmsg = "Return value void function";
            break;
          case uhdm::UHDM_ILLEGAL_DEFAULT_VALUE:
            errmsg = "Illegal default value";
            break;
          case uhdm::UHDM_MULTIPLE_CONT_ASSIGN:
            errmsg = "Multiple cont assign";
            break;
          case uhdm::UHDM_ILLEGAL_WIRE_LHS:
            errmsg = "Illegal wire LHS";
            break;
          case uhdm::UHDM_ILLEGAL_PACKED_DIMENSION:
            errmsg = "Illegal Packed dimension";
            break;
          case uhdm::UHDM_NON_SYNTHESIZABLE:
            errmsg = "Non synthesizable construct";
            break;
          case uhdm::UHDM_ENUM_CONST_SIZE_MISMATCH:
            errmsg = "Enum const size mismatch";
            break;
          case uhdm::UHDM_DIVIDE_BY_ZERO:
            errmsg = "Division by zero";
            break;
          case uhdm::UHDM_INTERNAL_ERROR_OUT_OF_BOUND:
            errmsg = "Internal error out of bound";
            break;
          case uhdm::UHDM_UNDEFINED_USER_FUNCTION:
            errmsg = "Undefined user function";
            break;
          case uhdm::UHDM_UNRESOLVED_HIER_PATH:
            errmsg = "Unresolved hierarchical path";
            break;
          case uhdm::UHDM_UNDEFINED_VARIABLE:
            errmsg = "Undefined variable";
            break;
          case uhdm::UHDM_INVALID_CASE_STMT_VALUE:
            errmsg = "Invalid case stmt value";
            break;
          case uhdm::UHDM_UNSUPPORTED_TYPESPEC:
            errmsg = "Unsupported typespec";
            break;
          case uhdm::UHDM_UNRESOLVED_PROPERTY:
            errmsg = "Unresolved property";
            break;
          case uhdm::UHDM_NON_TEMPORAL_SEQUENCE_USE:
            errmsg = "Sequence used in non-temporal context";
            break;
          case uhdm::UHDM_NON_POSITIVE_VALUE:
            errmsg = "Non positive (<1) value";
            break;
          case uhdm::UHDM_SIGNED_UNSIGNED_PORT_CONN:
            errmsg = "Signed vs Unsigned port connection";
            break;
          case uhdm::UHDM_FORCING_UNSIGNED_TYPE:
            errmsg =
                "Critical: Forcing signal to unsigned type due to unsigned "
                "port binding ";
            break;
        }

        if (object1) {
          std::cout << object1->getFile() << ":" << object1->getStartLine()
                    << ":" << object1->getStartColumn() << ": ";
          std::cout << errmsg << ", " << msg << std::endl;
        } else {
          std::cout << errmsg << ", " << msg << std::endl;
        }
        if (object2) {
          std::cout << "  \\_ " << object2->getFile() << ":"
                    << object2->getStartLine() << ":"
                    << object2->getStartColumn() << ":" << std::endl;
        }
      };
  serializer.get()->setErrorHandler(errHandler);

  vpiHandle designH = designs.at(0);
  uhdm::Design* design = UhdmDesignFromVpiHandle(designH);

  uhdm::UhdmLint* linter = new uhdm::UhdmLint(serializer.get(), design);
  linter->listenDesigns(designs);
  delete linter;

  uhdm::AnySet nonSynthesizableObjects;
  uhdm::SynthSubset* annotate = new uhdm::SynthSubset(
      serializer.get(), nonSynthesizableObjects, design, true, true);
  annotate->listenDesigns(designs);
  delete annotate;

  MyLinter* mylinter = new MyLinter(serializer.get());
  mylinter->listenDesigns(designs);
  delete mylinter;

  return 0;
}
