include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

file(GLOB SRCS "src/*.cpp" "src/*.h" "inc/kms++/*.h")
add_library(kms++ ${SRCS})

target_include_directories(kms++ PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(kms++ ${LIBDRM_LIBRARIES})
