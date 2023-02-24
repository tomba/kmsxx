if not get_option('libutils')
    error('"utils" option requires "libutils" option enabled')
endif

common_deps = [ libkmsxx_dep, libkmsxxutil_dep, libfmt_dep ]

libevdev_dep = dependency('libevdev', required : false)

executable('kmstest', 'kmstest.cpp', dependencies : [ common_deps ], install : true)
executable('kmsview', 'kmsview.cpp', dependencies : [ common_deps ], install : false)
executable('kmsprint', 'kmsprint.cpp', dependencies : [ common_deps ], install : true)
executable('fbtest', 'fbtest.cpp', dependencies : [ common_deps ], install : true)
executable('kmscapture', 'kmscapture.cpp', dependencies : [ common_deps ], install : false)
executable('kmsblank', 'kmsblank.cpp', dependencies : [ common_deps ], install : true)

if libevdev_dep.found()
    executable('kmstouch', 'kmstouch.cpp', dependencies : [ common_deps, libevdev_dep ], install : false)
endif
