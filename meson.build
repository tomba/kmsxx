project('kms++', 'cpp',
    default_options : [
        'cpp_std=c++17',
    ],
)

cpp = meson.get_compiler('cpp')
cpp_arguments = []

if (cpp.get_id() == 'gcc' and
    host_machine.cpu_family() == 'arm' and
    cpp.has_argument('-Wno-psabi'))
    cpp_arguments += [
        '-Wno-psabi',
    ]
endif

if cpp.has_argument('-Wno-c99-designator')
    cpp_arguments += [
        '-Wno-c99-designator',
    ]
endif

add_project_arguments(cpp_arguments, language : 'cpp')

link_arguments = []

if get_option('static-libc')
    link_arguments += ['-static-libgcc', '-static-libstdc++']
endif

add_global_link_arguments(link_arguments, language : 'cpp')

libfmt_includes = include_directories('ext/fmt/include')
libfmt_dep = declare_dependency(include_directories : libfmt_includes,
                                compile_args : '-DFMT_HEADER_ONLY')

pybind11_includes = include_directories('ext/pybind11/include')
pybind11_dep = declare_dependency(include_directories : pybind11_includes)

libdrmomap_dep = dependency('libdrm_omap', required : get_option('omap'))

subdir('kms++')

if get_option('utils')
    subdir('kms++util')
    subdir('utils')
endif

subdir('py')

if get_option('kmscube')
    subdir('kmscube')
endif
