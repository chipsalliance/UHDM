#include <pybind11/pybind11.h>
#include <uhdm/Utils.h>

namespace py = pybind11;

char const* hello() {
    return "hello from pybind11";
}

PYBIND11_MODULE(uhdm_py, m) {
    m.doc() = "pybind11 example plugin"; // optional module docstring
    m.def("hello", &hello, "A function that says hello");
    m.def("trim_string", &uhdm::trim, "Trims whitespace from a string");
}
