#ifndef UHDM_PY_EXCEPTIONS_HPP
#define UHDM_PY_EXCEPTIONS_HPP

#include <stdexcept>

class UhdmPyException : public std::runtime_error {
public:
    UhdmPyException(const char* msg) : std::runtime_error(msg) {}
};

#endif
