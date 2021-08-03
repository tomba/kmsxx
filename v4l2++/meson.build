libv4l2xx_sources = files([
    'src/mediabus.cpp',
    'src/mediadevice.cpp',
    'src/videodevice.cpp',
    'src/videosubdev.cpp',
    'src/pixelformats.cpp',
])

public_headers = [
    'inc/v4l2++/mediabus.h',
    'inc/v4l2++/mediadevice.h',
    'inc/v4l2++/videodevice.h',
    'inc/v4l2++/videosubdev.h',
    'inc/v4l2++/pixelformats.h',
    'inc/v4l2++/helpers.h',
]

private_includes = include_directories('src', 'inc')
public_includes = include_directories('inc')

libv4l2xx_deps = [ libfmt_dep ]

libv4l2xx = library('v4l2++',
                   libv4l2xx_sources,
                   install : true,
                   include_directories : [ private_includes, common_includes ],
                   dependencies : libv4l2xx_deps,
                   version : meson.project_version())


libv4l2xx_dep = declare_dependency(include_directories : public_includes,
                                  link_with : libv4l2xx)

install_headers(public_headers, subdir : 'v4l2++')

pkg = import('pkgconfig')
pkg.generate(libv4l2xx)
