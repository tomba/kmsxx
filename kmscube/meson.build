if not get_option('kmscube')
   kmscube_enabled = false
   subdir_done()
endif

if not get_option('libutils')
    error('"kmscube" option requires "libutils" option enabled')
endif

kmscube_enabled = true

kmscube_sources = files([
    'cube.cpp',
    'cube-egl.cpp',
    'cube-gbm.cpp',
    'cube-gles2.cpp',
    'cube-null.cpp',
    'cube-wl.cpp',
    'cube-x11.cpp',
    'esTransform.cpp',
])

kmscube_deps = [
    libdrm_dep, libkmsxx_dep, libkmsxxutil_dep, libfmt_dep,
    dependency('x11'),
    dependency('xcb'),
    dependency('x11-xcb'),
    dependency('gbm'),
    dependency('wayland-client'),
    dependency('wayland-egl'),
    dependency('glesv2'),
    dependency('egl'),
]

executable('kmscube', kmscube_sources, dependencies : kmscube_deps)
