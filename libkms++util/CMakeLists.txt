file(GLOB SRCS "*.cpp" "*.h")
add_library(kms++util ${SRCS})

target_link_libraries(kms++util kms++)
target_include_directories(kms++util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
