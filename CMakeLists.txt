cmake_minimum_required(VERSION 3.29)

project ("qonconv")
add_executable (qonconv "qonconv.cpp" "qon.h" "qoi2.h" "qoi.h")
target_include_directories(qonconv PRIVATE libpng)
target_link_libraries(qonconv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libpng/libpng16.lib" "${CMAKE_CURRENT_SOURCE_DIR}/libpng/zlib.lib")
set_property(TARGET qonconv PROPERTY CXX_STANDARD 20)

project ("qoi2conv")
add_executable (qoi2conv "qoi2conv.cpp" "qon.h" "qoi2.h" "qoi.h")
target_include_directories(qoi2conv PRIVATE libpng)
target_link_libraries(qoi2conv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libpng/libpng16.lib" "${CMAKE_CURRENT_SOURCE_DIR}/libpng/zlib.lib")
set_property(TARGET qoi2conv PROPERTY CXX_STANDARD 20)
