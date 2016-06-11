file(GLOB SRCS "src/*.cpp" "src/*.h" "inc/kms++util/*.h")
add_library(kms++util ${SRCS})

target_include_directories(kms++util PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(kms++util kms++ pthread)
