#include "remoteclient.h"
#include "hwdecoder.h"
#include "../common/latencymonitor.h"
#include <QDebug>
#include <QBuffer>
#include <QDataStream>
#include <QMutexLocker>

ImageProvider::ImageProvider(QObject *parent)
    : QObject(parent)
{
}

QPixmap ImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id)
    Q_UNUSED(requestedSize)

    // Quick check without lock
    if (!m_imageReady.load(std::memory_order_acquire)) {
        return QPixmap();
    }

    QMutexLocker locker(&m_mutex);
    if (size) {
        *size = m_image.size();
    }
    // Convert QImage to QPixmap only when needed
    return QPixmap::fromImage(m_image);
}

void ImageProvider::setPixmap(const QPixmap &pixmap)
{
    // Convert to QImage for faster storage and manipulation
    QImage image = pixmap.toImage();

    QMutexLocker locker(&m_mutex);
    m_image = std::move(image);
    m_imageReady.store(true, std::memory_order_release);
}

RemoteClient::RemoteClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_pingTimer(new QTimer(this))
    , m_imageProvider(new ImageProvider(this))
    , m_connected(false)
    , m_status("Disconnected")
    , m_decoder(std::make_unique<HWDecoder>())
    , m_decoderInitialized(false)
    , m_mouseBatchTimer(new QTimer(this))
    , m_hasPendingMouseMove(false)
    , m_frameCounter(0)
{
    connect(m_socket, &QTcpSocket::connected, this, &RemoteClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &RemoteClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &RemoteClient::onDataReceived);

    // Qt 5.14 compatibility - use error() instead of errorOccurred()
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &RemoteClient::onError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &RemoteClient::onError);
#endif

    connect(m_pingTimer, &QTimer::timeout, this, &RemoteClient::sendPing);
    m_pingTimer->setInterval(5000);

    connect(m_decoder.get(), &HWDecoder::frameReady, this, &RemoteClient::onDecodedFrameReady);

    // Mouse event batching - flush every 5ms to batch rapid mouse movements
    connect(m_mouseBatchTimer, &QTimer::timeout, this, &RemoteClient::flushPendingMouseEvents);
    m_mouseBatchTimer->setInterval(5);
    m_mouseBatchTimer->setTimerType(Qt::PreciseTimer);
}

void RemoteClient::connectToServer(const QString& host, int port)
{
    setStatus("Connecting...");
    m_socket->connectToHost(host, port);
}

void RemoteClient::disconnect()
{
    m_socket->disconnectFromHost();
    m_pingTimer->stop();
}

void RemoteClient::sendMouseMove(int x, int y)
{
    if (!m_connected) return;

    // Batch mouse move events to reduce network overhead
    m_lastMouseMove.x = x;
    m_lastMouseMove.y = y;
    m_lastMouseMove.button = -1;
    m_lastMouseMove.pressed = false;
    m_hasPendingMouseMove = true;

    // Start timer if not already running
    if (!m_mouseBatchTimer->isActive()) {
        m_mouseBatchTimer->start();
    }
}

void RemoteClient::sendMouseClick(int x, int y, int button, bool pressed)
{
    if (!m_connected) return;

    // Flush any pending mouse moves before sending click
    if (m_hasPendingMouseMove) {
        flushPendingMouseEvents();
    }

    MouseEvent event;
    event.x = x;
    event.y = y;
    event.button = button;
    event.pressed = pressed;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << event.x << event.y << event.button << event.pressed;

    Message msg;
    msg.type = MessageType::MouseClick;
    msg.data = data;
    msg.size = data.size();

    m_socket->write(msg.serialize());
}

void RemoteClient::sendKeyPress(int key, bool pressed, int modifiers)
{
    if (!m_connected) return;
    
    KeyEvent event;
    event.key = key;
    event.pressed = pressed;
    event.modifiers = modifiers;
    
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << event.key << event.pressed << event.modifiers;
    
    Message msg;
    msg.type = MessageType::KeyEvent;
    msg.data = data;
    msg.size = data.size();
    
    m_socket->write(msg.serialize());
}

