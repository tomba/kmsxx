py3_dep = dependency('python3', required : get_option('pyv4l2'))

if py3_dep.found() == false
    subdir_done()
endif

pybind11_dep = dependency('pybind11', fallback : ['pybind11', 'pybind11_dep'],
                          required : get_option('pyv4l2'))

if pybind11_dep.found() == false
    subdir_done()
endif

pyv4l2_sources = files([
    'pyv4l2.cpp',
])

pyv4l2_deps = [
    libv4l2xx_dep,
    py3_dep,
    pybind11_dep,
]

pyv4l2_args = [ '-fvisibility=hidden' ]

destdir = get_option('libdir') / 'python' + py3_dep.version() / 'site-packages/pyv4l2'

pyv4l2 = shared_module('pyv4l2',
                      pyv4l2_sources,
                      install : true,
                      install_dir : destdir,
                      name_prefix : '',
                      dependencies : pyv4l2_deps,
                      cpp_args : pyv4l2_args)

# Copy __init__.py to build dir so that we can run without installing
configure_file(input: '__init__.py', output: '__init__.py', copy: true,
               install : true, install_dir : destdir)
