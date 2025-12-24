cmake_minimum_required(VERSION 3.20)
project(uhdm_pybind_real)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

file(GLOB_RECURSE SOURCES 
    "*.cpp"
)

pybind11_add_module(pyuhdm ${SOURCES})
target_link_libraries(pyuhdm PRIVATE uhdm)
