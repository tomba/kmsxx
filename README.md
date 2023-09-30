[![Build Status](https://github.com/tomba/kmsxx/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/tomba/kmsxx/actions/workflows/c-cpp.yml)

# kms++ - C++ library for kernel mode setting

kms++ is a C++17 library for kernel mode setting.

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

```
meson setup build
ninja -C build
```

## Cross compiling instructions:

```
meson build --cross-file=<path-to-meson-cross-file>
ninja -C build
```

Here is my cross file for arm32 (where ${BROOT} is path to my buildroot output dir):

```
[binaries]
c = ['ccache', '${BROOT}/host/bin/arm-buildroot-linux-gnueabihf-gcc']
cpp = ['ccache', '${BROOT}/host/bin/arm-buildroot-linux-gnueabihf-g++']
ar = '${BROOT}/host/bin/arm-buildroot-linux-gnueabihf-ar'
strip = '${BROOT}/host/bin/arm-buildroot-linux-gnueabihf-strip'
pkgconfig = '${BROOT}/host/bin/pkg-config'

[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'arm'
endian = 'little'
```

## Build options

You can use meson options to configure the build. E.g.

```
meson build -Dbuildtype=debug -Dkmscube=true
```

Use `meson configure build` to see all the configuration options and their current values.

kms++ specific build options are:

Option name      | Values                  | Default         | Notes
---------------- | -------------           | --------------- | --------
pykms            | true, false             | true            | Python bindings
kmscube          | true, false             | false           | GLES kmscube
omap             | enabled, disabled, auto | auto            | libdrm-omap support

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
