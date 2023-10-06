project('kms++', 'cpp',
    default_options : [
        'cpp_std=c++17',
    ],
    version: '0.0.0',
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

libfmt_dep = dependency('fmt')

libdrmomap_dep = dependency('libdrm_omap', required : get_option('omap'))

if libdrmomap_dep.found()
    add_global_arguments('-DHAS_LIBDRM_OMAP', language : 'cpp')
endif

if get_option('libutils')
	add_global_arguments('-DHAS_KMSXXUTIL', language : 'cpp')
endif

subdir('kms++')
subdir('kms++util')
subdir('utils')
subdir('kmscube')
subdir('py')

summary({
            'omapdrm extensions': omapdrm_enabled,
            'kms++utils library': libutils_enabled,
            'Python bindings': pybindings_enabled,
            'kmscube': kmscube_enabled,
            'Utilities': utils_enabled,
        },
        section : 'Configuration',
        bool_yn : true)
