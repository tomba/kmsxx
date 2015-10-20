include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

add_executable (db db.cpp)
target_link_libraries(db kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (testpat testpat.cpp)
target_link_libraries(testpat kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (kmsview kmsview.cpp)
target_link_libraries(kmsview kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (optiontester optiontester.cpp)
