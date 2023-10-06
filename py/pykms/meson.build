
# Python bindings require libutils for now.
if not get_option('libutils')
    pybindings_enabled = false
    subdir_done()
endif

py3_dep = dependency('python3', required : get_option('pykms'))

if py3_dep.found() == false
    pybindings_enabled = false
    subdir_done()
endif

pybind11_dep = dependency('pybind11', fallback : ['pybind11', 'pybind11_dep'],
                          required : get_option('pykms'))

if pybind11_dep.found() == false
    pybindings_enabled = false
    subdir_done()
endif

pybindings_enabled = true

pykms_sources = files([
    'pykmsbase.cpp',
    'pykms.cpp',
])

if get_option('libutils')
    pykms_sources += files([
        'pykmsutil.cpp',
    ])
endif

if libdrmomap_dep.found()
    pykms_sources += files([
        'pykmsomap.cpp',
    ])
endif

pykms_deps = [
    libkmsxx_dep,
    py3_dep,
    pybind11_dep,
]

if get_option('libutils')
    pykms_deps += [ libkmsxxutil_dep ]
endif

pykms_args = [ '-fvisibility=hidden' ]

destdir = get_option('libdir') / 'python' + py3_dep.version() / 'site-packages/pykms'

pykms = shared_module('pykms',
                      pykms_sources,
                      install : true,
                      install_dir : destdir,
                      name_prefix : '',
                      dependencies : pykms_deps,
                      cpp_args : pykms_args)

# Copy __init__.py to build dir so that we can run without installing
configure_file(input: '__init__.py', output: '__init__.py', copy: true,
               install : true, install_dir : destdir)
