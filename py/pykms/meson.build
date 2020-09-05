py3_dep = dependency('python3', required : get_option('pykms'))

if py3_dep.found() == false
    subdir_done()
endif

pykms_sources = files([
    'pykmsbase.cpp',
    'pykms.cpp',
    'pykmsutil.cpp',
    'pyvid.cpp',
])

if libdrmomap_dep.found()
    pykms_sources += files([
        'pykmsomap.cpp',
    ])
endif

pykms_deps = [
    libkmsxx_dep,
    libkmsxxutil_dep,
    py3_dep,
    pybind11_dep,
]

destdir = get_option('libdir') / 'python' + py3_dep.version() / 'site-packages/pykms'

pykms = shared_module('pykms',
                      pykms_sources,
                      install : true,
                      install_dir : destdir,
                      name_prefix : '',
                      dependencies : pykms_deps)

# Copy __init__.py to build dir so that we can run without installing
configure_file(input: '__init__.py', output: '__init__.py', copy: true,
               install : true, install_dir : destdir)
