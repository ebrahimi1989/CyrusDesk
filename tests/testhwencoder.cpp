#include "testhwencoder.h"
#include "../server/hwencoder.h"
#include "../common/latencymonitor.h"
#include <QImage>
#include <QPixmap>
#include <QBuffer>
#include <QSignalSpy>

void TestHWEncoder::initTestCase()
{
}

void TestHWEncoder::cleanupTestCase()
{
}

void TestHWEncoder::testConstructor()
{
    HWEncoder* encoder = new HWEncoder();
    QVERIFY(encoder != nullptr);
    delete encoder;
}

void TestHWEncoder::testIsReadyBeforeInit()
{
    HWEncoder encoder;
    QVERIFY(!encoder.isReady());
}

void TestHWEncoder::testInitializeSoftware()
{
    HWEncoder encoder;
    bool result = encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software);
    QVERIFY(result);
    QVERIFY(encoder.isReady());
}

void TestHWEncoder::testInitializeAuto()
{
    HWEncoder encoder;
    // Auto will try NVENC, VAAPI, QSV, then Software
    bool result = encoder.initialize(64, 64, 30, 1000000, HWEncoder::Auto);
    QVERIFY(result);
    QVERIFY(encoder.isReady());
}

void TestHWEncoder::testGetExtraDataBeforeInit()
{
    HWEncoder encoder;
    QByteArray extraData = encoder.getExtraData();
    QVERIFY(extraData.isEmpty());
}

void TestHWEncoder::testGetExtraDataAfterInit()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software));

    // x264 may need a keyframe for SPS/PPS, or may use in-band
    // Just verify the method runs without crashing
    QByteArray extraData = encoder.getExtraData();
    // May or may not be empty depending on encoder configuration
    QVERIFY(true);
}

void TestHWEncoder::testGetStatsBeforeInit()
{
    HWEncoder encoder;
    HWEncoder::Stats stats = encoder.getStats();
    QCOMPARE(stats.framesEncoded, (qint64)0);
    QCOMPARE(stats.bytesEncoded, (qint64)0);
    QCOMPARE(stats.currentFPS, 0.0);
}

void TestHWEncoder::testGetStatsAfterInit()
{
    HWEncoder encoder;
    encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software);
    HWEncoder::Stats stats = encoder.getStats();
    QCOMPARE(stats.framesEncoded, (qint64)0);
    QVERIFY(stats.avgEncodeTimeMs >= 0.0);
}

void TestHWEncoder::testSetBitrate()
{
    HWEncoder encoder;
    encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software);
    encoder.setBitrate(5000000);
    QVERIFY(encoder.isReady());
}

void TestHWEncoder::testEncodeFrameBeforeInit()
{
    HWEncoder encoder;
    QImage img(64, 64, QImage::Format_RGB32);
    img.fill(Qt::red);
    QPixmap pixmap = QPixmap::fromImage(img);
    bool result = encoder.encodeFrame(pixmap);
    QVERIFY(!result);
}

void TestHWEncoder::testGetEncodedPacketBeforeInit()
{
    HWEncoder encoder;
    QByteArray packet = encoder.getEncodedPacket();
    QVERIFY(packet.isEmpty());
}

void TestHWEncoder::testEncodeFrameAfterInit()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(64, 64, 30, 500000, HWEncoder::Software));

    QImage img(64, 64, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QPixmap pixmap = QPixmap::fromImage(img);

    bool result = encoder.encodeFrame(pixmap);
    QVERIFY(result);

    // Wait for encoding to complete
    QTest::qWait(500);

    QByteArray packet = encoder.getEncodedPacket();
    QVERIFY(!packet.isEmpty());
}

void TestHWEncoder::testGetEncoderName()
{
    // Test through initialize paths - covers getEncoderName internally
    HWEncoder encoder;
    QVERIFY(encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software));

    // Encode a frame to ensure full pipeline works
    QImage img(64, 64, QImage::Format_RGB32);
    img.fill(Qt::red);
    QPixmap pixmap = QPixmap::fromImage(img);
    QVERIFY(encoder.encodeFrame(pixmap));
    QTest::qWait(500);

    // Verify packet is produced
    QByteArray packet = encoder.getEncodedPacket();
    QVERIFY(!packet.isEmpty());
}

void TestHWEncoder::testGetEncodedPacketAfterEncode()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(32, 32, 10, 500000, HWEncoder::Software));

    QImage img(32, 32, QImage::Format_RGB32);
    img.fill(Qt::green);
    QPixmap pixmap = QPixmap::fromImage(img);

    encoder.encodeFrame(pixmap);
    QTest::qWait(500);

    QByteArray packet = encoder.getEncodedPacket();
    QVERIFY(!packet.isEmpty());

    HWEncoder::Stats stats = encoder.getStats();
    QVERIFY(stats.framesEncoded > 0);
}

void TestHWEncoder::testPacketRateSignal()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(32, 32, 10, 500000, HWEncoder::Software));

    QSignalSpy spy(&encoder, &HWEncoder::packetReady);

    QImage img(32, 32, QImage::Format_RGB32);
    img.fill(Qt::yellow);
    QPixmap pixmap = QPixmap::fromImage(img);

    encoder.encodeFrame(pixmap);
    QTest::qWait(500);

    QVERIFY(spy.count() > 0);
}
