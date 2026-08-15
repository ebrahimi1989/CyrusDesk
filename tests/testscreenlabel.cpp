#include "testscreenlabel.h"
#define private public
#include "../client/remotescreenlabel.h"
#include "../client/remoteclient.h"
#undef private
#include "../common/protocol.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QSignalSpy>

class TestableScreenLabel : public RemoteScreenLabel
{
public:
    using RemoteScreenLabel::mousePressEvent;
    using RemoteScreenLabel::mouseReleaseEvent;
    using RemoteScreenLabel::mouseMoveEvent;
    using RemoteScreenLabel::keyPressEvent;
    using RemoteScreenLabel::keyReleaseEvent;
    using RemoteScreenLabel::paintEvent;
    using RemoteScreenLabel::getScaledPosition;
    using RemoteScreenLabel::getDisplayPosition;
};

void TestScreenLabel::initTestCase()
{
}

void TestScreenLabel::cleanupTestCase()
{
}

void TestScreenLabel::testConstructor()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QVERIFY(label != nullptr);
    QVERIFY(label->size().isValid());
    delete label;
}

void TestScreenLabel::testSetRemoteClient()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);
    QCOMPARE(label->m_client, client);
    delete client;
    delete label;
}

void TestScreenLabel::testUpdateRemoteCursor()
{
    TestableScreenLabel* label = new TestableScreenLabel();

    label->updateRemoteCursor(100, 200, true);
    QCOMPARE(label->m_remoteCursorPos, QPoint(100, 200));
    QVERIFY(label->m_remoteCursorVisible);

    label->updateRemoteCursor(50, 75, false);
    QVERIFY(!label->m_remoteCursorVisible);

    delete label;
}

void TestScreenLabel::testGetScaledPositionWithPixmap()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QPoint result = label->getScaledPosition(QPoint(100, 100));
    QVERIFY(result.x() >= 0 && result.y() >= 0);

    delete label;
}

void TestScreenLabel::testGetScaledPositionWithoutPixmap()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QPoint result = label->getScaledPosition(QPoint(100, 100));
    QCOMPARE(result, QPoint(-1, -1));

    delete label;
}

void TestScreenLabel::testGetDisplayPositionWithPixmap()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QPoint result = label->getDisplayPosition(QPoint(320, 240));
    QVERIFY(result.x() >= 0 && result.y() >= 0);

    delete label;
}

void TestScreenLabel::testGetDisplayPositionWithoutPixmap()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QPoint result = label->getDisplayPosition(QPoint(100, 100));
    QCOMPARE(result, QPoint(-1, -1));

    delete label;
}

void TestScreenLabel::testMousePressEventConnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);

    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseButtonPress, QPoint(50, 50),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    label->mousePressEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testMousePressEventDisconnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseButtonPress, QPoint(50, 50),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    label->mousePressEvent(&event);

    delete label;
}

void TestScreenLabel::testMouseReleaseEventConnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);

    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseButtonRelease, QPoint(50, 50),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    label->mouseReleaseEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testMouseReleaseEventDisconnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseButtonRelease, QPoint(50, 50),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    label->mouseReleaseEvent(&event);

    delete label;
}

void TestScreenLabel::testMouseMoveEventConnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);

    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseMove, QPoint(50, 50),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    label->mouseMoveEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testMouseMoveEventDisconnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseMove, QPoint(50, 50),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    label->mouseMoveEvent(&event);

    delete label;
}

void TestScreenLabel::testKeyPressEvent()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    label->keyPressEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testKeyPressEventAutoRepeat()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a", true);
    QVERIFY(event.isAutoRepeat());
    label->keyPressEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testKeyReleaseEvent()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);

    QKeyEvent event(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a");
    label->keyReleaseEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testKeyReleaseEventAutoRepeat()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);

    QKeyEvent event(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a", true);
    QVERIFY(event.isAutoRepeat());
    label->keyReleaseEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testKeyPressEventDisconnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();

    QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    label->keyPressEvent(&event);

    delete label;
}

void TestScreenLabel::testKeyReleaseEventDisconnected()
{
    TestableScreenLabel* label = new TestableScreenLabel();

    QKeyEvent event(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a");
    label->keyReleaseEvent(&event);

    delete label;
}

void TestScreenLabel::testPaintEventWithCursor()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    label->updateRemoteCursor(100, 100, true);

    QPaintEvent event(QRect(0, 0, 320, 240));
    label->paintEvent(&event);

    delete label;
}

void TestScreenLabel::testPaintEventWithoutCursor()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QPaintEvent event(QRect(0, 0, 320, 240));
    label->paintEvent(&event);

    delete label;
}

void TestScreenLabel::testMousePressConnectedClient()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);
    client->m_connected = true;

    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseButtonPress, QPoint(50, 50),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    label->mousePressEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testMouseReleaseConnectedClient()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);
    client->m_connected = true;

    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseButtonRelease, QPoint(50, 50),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    label->mouseReleaseEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testMouseMoveConnectedClient()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);
    client->m_connected = true;

    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    QMouseEvent event(QEvent::MouseMove, QPoint(50, 50),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    label->mouseMoveEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testKeyPressConnectedClient()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);
    client->m_connected = true;

    QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    label->keyPressEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testKeyReleaseConnectedClient()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    RemoteClient* client = new RemoteClient();
    label->setRemoteClient(client);
    client->m_connected = true;

    QKeyEvent event(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a");
    label->keyReleaseEvent(&event);

    delete client;
    delete label;
}

void TestScreenLabel::testUpdateRemoteCursorNoChange()
{
    TestableScreenLabel* label = new TestableScreenLabel();

    label->m_remoteCursorPos = QPoint(100, 200);
    label->m_remoteCursorVisible = true;
    label->updateRemoteCursor(100, 200, true);

    QCOMPARE(label->m_remoteCursorPos, QPoint(100, 200));
    QVERIFY(label->m_remoteCursorVisible);

    delete label;
}

void TestScreenLabel::testGetScaledPositionSmallPixmap()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(10, 10, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(100, 100);

    QPoint result = label->getScaledPosition(QPoint(5, 5));
    QVERIFY(result.x() >= 0 && result.y() >= 0);

    delete label;
}

void TestScreenLabel::testGetDisplayPositionSmallPixmap()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(10, 10, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(100, 100);

    QPoint result = label->getDisplayPosition(QPoint(5, 5));
    QVERIFY(result.x() >= 0 && result.y() >= 0);

    delete label;
}

void TestScreenLabel::testPaintEventWithMultipleCursors()
{
    TestableScreenLabel* label = new TestableScreenLabel();
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    label->setPixmap(QPixmap::fromImage(img));
    label->resize(320, 240);

    label->updateRemoteCursor(0, 0, true);
    QPaintEvent event(QRect(0, 0, 320, 240));
    label->paintEvent(&event);

    label->updateRemoteCursor(300, 200, true);
    label->paintEvent(&event);

    delete label;
}
