libkmsxx_sources = files([
    'src/atomicreq.cpp',
    'src/blob.cpp',
    'src/card.cpp',
    'src/connector.cpp',
    'src/crtc.cpp',
    'src/dmabufframebuffer.cpp',
    'src/drmobject.cpp',
    'src/drmpropobject.cpp',
    'src/dumbframebuffer.cpp',
    'src/encoder.cpp',
    'src/extframebuffer.cpp',
    'src/framebuffer.cpp',
    'src/helpers.cpp',
    'src/mode_cvt.cpp',
    'src/modedb_cea.cpp',
    'src/modedb.cpp',
    'src/modedb_dmt.cpp',
    'src/pixelformats.cpp',
    'src/plane.cpp',
    'src/property.cpp',
    'src/videomode.cpp',
])

public_headers = [
    'inc/kms++/dmabufframebuffer.h',
    'inc/kms++/atomicreq.h',
    'inc/kms++/property.h',
    'inc/kms++/plane.h',
    'inc/kms++/kms++.h',
    'inc/kms++/connector.h',
    'inc/kms++/card.h',
    'inc/kms++/modedb.h',
    'inc/kms++/pagefliphandler.h',
    'inc/kms++/encoder.h',
    'inc/kms++/decls.h',
    'inc/kms++/videomode.h',
    'inc/kms++/drmobject.h',
    'inc/kms++/pixelformats.h',
    'inc/kms++/crtc.h',
    'inc/kms++/framebuffer.h',
    'inc/kms++/extframebuffer.h',
    'inc/kms++/pipeline.h',
    'inc/kms++/drmpropobject.h',
    'inc/kms++/mode_cvt.h',
    'inc/kms++/blob.h',
    'inc/kms++/dumbframebuffer.h',
]

public_headers_omap = [
    'inc/kms++/omap/omapframebuffer.h',
    'inc/kms++/omap/omapkms++.h',
    'inc/kms++/omap/omapcard.h',
]

private_includes = include_directories('src', 'inc')
public_includes = include_directories('inc')

libdrm_dep = dependency('libdrm')

if libdrmomap_dep.found()
    libkmsxx_sources += files([
        'src/omap/omapcard.cpp',
        'src/omap/omapframebuffer.cpp',
    ])
    omapdrm_enabled = true
else
    omapdrm_enabled = false
endif

libkmsxx_deps = [ libdrm_dep, libfmt_dep, libdrmomap_dep ]

libkmsxx = library('kms++',
                   libkmsxx_sources,
                   install : true,
                   include_directories : private_includes,
                   dependencies : libkmsxx_deps,
                   version : meson.project_version())


libkmsxx_dep = declare_dependency(include_directories : public_includes,
                                  link_with : libkmsxx)

install_headers(public_headers, subdir : 'kms++')

if libdrmomap_dep.found()
    install_headers(public_headers_omap, subdir : 'kms++/omap')
endif

pkg = import('pkgconfig')
pkg.generate(libkmsxx)
