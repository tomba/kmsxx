# libkms++ - C++ library for kernel mode setting

libkms++ is a C++11 library for kernel mode setting.

Also included are simple test tools for KMS and python and lua wrappers for libkms++.

## Dependencies:

- libdrm
- SWIG 3.x (for python & lua bindings)
- Python 3.x (for python bindings)
- Lua 5.x (for lua bindings)

## Build instructions:

```
$ mkdir build
$ cd build
$ cmake ..
$ make -j4
```

## Cross compiling instructions:

Directions for cross compiling depend on your environment. These are for mine (buildroot):

As above, but specify `-DCMAKE_TOOLCHAIN_FILE=<path>/your-toolchain.cmake` for cmake, where your-toolchain.cmake is something similar to:

```
SET(CMAKE_SYSTEM_NAME Linux)

SET(BROOT "<buildroot>/output/")

# specify the cross compiler
SET(CMAKE_C_COMPILER   ${BROOT}/host/usr/bin/arm-buildroot-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER ${BROOT}/host/usr/bin/arm-buildroot-linux-gnueabihf-g++)

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH ${BROOT}/target ${BROOT}/host)

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

## Build options

You can use the following cmake flags to control the build. Use `-DFLAG=VALUE` to set them.

Option name          | Values        | Default
-------------------- | ------------- | --------
CMAKE_BUILD_TYPE     | Release/Debug | Release
LIBKMS_ENABLE_PYTHON | ON/OFF        | ON
LIBKMS_ENABLE_LUA    | ON/OFF        | ON
LIBKMS_ENABLE_KMSCUBE | ON/OFF       | OFF

## Env variables

You can use the following environmental variables to control the behavior of libkms.

Variable                          | Description
--------------------------------- | -------------
LIBKMSXX_DISABLE_UNIVERSAL_PLANES | Set to disable the use of universal planes
LIBKMSXX_DISABLE_ATOMIC           | Set to disable the use of atomic modesetting
