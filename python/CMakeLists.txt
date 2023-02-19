# Set up swig
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH} ${GENDIR}/uhdm ${CMAKE_SOURCE_DIR}/python)

#set( swig_INC
#${GENDIR}/uhdm
#${CMAKE_SOURCE_DIR}/python
#)

#set(CMAKE_SWIG_FLAGS "")
set_source_files_properties(../python/${PROJECT_NAME}.i 
	PROPERTIES CPLUSPLUS ON 
	INCLUDE_DIRECTORIES ${GENDIR}/uhdm
	GENERATED_INCLUDE_DIRECTORIES ${GENDIR}/uhdm
#	INCLUDE_DIRECTORIES ${swig_INC}
#	GENERATED_INCLUDE_DIRECTORIES ${swig_INC}
)
#include_directories(../..)

# Add swig module
swig_add_library(swig_example
	TYPE MODULE
	LANGUAGE python
	SOURCES 
	../python/${PROJECT_NAME}.i ${CMAKE_SOURCE_DIR}/python/swig_main.cpp
	
)
#set_target_properties(swig_example PROPERTIES
#	INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/python
#)
target_link_libraries(swig_example 
	PRIVATE uhdm ${PYTHON_LIBRARIES}

)