void RemoteClient::onConnected()
{
    // CRITICAL: Optimize TCP for ultra-low latency
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1); // TCP_NODELAY
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    // SMALL buffers for lowest latency (avoid bufferbloat)
    m_socket->setReadBufferSize(256 * 1024); // 256KB
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 128 * 1024);
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 256 * 1024);

    // Initialize hardware decoder
    if (m_decoder->initialize(HWDecoder::Auto)) {
        qDebug() << "Hardware decoder initialized";
        m_decoderInitialized = true;
    } else {
        qWarning() << "Failed to initialize hardware decoder";
    }

    setConnected(true);
    setStatus("Connected");
    m_pingTimer->start();
}

void RemoteClient::onDisconnected()
{
    qDebug() << "Disconnected from server, cleaning up...";

    setConnected(false);
    setStatus("Disconnected");
    m_pingTimer->stop();

    // Reset decoder state
    if (m_decoderInitialized) {
        m_decoderInitialized = false;

        // Clear buffer
        m_buffer.clear();

        // Recreate decoder for next connection
        m_decoder.reset();
        m_decoder = std::make_unique<HWDecoder>();
        connect(m_decoder.get(), &HWDecoder::frameReady, this, &RemoteClient::onDecodedFrameReady);
    }

    qDebug() << "Client cleanup complete, ready to reconnect";
}

void RemoteClient::onDataReceived()
{
    // Read directly into buffer - avoid extra allocations
    const qint64 bytesAvailable = m_socket->bytesAvailable();
    const int currentSize = m_buffer.size();
    m_buffer.resize(currentSize + bytesAvailable);
    const qint64 bytesRead = m_socket->read(m_buffer.data() + currentSize, bytesAvailable);

    if (bytesRead < 0) {
        m_buffer.resize(currentSize);
        return;
    }

    m_buffer.resize(currentSize + bytesRead);

    int processedBytes = 0;

    while (m_buffer.size() - processedBytes >= static_cast<int>(sizeof(quint8) + sizeof(quint32))) {
        // Peek at header to get message size - work directly with buffer data
        const char* headerPtr = m_buffer.constData() + processedBytes;
        QDataStream headerStream(QByteArray::fromRawData(headerPtr, sizeof(quint8) + sizeof(quint32)));
        headerStream.setByteOrder(QDataStream::BigEndian);

        quint8 typeValue;
        quint32 size;
        headerStream >> typeValue >> size;

        qint64 totalMessageSize = sizeof(quint8) + sizeof(quint32) + size;

        if (m_buffer.size() - processedBytes < totalMessageSize) {
            break; // Wait for more data
        }

        // Process message without extra copy
        QByteArray messageData = QByteArray::fromRawData(headerPtr, totalMessageSize);
        Message msg = Message::deserialize(messageData);

        switch (msg.type) {
            case MessageType::ScreenData:
                handleScreenData(msg.data);
                break;
            case MessageType::VideoData:
                handleVideoData(msg.data);
                break;
            case MessageType::CodecInfo:
                handleCodecInfo(msg.data);
                break;
            case MessageType::CursorUpdate:
                handleCursorUpdate(msg.data);
                break;
            case MessageType::Pong:
                break;
            default:
                break;
        }

        processedBytes += totalMessageSize;
    }

    // Remove processed data efficiently
    if (processedBytes > 0) {
        m_buffer.remove(0, processedBytes);
    }
}

void RemoteClient::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    setStatus("Error: " + m_socket->errorString());
}

void RemoteClient::sendPing()
{
    if (!m_connected) return;
    
    Message msg;
    msg.type = MessageType::Ping;
    msg.size = 0;
    
    m_socket->write(msg.serialize());
}

