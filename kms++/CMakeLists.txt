include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

include_directories(${LIBDRM_OMAP_INCLUDE_DIRS})
link_directories(${LIBDRM_OMAP_LIBRARY_DIRS})

file(GLOB SRCS "src/*.cpp" "src/*.h")
file(GLOB PUB_HDRS "inc/kms++/*.h")

if(LIBDRM_OMAP_FOUND)
    file(GLOB OMAP_SRCS "src/omap/*.cpp" "src/omap/*.h")
    file(GLOB OMAP_PUB_HDRS "inc/kms++/omap/*.h")

    set(SRCS ${SRCS} ${OMAP_SRCS})
    set(PUB_HDRS ${PUB_HDRS} ${OMAP_PUB_HDRS})
endif()

add_library(kms++ ${SRCS} ${PUB_HDRS})

target_include_directories(kms++ PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(kms++ ${LIBDRM_LIBRARIES} ${LIBDRM_OMAP_LIBRARIES})

set_target_properties(kms++ PROPERTIES
    PUBLIC_HEADER "${PUB_HDRS}")

install(TARGETS kms++
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/kms++)
