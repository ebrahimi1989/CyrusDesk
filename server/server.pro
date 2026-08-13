QT += core widgets network gui

CONFIG += c++17

# Enable all optimizations
QMAKE_CXXFLAGS += -O3 -march=native -mtune=native
QMAKE_CXXFLAGS += -ffast-math -funroll-loops
QMAKE_CXXFLAGS += -flto # Link-time optimization
QMAKE_LFLAGS += -flto

TARGET = cyrusdesk-server
TEMPLATE = app

SOURCES += \
    main.cpp \
    remoteserver.cpp \
    hwencoder.cpp

HEADERS += \
    remoteserver.h \
    hwencoder.h \
    ../common/protocol.h

INCLUDEPATH += ../common

# FFmpeg libraries (only what is actually used)
CONFIG += link_pkgconfig
PKGCONFIG += libavcodec libavutil libswscale

unix:!macx {
    LIBS += -lX11 -lXtst -lpthread

    # Add SIMD support
    QMAKE_CXXFLAGS += -mavx2 -msse4.2
}

win32 {
    LIBS += -luser32

    # Add SIMD support for Windows
    QMAKE_CXXFLAGS += /arch:AVX2

    # FFmpeg for Windows (adjust paths as needed)
    INCLUDEPATH += C:/ffmpeg/include
    LIBS += -LC:/ffmpeg/lib -lavcodec -lavformat -lavutil -lswscale -lavfilter
}