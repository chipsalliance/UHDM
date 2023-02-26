# Set up swig
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH} ${GENDIR}/uhdm ${CMAKE_CURRENT_SOURCE_DIR})


set_source_files_properties(../python/${PROJECT_NAME}.i 
	PROPERTIES CPLUSPLUS ON 
	INCLUDE_DIRECTORIES ${GENDIR}/uhdm
	GENERATED_INCLUDE_DIRECTORIES ${GENDIR}/uhdm
	DEPENDS uhdm
)

# Add swig module
swig_add_library(swig_example
	TYPE MODULE
	LANGUAGE python
	SOURCES 
	../python/${PROJECT_NAME}.i ${CMAKE_CURRENT_SOURCE_DIR}/swig_main.cpp
)
target_link_libraries(swig_example 
	PRIVATE uhdm ${PYTHON_LIBRARIES}

)

