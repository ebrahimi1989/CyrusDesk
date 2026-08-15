#ifndef TESTLATENCYMONITOR_H
#define TESTLATENCYMONITOR_H

#include <QObject>
#include <QtTest/QtTest>

class TestLatencyMonitor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testAddTimestampRoundTrip();
    void testGetLatencyMsValidData();
    void testGetLatencyMsTooShortData();
    void testGetLatencyMsEmptyData();
    void testTimestampIsMonotonic();
};

#endif // TESTLATENCYMONITOR_H
