file(GLOB SRCS "*.cpp" "*.h")
add_library(kmstest ${SRCS})

target_link_libraries(kmstest kms++)
target_include_directories(kmstest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
