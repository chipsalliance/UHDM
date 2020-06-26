# -*- mode:cmake -*-
cmake_minimum_required(VERSION 3.15)

# Detect build type, fallback to release and throw a warning if use didn't
# specify any
if(NOT CMAKE_BUILD_TYPE)
  message(WARNING "Build type not set, falling back to Release mode.
 To specify build type use:
 -DCMAKE_BUILD_TYPE=<mode> where <mode> is Debug or Release.")
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE STRING "Choose the type of build, options are: Debug Release."
            FORCE)
endif(NOT CMAKE_BUILD_TYPE)

option(
  WITH_LIBCXX
  "Building with clang++ and libc++(in Linux). To enable with: -DWITH_LIBCXX=On"
  On)
option(UHDM_BUILD_TESTS "Enable testing." ON)

project(UHDM)

# NOTE: Policy changes has to happen before adding any subprojects
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(BUILD_TESTING OFF CACHE BOOL "Don't build capnproto tests")
add_subdirectory(third_party/capnproto EXCLUDE_FROM_ALL)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# NOTE: Set the global output directories after the subprojects have had their go at it
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}/include/")
include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_SOURCE_DIR}/headers/")
include_directories("${PROJECT_SOURCE_DIR}/src/")
include_directories("${PROJECT_SOURCE_DIR}/third_party/capnproto/c++/src/")
include_directories("${PROJECT_SOURCE_DIR}/third_party/UHDM/src/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CXX_WARNING_FLAGS}")

if(MSVC)
  add_compile_definitions(PLI_DLLISPEC=)

  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} ${TCMALLOC_COMPILE_OPTIONS} /W3 /bigobj ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} /W3 /bigobj ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} /W3 /bigobj ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_EXE_LINKER_FLAGS /STACK:8388608)  # 8MB stack size
else()
  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O0 -g ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O0 -DNDEBUG ${MY_CXX_WARNING_FLAGS}"
  )
endif()

# model_gen generated
set(model-GENERATED_SRC ${PROJECT_SOURCE_DIR}/src/UHDM.capnp.h
                        ${PROJECT_SOURCE_DIR}/src/UHDM.capnp.c++)

