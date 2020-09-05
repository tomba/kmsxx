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
