set(TARGET_NAME ${PROJECT_NAME})

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
endif()

find_package(QT NAMES Qt6 CONFIG REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS Core Gui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

if(QMVIDEO_BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
    add_library(${TARGET_NAME} SHARED)
else()
    add_library(${TARGET_NAME} STATIC)
    target_compile_definitions(${TARGET_NAME} PUBLIC QMVIDEO_BUILD_STATIC)
endif()

target_sources(${TARGET_NAME} PRIVATE qmvideodecoder.h qmvideodecoder.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
target_include_directories(${TARGET_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

if(QMVIDEO_BUILD_YUVVIEW)
    find_package(QT NAMES Qt6 CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
    find_package(Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
    target_sources(${TARGET_NAME} PRIVATE qmyuvview.h qmyuvview.cpp)
    target_link_libraries(${TARGET_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGLWidgets Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

set(FFMPEG_LIBS ffmpeg::avformat ffmpeg::swscale)
target_link_libraries(${TARGET_NAME} PRIVATE ${FFMPEG_LIBS})
target_compile_definitions(${TARGET_NAME} PRIVATE "QMVIDEO_COMPILE_LIB")

if(COMMAND ParseDependTargets)
    set(ffmpeg_targets "")
    ParseDependTargets(ffmpeg_targets TARGETS ${FFMPEG_LIBS})

    foreach(_dll ${ffmpeg_targets})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endforeach()
endif()