#ifndef TESTHWDECODER_H
#define TESTHWDECODER_H

#include <QObject>
#include <QtTest/QtTest>

class TestHWDecoder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testConstructor();
    void testIsReadyBeforeInit();
    void testSetExtradataEmpty();
    void testSetExtradataWithSpsPps();
    void testInitializeSoftwareFallback();
    void testInitializeAuto();
    void testGetStatsBeforeInit();
    void testDecodePacketBeforeInit();
    void testSetExtradataBeforeInit();
    void testDecodePacketAfterInit();
    void testGetDecodedFrameWithoutData();
    void testGetStatsAfterInit();
    void testFullEncodeDecodeCycle();
    void testInitializeWithExtradata();
    void testDecodePacketQueueDrop();
    void testFullEncodeDecodeCycleWithStats();
};

#endif // TESTHWDECODER_H
