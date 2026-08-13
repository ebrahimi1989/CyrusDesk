#include "remoteserver.h"
#include "hwencoder.h"
#include "../common/latencymonitor.h"
#include <QGuiApplication>
#include <QScreen>
#include <QBuffer>
#include <QDataStream>
#include <QDebug>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#endif

#ifdef Q_OS_LINUX
// Convert Qt Key codes to X11 KeySym
KeySym qtKeyToXKeySym(int qtKey) {
    switch (qtKey) {
        // Special keys
        case Qt::Key_Escape: return XK_Escape;
        case Qt::Key_Backspace: return XK_BackSpace;
        case Qt::Key_Return: return XK_Return;
        case Qt::Key_Enter: return XK_KP_Enter;
        case Qt::Key_Tab: return XK_Tab;
        case Qt::Key_Space: return XK_space;
        
        // Navigation keys
        case Qt::Key_Home: return XK_Home;
        case Qt::Key_End: return XK_End;
        case Qt::Key_PageUp: return XK_Page_Up;
        case Qt::Key_PageDown: return XK_Page_Down;
        case Qt::Key_Insert: return XK_Insert;
        case Qt::Key_Delete: return XK_Delete;
        
        // Arrow keys
        case Qt::Key_Left: return XK_Left;
        case Qt::Key_Right: return XK_Right;
        case Qt::Key_Up: return XK_Up;
        case Qt::Key_Down: return XK_Down;
        
        // Function keys
        case Qt::Key_F1: return XK_F1;
        case Qt::Key_F2: return XK_F2;
        case Qt::Key_F3: return XK_F3;
        case Qt::Key_F4: return XK_F4;
        case Qt::Key_F5: return XK_F5;
        case Qt::Key_F6: return XK_F6;
        case Qt::Key_F7: return XK_F7;
        case Qt::Key_F8: return XK_F8;
        case Qt::Key_F9: return XK_F9;
        case Qt::Key_F10: return XK_F10;
        case Qt::Key_F11: return XK_F11;
        case Qt::Key_F12: return XK_F12;
        
        // Modifier keys
        case Qt::Key_Control: return XK_Control_L;
        case Qt::Key_Alt: return XK_Alt_L;
        case Qt::Key_Shift: return XK_Shift_L;
        case Qt::Key_Meta: return XK_Super_L;
        
        // Alphanumeric keys - Qt uses ASCII values
        default:
            if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
                return qtKey; // ASCII values match
            }
            if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
                return qtKey; // ASCII values match
            }
            if (qtKey >= 32 && qtKey <= 126) {
                return qtKey; // ASCII printable characters
            }
            return NoSymbol;
    }
}
#endif

RemoteServer::RemoteServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_client(nullptr)
    , m_captureTimer(new QTimer(this))
    , m_cursorTimer(new QTimer(this))
    , m_frameRate(60)
    , m_lastCaptureTime(0)
    , m_lastCursorPos(-1, -1)
    , m_encoder(std::make_unique<HWEncoder>())
    , m_encoderInitialized(false)
    , m_consecutiveDrops(0)
    , m_frameCounter(0)
    , m_lastDebugTime(0)
    , m_debugCounter(0)
#ifdef Q_OS_LINUX
    , m_display(nullptr)
#endif
{
    connect(m_server, &QTcpServer::newConnection, this, &RemoteServer::onNewConnection);
    connect(m_captureTimer, &QTimer::timeout, this, &RemoteServer::captureAndSendScreen);
    connect(m_cursorTimer, &QTimer::timeout, this, &RemoteServer::sendCursorUpdate);
    connect(m_encoder.get(), &HWEncoder::packetReady, this, &RemoteServer::onEncodedPacketReady);

    // Use precise timer for accurate frame rate
    m_captureTimer->setTimerType(Qt::PreciseTimer);
    m_captureTimer->setInterval(16); // Fixed 16ms = ~62.5fps (will be limited to 60fps)
    
    // Cursor update timer - lower frequency than screen capture
    m_cursorTimer->setTimerType(Qt::CoarseTimer);
    m_cursorTimer->setInterval(50); // 20 FPS for cursor updates
    
    m_frameTimer.start();

#ifdef Q_OS_LINUX
    // Open persistent X11 display connection for better performance
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        qWarning() << "Failed to open X11 display";
    }
