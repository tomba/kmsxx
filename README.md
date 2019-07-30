[![Build Status](https://travis-ci.org/tomba/kmsxx.svg?branch=master)](https://travis-ci.org/tomba/kmsxx)

# kms++ - C++ library for kernel mode setting

kms++ is a C++11 library for kernel mode setting.

Also included are some simple utilities for KMS and python bindings for kms++.

## Utilities

- kmstest - set modes and planes and show test pattern on crtcs/planes, and test page flips
- kmsprint - print information about DRM objects
- kmsview - view raw images
- kmscube - rotating 3D cube on crtcs/planes
- kmscapture - show captured frames from a camera on screen

## Dependencies:

- libdrm
- Python 3.x (for python bindings)

## Build instructions:

To build the Python bindings you need to set up the git-submodule for pybind11:

```
git submodule update --init
```

And to compile:

```
$ mkdir build
$ cd build
$ cmake ..
$ make -j4
```

## Cross compiling instructions:

Directions for cross compiling depend on your environment.

These are for mine with buildroot:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=<buildrootpath>/output/host/usr/share/buildroot/toolchainfile.cmake ..
$ make -j4
```

Your environment may provide similar toolchainfile. If not, you can create a toolchainfile of your own, something along these lines:

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

Option name           | Values          | Default         | Notes
--------------------- | -------------   | --------------- | --------
CMAKE_BUILD_TYPE      | Release/Debug   | Release         |
BUILD_SHARED_LIBS     | ON/OFF          | OFF             |
KMSXX_ENABLE_PYTHON   | ON/OFF          | ON              |
KMSXX_ENABLE_KMSCUBE  | ON/OFF          | OFF             |
KMSXX_PYTHON_VERSION  | python3/python2 | python3;python2 | Name of the python pkgconfig file

## Env variables

You can use the following runtime environmental variables to control the behavior of kms++.

Variable                          | Description
--------------------------------- | -------------
KMSXX_DISABLE_UNIVERSAL_PLANES    | Set to disable the use of universal planes
KMSXX_DISABLE_ATOMIC              | Set to disable the use of atomic modesetting
KMSXX_DEVICE                      | Path to the card device node to use
KMSXX_DRIVER                      | Name of the driver to use. The format is either "drvname" or "drvname:idx"

## Python notes

You can run the python code directly from the build dir by defining PYTHONPATH env variable. For example:

```
PYTHONPATH=build/py py/tests/hpd.py

```