void RemoteClient::handleScreenData(const QByteArray& data)
{
    // Legacy JPEG decompression (fallback)
    QPixmap pixmap;
    if (pixmap.loadFromData(data, "JPEG")) {
        m_imageProvider->setPixmap(pixmap);
        emit frameReceived();
    }
}

void RemoteClient::handleVideoData(const QByteArray& data)
{
    if (!m_decoderInitialized) {
        qWarning() << "Decoder not initialized, ignoring video packet";
        return;
    }

    // Extract timestamp and measure latency
    QByteArray actualData;
    double latencyMs = LatencyMonitor::getLatencyMs(data, actualData);

    // Log latency every second
    static int latencyCounter = 0;
    static double avgLatency = 0.0;
    static qint64 lastLatencyLog = 0;
    static QElapsedTimer latencyTimer;

    if (!latencyTimer.isValid()) {
        latencyTimer.start();
    }

    if (latencyMs > 0) {
        avgLatency = (avgLatency * 0.9) + (latencyMs * 0.1);
        latencyCounter++;

        qint64 elapsed = latencyTimer.elapsed();
        if (elapsed - lastLatencyLog >= 1000) {
            qDebug() << "Network latency:" << qRound(avgLatency * 10) / 10.0 << "ms"
                     << "| Packets/sec:" << latencyCounter;
            latencyCounter = 0;
            lastLatencyLog = elapsed;
        }
    }

    // Send to hardware decoder (non-blocking)
    m_decoder->decodePacket(actualData);
}

void RemoteClient::handleCodecInfo(const QByteArray& data)
{
    qDebug() << "Received codec info:" << data.size() << "bytes";
    // Codec info (SPS/PPS) can be used for decoder initialization if needed
    // Currently the decoder initializes itself automatically
}

void RemoteClient::onDecodedFrameReady()
{
    // Get decoded frame from hardware decoder
    QPixmap pixmap = m_decoder->getDecodedFrame();

    if (!pixmap.isNull()) {
        m_imageProvider->setPixmap(pixmap);
        emit frameReceived();

        // Debug output every second

        if (!m_debugTimer.isValid()) {
            m_debugTimer.start();
        }

        m_frameCounter++;
        qint64 elapsed = m_debugTimer.elapsed();

        if (elapsed >= 1000) {
            HWDecoder::Stats stats = m_decoder->getStats();
            qDebug() << "Client - Display:" << m_frameCounter << "fps"
                     << "| Decode:" << qRound(stats.currentFPS) << "fps"
                     << "| Time:" << qRound(stats.avgDecodeTimeMs) << "ms";
            m_frameCounter = 0;
            m_debugTimer.restart();
        }
    }
}

void RemoteClient::setStatus(const QString& status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void RemoteClient::setConnected(bool connected)
{
    if (m_connected != connected) {
        m_connected = connected;
        emit connectedChanged();
    }
}

void RemoteClient::flushPendingMouseEvents()
{
    if (!m_hasPendingMouseMove || !m_connected) {
        m_mouseBatchTimer->stop();
        return;
    }

    MouseEvent event;
    event.x = m_lastMouseMove.x;
    event.y = m_lastMouseMove.y;
    event.button = m_lastMouseMove.button;
    event.pressed = m_lastMouseMove.pressed;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << event.x << event.y << event.button << event.pressed;

    Message msg;
    msg.type = MessageType::MouseMove;
    msg.data = data;
    msg.size = data.size();

    m_socket->write(msg.serialize());

    m_hasPendingMouseMove = false;
    m_mouseBatchTimer->stop();
}

void RemoteClient::handleCursorUpdate(const QByteArray& data)
{
    QDataStream stream(data);
    int x, y, hotspotX, hotspotY;
    bool visible;
    
    stream >> x >> y >> visible >> hotspotX >> hotspotY;
    
    // Emit signal to update remote cursor display
    emit cursorUpdated(x, y, visible);
}