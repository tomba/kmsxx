if not get_option('libutils')
    libutils_enabled = false
    subdir_done()
endif

libutils_enabled = true

libkmsxxutil_sources = files([
    'src/colorbar.cpp',
    'src/color.cpp',
    'src/cpuframebuffer.cpp',
    'src/drawing.cpp',
    'src/extcpuframebuffer.cpp',
    'src/opts.cpp',
    'src/resourcemanager.cpp',
    'src/strhelpers.cpp',
    'src/testpat.cpp',
])

public_headers = [
    'inc/kms++util/color.h',
    'inc/kms++util/kms++util.h',
    'inc/kms++util/stopwatch.h',
    'inc/kms++util/cpuframebuffer.h',
    'inc/kms++util/strhelpers.h',
    'inc/kms++util/opts.h',
    'inc/kms++util/extcpuframebuffer.h',
    'inc/kms++util/resourcemanager.h',
]

private_includes = include_directories('src', 'inc')
public_includes = include_directories('inc')

thread_dep = dependency('threads', required : false)

libkmsxxutil_args = [ ]

if thread_dep.found()
    libkmsxxutil_args += [ '-DHAS_PTHREAD' ]
endif

libkmsxxutil_deps = [ libkmsxx_dep, libfmt_dep, thread_dep ]

libkmsxxutil = library('kms++util',
                       libkmsxxutil_sources,
                       install : true,
                       include_directories : private_includes,
                       dependencies : libkmsxxutil_deps,
                       cpp_args : libkmsxxutil_args,
                       version : meson.project_version())

libkmsxxutil_dep = declare_dependency(include_directories : public_includes,
                                      link_with : libkmsxxutil)

install_headers(public_headers, subdir : 'kms++util')

pkg = import('pkgconfig')
pkg.generate(libkmsxxutil)
