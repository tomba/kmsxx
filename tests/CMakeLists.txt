include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

add_executable (db db.cpp)
target_link_libraries(db kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (testpat testpat.cpp)
target_link_libraries(testpat kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (kmsview kmsview.cpp)
target_link_libraries(kmsview kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (kmsprint kmsprint.cpp)
target_link_libraries(kmsprint kms++ ${LIBDRM_LIBRARIES})

add_executable (optiontester optiontester.cpp)

add_executable (wb wb.cpp omapwb.h)
target_link_libraries(wb kms++ kmstest ${LIBDRM_LIBRARIES})

