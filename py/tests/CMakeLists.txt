file(GLOB PY_SRCS "*.py")
add_custom_target(pyextras SOURCES ${PY_SRCS})

add_test(NAME pytest COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/functest.py")
set_property(TEST pytest PROPERTY
        ENVIRONMENT "PYTHONPATH=." "LD_LIBRARY_PATH=."
)
