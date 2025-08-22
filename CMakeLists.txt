cmake_minimum_required(VERSION 3.23.0 FATAL_ERROR)

project(qmvideo LANGUAGES CXX)

option(QMVIDEO_BUILD_TESTS "Build test tools" OFF)
option(QMVIDEO_BUILD_YUVVIEW "Build viewer" OFF)
option(QMVIDEO_BUILD_SHARED_LIBS "Build shared library" OFF)
option(QMVIDEO_FFMPEG_EXTERNAL "Use external ffmpeg library" OFF)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
endif()

add_subdirectory(thirdparty)
add_subdirectory(source)

if(QMVIDEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()