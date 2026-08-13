#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QScreen>
#include <QPixmap>
#include <QApplication>
#include <QDesktopWidget>
#include <QBuffer>
#include <QElapsedTimer>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "../common/protocol.h"
#include "hwencoder.h"

#ifdef Q_OS_LINUX
// Forward declaration for X11 Display
typedef struct _XDisplay Display;
#include <set>
#endif

class RemoteServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serverRunning READ serverRunning NOTIFY serverRunningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString clientAddress READ clientAddress NOTIFY clientConnected)

public:
    explicit RemoteServer(QObject *parent = nullptr);
    ~RemoteServer();
    bool start(quint16 port);
    void stop();
    bool serverRunning() const { return m_serverRunning; }
    QString status() const { return m_status; }
    QString clientAddress() const { return m_clientAddress; }

signals:
    void connectedChanged();
    void statusChanged();
    void serverRunningChanged();
    void clientConnected(const QString& clientAddress);
    void clientDisconnected();

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientDataReceived();
    void captureAndSendScreen();
    void onEncodedPacketReady();
    void sendCursorUpdate();

private:
    void handleMouseEvent(const QByteArray& data);
    void handleKeyEvent(const QByteArray& data);
    void sendCodecInfo();
    void releaseAllPressedKeys(); // Release all stuck keys

    QTcpServer* m_server;
    QTcpSocket* m_client;
    QTimer* m_captureTimer;
    QTimer* m_cursorTimer;
    QPixmap m_lastFrame;
    int m_frameRate;
    QElapsedTimer m_frameTimer;
    qint64 m_lastCaptureTime;
    QPoint m_lastCursorPos;

    bool m_serverRunning;
    QString m_status;
    QString m_clientAddress;

    // Hardware-accelerated encoder
    std::unique_ptr<HWEncoder> m_encoder;
    bool m_encoderInitialized;

    // Instance-specific variables (previously static)
    QPixmap m_lastPixmap;
    int m_consecutiveDrops;
    int m_frameCounter;
    qint64 m_lastDebugTime;
    int m_debugCounter;

#ifdef Q_OS_LINUX
    // Persistent X11 display connection for performance
    Display* m_display;
    // Track pressed keys to prevent stuck keys
    std::set<unsigned int> m_pressedKeys;
#endif
};

#endif // REMOTESERVER_H