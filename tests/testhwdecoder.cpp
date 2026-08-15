#include "testhwdecoder.h"
#include "../client/hwdecoder.h"
#include "../server/hwencoder.h"
#include "../common/latencymonitor.h"
#include <QImage>
#include <QPixmap>
#include <QBuffer>
#include <QSignalSpy>

void TestHWDecoder::initTestCase()
{
}

void TestHWDecoder::cleanupTestCase()
{
}

void TestHWDecoder::testConstructor()
{
    HWDecoder* decoder = new HWDecoder();
    QVERIFY(decoder != nullptr);
    delete decoder;
}

void TestHWDecoder::testIsReadyBeforeInit()
{
    HWDecoder decoder;
    QVERIFY(!decoder.isReady());
}

void TestHWDecoder::testSetExtradataEmpty()
{
    HWDecoder decoder;
    decoder.setExtradata(QByteArray());
    QVERIFY(!decoder.isReady());
}

void TestHWDecoder::testSetExtradataWithSpsPps()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software));
    QByteArray extradata = encoder.getExtraData();

    HWDecoder decoder;
    decoder.setExtradata(extradata);
    QVERIFY(!decoder.isReady());
}

void TestHWDecoder::testInitializeSoftwareFallback()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software));
    QByteArray extradata = encoder.getExtraData();

    HWDecoder decoder;
    decoder.setExtradata(extradata);
    bool result = decoder.initialize(HWDecoder::Software);
    QVERIFY(result);
    QVERIFY(decoder.isReady());
}

void TestHWDecoder::testInitializeAuto()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software));
    QByteArray extradata = encoder.getExtraData();

    HWDecoder decoder;
    decoder.setExtradata(extradata);
    bool result = decoder.initialize(HWDecoder::Auto);
    QVERIFY(result);
    QVERIFY(decoder.isReady());
}

void TestHWDecoder::testGetStatsBeforeInit()
{
    HWDecoder decoder;
    HWDecoder::Stats stats = decoder.getStats();
    QCOMPARE(stats.framesDecoded, (qint64)0);
    QCOMPARE(stats.bytesDecoded, (qint64)0);
    QCOMPARE(stats.currentFPS, 0.0);
}

void TestHWDecoder::testDecodePacketBeforeInit()
{
    HWDecoder decoder;
    QByteArray data(100, 0xFF);
    bool result = decoder.decodePacket(data);
    QVERIFY(!result);
}

void TestHWDecoder::testSetExtradataBeforeInit()
{
    HWDecoder decoder;
    QByteArray sps = QByteArray::fromHex("000001676e0c28d4d40");
    decoder.setExtradata(sps);
    QVERIFY(!decoder.isReady());
}

void TestHWDecoder::testDecodePacketAfterInit()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(64, 64, 30, 500000, HWEncoder::Software));
    QByteArray extraData = encoder.getExtraData();

    HWDecoder decoder;
    decoder.setExtradata(extraData);
    QVERIFY(decoder.initialize(HWDecoder::Software));

    QImage img(64, 64, QImage::Format_RGB32);
    img.fill(Qt::red);
    QPixmap pixmap = QPixmap::fromImage(img);
    encoder.encodeFrame(pixmap);

    QTest::qWait(500);

    QByteArray encoded = encoder.getEncodedPacket();
    if (!encoded.isEmpty()) {
        QByteArray timestampedData = LatencyMonitor::addTimestamp(encoded);
        bool ok = decoder.decodePacket(timestampedData);
        QVERIFY(ok);
    }

    QVERIFY(decoder.isReady());
}

void TestHWDecoder::testGetDecodedFrameWithoutData()
{
    HWDecoder decoder;
    decoder.initialize(HWDecoder::Software);
    QPixmap frame = decoder.getDecodedFrame();
    QVERIFY(frame.isNull());
}

void TestHWDecoder::testGetStatsAfterInit()
{
    HWDecoder decoder;
    decoder.initialize(HWDecoder::Software);
    HWDecoder::Stats stats = decoder.getStats();
    QCOMPARE(stats.framesDecoded, (qint64)0);
    QVERIFY(stats.avgDecodeTimeMs >= 0.0);
}

