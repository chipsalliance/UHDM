cmake_minimum_required(VERSION 3.20)
project(uhdm_py_poc)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)
# Make pybind11 available (will also define pybind11_add_module)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(uhdm_py uhdm_py.cpp)
target_link_libraries(uhdm_py PRIVATE uhdm)
