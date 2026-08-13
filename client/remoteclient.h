#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QPixmap>
#include <QMutex>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QImage>
#include <memory>
#include <atomic>
#include "../common/protocol.h"
#include "hwdecoder.h"

class ImageProvider : public QObject
{
    Q_OBJECT
public:
    ImageProvider(QObject *parent = nullptr);
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);
    void setPixmap(const QPixmap &pixmap);

private:
    QImage m_image;  // Use QImage instead of QPixmap for better performance
    std::atomic<bool> m_imageReady{false};
    QMutex m_mutex;
};

class RemoteClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double packetRate READ packetRate NOTIFY packetRateChanged)

public:
    explicit RemoteClient(QObject *parent = nullptr);
    
    Q_INVOKABLE void connectToServer(const QString& host, int port);
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void sendMouseMove(int x, int y);
    Q_INVOKABLE void sendMouseClick(int x, int y, int button, bool pressed);
    Q_INVOKABLE void sendKeyPress(int key, bool pressed, int modifiers);
    
    bool connected() const { return m_connected; }
    QString status() const { return m_status; }
    double packetRate() const { return m_packetRate; }
    
    ImageProvider* imageProvider() { return m_imageProvider; }

signals:
    void connectedChanged();
    void statusChanged();
    void frameReceived();
    void cursorUpdated(int x, int y, bool visible);
    void packetRateChanged();

private slots:
    void onConnected();
    void onDisconnected();
    void onDataReceived();
    void onError(QAbstractSocket::SocketError error);
    void sendPing();
    void onDecodedFrameReady();

private:
    void handleScreenData(const QByteArray& data);
    void handleVideoData(const QByteArray& data);
    void handleCodecInfo(const QByteArray& data);
    void handleCursorUpdate(const QByteArray& data);
    void setStatus(const QString& status);
    void setConnected(bool connected);

    QTcpSocket* m_socket;
    QTimer* m_pingTimer;
    ImageProvider* m_imageProvider;
    bool m_connected;
    QString m_status;
    QByteArray m_buffer;

    // Hardware-accelerated decoder
    std::unique_ptr<HWDecoder> m_decoder;
    bool m_decoderInitialized;

    // Mouse event batching for performance
    struct PendingMouseEvent {
        int x, y, button;
        bool pressed;
        qint64 timestamp;
    };
    PendingMouseEvent m_lastMouseMove;
    QTimer* m_mouseBatchTimer;
    bool m_hasPendingMouseMove;

    // Instance-specific debug variables (previously static)
    int m_frameCounter;
    QElapsedTimer m_debugTimer;

    // Packet rate tracking for UI display
    int m_packetCounter;
    double m_packetRate;
    QElapsedTimer m_packetTimer;
    qint64 m_lastPacketLog;
    double m_avgLatency;

private slots:
    void flushPendingMouseEvents();
};

#endif // REMOTECLIENT_H