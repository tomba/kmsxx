cmake_minimum_required(VERSION 2.8)
project(kms++)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: Debug, Release."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

set(LIBKMS_ENABLE_PYTHON ON CACHE BOOL "Enable Python wrappers")
set(LIBKMS_ENABLE_LUA ON CACHE BOOL "Enable Lua wrappers")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# static link libc
# set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM libdrm REQUIRED)

enable_testing()

add_subdirectory(libkms++)
add_subdirectory(libkmstest)
add_subdirectory(tests)

if(LIBKMS_ENABLE_PYTHON)
        add_subdirectory(py)
endif()

if(LIBKMS_ENABLE_LUA)
        add_subdirectory(lua)
endif()
