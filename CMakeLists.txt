# -*- mode:cmake -*-
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

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
option(WITH_PYTHON_GENERATOR "Use Python generator instead of TCL." OFF)

project(UHDM)

# NOTE: Policy changes has to happen before adding any subprojects
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
message(STATUS "Python3_EXECUTABLE = ${Python3_EXECUTABLE}")

set(BUILD_TESTING OFF CACHE BOOL "Don't build capnproto tests")
add_subdirectory(third_party/capnproto EXCLUDE_FROM_ALL)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# NOTE: Set the global output directories after the subprojects have had their go at it
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CXX_WARNING_FLAGS}")

# Directory where code is generated into.
set(GENDIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

if(MSVC)
  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} ${TCMALLOC_COMPILE_OPTIONS} /W3 /bigobj ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} /Zi /W3 /bigobj ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} /W3 /bigobj ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_EXE_LINKER_FLAGS /STACK:8388608)  # 8MB stack size
else()
  if(DEFINED ENV{MSYSTEM})
    # Under MSYS some files are too large to build without additional flags
    set(MSYS_COMPILE_OPTIONS "-m64 -Wa,-mbig-obj")
  endif()

  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O0 -g ${MSYS_COMPILE_OPTIONS} ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O3 -g ${MSYS_COMPILE_OPTIONS} ${MY_CXX_WARNING_FLAGS}"
  )
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O3 -DNDEBUG ${MSYS_COMPILE_OPTIONS} ${MY_CXX_WARNING_FLAGS}"
  )

  # Some of these files make the optimizer take so long, that we excempt some
  # of these files and compile with reduced optimization.
  # TODO: Reduce complexity/branches etc. whatever makes the optimizer freak
  # out. The code-generation is pretty linear right now.
  set_property(SOURCE
    ${GENDIR}/src/Serializer_save.cpp
    ${GENDIR}/src/Serializer_restore.cpp
    PROPERTY COMPILE_FLAGS -O0
 )

endif()