#endif
}

RemoteServer::~RemoteServer()
{
#ifdef Q_OS_LINUX
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
#endif
}

bool RemoteServer::start(quint16 port)
{
    return m_server->listen(QHostAddress::Any, port);
}

void RemoteServer::onNewConnection()
{
    if (m_client) {
        m_client->disconnectFromHost();
    }

    m_client = m_server->nextPendingConnection();

    // CRITICAL: Optimize TCP for ultra-low latency
    m_client->setSocketOption(QAbstractSocket::LowDelayOption, 1); // TCP_NODELAY
    m_client->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    // SMALL buffers for lowest latency (avoid bufferbloat)
    m_client->setReadBufferSize(128 * 1024); // 128KB
    m_client->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 256 * 1024); // 256KB
    m_client->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 128 * 1024);

    connect(m_client, &QTcpSocket::disconnected, this, &RemoteServer::onClientDisconnected);
    connect(m_client, &QTcpSocket::readyRead, this, &RemoteServer::onClientDataReceived);

    // Initialize encoder
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();

        // Calculate optimal bitrate based on resolution
        // Formula: width * height * fps * 0.1 (bits per pixel)
        int bitrate = screenGeometry.width() * screenGeometry.height() * m_frameRate * 0.1;
        bitrate = qMax(10000000, qMin(bitrate, 25000000)); // 10-25 Mbps range

        if (m_encoder->initialize(screenGeometry.width(), screenGeometry.height(), m_frameRate, bitrate, HWEncoder::Auto)) {
            qDebug() << "Hardware encoder initialized:" << screenGeometry.size()
                     << "@ bitrate:" << (bitrate / 1000000) << "Mbps";
            m_encoderInitialized = true;

            // Send codec configuration data (SPS/PPS)
            sendCodecInfo();

            m_captureTimer->start();
            m_cursorTimer->start();
        } else {
            qCritical() << "Failed to initialize encoder";
        }
    }
}

void RemoteServer::onClientDisconnected()
{
    qDebug() << "Client disconnected, cleaning up...";

    m_captureTimer->stop();
    m_cursorTimer->stop();

    // CRITICAL: Release all pressed keys to prevent stuck keyboard
    releaseAllPressedKeys();

    // Reset encoder state
    if (m_encoderInitialized) {
        m_encoderInitialized = false;

        // Recreate encoder for next connection
        m_encoder.reset();
        m_encoder = std::make_unique<HWEncoder>();
        connect(m_encoder.get(), &HWEncoder::packetReady, this, &RemoteServer::onEncodedPacketReady);
    }

    m_client = nullptr;
    qDebug() << "Cleanup complete, ready for new connection";
}

void RemoteServer::onClientDataReceived()
{
    if (!m_client) return;
    
    while (m_client->bytesAvailable() >= static_cast<qint64>(sizeof(quint8) + sizeof(quint32))) {
        QByteArray header = m_client->peek(sizeof(quint8) + sizeof(quint32));
        QDataStream headerStream(header);
        
        quint8 typeValue;
        quint32 size;
        headerStream >> typeValue >> size;
        
        if (m_client->bytesAvailable() < static_cast<qint64>(sizeof(quint8) + sizeof(quint32) + size)) {
            break;
        }
        
        QByteArray messageData = m_client->read(sizeof(quint8) + sizeof(quint32) + size);
        Message msg = Message::deserialize(messageData);
        
        switch (msg.type) {
            case MessageType::MouseMove:
            case MessageType::MouseClick:
                handleMouseEvent(msg.data);
                break;
            case MessageType::KeyEvent:
                handleKeyEvent(msg.data);
                break;
            case MessageType::Ping: {
                Message pong;
                pong.type = MessageType::Pong;
                pong.size = 0;
                m_client->write(pong.serialize());
                break;
            }
            default:
                break;
        }
    }
}

