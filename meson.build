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

libfmt_dep = dependency('fmt')

libdrmomap_dep = dependency('libdrm_omap', required : get_option('omap'))

subdir('kms++')

if get_option('libutils')
    subdir('kms++util')
endif

if get_option('utils')
    subdir('utils')
endif

subdir('py')

if get_option('kmscube')
    subdir('kmscube')
endif