# All the files the generator depends on.
file(GLOB yaml_SRC ${PROJECT_SOURCE_DIR}/model/*.yaml)
file(GLOB templates_SRC ${PROJECT_SOURCE_DIR}/templates/*.h
     ${PROJECT_SOURCE_DIR}/templates/*.cpp)

set(model-GENERATED_UHDM ${GENDIR}/src/UHDM.capnp)
set_source_files_properties(${model-GENERATED_UHDM} PROPERTIES GENERATED TRUE)

if (WITH_PYTHON_GENERATOR)
  file(GLOB py_SRC ${PROJECT_SOURCE_DIR}/scripts/*.py)
  add_custom_command(
    OUTPUT ${model-GENERATED_UHDM}
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/generate.py --source-dirpath=${UHDM_SOURCE_DIR} -output-dirpath=${GENDIR}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DEPENDS ${PROJECT_SOURCE_DIR}/model/models.lst
            ${PROJECT_SOURCE_DIR}/templates/UHDM.capnp
            ${py_SRC}
            ${yaml_SRC}
            ${templates_SRC})
else()
  add_custom_command(
    OUTPUT ${model-GENERATED_UHDM}
    COMMAND tclsh ${PROJECT_SOURCE_DIR}/model_gen/model_gen.tcl
            ${PROJECT_SOURCE_DIR}/model/models.lst ${CMAKE_CURRENT_BINARY_DIR}
            ${GENDIR}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DEPENDS ${PROJECT_SOURCE_DIR}/model_gen/model_gen.tcl
            ${PROJECT_SOURCE_DIR}/model_gen/generate_elaborator.tcl
            ${PROJECT_SOURCE_DIR}/model_gen/parse_model.tcl
            ${PROJECT_SOURCE_DIR}/model/models.lst
            ${PROJECT_SOURCE_DIR}/templates/UHDM.capnp
            ${yaml_SRC}
            ${templates_SRC})
endif()

set(model-GENERATED_SRC ${GENDIR}/src/UHDM.capnp.h
                        ${GENDIR}/src/UHDM.capnp.c++)
foreach(header_file ${model-GENERATED_SRC})
  set_source_files_properties(${header_file} PROPERTIES GENERATED TRUE)
endforeach(header_file ${model-GENERATED_SRC})

set(CAPNP_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party/capnproto/c++/src/capnp)

add_custom_command(
  OUTPUT ${model-GENERATED_SRC}
  COMMAND ${CAPNP_DIR}/capnp compile -o${CAPNP_DIR}/capnpc-c++ ${model-GENERATED_UHDM}
  DEPENDS capnpc capnpc_cpp ${model-GENERATED_UHDM})
add_custom_target(GenerateCode DEPENDS ${model-GENERATED_SRC})

set(uhdm-GENERATED_SRC
    ${GENDIR}/src/ExprEval.cpp
    ${GENDIR}/src/Serializer_restore.cpp
    ${GENDIR}/src/Serializer_save.cpp
    ${GENDIR}/src/SymbolFactory.cpp
    ${GENDIR}/src/UHDM.capnp.c++
    ${GENDIR}/src/actual_group.cpp
    ${GENDIR}/src/assertion.cpp
    ${GENDIR}/src/clone_tree.cpp
    ${GENDIR}/src/constraint_item_group.cpp
    ${GENDIR}/src/enum_struct_packed_net_group.cpp
    ${GENDIR}/src/enum_struct_union_packed_array_typespec_group.cpp
    ${GENDIR}/src/enum_struct_union_packed_var_group.cpp
    ${GENDIR}/src/expr_constr_group.cpp
    ${GENDIR}/src/expr_dist.cpp
    ${GENDIR}/src/expr_interf_expr_group.cpp
    ${GENDIR}/src/expr_range_group.cpp
    ${GENDIR}/src/expr_ref_obj_group.cpp
    ${GENDIR}/src/expr_sequence_inst_group.cpp
    ${GENDIR}/src/expr_sequence_inst_named_event_group.cpp
    ${GENDIR}/src/expr_tchk_term_group.cpp
    ${GENDIR}/src/expr_typespec_group.cpp
    ${GENDIR}/src/instance_item.cpp
    ${GENDIR}/src/interf_prog_mod_group.cpp
    ${GENDIR}/src/method_func_task_call_group.cpp
    ${GENDIR}/src/named_event_sequence_expr_group.cpp
    ${GENDIR}/src/nets_vars_ref_obj_group.cpp
    ${GENDIR}/src/operand_group.cpp
    ${GENDIR}/src/parameters.cpp
    ${GENDIR}/src/pattern.cpp
    ${GENDIR}/src/pattern_expr_group.cpp
    ${GENDIR}/src/property_expr_group.cpp
    ${GENDIR}/src/property_expr_named_event_group.cpp
    ${GENDIR}/src/property_inst_spec_group.cpp
    ${GENDIR}/src/ref_obj_interf_net_var_group.cpp
    ${GENDIR}/src/sequence_expr_group.cpp
    ${GENDIR}/src/sequence_expr_multiclock_group.cpp
    ${GENDIR}/src/simple_expr_use_group.cpp
    ${GENDIR}/src/stmt.cpp
    ${GENDIR}/src/sys_func_task_call_group.cpp
    ${GENDIR}/src/task_func_named_begin_fork_group.cpp
    ${GENDIR}/src/tf_call_args.cpp
    ${GENDIR}/src/variable_drivers_group.cpp
    ${GENDIR}/src/variable_loads_group.cpp
    ${GENDIR}/src/variables_operation_group.cpp
    ${GENDIR}/src/vpi_listener.cpp
    ${GENDIR}/src/vpi_user.cpp
    ${GENDIR}/src/vpi_visitor.cpp
)

foreach(src_file ${uhdm-GENERATED_SRC})
  set_source_files_properties(${src_file} PROPERTIES GENERATED TRUE)
endforeach(src_file ${uhdm-GENERATED_SRC})

add_library(uhdm STATIC ${uhdm-GENERATED_SRC})
set_target_properties(uhdm PROPERTIES PUBLIC_HEADER ${GENDIR}/uhdm/uhdm.h)

# Our uhdm headers come with angle brackets (indicating system headers),
# make sure that the compiler looks here first before finding an existing UHDM
# installation. Hence we use SYSTEM
target_include_directories(uhdm SYSTEM PUBLIC
  $<BUILD_INTERFACE:${GENDIR}>
  $<INSTALL_INTERFACE:include>)
target_include_directories(uhdm PRIVATE
  ${PROJECT_SOURCE_DIR}/third_party/capnproto/c++/src
  ${PROJECT_SOURCE_DIR}/third_party/UHDM/src)
target_compile_definitions(uhdm
  PUBLIC PLI_DLLISPEC=
  PUBLIC PLI_DLLESPEC=)
target_link_libraries(uhdm PUBLIC capnp)

if(APPLE)
  target_link_libraries(uhdm
      PUBLIC dl
      PUBLIC util
      PUBLIC m
      PUBLIC pthread)
elseif(UNIX)
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
    tests/vpi_get_test.cpp
    tests/vpi_value_conversion_test.cpp
    tests/testErrorHandler.cpp
    tests/test_classes.cpp
    tests/test_garbage_collect.cpp
  )
endif()

# Useful utilities
add_executable(uhdm-dump ${PROJECT_SOURCE_DIR}/util/uhdm-dump.cpp)
target_link_libraries(uhdm-dump PRIVATE uhdm)
add_executable(uhdm-hier ${PROJECT_SOURCE_DIR}/util/uhdm-hier.cpp)
target_link_libraries(uhdm-hier PRIVATE uhdm)

# Installation tester
add_executable(test_inst EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tests/test1.cpp)
set_property(TARGET test_inst PROPERTY INCLUDE_DIRECTORIES) # Clear the list of inherited include directories
set_property(TARGET test_inst PROPERTY LINK_LIBRARIES) # Clear the list of inherited link libraries
set_property(TARGET test_inst PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin) # Generate it in a known location
target_compile_definitions(test_inst
  PUBLIC PLI_DLLISPEC=
  PUBLIC PLI_DLLESPEC=)
target_include_directories(test_inst SYSTEM PRIVATE
  ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(test_inst PRIVATE
  ${CMAKE_INSTALL_PREFIX}/lib/uhdm/${CMAKE_STATIC_LIBRARY_PREFIX}uhdm${CMAKE_STATIC_LIBRARY_SUFFIX}
  ${CMAKE_INSTALL_PREFIX}/lib/uhdm/${CMAKE_STATIC_LIBRARY_PREFIX}capnp${CMAKE_STATIC_LIBRARY_SUFFIX}
  ${CMAKE_INSTALL_PREFIX}/lib/uhdm/${CMAKE_STATIC_LIBRARY_PREFIX}kj${CMAKE_STATIC_LIBRARY_SUFFIX}
  $<$<PLATFORM_ID:Darwin>:dl>
  $<$<PLATFORM_ID:Darwin>:util>
  $<$<AND:$<PLATFORM_ID:Darwin>,$<NOT:$<PLATFORM_ID:Apple>>>:m>
  $<$<PLATFORM_ID:Darwin>:rt>
  $<$<PLATFORM_ID:Darwin>:pthread>)

# Installation target
install(
  TARGETS uhdm capnp kj uhdm-dump uhdm-hier
  ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/uhdm
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm)
install(DIRECTORY ${GENDIR}/uhdm/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm/)
install(FILES ${GENDIR}/src/UHDM.capnp
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/uhdm)
