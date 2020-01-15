# -*- mode:cmake -*-
cmake_minimum_required (VERSION 3.0)

# Detect build type, fallback to release and throw a warning if use didn't specify any
if(NOT CMAKE_BUILD_TYPE)
  message(WARNING "Build type not set, falling back to Release mode.
 To specify build type use:
 -DCMAKE_BUILD_TYPE=<mode> where <mode> is Debug or Release.")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
       "Choose the type of build, options are: Debug Release."
       FORCE)
endif(NOT CMAKE_BUILD_TYPE)

option(WITH_LIBCXX "Building with clang++ and libc++(in Linux). To enable with: -DWITH_LIBCXX=On" On)

project(UHDM)

set(CMAKE_CXX_STANDARD 14)

INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/include/")
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/")
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/headers/")
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/src/")

# Directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist/${CMAKE_BUILD_TYPE}/)
set(CMAKE_BUILD_FILES_DIRECTORY ${dir})
set(CMAKE_BUILD_DIRECTORY ${dir})

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${MY_CXX_WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O0 -g ${MY_CXX_WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${TCMALLOC_COMPILE_OPTIONS} -Wall -O3 -DNDEBUG ${MY_CXX_WARNING_FLAGS}")

# model_gen generated
set(model-GENERATED_SRC
  ${PROJECT_SOURCE_DIR}/src/UHDM.capnp.h
  ${PROJECT_SOURCE_DIR}/src/UHDM.capnp.c++ 
 )

foreach(header_file ${model-GENERATED_SRC} )
      set_source_files_properties(
          ${header_file}
          PROPERTIES
          GENERATED TRUE
          )
endforeach( header_file ${model-GENERATED_SRC} )
add_custom_target(GenerateCode DEPENDS ${model-GENERATED_SRC})
add_custom_command(
   OUTPUT ${model-GENERATED_SRC}
   COMMAND
   tclsh ${PROJECT_SOURCE_DIR}/model_gen.tcl ${PROJECT_SOURCE_DIR}/model/models.lst
   WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/../"
   DEPENDS ${PROJECT_SOURCE_DIR}/model_gen.tcl ${PROJECT_SOURCE_DIR}/model/models.lst ${PROJECT_SOURCE_DIR}/model/*.yaml ${PROJECT_SOURCE_DIR}/templates/*.h ${PROJECT_SOURCE_DIR}/templates/*.cpp ${PROJECT_SOURCE_DIR}/templates/UHDM.capnp
   )

set (uhdm_SRC
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
  )

foreach( src_file ${uhdm_SRC} )
      set_source_files_properties(
          ${src_file}
          PROPERTIES
          GENERATED TRUE
          )
endforeach( src_file ${uhdm_SRC} )

set (UHDM_PUBLIC_HEADERS
  ${PROJECT_SOURCE_DIR}/headers/uhdm.h
) 
  
add_library(uhdm STATIC ${uhdm_SRC})
set_target_properties(uhdm PROPERTIES PUBLIC_HEADER "${UHDM_PUBLIC_HEADERS}")

add_dependencies(uhdm GenerateCode)

set (ALL_LIBRARIES_FOR_UHDM
  uhdm capnp kj
  dl util m rt pthread
)

ENABLE_TESTING()

add_executable(uhdm-test1 ${PROJECT_SOURCE_DIR}/tests/test1.cpp )
set_target_properties(uhdm-test1 PROPERTIES OUTPUT_NAME uhdm-test1 )
add_dependencies(uhdm-test1 uhdm)
target_link_libraries( uhdm-test1 ${ALL_LIBRARIES_FOR_UHDM} )
ADD_TEST(NAME uhdm-test1
         COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test1
         WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
       
add_executable(uhdm-test2 ${PROJECT_SOURCE_DIR}/tests/test2.cpp )
set_target_properties(uhdm-test2 PROPERTIES OUTPUT_NAME uhdm-test2 )
add_dependencies(uhdm-test2 uhdm)
target_link_libraries( uhdm-test2 ${ALL_LIBRARIES_FOR_UHDM} )
ADD_TEST(NAME uhdm-test2
         COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test2
         WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

       

# Installation target     
INSTALL(
  TARGETS uhdm
  ARCHIVE DESTINATION /usr/local/lib/uhdm
  PUBLIC_HEADER DESTINATION /usr/local/include/uhdm
)
INSTALL(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION /usr/local/include/uhdm/include
)
INSTALL(
  DIRECTORY  ${PROJECT_SOURCE_DIR}/headers/
  DESTINATION /usr/local/include/uhdm/headers/
  )
