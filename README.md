# libkms++ - C++ library for kernel mode setting

libkms++ is a C++11 library for kernel mode setting.

Also included are some simple utilities for KMS and python bindings for libkms++.

## Utilities

- testpat - set modes and planes and show test pattern on crtcs/planes, and test page flips
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

## Build options

You can use the following cmake flags to control the build. Use `-DFLAG=VALUE` to set them.

Option name           | Values        | Default  | Notes
--------------------- | ------------- | -------- | --------
CMAKE_BUILD_TYPE      | Release/Debug | Release  |
LIBKMS_ENABLE_PYTHON  | ON/OFF        | ON       |
LIBKMS_ENABLE_KMSCUBE | ON/OFF        | OFF      |

## Env variables

You can use the following runtime environmental variables to control the behavior of libkms.

Variable                          | Description
--------------------------------- | -------------
LIBKMSXX_DISABLE_UNIVERSAL_PLANES | Set to disable the use of universal planes
LIBKMSXX_DISABLE_ATOMIC           | Set to disable the use of atomic modesetting
