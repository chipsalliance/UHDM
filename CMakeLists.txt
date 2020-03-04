# -*- mode:cmake -*-
cmake_minimum_required(VERSION 3.0)

set(BUILD_TESTING OFF CACHE BOOL "Don't build capnproto tests")
add_subdirectory(third_party/capnproto)

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

set(CMAKE_CXX_STANDARD 14)

include_directories("${PROJECT_SOURCE_DIR}/include/")
include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_SOURCE_DIR}/headers/")
include_directories("${PROJECT_SOURCE_DIR}/src/")
include_directories("${PROJECT_SOURCE_DIR}/third_party/capnproto/c++/src/")
include_directories("${PROJECT_SOURCE_DIR}/third_party/UHDM/src/")

# Directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/dist/${CMAKE_BUILD_TYPE}/)
set(CMAKE_BUILD_FILES_DIRECTORY ${dir})
set(CMAKE_BUILD_DIRECTORY ${dir})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CXX_WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O0 -g ${MY_CXX_WARNING_FLAGS}"
)
set(CMAKE_CXX_FLAGS_RELEASE
    "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O0 -DNDEBUG ${MY_CXX_WARNING_FLAGS}"
)

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
  COMMAND tclsh ${PROJECT_SOURCE_DIR}/model_gen.tcl
          ${PROJECT_SOURCE_DIR}/model/models.lst ${CMAKE_BINARY_DIR}
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/../"
  DEPENDS ${PROJECT_SOURCE_DIR}/model_gen.tcl
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
    ${PROJECT_SOURCE_DIR}/src/tf_call_args.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_interf_expr_group.cpp
    ${PROJECT_SOURCE_DIR}/src/expr_dist.cpp)

foreach(src_file ${uhdm_SRC})
  set_source_files_properties(${src_file} PROPERTIES GENERATED TRUE)
endforeach(src_file ${uhdm_SRC})

set(UHDM_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/headers/uhdm.h)

add_library(uhdm STATIC ${uhdm_SRC})
set_target_properties(uhdm PROPERTIES PUBLIC_HEADER "${UHDM_PUBLIC_HEADERS}")

add_dependencies(uhdm GenerateCode)

set(ALL_LIBRARIES_FOR_UHDM
    uhdm
    capnp
    kj
    dl
    util
    m
    rt
    pthread)

add_dependencies(GenerateCode capnp capnpc capnp_tool capnpc_cpp)

enable_testing()

add_executable(uhdm-test1 ${PROJECT_SOURCE_DIR}/tests/test1.cpp)
set_target_properties(uhdm-test1 PROPERTIES OUTPUT_NAME uhdm-test1)
add_dependencies(uhdm-test1 uhdm)
target_link_libraries(uhdm-test1 ${ALL_LIBRARIES_FOR_UHDM})
add_test(
  NAME uhdm-test1
  COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test1
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(uhdm-test2 ${PROJECT_SOURCE_DIR}/tests/test2.cpp)
set_target_properties(uhdm-test2 PROPERTIES OUTPUT_NAME uhdm-test2)
add_dependencies(uhdm-test2 uhdm)
target_link_libraries(uhdm-test2 ${ALL_LIBRARIES_FOR_UHDM})
add_test(
  NAME uhdm-test2
  COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test2
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(uhdm-test3 ${PROJECT_SOURCE_DIR}/tests/test3.cpp)
set_target_properties(uhdm-test3 PROPERTIES OUTPUT_NAME uhdm-test3)
add_dependencies(uhdm-test3 uhdm)
target_link_libraries(uhdm-test3 ${ALL_LIBRARIES_FOR_UHDM})
add_test(
  NAME uhdm-test3
  COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test3
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(uhdm-test4 ${PROJECT_SOURCE_DIR}/tests/test4.cpp)
set_target_properties(uhdm-test4 PROPERTIES OUTPUT_NAME uhdm-test4)
add_dependencies(uhdm-test4 uhdm)
target_link_libraries(uhdm-test4 ${ALL_LIBRARIES_FOR_UHDM})
add_test(
  NAME uhdm-test4
  COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test4
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(uhdm-test-tf-call ${PROJECT_SOURCE_DIR}/tests/test_tf_call.cpp)
set_target_properties(uhdm-test-tf-call PROPERTIES OUTPUT_NAME uhdm-test-tf-call)
add_dependencies(uhdm-test-tf-call uhdm)
target_link_libraries(uhdm-test-tf-call ${ALL_LIBRARIES_FOR_UHDM})
add_test(
  NAME uhdm-test-tf-call
  COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test-tf-call
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(uhdm-listener ${PROJECT_SOURCE_DIR}/tests/test_listener.cpp)
set_target_properties(uhdm-listener PROPERTIES OUTPUT_NAME uhdm-listener)
add_dependencies(uhdm-listener uhdm)
target_link_libraries(uhdm-listener ${ALL_LIBRARIES_FOR_UHDM})
add_test(
  NAME uhdm-listener
  COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-listener
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(uhdm-dump ${PROJECT_SOURCE_DIR}/tests/dump.cpp)
set_target_properties(uhdm-dump PROPERTIES OUTPUT_NAME uhdm-dump)
add_dependencies(uhdm-dump uhdm)
target_link_libraries(uhdm-dump ${ALL_LIBRARIES_FOR_UHDM})
add_test(
  NAME uhdm-dump
  COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-dump
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

# Installation target
install(
  TARGETS uhdm
  ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/uhdm
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm/include)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/headers/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uhdm/headers/)
