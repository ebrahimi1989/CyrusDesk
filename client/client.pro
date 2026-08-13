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
    # FFmpeg for Windows (adjust paths as needed)
    INCLUDEPATH += C:/ffmpeg/include
    LIBS += -LC:/ffmpeg/lib -lavcodec -lavutil -lswscale
}