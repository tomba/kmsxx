set(SWIG_EXECUTABLE "swig3.0")
find_package(SWIG 3.0 REQUIRED)
include(${SWIG_USE_FILE})

include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

pkg_search_module(LUA REQUIRED lua5.2 lua)
include_directories(${LUA_INCLUDE_DIRS})
link_directories(${LUA_LIBRARY_DIRS})

#include_directories(libkms)
#XXX
include_directories(../libkms++)

set_source_files_properties(luakms.i PROPERTIES CPLUSPLUS ON)
swig_add_module(luakms lua luakms.i)
swig_link_libraries(luakms kms++ ${LIBDRM_LIBRARIES} ${LUA_LIBRARIES})

add_custom_target(luaextras SOURCES test.lua)
