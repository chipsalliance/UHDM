# Set up swig
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH} ${GENDIR}/uhdm ${CMAKE_CURRENT_SOURCE_DIR})


set_source_files_properties(${PROJECT_NAME}.i 
	PROPERTIES CPLUSPLUS ON 
	INCLUDE_DIRECTORIES ${GENDIR}/uhdm
	GENERATED_INCLUDE_DIRECTORIES ${GENDIR}/uhdm
	DEPENDS uhdm
)

# Add swig module
swig_add_library(py_uhdm
	TYPE MODULE
	LANGUAGE python
	SOURCES 
	${PROJECT_NAME}.i swig_main.cpp
)
target_link_libraries(py_uhdm 
	PRIVATE uhdm ${PYTHON_LIBRARIES}
)
set_target_properties(py_uhdm PROPERTIES 
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
)
install(
	FILES uhdm.py 
	DESTINATION ${PYTHON_SITE_PACKAGES}/
)
install(
	TARGETS ${SWIG_MODULE_py_udhm_REAL_NAME} 
	LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES}
)

add_test(
	NAME python-tests-import
	COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=$<TARGET_PROPERTY:uhdm,SWIG_SUPPORT_FILES_DIRECTORY>${PS}$<TARGET_FILE_DIR:uhdm>" "${Python3_EXECUTABLE}" -c "import uhdm"
)