void RemoteServer::captureAndSendScreen()
{
    if (!m_client || m_client->state() != QTcpSocket::ConnectedState || !m_encoderInitialized) {
        return;
    }

    // Strict frame rate limiting with adaptive quality
    qint64 currentTime = m_frameTimer.elapsed();
    qint64 minInterval = 1000 / m_frameRate;

    if (currentTime - m_lastCaptureTime < minInterval) {
        return;
    }
    m_lastCaptureTime = currentTime;

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qWarning() << "No primary screen available";
        return;
    }

    // Fast screen capture with optimized settings
    QPixmap pixmap;
    
#ifdef Q_OS_LINUX
    // Use X11 for faster capture on Linux
    if (m_display) {
        // Get root window properties for fast capture
        Window root = DefaultRootWindow(m_display);
        XWindowAttributes attrs;
        XGetWindowAttributes(m_display, root, &attrs);
        
        // Quick dirty region detection - only capture if something changed
        pixmap = screen->grabWindow(0, 0, 0, 
                                   qMin(1920, attrs.width), 
                                   qMin(1080, attrs.height));
        
        // Simple change detection to avoid unnecessary encoding
        if (!m_lastPixmap.isNull() && pixmap.size() == m_lastPixmap.size()) {
            QImage currentImg = pixmap.toImage();
            QImage lastImg = m_lastPixmap.toImage();
            
            // Sample-based quick comparison (check every 10th pixel)
            bool hasChanged = false;
            int step = 10;
            for (int y = 0; y < currentImg.height() && !hasChanged; y += step) {
                for (int x = 0; x < currentImg.width() && !hasChanged; x += step) {
                    if (currentImg.pixel(x, y) != lastImg.pixel(x, y)) {
                        hasChanged = true;
                    }
                }
            }
            
            if (!hasChanged) {
                return; // Skip encoding if no significant changes
            }
        }
        m_lastPixmap = pixmap;
    } else {
        pixmap = screen->grabWindow(0);
    }
#else
    // Standard capture for other platforms
    pixmap = screen->grabWindow(0);
#endif

    if (pixmap.isNull()) {
        qWarning() << "Failed to capture screen";
        return;
    }

    // Adaptive resolution based on network conditions
    if (m_client->bytesToWrite() > 1024 * 1024) { // 1MB backlog
        m_consecutiveDrops++;
        if (m_consecutiveDrops > 3) {
            // Scale down resolution to reduce bandwidth
            QSize currentSize = pixmap.size();
            QSize targetSize(currentSize.width() * 0.8, currentSize.height() * 0.8);
            pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::FastTransformation);
            m_consecutiveDrops = 0;
        }
    } else {
        m_consecutiveDrops = 0;
    }

    // Send to hardware encoder (non-blocking)
    if (!m_encoder->encodeFrame(pixmap)) {
        qWarning() << "Failed to encode frame";
    }

    // Debug output every second with Persian text
    m_frameCounter++;

    if (currentTime - m_lastDebugTime >= 1000) {
        HWEncoder::Stats stats = m_encoder->getStats();
        qDebug() << "Server - ضبط صفحه:" << m_frameCounter << "fps"
                 << "| رمزگذاری:" << qRound(stats.currentFPS) << "fps"
                 << "| زمان:" << qRound(stats.avgEncodeTimeMs) << "ms"
                 << "| پهنای باند:" << (m_client->bytesToWrite() / 1024) << "KB";
        m_frameCounter = 0;
        m_lastDebugTime = currentTime;
    }
}

void RemoteServer::onEncodedPacketReady()
{
    if (!m_client || m_client->state() != QTcpSocket::ConnectedState || !m_encoderInitialized) {
        return;
    }

    // Get encoded packet from hardware encoder
    QByteArray packet = m_encoder->getEncodedPacket();

    if (packet.isEmpty()) {
        return;
    }

    // Add timestamp for latency measurement
    QByteArray timestampedPacket = LatencyMonitor::addTimestamp(packet);

    Message msg;
    msg.type = MessageType::VideoData;
    msg.data = timestampedPacket;
    msg.size = timestampedPacket.size();

    // Check connection state before writing
    if (m_client && m_client->state() == QTcpSocket::ConnectedState) {
        qint64 written = m_client->write(msg.serialize());
        if (written == -1) {
            qWarning() << "Failed to write data to client";
        }
        // Note: flush() is called automatically by Qt's event loop
        // Manual flush can add overhead, removed for better performance
    }
}

