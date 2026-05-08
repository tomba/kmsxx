add_languages('c', native : false)

test('pixpat-smoke-cpp',
     executable('test_pixpat_cpp',
                'test_pixpat.cpp',
                dependencies : libpixpat_dep))

test('pixpat-smoke-c',
     executable('test_pixpat_c',
                'test_pixpat_c.c',
                dependencies : libpixpat_dep))
