
pkg_check_modules(GLESv2 glesv2 REQUIRED)
pkg_check_modules(EGL egl REQUIRED)
pkg_check_modules(GBM gbm REQUIRED)
pkg_check_modules(X11 x11 REQUIRED)
pkg_check_modules(XCB xcb REQUIRED)
pkg_check_modules(X11XCB x11-xcb REQUIRED)
pkg_check_modules(WL wayland-client REQUIRED)
pkg_check_modules(WL_EGL wayland-egl REQUIRED)


include_directories(
    ${LIBDRM_INCLUDE_DIRS}
    ${GLESv2_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${XCB_INCLUDE_DIRS}
    ${X11XCB_INCLUDE_DIRS}
)

link_directories(
    ${LIBDRM_LIBRARY_DIRS}
    ${GLESv2_LIBRARY_DIRS}
    ${EGL_LIBRARY_DIRS}
    ${GBM_LIBRARY_DIRS}
    ${X11_LIBRARY_DIRS}
    ${XCB_LIBRARY_DIRS}
    ${X11XCB_LIBRARY_DIRS}
)

add_executable (kmscube cube.cpp cube.h cube-egl.cpp cube-egl.h cube-gles2.cpp cube-gles2.h
    cube-null.cpp cube-gbm.cpp cube-x11.cpp cube-wl.cpp
    esTransform.c esTransform.h)
target_link_libraries(kmscube kms++ kms++util
    ${LIBDRM_LIBRARIES}
    ${GLESv2_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GBM_LIBRARIES}
    ${X11_LIBRARIES}
    ${XCB_LIBRARIES}
    ${X11XCB_LIBRARIES}
    ${WL_LIBRARIES}
    ${WL_EGL_LIBRARIES}
)
