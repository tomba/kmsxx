include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

pkg_check_modules(PYTHON python3 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

include_directories(${PROJECT_SOURCE_DIR}/ext/pybind11/include)

set(SRCS pykms.cpp pykmsbase.cpp pykmsutil.cpp pyvid.cpp)

if(LIBDRM_OMAP_FOUND)
    set(SRCS ${SRCS} pykmsomap.cpp)
endif()

add_library(pykms SHARED ${SRCS})
target_link_libraries(pykms kms++ kms++util ${LIBDRM_LIBRARIES})

# Don't add a 'lib' prefix to the shared library
set_target_properties(pykms PROPERTIES PREFIX "")

file(GLOB PY_SRCS "*.py")
add_custom_target(pyextras SOURCES ${PY_SRCS})

add_test(NAME pytest COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/functest.py")
set_property(TEST pytest PROPERTY
        ENVIRONMENT "PYTHONPATH=." "LD_LIBRARY_PATH=."
)

# XXX Where should pykms.so be installed?
#install(TARGETS pykms DESTINATION lib)
