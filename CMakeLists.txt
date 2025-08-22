cmake_minimum_required(VERSION 3.23.0 FATAL_ERROR)

set(TARGET_NAME qmvideoplayer)

project(${TARGET_NAME} LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)

find_package(QT NAMES Qt6 CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

add_executable(${TARGET_NAME} main.cpp
    qmyuvview.h qmyuvview.cpp
    qmvideodecoder.h qmvideodecoder.cpp
)
target_link_libraries(${TARGET_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGLWidgets Qt${QT_VERSION_MAJOR}::OpenGL)
add_subdirectory(thirdparty)

set(FFMPEG_LIBS ffmpeg::avformat ffmpeg::swscale)
target_link_libraries(${TARGET_NAME} PRIVATE ${FFMPEG_LIBS})

set(ffmpeg_targets "")
ParseDependTargets(ffmpeg_targets TARGETS ${FFMPEG_LIBS})

foreach(_dll ${ffmpeg_targets})
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach()
