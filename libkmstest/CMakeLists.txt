include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

file(GLOB SRCS "*.cpp" "*.h")
add_library(kmstest ${SRCS})

target_link_libraries(kmstest kms++ ${LIBDRM_LIBRARIES})
target_include_directories(kmstest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
