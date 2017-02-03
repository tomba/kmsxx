include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

pkg_search_module(PYTHON REQUIRED ${KMSXX_PYTHON_VERSION})
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
set_target_properties(pykms PROPERTIES LIBRARY_OUTPUT_DIRECTORY "")

configure_file(__init__.py __init__.py COPYONLY)

set(PY_DESTDIR lib/python${PYTHON_VERSION}/site-packages/pykms)
install(FILES __init__.py DESTINATION ${PY_DESTDIR})
install(TARGETS pykms DESTINATION ${PY_DESTDIR})
