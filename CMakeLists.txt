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
    ${PROJECT_SOURCE_DIR}/src/interf_prog_mod_group.cpp)

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

enable_testing()

add_executable(uhdm-test1 ${PROJECT_SOURCE_DIR}/tests/test1.cpp)
target_link_libraries(uhdm-test1 PRIVATE uhdm)
add_test(
  NAME uhdm-test1
  COMMAND uhdm-test1
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(uhdm-test2 ${PROJECT_SOURCE_DIR}/tests/test2.cpp)
target_link_libraries(uhdm-test2 PRIVATE uhdm)
add_test(
  NAME uhdm-test2
  COMMAND uhdm-test2
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(uhdm-test3 ${PROJECT_SOURCE_DIR}/tests/test3.cpp)
target_link_libraries(uhdm-test3 PRIVATE uhdm)
add_test(
  NAME uhdm-test3
  COMMAND uhdm-test3
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(uhdm-test4 ${PROJECT_SOURCE_DIR}/tests/test4.cpp)
target_link_libraries(uhdm-test4 PRIVATE uhdm)
add_test(
  NAME uhdm-test4
  COMMAND uhdm-test4
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(uhdm-test-tf-call ${PROJECT_SOURCE_DIR}/tests/test_tf_call.cpp)
target_link_libraries(uhdm-test-tf-call PRIVATE uhdm)
add_test(
  NAME uhdm-test-tf-call
  COMMAND uhdm-test-tf-call
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(uhdm-test-process ${PROJECT_SOURCE_DIR}/tests/test_process.cpp)
target_link_libraries(uhdm-test-process PRIVATE uhdm)
add_test(
  NAME uhdm-test-process
  COMMAND uhdm-test-process
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(uhdm-listener ${PROJECT_SOURCE_DIR}/tests/test_listener.cpp)
target_link_libraries(uhdm-listener PRIVATE uhdm)
add_test(
  NAME uhdm-listener
  COMMAND uhdm-listener
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(uhdm-dump ${PROJECT_SOURCE_DIR}/tests/dump.cpp)
target_link_libraries(uhdm-dump PRIVATE uhdm)
add_test(
  NAME uhdm-dump
  COMMAND uhdm-dump --elab surelog.uhdm
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(listener-elab ${PROJECT_SOURCE_DIR}/tests/listener_elab.cpp)
target_link_libraries(listener-elab PRIVATE uhdm)
add_test(
  NAME listener-elab
  COMMAND listener-elab
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(full-elab ${PROJECT_SOURCE_DIR}/tests/full_elab.cpp)
target_link_libraries(full-elab PRIVATE uhdm)
add_test(
  NAME full-elab
  COMMAND full-elab
  WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Installation target
install(
  TARGETS uhdm capnp kj
  ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/uhdm
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm/include)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/headers/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm/headers/)
