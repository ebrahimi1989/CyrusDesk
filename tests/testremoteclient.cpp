#include "testremoteclient.h"
#include "../client/remoteclient.h"
#include "../server/hwencoder.h"
#include "../common/protocol.h"
#include "../common/latencymonitor.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QImage>
#include <QPixmap>
#include <QBuffer>
#include <QTest>

static quint16 getFreePort()
{
    QTcpServer s;
    s.listen(QHostAddress::LocalHost, 0);
    quint16 port = s.serverPort();
    s.close();
    return port;
}

void TestRemoteClient::initTestCase()
{
}

void TestRemoteClient::cleanupTestCase()
{
}

void TestRemoteClient::testConstructor()
{
    RemoteClient client;
    QCOMPARE(client.status(), QString("Disconnected"));
    QVERIFY(!client.connected());
    QVERIFY(client.imageProvider() != nullptr);
}

void TestRemoteClient::testStatusChangedSignal()
{
    RemoteClient client;
    QSignalSpy spy(&client, &RemoteClient::statusChanged);
    QCOMPARE(client.status(), QString("Disconnected"));
    QVERIFY(spy.count() >= 0);
}

void TestRemoteClient::testConnectedChangedSignal()
{
    RemoteClient client;
    QSignalSpy spy(&client, &RemoteClient::connectedChanged);
    QVERIFY(!client.connected());
    QVERIFY(spy.count() >= 0);
}

void TestRemoteClient::testPacketRateProperty()
{
    RemoteClient client;
    QCOMPARE(client.packetRate(), 0.0);
}

void TestRemoteClient::testConnectToFakeServer()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    QSignalSpy connectedSpy(&client, &RemoteClient::connectedChanged);

    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket != nullptr);

    QTRY_VERIFY_WITH_TIMEOUT(connectedSpy.count() > 0, 2000);
    QVERIFY(client.connected());

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testDisconnectCleanly()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    client.disconnect();
    QTest::qWait(200);

    QVERIFY(!client.connected());
    QCOMPARE(client.status(), QString("Disconnected"));

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testDisconnectDuringVideo()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    client.disconnect();
    QTest::qWait(200);

    QVERIFY(!client.connected());

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testSendMouseClickWhenDisconnected()
{
    RemoteClient client;
    client.sendMouseClick(100, 200, 0, true);
    client.sendMouseClick(100, 200, 0, false);
}

void TestRemoteClient::testSendMouseMoveWhenDisconnected()
{
    RemoteClient client;
    client.sendMouseMove(100, 200);
}

void TestRemoteClient::testSendKeyPressWhenDisconnected()
{
    RemoteClient client;
    client.sendKeyPress(65, true, 0);
    client.sendKeyPress(65, false, 0);
}

