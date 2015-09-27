include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

add_executable (testpat testpat.cpp)

target_link_libraries(testpat kms++ ${LIBDRM_LIBRARIES})
