cmake_minimum_required(VERSION 2.8)
project(kms++)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: Debug, Release."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM libdrm REQUIRED)

enable_testing()

add_subdirectory(libkms++)
add_subdirectory(tests)
add_subdirectory(py)
add_subdirectory(lua)

