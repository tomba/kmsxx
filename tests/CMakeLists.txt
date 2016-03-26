include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

add_executable (db db.cpp)
target_link_libraries(db kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (testpat testpat.cpp)
target_link_libraries(testpat kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (kmsview kmsview.cpp)
target_link_libraries(kmsview kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (kmsprint kmsprint.cpp)
target_link_libraries(kmsprint kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (fbtestpat fbtestpat.cpp)
target_link_libraries(fbtestpat kmstest)

add_executable (kmscapture kmscapture.cpp)
target_link_libraries(kmscapture kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (producer producer.cpp prodcons.cpp prodcons.h)
target_link_libraries(producer kms++ kmstest ${LIBDRM_LIBRARIES})

add_executable (consumer consumer.cpp prodcons.cpp prodcons.h)
target_link_libraries(consumer kms++ kmstest ${LIBDRM_LIBRARIES})
