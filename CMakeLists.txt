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

file(GLOB uhdm_SRC
  ${PROJECT_SOURCE_DIR}/src/vpi_user.cpp
  ${PROJECT_SOURCE_DIR}/src/Serializer.cpp
  ${PROJECT_SOURCE_DIR}/src/UHDM.capnp.c++
  )

set (UHDM_PUBLIC_HEADERS
  ${PROJECT_SOURCE_DIR}/include/sv_vpi_user.h
  ${PROJECT_SOURCE_DIR}/include/vhpi_user.h
  ${PROJECT_SOURCE_DIR}/include/vpi_uhdm.h
  ${PROJECT_SOURCE_DIR}/include/vpi_user.h
  ${PROJECT_SOURCE_DIR}/headers/*.h
) 
  
add_library(uhdm STATIC ${uhdm_SRC})
set_target_properties(uhdm PROPERTIES PUBLIC_HEADER "${UHDM_PUBLIC_HEADERS}")

add_executable(uhdm-test ${PROJECT_SOURCE_DIR}/src/main.cpp
)
set_target_properties(uhdm-test
  PROPERTIES OUTPUT_NAME uhdm-test
)

add_dependencies(uhdm-test uhdm)
add_dependencies(uhdm GenerateCode)

set (ALL_LIBRARIES_FOR_UHDM
  uhdm capnp kj
  dl util m rt pthread
)

# Linkage instructions
target_link_libraries(
  uhdm-test ${ALL_LIBRARIES_FOR_UHDM} 
)

ENABLE_TESTING()

ADD_TEST(NAME uhdm-test
         COMMAND dist/${CMAKE_BUILD_TYPE}/uhdm-test
         WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

# Installation target     
INSTALL(
  TARGETS uhdm
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/uhdm
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uhdm
)
INSTALL(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uhdm/
)
INSTALL(
  DIRECTORY  ${PROJECT_SOURCE_DIR}/headers/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uhdm/
  )
