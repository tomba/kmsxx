include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

add_executable (testpat testpat.cpp)
target_link_libraries(testpat kms++ kms++util ${LIBDRM_LIBRARIES})

add_executable (kmsview kmsview.cpp)
target_link_libraries(kmsview kms++ kms++util ${LIBDRM_LIBRARIES})

add_executable (kmsprint kmsprint.cpp)
target_link_libraries(kmsprint kms++ kms++util ${LIBDRM_LIBRARIES})

add_executable (fbtestpat fbtestpat.cpp)
target_link_libraries(fbtestpat kms++util)

add_executable (kmscapture kmscapture.cpp)
target_link_libraries(kmscapture kms++ kms++util ${LIBDRM_LIBRARIES})

add_executable (kmsblank kmsblank.cpp)
target_link_libraries(kmsblank kms++ kms++util ${LIBDRM_LIBRARIES})

add_executable (wbcap wbcap.cpp)
target_link_libraries(wbcap kms++ kms++util ${LIBDRM_LIBRARIES})
