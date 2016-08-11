include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

file(GLOB SRCS "src/*.cpp" "src/*.h")
file(GLOB PUB_HDRS "inc/kms++/*.h")
add_library(kms++ ${SRCS} ${PUB_HDRS})

target_include_directories(kms++ PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(kms++ ${LIBDRM_LIBRARIES})

set_target_properties(kms++ PROPERTIES
    PUBLIC_HEADER "${PUB_HDRS}")

install(TARGETS kms++
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/kms++)
