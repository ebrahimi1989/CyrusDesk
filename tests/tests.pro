QT += testlib core gui widgets network

CONFIG += c++17
CONFIG += console
CONFIG -= app_bundle

TARGET = cyrusdesk-tests

INCLUDEPATH += ../common

# Force moc on source headers with Q_OBJECT
QMAKE_MOC += ../client/hwdecoder.h ../client/remoteclient.h ../client/remotescreenlabel.h ../server/hwencoder.h

SOURCES += \
    testmain.cpp \
    testprotocol.cpp \
    testlatencymonitor.cpp \
    testhwdecoder.cpp \
    testhwencoder.cpp \
    testimageprovider.cpp \
    testremoteclient.cpp \
    testscreenlabel.cpp \
    ../client/hwdecoder.cpp \
    ../client/remoteclient.cpp \
    ../client/remotescreenlabel.cpp \
    ../server/hwencoder.cpp

HEADERS += \
    testprotocol.h \
    testlatencymonitor.h \
    testhwdecoder.h \
    testhwencoder.h \
    testimageprovider.h \
    testremoteclient.h \
    testscreenlabel.h \
    ../client/hwdecoder.h \
    ../client/remoteclient.h \
    ../client/remotescreenlabel.h \
    ../server/hwencoder.h

unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += libavcodec libavutil libswscale
    LIBS += -lpthread
}
