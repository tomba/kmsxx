cmake_minimum_required(VERSION 2.8)
project(kms++)

include(CheckCXXCompilerFlag)

IF(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
ENDIF()

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

set(LIBKMS_ENABLE_PYTHON ON CACHE BOOL "Enable Python wrappers")
set(LIBKMS_ENABLE_KMSCUBE OFF CACHE BOOL "Enable kmscube")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wno-unused-parameter")

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# static link libc
# set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
    CHECK_CXX_COMPILER_FLAG("-flto" HAS_LTO_FLAG)

    if (HAS_LTO_FLAG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
        set(CMAKE_AR gcc-ar)
        set(CMAKE_RANLIB gcc-ranlib)
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM libdrm REQUIRED)

enable_testing()

add_subdirectory(libkms++)
add_subdirectory(libkms++util)
add_subdirectory(utils)

if(LIBKMS_ENABLE_KMSCUBE)
	add_subdirectory(kmscube)
endif()

if(LIBKMS_ENABLE_PYTHON)
        add_subdirectory(py)
endif()