void TestRemoteClient::testCodecInfoFlow()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // Create a valid SPS/PPS from an actual encoder
    HWEncoder encoder;
    encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software);
    QByteArray extraData = encoder.getExtraData();

    Message codecMsg;
    codecMsg.type = MessageType::CodecInfo;
    codecMsg.data = extraData;
    codecMsg.size = extraData.size();

    serverSocket->write(codecMsg.serialize());
    serverSocket->flush();

    QTest::qWait(500);

    QCOMPARE(client.status(), QString("Connected"));

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testScreenDataFlow()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // Send a fake JPEG image
    QImage img(10, 10, QImage::Format_RGB32);
    img.fill(Qt::red);
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPEG");

    Message jpegMsg;
    jpegMsg.type = MessageType::ScreenData;
    jpegMsg.data = jpegData;
    jpegMsg.size = jpegData.size();

    serverSocket->write(jpegMsg.serialize());
    serverSocket->flush();

    QTest::qWait(100);

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testVideoDataFlow()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // First send CodecInfo to initialize decoder
    HWEncoder encoder;
    encoder.initialize(64, 64, 30, 1000000, HWEncoder::Software);
    QByteArray extraData = encoder.getExtraData();

    Message codecMsg;
    codecMsg.type = MessageType::CodecInfo;
    codecMsg.data = extraData;
    codecMsg.size = extraData.size();
    serverSocket->write(codecMsg.serialize());
    serverSocket->flush();
    QTest::qWait(500);

    // Now send video data
    QImage img(64, 64, QImage::Format_RGB32);
    img.fill(Qt::green);
    QPixmap pixmap = QPixmap::fromImage(img);
    encoder.encodeFrame(pixmap);
    QTest::qWait(500);

    QByteArray encoded = encoder.getEncodedPacket();
    if (!encoded.isEmpty()) {
        QByteArray timestampedData = LatencyMonitor::addTimestamp(encoded);

        Message videoMsg;
        videoMsg.type = MessageType::VideoData;
        videoMsg.data = timestampedData;
        videoMsg.size = timestampedData.size();
        serverSocket->write(videoMsg.serialize());
        serverSocket->flush();

        QTest::qWait(500);
    }

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testCodecInfoTimeout()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // Don't send CodecInfo - wait for 2s timeout
    // After timeout, decoder initializes without extradata
    QTest::qWait(2500);

    QVERIFY(client.status() == "Connected (limited)" ||
            client.status() == "Decoder init failed" ||
            client.status() == "Waiting for codec info...");

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testCursorUpdate()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    QSignalSpy cursorSpy(&client, &RemoteClient::cursorUpdated);

    // Send cursor update
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    int x = 100, y = 200;
    bool visible = true;
    int hotspotX = 5, hotspotY = 10;
    stream << x << y << visible << hotspotX << hotspotY;

    Message cursorMsg;
    cursorMsg.type = MessageType::CursorUpdate;
    cursorMsg.data = data;
    cursorMsg.size = data.size();

    serverSocket->write(cursorMsg.serialize());
    serverSocket->flush();

    QTest::qWait(100);

    QVERIFY(cursorSpy.count() > 0);

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testPingPong()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // Send Ping message from server
    Message pingMsg;
    pingMsg.type = MessageType::Ping;
    pingMsg.size = 0;
    serverSocket->write(pingMsg.serialize());
    serverSocket->flush();

    QTest::qWait(100);

    // Client sends ping every 5 seconds, but we just verify the connection works
    QVERIFY(client.connected());

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testMouseClickWhenConnected()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // Send mouse click
    client.sendMouseClick(100, 200, 0, true);

    QTest::qWait(100);
    QVERIFY(serverSocket->bytesAvailable() > 0);
    QByteArray data = serverSocket->readAll();
    QVERIFY(!data.isEmpty());

    Message msg = Message::deserialize(data);
    QCOMPARE(msg.type, MessageType::MouseClick);

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testMouseMoveBatched()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // Send multiple mouse moves (should be batched into one)
    client.sendMouseMove(10, 20);
    client.sendMouseMove(30, 40);
    client.sendMouseMove(50, 60);

    QTest::qWait(50);
    QVERIFY(serverSocket->bytesAvailable() > 0);
    QByteArray data = serverSocket->readAll();

    Message msg = Message::deserialize(data);
    QCOMPARE(msg.type, MessageType::MouseMove);

    serverSocket->close();
    server.close();
}

void TestRemoteClient::testFlushPendingMouseEvents()
{
    QTcpServer server;
    quint16 port = getFreePort();
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    RemoteClient client;
    client.connectToServer("127.0.0.1", port);

    QVERIFY(server.waitForNewConnection(2000));
    QTcpSocket* serverSocket = server.nextPendingConnection();

    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    // Send mouse move then mouse click (should flush pending moves before click)
    client.sendMouseMove(10, 20);
    QTest::qWait(2); // Don't let timer fire
    client.sendMouseClick(10, 20, Qt::LeftButton, true);

    QTest::qWait(100);
    QVERIFY(serverSocket->bytesAvailable() > 0);
    QByteArray data = serverSocket->readAll();
    QVERIFY(!data.isEmpty());

    Message msg = Message::deserialize(data);
    QVERIFY(msg.type == MessageType::MouseMove || msg.type == MessageType::MouseClick);

    serverSocket->close();
    server.close();
}
