cmake_minimum_required(VERSION 2.8)
project(kms++)

include(LTO.cmake)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CheckCXXCompilerFlag)

IF(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
ENDIF()

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

option(BUILD_SHARED_LIBS "Build shared libs" FALSE)
option(TREAT_WARNINGS_AS_ERRORS "Treat warnings as errors" FALSE)

set(KMSXX_ENABLE_PYTHON ON CACHE BOOL "Enable Python wrappers")
set(KMSXX_PYTHON_VERSION "python3;python2" CACHE STRING "Python pkgconfig package")

set(KMSXX_ENABLE_KMSCUBE OFF CACHE BOOL "Enable kmscube")

set(KMSXX_ENABLE_THREADING ON CACHE BOOL "Enable threading for parallelized drawing")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -Wno-unused-parameter")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wno-unused-parameter")

if (CMAKE_COMPILER_IS_GNUCC)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        # GCC 4.x seems to warn too much
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
    endif()
endif()

# HACK: cmake always adds "-rdynamic", this removes it
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if (TREAT_WARNINGS_AS_ERRORS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

# static link libc
# set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

check_lto()

if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
    if (LTO_WORKS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
        set(CMAKE_AR "${LTO_AR}")
        set(CMAKE_RANLIB "${LTO_RANLIB}")
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM libdrm>=2.4.64 REQUIRED)

pkg_check_modules(LIBDRM_OMAP libdrm_omap)
if(LIBDRM_OMAP_FOUND)
    add_definitions(-DHAS_LIBDRM_OMAP)
endif()

enable_testing()

add_subdirectory(kms++)
add_subdirectory(kms++util)
add_subdirectory(utils)

if(KMSXX_ENABLE_KMSCUBE)
	add_subdirectory(kmscube)
endif()

if(KMSXX_ENABLE_PYTHON)
        add_subdirectory(py)
endif()

add_custom_target(docs SOURCES "README.md")