void RemoteServer::sendCodecInfo()
{
    if (!m_client || !m_encoderInitialized) {
        return;
    }

    // Send codec extra data (SPS/PPS for H.264)
    QByteArray extraData = m_encoder->getExtraData();

    if (!extraData.isEmpty()) {
        Message msg;
        msg.type = MessageType::CodecInfo;
        msg.data = extraData;
        msg.size = extraData.size();

        m_client->write(msg.serialize());
        qDebug() << "Sent codec info:" << extraData.size() << "bytes";
    }
}

void RemoteServer::handleMouseEvent(const QByteArray& data)
{
    QDataStream stream(data);
    MouseEvent event;
    stream >> event.x >> event.y >> event.button >> event.pressed;

    qDebug() << "Mouse event: pos(" << event.x << "," << event.y << ") button:" << event.button << "pressed:" << event.pressed;

#ifdef Q_OS_WIN
    POINT pt;
    pt.x = event.x;
    pt.y = event.y;
    SetCursorPos(pt.x, pt.y);

    if (event.button >= 0) {
        DWORD flags = 0;
        switch (event.button) {
            case 0: // Left button
                flags = event.pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
                break;
            case 1: // Right button
                flags = event.pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
                break;
            case 2: // Middle button
                flags = event.pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
                break;
        }
        if (flags) {
            mouse_event(flags, pt.x, pt.y, 0, 0);
        }
    }
#elif defined(Q_OS_LINUX)
    // Use persistent display connection for better performance
    if (m_display) {
        // Move mouse cursor first
        XTestFakeMotionEvent(m_display, DefaultScreen(m_display), event.x, event.y, CurrentTime);
        XFlush(m_display);
        
        // Handle button clicks
        if (event.button >= 0 && event.button <= 2) {
            // Qt to X11 button mapping:
            // Qt: 0=left, 1=right, 2=middle
            // X11: 1=left, 2=middle, 3=right
            int xButton;
            switch (event.button) {
                case 0: xButton = 1; break; // Qt Left → X11 Left
                case 1: xButton = 3; break; // Qt Right → X11 Right  
                case 2: xButton = 2; break; // Qt Middle → X11 Middle
                default: xButton = 1; break;
            }
            
            XTestFakeButtonEvent(m_display, xButton, event.pressed ? True : False, CurrentTime);
            XFlush(m_display);
            
            qDebug() << "X11 button event: button" << xButton << (event.pressed ? "pressed" : "released");
        }
    }
#endif
}

void RemoteServer::handleKeyEvent(const QByteArray& data)
{
    QDataStream stream(data);
    KeyEvent event;
    stream >> event.key >> event.pressed >> event.modifiers;

    qDebug() << "Server received key:" << event.key << "pressed:" << event.pressed << "modifiers:" << event.modifiers;

#ifdef Q_OS_WIN
    BYTE vk = static_cast<BYTE>(event.key);
    DWORD flags = event.pressed ? 0 : KEYEVENTF_KEYUP;
    keybd_event(vk, 0, flags, 0);
#elif defined(Q_OS_LINUX)
    // Use persistent display connection for better performance
    if (!m_display) {
        qWarning() << "X11 display not available";
        return;
    }

    KeySym keysym = qtKeyToXKeySym(event.key);

    if (keysym == NoSymbol) {
        qWarning() << "Failed to convert Qt key to X11 keysym:" << event.key;
        return;
    }

    KeyCode keycode = XKeysymToKeycode(m_display, keysym);
    if (keycode == 0) {
        qWarning() << "Failed to convert keysym to keycode:" << keysym;
        return;
    }

    // Track pressed keys to prevent stuck keys
    if (event.pressed) {
        // Check if key is already pressed (prevent double-press)
        if (m_pressedKeys.find(keycode) != m_pressedKeys.end()) {
            qWarning() << "Key" << keycode << "already pressed, skipping duplicate press event";
            return;
        }
        m_pressedKeys.insert(keycode);
    } else {
        // Check if key was actually pressed before releasing
        if (m_pressedKeys.find(keycode) == m_pressedKeys.end()) {
            qWarning() << "Key" << keycode << "not in pressed state, skipping release event";
            return;
        }
        m_pressedKeys.erase(keycode);
    }

    // Send the key event to X11
    Bool result = XTestFakeKeyEvent(m_display, keycode, event.pressed, CurrentTime);

    if (!result) {
        qWarning() << "XTestFakeKeyEvent failed for keycode" << keycode;
        // Remove from tracking if press failed
        if (event.pressed) {
            m_pressedKeys.erase(keycode);
        }
        return;
    }

    // CRITICAL: Use XSync instead of just XFlush to ensure events are processed
    // XFlush only sends data to X server, XSync waits for processing to complete
    XSync(m_display, False);

    qDebug() << "Key event: Qt" << event.key << "-> X11 keycode" << keycode
             << (event.pressed ? "pressed" : "released")
             << "| Pressed keys count:" << m_pressedKeys.size();
#endif
}