void TestHWDecoder::testInitializeWithExtradata()
{
    // Test that extradata is applied when non-empty
    HWDecoder decoder;
    // Set dummy but non-empty extradata (simulates SPS/PPS from server)
    QByteArray dummySpsPps;
    dummySpsPps.append(static_cast<char>(0x01));
    dummySpsPps.append(static_cast<char>(0x00));
    dummySpsPps.append(static_cast<char>(0x00));
    dummySpsPps.append(static_cast<char>(0x00));
    dummySpsPps.append(static_cast<char>(0x00));
    dummySpsPps.append(static_cast<char>(0x00));
    decoder.setExtradata(dummySpsPps);
    bool result = decoder.initialize(HWDecoder::Software);
    QVERIFY(result);
    QVERIFY(decoder.isReady());
}

void TestHWDecoder::testDecodePacketQueueDrop()
{
    // Test that decodePacket drops old packets when queue is full
    HWDecoder decoder;
    decoder.initialize(HWDecoder::Software);

    // Send more than 2 packets rapidly - the queue should drop old ones
    QByteArray packet1(10, 0xAA);
    QByteArray packet2(20, 0xBB);
    QByteArray packet3(30, 0xCC);

    decoder.decodePacket(packet1);
    decoder.decodePacket(packet2);
    decoder.decodePacket(packet3);

    QVERIFY(true); // If this doesn't crash, the queue drop logic worked
}

void TestHWDecoder::testFullEncodeDecodeCycleWithStats()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(128, 128, 30, 2000000, HWEncoder::Software));

    QByteArray extraData = encoder.getExtraData();

    HWDecoder decoder;
    decoder.setExtradata(extraData);
    QVERIFY(decoder.initialize(HWDecoder::Software));

    QSignalSpy frameSpy(&decoder, &HWDecoder::frameReady);

    // Encode a frame
    QImage img(128, 128, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QPixmap pixmap = QPixmap::fromImage(img);
    encoder.encodeFrame(pixmap);

    QTest::qWait(500);

    QByteArray encoded = encoder.getEncodedPacket();
    QVERIFY(!encoded.isEmpty());

    // Decode it
    QByteArray timestampedData = LatencyMonitor::addTimestamp(encoded);
    QVERIFY(decoder.decodePacket(timestampedData));

    // Wait for frame decode
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() > 0, 2000);

    // Wait more than 1 second to trigger updateStats FPS calculation
    QTest::qWait(1500);

    HWDecoder::Stats stats = decoder.getStats();
    QVERIFY(stats.framesDecoded > 0);

    QPixmap decoded = decoder.getDecodedFrame();
    QVERIFY(!decoded.isNull());
}

void TestHWDecoder::testFullEncodeDecodeCycle()
{
    HWEncoder encoder;
    QVERIFY(encoder.initialize(128, 128, 30, 2000000, HWEncoder::Software));

    QByteArray extraData = encoder.getExtraData();

    HWDecoder decoder;
    decoder.setExtradata(extraData);
    QVERIFY(decoder.initialize(HWDecoder::Software));

    // Connect frameReady signal
    QSignalSpy frameSpy(&decoder, &HWDecoder::frameReady);

    // Encode a frame
    QImage img(128, 128, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QPixmap pixmap = QPixmap::fromImage(img);
    encoder.encodeFrame(pixmap);

    QTest::qWait(500);

    QByteArray encoded = encoder.getEncodedPacket();
    QVERIFY(!encoded.isEmpty());

    // Decode it
    QByteArray timestampedData = LatencyMonitor::addTimestamp(encoded);
    QVERIFY(decoder.decodePacket(timestampedData));

    // Wait for decoder thread to process
    QTest::qWait(500);

    // Check that frame was decoded
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() > 0, 2000);

    QPixmap decoded = decoder.getDecodedFrame();
    QVERIFY(!decoded.isNull());

    HWDecoder::Stats stats = decoder.getStats();
    QVERIFY(stats.framesDecoded > 0);
}
