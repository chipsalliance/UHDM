#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declaration of binding functions
void bind_db(py::module& m);

PYBIND11_MODULE(pyuhdm, m) {
    m.doc() = "UHDM pybind11 module - Python bindings for UHDM database operations";
    
    // Register bindings
    bind_db(m);
}
