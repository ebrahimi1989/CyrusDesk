QT += core gui widgets network

CONFIG += c++17

TARGET = cyrusdesk-client
TEMPLATE = app

SOURCES += \
    main.cpp \
    remoteclient.cpp \
    hwdecoder.cpp \
    remotescreenlabel.cpp

HEADERS += \
    remoteclient.h \
    hwdecoder.h \
    remotescreenlabel.h \
    ../common/protocol.h

INCLUDEPATH += ../common

# FFmpeg libraries (only what is actually used)
CONFIG += link_pkgconfig
PKGCONFIG += libavcodec libavutil libswscale

unix:!macx {
    LIBS += -lpthread
}

win32 {
    # FFmpeg for Windows - override the path with: qmake FFMPEG_DIR=C:/path/to/ffmpeg
    isEmpty(FFMPEG_DIR):FFMPEG_DIR = C:/ffmpeg
    INCLUDEPATH += $$FFMPEG_DIR/include
    LIBS += -L$$FFMPEG_DIR/lib -L$$FFMPEG_DIR/lib/x64 -lavcodec -lavutil -lswscale
}