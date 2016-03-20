cmake_minimum_required(VERSION 2.8)
project(kms++)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: Debug, Release."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

set(LIBKMS_ENABLE_PYTHON ON CACHE BOOL "Enable Python wrappers")
set(LIBKMS_ENABLE_KMSCUBE OFF CACHE BOOL "Enable kmscube")
set(LIBKMS_ENABLE_LTO OFF CACHE BOOL "Enable LTO")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# static link libc
# set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

if(LIBKMS_ENABLE_LTO)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -fwhole-program")
	set(CMAKE_AR gcc-ar)
	set(CMAKE_RANLIB gcc-ranlib)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM libdrm REQUIRED)

enable_testing()

add_subdirectory(libkms++)
add_subdirectory(libkmstest)
add_subdirectory(tests)

if(LIBKMS_ENABLE_KMSCUBE)
	add_subdirectory(kmscube)
endif()

if(LIBKMS_ENABLE_PYTHON)
        add_subdirectory(py)
endif()
