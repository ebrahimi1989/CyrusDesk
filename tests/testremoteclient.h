#ifndef TESTREMOTECLIENT_H
#define TESTREMOTECLIENT_H

#include <QObject>
#include <QtTest/QtTest>

class TestRemoteClient : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testConstructor();
    void testStatusChangedSignal();
    void testConnectedChangedSignal();
    void testPacketRateProperty();

    void testConnectToFakeServer();
    void testDisconnectCleanly();
    void testDisconnectDuringVideo();

    void testSendMouseClickWhenDisconnected();
    void testSendMouseMoveWhenDisconnected();
    void testSendKeyPressWhenDisconnected();

    void testCodecInfoFlow();
    void testScreenDataFlow();
    void testVideoDataFlow();
    void testCodecInfoTimeout();
    void testCursorUpdate();
    void testPingPong();

    void testMouseClickWhenConnected();
    void testMouseMoveBatched();
    void testFlushPendingMouseEvents();
};

#endif // TESTREMOTECLIENT_H
