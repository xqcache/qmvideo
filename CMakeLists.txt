cmake_minimum_required(VERSION 3.23.0 FATAL_ERROR)

set(TARGET_NAME qmvideoplayer)

project(${TARGET_NAME} LANGUAGES CXX)

find_package(QT NAMES Qt6 CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

add_executable(${TARGET_NAME} main.cpp
    qmvideoplayer.h qmvideoplayer.cpp
)
target_link_libraries(${TARGET_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGLWidgets Qt${QT_VERSION_MAJOR}::OpenGL)