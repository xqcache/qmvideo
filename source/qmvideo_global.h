#pragma once

#include <QtCore/qglobal.h>

#ifndef QMVIDEO_BUILD_STATIC
#if defined(QMVIDEO_COMPILE_LIB)
#define QMVIDEO_LIB_EXPORT Q_DECL_EXPORT
#else
#define QMVIDEO_LIB_EXPORT Q_DECL_IMPORT
#endif
#else
#define QMVIDEO_LIB_EXPORT
#endif
