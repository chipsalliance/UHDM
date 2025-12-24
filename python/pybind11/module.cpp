#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(pyuhdm, m) {
    m.doc() = "UHDM pybind11 module";
}
