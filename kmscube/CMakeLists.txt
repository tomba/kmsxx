include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

pkg_check_modules(GLESv2 glesv2 REQUIRED)
include_directories(${GLESv2_INCLUDE_DIRS})
link_directories(${GLESv2_LIBRARY_DIRS})

pkg_check_modules(EGL egl REQUIRED)
include_directories(${EGL_INCLUDE_DIRS})
link_directories(${EGL_LIBRARY_DIRS})

pkg_check_modules(GBM gbm REQUIRED)
include_directories(${GBM_INCLUDE_DIRS})
link_directories(${GBM_LIBRARY_DIRS})

add_executable (kmscube kmscube.cpp esTransform.c esUtil.h cube.h)
target_link_libraries(kmscube kms++ kmstest ${LIBDRM_LIBRARIES} ${GLESv2_LIBRARIES} ${EGL_LIBRARIES} ${GBM_LIBRARIES})
