pkg_check_modules(GLESv2 glesv2 REQUIRED)
pkg_check_modules(EGL egl REQUIRED)
pkg_check_modules(GBM gbm REQUIRED)

include_directories(
    ${LIBDRM_INCLUDE_DIRS}
    ${GLESv2_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
)

link_directories(
    ${LIBDRM_LIBRARY_DIRS}
    ${GLESv2_LIBRARY_DIRS}
    ${EGL_LIBRARY_DIRS}
    ${GBM_LIBRARY_DIRS}
)

file(GLOB SRCS "src/*.cpp" "src/*.h")
file(GLOB PUB_HDRS "inc/kms++gl/*.h")
add_library(kms++gl ${SRCS} ${PUB_HDRS})

target_include_directories(kms++gl PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(kms++gl kms++
    ${LIBDRM_LIBRARIES}
    ${GLESv2_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GBM_LIBRARIES}
)

if (KMSXX_ENABLE_THREADING)
    target_link_libraries(kms++gl pthread)
    add_definitions(-DHAS_PTHREAD)
endif()

# Set a dummy SOVERSION just to avoid havig a naked .so file in the filesystem.
# This version number doesn't make any promise about API/ABI stability.
set_target_properties(kms++gl PROPERTIES
    PUBLIC_HEADER "${PUB_HDRS}"
    SOVERSION 0)

install(TARGETS kms++gl
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/kms++gl)
