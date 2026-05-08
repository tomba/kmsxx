project('pixpat', 'cpp',
    default_options : [
        'cpp_std=c++20',
        'buildtype=debugoptimized',
        # Strip asserts in release builds; keep them live in
        # debug / debugoptimized (the project default) so deep-level
        # invariant checks remain a debugging aid.
        'b_ndebug=if-release',
    ],
    version : '1.0.0',
)

python3 = find_program('python3')
gen_pixpat = files('pixpat-native/codegen/gen_pixpat.py')
format_catalog_h = files('pixpat-native/src/format_catalog.h')
pattern_catalog_h = files('pixpat-native/src/pattern_catalog.h')
config_toml = files(get_option('config'))

generated = custom_target('pixpat-codegen',
    input  : config_toml,
    output : [
        'pixpat_config.h',
        'pixpat_caps.inc',
    ],
    command : [
        python3, gen_pixpat,
        '--config',          '@INPUT@',
        '--format-catalog',  format_catalog_h,
        '--pattern-catalog', pattern_catalog_h,
        '--out-config-h',    '@OUTPUT0@',
        '--out-caps-inc',    '@OUTPUT1@',
    ],
    depend_files : [gen_pixpat, format_catalog_h, pattern_catalog_h],
)

# Ask the generator which features are enabled. Drives the conditional
# source list (pixpat_pattern.cpp / pixpat_convert.cpp) and gates the
# C/C++ smoke tests, which assume both APIs work.
feature_pattern = run_command(
    python3, gen_pixpat,
    '--config',          config_toml,
    '--format-catalog',  format_catalog_h,
    '--pattern-catalog', pattern_catalog_h,
    '--query',           'feature_pattern',
    check : true,
).stdout().strip() == '1'

feature_convert = run_command(
    python3, gen_pixpat,
    '--config',          config_toml,
    '--format-catalog',  format_catalog_h,
    '--pattern-catalog', pattern_catalog_h,
    '--query',           'feature_convert',
    check : true,
).stdout().strip() == '1'

have_both = feature_pattern and feature_convert

pixpat_sources = files([
    'pixpat-native/src/pixpat.cpp',
])
if feature_pattern
    pixpat_sources += files(['pixpat-native/src/pixpat_pattern.cpp'])
endif
if feature_convert
    pixpat_sources += files(['pixpat-native/src/pixpat_convert.cpp'])
endif

private_includes = include_directories(
    'pixpat-native/src',
    'pixpat-native/inc',
)
# Build-dir include for the generated pixpat_config.h and .inc files.
config_includes = include_directories('.')
public_includes = include_directories('pixpat-native/inc')

pixpat_args = ['-fvisibility-inlines-hidden']

libpixpat = both_libraries('pixpat',
                           pixpat_sources, generated,
                           install : true,
                           include_directories : [private_includes, config_includes],
                           cpp_args : pixpat_args,
                           gnu_symbol_visibility : 'hidden',
                           version : meson.project_version())

libpixpat_dep = declare_dependency(include_directories : public_includes,
                                   link_with : libpixpat.get_shared_lib())

# Internal dep that embeds pixpat into the linking target, so consumers can
# avoid a runtime libpixpat.so dep.
libpixpat_static_dep = declare_dependency(include_directories : public_includes,
                                          link_with : libpixpat.get_static_lib())

install_headers('pixpat-native/inc/pixpat/pixpat.h', subdir : 'pixpat')

pkg = import('pkgconfig')
pkg.generate(libpixpat.get_shared_lib())

if have_both
    subdir('pixpat-native/tests')
endif
