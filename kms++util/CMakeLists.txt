file(GLOB SRCS "src/*.cpp" "src/*.h")
file(GLOB PUB_HDRS "inc/kms++util/*.h")
add_library(kms++util ${SRCS} ${PUB_HDRS})

target_include_directories(kms++util PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(kms++util kms++ pthread)

set_target_properties(kms++util PROPERTIES
    PUBLIC_HEADER "${PUB_HDRS}")

install(TARGETS kms++util
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/kms++util)
