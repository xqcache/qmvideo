set(TARGET_NAME qmvideo_tests)

find_package(QT NAMES Qt6 CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)

if(NOT QMVIDEO_BUILD_YUVVIEW)
    set(QMVIDEO_BUILD_YUVVIEW ON CACHE BOOL "" FORCE)
endif()

add_executable(${TARGET_NAME} main.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGLWidgets Qt${QT_VERSION_MAJOR}::OpenGL)
target_link_libraries(${TARGET_NAME} PRIVATE qmvideo)