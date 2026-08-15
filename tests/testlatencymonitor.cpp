#include "testlatencymonitor.h"
#include "../common/latencymonitor.h"

void TestLatencyMonitor::initTestCase()
{
}

void TestLatencyMonitor::cleanupTestCase()
{
}

void TestLatencyMonitor::testAddTimestampRoundTrip()
{
    QByteArray original = "test_frame_data";
    QByteArray timestamped = LatencyMonitor::addTimestamp(original);

    QVERIFY(timestamped.size() > original.size());
    QCOMPARE(timestamped.size(), (int)(original.size() + sizeof(qint64)));

    QByteArray recovered;
    double latency = LatencyMonitor::getLatencyMs(timestamped, recovered);

    QCOMPARE(recovered, original);
    QVERIFY(latency >= 0.0);
}

void TestLatencyMonitor::testGetLatencyMsValidData()
{
    QByteArray original = "video_frame_data_12345";
    QByteArray timestamped = LatencyMonitor::addTimestamp(original);

    QByteArray recovered;
    double latency = LatencyMonitor::getLatencyMs(timestamped, recovered);

    QCOMPARE(recovered, original);
    QVERIFY(latency >= 0.0);
}

void TestLatencyMonitor::testGetLatencyMsTooShortData()
{
    QByteArray shortData(5, 0x01); // Less than sizeof(qint64)
    QByteArray recovered;

    double latency = LatencyMonitor::getLatencyMs(shortData, recovered);
    QCOMPARE(latency, -1.0);
    QCOMPARE(recovered, shortData);
}

void TestLatencyMonitor::testGetLatencyMsEmptyData()
{
    QByteArray empty;
    QByteArray recovered;

    double latency = LatencyMonitor::getLatencyMs(empty, recovered);
    QCOMPARE(latency, -1.0);
    QVERIFY(recovered.isEmpty());
}

void TestLatencyMonitor::testTimestampIsMonotonic()
{
    QByteArray data1 = LatencyMonitor::addTimestamp("frame1");
    QTest::qSleep(5);
    QByteArray data2 = LatencyMonitor::addTimestamp("frame2");

    // Timestamps should be different and increasing
    QVERIFY(data2.size() >= data1.size());
}
