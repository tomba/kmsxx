include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

file(GLOB SRCS "*.cpp" "*.h")
add_library(kms++ ${SRCS})

target_include_directories(kms++ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
