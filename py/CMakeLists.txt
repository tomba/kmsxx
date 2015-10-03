set(SWIG_EXECUTABLE "swig3.0")
find_package(SWIG 3.0 REQUIRED)
include(${SWIG_USE_FILE})

include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

pkg_check_modules(PYTHON python-3.4 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# XXX how to add these inc dirs in a proper way?
include_directories(../libkms++ ../libkmstest)

#set(CMAKE_SWIG_FLAGS "-I../../libkms")

set_source_files_properties(pykms.i PROPERTIES CPLUSPLUS ON)
swig_add_module(pykms python pykms.i)
swig_link_libraries(pykms kms++ kmstest ${LIBDRM_LIBRARIES} ${PYTHON_LIBRARIES})

add_custom_target(pyextras SOURCES test.py functest.py db.py)

add_test(NAME pytest COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/functest.py")
set_property(TEST pytest PROPERTY
        ENVIRONMENT "PYTHONPATH=." "LD_LIBRARY_PATH=."
)
