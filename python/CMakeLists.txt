# Set up swig
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python3_INCLUDE_DIRS} ${GENDIR}/uhdm ${CMAKE_CURRENT_SOURCE_DIR})

set_source_files_properties(${PROJECT_NAME}.i
  PROPERTIES CPLUSPLUS ON
             COMPILE_DEFINITIONS final=
             GENERATED_COMPILE_DEFINITIONS final=
  INCLUDE_DIRECTORIES ${GENDIR}/uhdm
  GENERATED_INCLUDE_DIRECTORIES ${GENDIR}/uhdm
  DEPENDS uhdm
)

# Add swig module
swig_add_library(py_uhdm
  TYPE MODULE
  LANGUAGE python
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/uhdm
  SOURCES
  ${PROJECT_NAME}.i swig_test.cpp
)
target_link_libraries(py_uhdm
  PRIVATE uhdm
)
IF(APPLE)
    set_property(TARGET py_uhdm APPEND_STRING PROPERTY
        LINK_FLAGS "-undefined dynamic_lookup")
ENDIF()

set_target_properties(py_uhdm PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/uhdm
)

file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/setup.py ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/uhdm/__init__.py
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/uhdm
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/uhdm/util.py.in ${CMAKE_CURRENT_BINARY_DIR}/uhdm/util.py)

add_custom_target(PyPackageBuild ALL
    COMMAND ${Python3_EXECUTABLE} -m build --sdist
    COMMAND ${Python3_EXECUTABLE} -m pip install .
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building python sdist package"
    DEPENDS py_uhdm
)

add_test(
  NAME python-tests-import
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}" "${Python3_EXECUTABLE}" -c "import uhdm"
)

add_test(
  NAME python-tests-import-uhdm
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}" "${Python3_EXECUTABLE}" -c "from uhdm import uhdm"
)

add_test(
  NAME python-tests-import-util
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}" "${Python3_EXECUTABLE}" -c "from uhdm import util"
)

add_test(
  NAME python-tests-unit-module
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}" "${Python3_EXECUTABLE}" ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_module.py
)
