python_mod = import('python')
py3 = python_mod.find_installation('python3', required : get_option('pykms'))

if py3.found() == false
    pybindings_enabled = false
    subdir_done()
endif

py3_dep = py3.dependency(required : get_option('pykms'))

if py3_dep.found() == false
    pybindings_enabled = false
    subdir_done()
endif

nanobind_dep = dependency('nanobind', fallback : ['nanobind', 'nanobind_dep'],
                          required : get_option('pykms'))

if nanobind_dep.found() == false
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
    nanobind_dep,
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

# Copy Python package files to the build dir so that we can run without installing
configure_file(input: '__init__.py', output: '__init__.py', copy: true,
               install : true, install_dir : destdir)
configure_file(input: '__init__.pyi', output: '__init__.pyi', copy: true,
               install : true, install_dir : destdir)

nanobind_stubgen = nanobind_dep.get_variable('stubgen', default_value : '')
if nanobind_stubgen == ''
    stubgen_cmd = [ py3, '-m', 'nanobind.stubgen' ]
else
    stubgen_cmd = [ py3, nanobind_stubgen ]
endif

custom_target('pykms-stubs',
              output : [ 'pykms.pyi', 'py.typed' ],
              depends : [ pykms ],
              command : stubgen_cmd + [
                  '-m', 'pykms.pykms',
                  '-o', '@OUTPUT0@',
                  '-M', '@OUTPUT1@',
                  '-i', meson.project_build_root() / 'py',
                  '-q',
              ],
              build_by_default : true,
              install : true,
              install_dir : destdir)
