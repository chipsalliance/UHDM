#ifndef UHDM_PY_EXCEPTIONS_HPP
#define UHDM_PY_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

namespace uhdm_py {

// Base exception for all UHDM Python binding errors
class UhdmError : public std::runtime_error {
public:
    explicit UhdmError(const std::string& msg) : std::runtime_error(msg) {}
    explicit UhdmError(const char* msg) : std::runtime_error(msg) {}
};

// File-related errors (file not found, invalid path, permission errors)
class FileError : public UhdmError {
public:
    explicit FileError(const std::string& msg) : UhdmError(msg) {}
    explicit FileError(const char* msg) : UhdmError(msg) {}
};

// Serialization/deserialization errors
class SerializationError : public UhdmError {
public:
    explicit SerializationError(const std::string& msg) : UhdmError(msg) {}
    explicit SerializationError(const char* msg) : UhdmError(msg) {}
};

}  // namespace uhdm_py

#endif  // UHDM_PY_EXCEPTIONS_HPP
