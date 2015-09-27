include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

add_executable (db db.cpp)

target_link_libraries(db kms++ ${LIBDRM_LIBRARIES})