void RemoteServer::sendCursorUpdate()
{
    if (!m_client || m_client->state() != QTcpSocket::ConnectedState) {
        return;
    }

    QPoint currentCursorPos;
    bool cursorVisible = true;

#ifdef Q_OS_WIN
    POINT pt;
    if (GetCursorPos(&pt)) {
        currentCursorPos = QPoint(pt.x, pt.y);
    }
    
    CURSORINFO ci;
    ci.cbSize = sizeof(ci);
    if (GetCursorInfo(&ci)) {
        cursorVisible = (ci.flags & CURSOR_SHOWING) != 0;
    }
#elif defined(Q_OS_LINUX)
    if (m_display) {
        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;
        
        if (XQueryPointer(m_display, DefaultRootWindow(m_display),
                         &root, &child, &rootX, &rootY, &winX, &winY, &mask)) {
            currentCursorPos = QPoint(rootX, rootY);
        }
    }
#else
    // For other platforms, use Qt's global cursor position
    currentCursorPos = QCursor::pos();
#endif

    // Only send update if cursor position has changed significantly
    if (qAbs(currentCursorPos.x() - m_lastCursorPos.x()) < 3 && 
        qAbs(currentCursorPos.y() - m_lastCursorPos.y()) < 3) {
        return;
    }

    // Debug cursor tracking
    if (++m_debugCounter % 20 == 0) { // Every 20th update (1 second at 20fps)
        qDebug() << "Server cursor:" << currentCursorPos << "-> sending to client";
        
        // Get screen info for debugging
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            qDebug() << "Server screen geometry:" << screen->geometry();
            qDebug() << "Server screen available geometry:" << screen->availableGeometry();
        }
    }

    m_lastCursorPos = currentCursorPos;

    // Create cursor update message
    CursorInfo cursorInfo;
    cursorInfo.x = currentCursorPos.x();
    cursorInfo.y = currentCursorPos.y();
    cursorInfo.visible = cursorVisible;
    cursorInfo.hotspotX = 0; // Default hotspot
    cursorInfo.hotspotY = 0;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << cursorInfo.x << cursorInfo.y << cursorInfo.visible 
           << cursorInfo.hotspotX << cursorInfo.hotspotY;

    Message msg;
    msg.type = MessageType::CursorUpdate;
    msg.data = data;
    msg.size = data.size();

    m_client->write(msg.serialize());
}

void RemoteServer::releaseAllPressedKeys()
{
#ifdef Q_OS_LINUX
    if (!m_display) {
        return;
    }

    if (m_pressedKeys.empty()) {
        qDebug() << "No pressed keys to release";
        return;
    }

    qDebug() << "Releasing" << m_pressedKeys.size() << "stuck keys...";

    // Release all currently pressed keys
    for (unsigned int keycode : m_pressedKeys) {
        qDebug() << "Releasing stuck key:" << keycode;
        XTestFakeKeyEvent(m_display, keycode, False, CurrentTime);
    }

    // Ensure all release events are processed
    XSync(m_display, False);

    // Clear the tracking set
    m_pressedKeys.clear();

    qDebug() << "All stuck keys released";
#endif
}