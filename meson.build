project('kms++', 'cpp',
  default_options : ['cpp_std=c++20'],
  version : '0.0.0',
)

cpp = meson.get_compiler('cpp')

cpp_args = []
cpp_defs = []

host = host_machine

# --- compiler warning flags ---
if cpp.get_id() == 'gcc' and host.cpu_family() == 'arm' and cpp.has_argument('-Wno-psabi')
  cpp_args += ['-Wno-psabi']
endif

if cpp.has_argument('-Wno-c99-designator')
  cpp_args += ['-Wno-c99-designator']
endif

# --- feature detection ---
if cpp.compiles('#include <string>\nconstexpr bool test(const std::string& x){ return x.empty(); }')
  cpp_defs += ['-DHAS_CONSTEXPR_STR']
else
  message('Compiler does not support constexpr strings')
endif

if cpp.compiles('#include <vector>\nconstexpr bool test(const std::vector<int>& x){ return x.empty(); }')
  cpp_defs += ['-DHAS_CONSTEXPR_VEC']
else
  message('Compiler does not support constexpr vectors')
endif

# --- options ---
if get_option('libutils')
  cpp_defs += ['-DHAS_KMSXXUTIL']
endif

# NOTE: omap option handled via dependency(required: false)
omap_opt = get_option('omap')

libfmt_dep = dependency('fmt')

libdrmomap_dep = dependency(
  'libdrm_omap',
  required : omap_opt,
)

if libdrmomap_dep.found()
  cpp_defs += ['-DHAS_LIBDRM_OMAP']
endif

# --- apply BEFORE subdirs (IMPORTANT) ---
add_project_arguments(cpp_args + cpp_defs, language : 'cpp')

# --- subprojects ---
subdir('kms++')
subdir('kms++util')
subdir('utils')
subdir('kmscube')
subdir('py')

# --- summary ---
summary({
  'omapdrm extensions (omap)': omap_opt.enabled(),
  'kms++utils library (libutils)': get_option('libutils'),
}, section : 'Configuration', bool_yn : true)
