#ifndef TESTSCREENLABEL_H
#define TESTSCREENLABEL_H

#include <QObject>
#include <QtTest/QtTest>

class TestScreenLabel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testConstructor();
    void testSetRemoteClient();
    void testUpdateRemoteCursor();
    void testGetScaledPositionWithPixmap();
    void testGetScaledPositionWithoutPixmap();
    void testGetDisplayPositionWithPixmap();
    void testGetDisplayPositionWithoutPixmap();
    void testMousePressEventConnected();
    void testMousePressEventDisconnected();
    void testMouseReleaseEventConnected();
    void testMouseReleaseEventDisconnected();
    void testMouseMoveEventConnected();
    void testMouseMoveEventDisconnected();
    void testKeyPressEvent();
    void testKeyPressEventAutoRepeat();
    void testKeyReleaseEvent();
    void testKeyReleaseEventAutoRepeat();
    void testKeyPressEventDisconnected();
    void testKeyReleaseEventDisconnected();
    void testPaintEventWithCursor();
    void testPaintEventWithoutCursor();
    void testMousePressConnectedClient();
    void testMouseReleaseConnectedClient();
    void testMouseMoveConnectedClient();
    void testKeyPressConnectedClient();
    void testKeyReleaseConnectedClient();
    void testUpdateRemoteCursorNoChange();
    void testGetScaledPositionSmallPixmap();
    void testGetDisplayPositionSmallPixmap();
    void testPaintEventWithMultipleCursors();
};

#endif // TESTSCREENLABEL_H
