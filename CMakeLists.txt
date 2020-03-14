cmake_minimum_required(VERSION 3.0)

project(x11camoverlay)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(X11 REQUIRED)
find_package(Xfixes REQUIRED)

include_directories(
   ${OpenCV_INCLUDE_DIRS}
   ${X11_INCLUDE_DIR}
   ${XFIXES_INCLUDE_DIR}
   ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(x11camoverlay main.cpp)

target_link_libraries(x11camoverlay
    ${X11_LIBRARIES}
    ${OpenCV_LIBS}
    ${XFIXES_LIBRARY}
)

install(TARGETS x11camoverlay RUNTIME DESTINATION bin)
