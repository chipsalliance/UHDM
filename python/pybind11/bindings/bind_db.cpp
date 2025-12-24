#include <pybind11/pybind11.h>
#include "../uhdm_db.hpp"
#include "../utils/exceptions.hpp"

namespace py = pybind11;

void bind_db(py::module& m) {
    // Register exception translators
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p) std::rethrow_exception(p);
        } catch (const uhdm_py::FileError& e) {
            PyErr_SetString(PyExc_FileNotFoundError, e.what());
        } catch (const uhdm_py::SerializationError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const uhdm_py::UhdmError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // Bind UHDMDatabase class
    py::class_<uhdm_py::UHDMDatabase>(m, "UHDMDatabase",
        "A wrapper class that owns UHDM serializer state and provides load/save functionality.")
        .def(py::init<>(), "Create a new empty UHDMDatabase instance.")
        .def("load", &uhdm_py::UHDMDatabase::load,
            py::arg("path"),
            "Load a UHDM binary file from disk.\n\n"
            "Args:\n"
            "    path: Path to the UHDM file to load.\n\n"
            "Raises:\n"
            "    FileNotFoundError: If the file does not exist.\n"
            "    RuntimeError: If the file cannot be parsed.")
        .def("save", &uhdm_py::UHDMDatabase::save,
            py::arg("path"),
            "Save the current UHDM state to a binary file.\n\n"
            "Args:\n"
            "    path: Path where the UHDM file should be saved.\n\n"
            "Raises:\n"
            "    FileNotFoundError: If the parent directory does not exist.\n"
            "    RuntimeError: If serialization fails.")
        .def("is_loaded", &uhdm_py::UHDMDatabase::isLoaded,
            "Check if a design has been loaded.\n\n"
            "Returns:\n"
            "    bool: True if designs are loaded, False otherwise.");
}
