#ifndef TESTHWENCODER_H
#define TESTHWENCODER_H

#include <QObject>
#include <QtTest/QtTest>

class TestHWEncoder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testConstructor();
    void testIsReadyBeforeInit();
    void testInitializeSoftware();
    void testInitializeAuto();
    void testGetExtraDataBeforeInit();
    void testGetExtraDataAfterInit();
    void testGetStatsBeforeInit();
    void testGetStatsAfterInit();
    void testSetBitrate();
    void testEncodeFrameBeforeInit();
    void testGetEncodedPacketBeforeInit();
    void testEncodeFrameAfterInit();
    void testGetEncoderName();
    void testGetEncodedPacketAfterEncode();
    void testPacketRateSignal();
};

#endif // TESTHWENCODER_H