# All the files the generator depends on.
file(GLOB yaml_SRC ${PROJECT_SOURCE_DIR}/model/*.yaml)

file(GLOB templates_SRC ${PROJECT_SOURCE_DIR}/templates/*.h
     ${PROJECT_SOURCE_DIR}/templates/*.cpp)

foreach(header_file ${model-GENERATED_SRC})
  set_source_files_properties(${header_file} PROPERTIES GENERATED TRUE)
endforeach(header_file ${model-GENERATED_SRC})
add_custom_target(GenerateCode DEPENDS ${model-GENERATED_SRC})
add_custom_command(
  OUTPUT ${model-GENERATED_SRC}
  COMMAND tclsh ${PROJECT_SOURCE_DIR}/model_gen/model_gen.tcl
          ${PROJECT_SOURCE_DIR}/model/models.lst ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  DEPENDS ${PROJECT_SOURCE_DIR}/model_gen/model_gen.tcl
          ${PROJECT_SOURCE_DIR}/model/models.lst
          ${PROJECT_SOURCE_DIR}/templates/UHDM.capnp
          ${yaml_SRC}
          ${templates_SRC})

set(uhdm_SRC
    ${PROJECT_SOURCE_DIR}/src/vpi_listener.cpp
    ${PROJECT_SOURCE_DIR}/src/vpi_visitor.cpp
    ${PROJECT_SOURCE_DIR}/src/vpi_user.cpp
    ${PROJECT_SOURCE_DIR}/src/SymbolFactory.cpp
    ${PROJECT_SOURCE_DIR}/src/Serializer_save.cpp
    ${PROJECT_SOURCE_DIR}/src/Serializer_restore.cpp
    ${PROJECT_SOURCE_DIR}/src/UHDM.capnp.c++
    ${PROJECT_SOURCE_DIR}/src/clone_tree.cpp
    ${PROJECT_SOURCE_DIR}/src/actual_group.cpp
    ${PROJECT_SOURCE_DIR}/src/instance_item.cpp
    ${PROJECT_SOURCE_DIR}/src/enum_struct_packed_net_group.cpp
    ${PROJECT_SOURCE_DIR}/src/ref_obj_interf_net_var_group.cpp
    ${PROJECT_SOURCE_DIR}/src/enum_struct_union_packed_var_group.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_ref_obj_group.cpp
    ${PROJECT_SOURCE_DIR}/src/variable_drivers_group.cpp
    ${PROJECT_SOURCE_DIR}/src/variable_loads_group.cpp
    ${PROJECT_SOURCE_DIR}/src/enum_struct_union_packed_array_typespec_group.cpp
    ${PROJECT_SOURCE_DIR}/src/operand_group.cpp
    ${PROJECT_SOURCE_DIR}/src/stmt.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_sequence_inst_named_event_group.cpp
    ${PROJECT_SOURCE_DIR}/src/tf_call_args.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_interf_expr_group.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_tchk_term_group.cpp
    ${PROJECT_SOURCE_DIR}/src/simple_expr_use_group.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_dist.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_typespec_group.cpp
    ${PROJECT_SOURCE_DIR}/src/property_inst_spec_group.cpp
    ${PROJECT_SOURCE_DIR}/src/property_expr_named_event_group.cpp
    ${PROJECT_SOURCE_DIR}/src/property_expr_group.cpp
    ${PROJECT_SOURCE_DIR}/src/pattern_expr_group.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_constr_group.cpp
    ${PROJECT_SOURCE_DIR}/src/parameters.cpp
    ${PROJECT_SOURCE_DIR}/src/interf_prog_mod_group.cpp
    ${PROJECT_SOURCE_DIR}/src/variables_operation_group.cpp)

foreach(src_file ${uhdm_SRC})
  set_source_files_properties(${src_file} PROPERTIES GENERATED TRUE)
endforeach(src_file ${uhdm_SRC})

set(UHDM_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/headers/uhdm.h)

add_library(uhdm STATIC ${uhdm_SRC})
set_target_properties(uhdm PROPERTIES PUBLIC_HEADER "${UHDM_PUBLIC_HEADERS}")
target_link_libraries(uhdm PUBLIC capnp)

if (UNIX)
  target_link_libraries(uhdm
      PUBLIC dl
      PUBLIC util
      PUBLIC m
      PUBLIC rt
      PUBLIC pthread)
endif()

add_dependencies(uhdm GenerateCode)
add_dependencies(GenerateCode capnpc capnp_tool capnpc_cpp)

if (UHDM_BUILD_TESTS)
  enable_testing()
  # All unit-tests are registered with this custom target as dependency, so
  # just `make UnitTests` will build them all.
  add_custom_target(UnitTests)

  function(register_tests)
    foreach(test_cc_file IN LISTS ARGN)
      # We create the binary name and test prefix from the cpp-filepath
      get_filename_component(test_bin ${test_cc_file} NAME_WE)
      get_filename_component(test_prefix ${test_cc_file} DIRECTORY)

      add_executable(${test_bin} ${PROJECT_SOURCE_DIR}/${test_cc_file})
      target_link_libraries(${test_bin} PRIVATE uhdm)
      add_test(
        NAME ${test_prefix}/${test_bin} COMMAND ${test_bin}
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

      # Now, add this binary to our UnitTests target that it builds this
      add_dependencies(UnitTests ${test_bin})
    endforeach()
  endfunction()
  register_tests(
    tests/test1.cpp
    tests/test2.cpp
    tests/test3.cpp
    tests/test4.cpp
    tests/test_tf_call.cpp
    tests/test_process.cpp
    tests/test_listener.cpp
    tests/test-dump.cpp
    tests/listener_elab.cpp
    tests/full_elab.cpp
  )
endif()

# A debugging binary
add_executable(uhdm-dump ${PROJECT_SOURCE_DIR}/util/uhdm-dump.cpp)
target_link_libraries(uhdm-dump PRIVATE uhdm)

# Installation target
install(
  TARGETS uhdm capnp kj uhdm-dump
  ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/uhdm
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm/include)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/headers/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm/headers/)
