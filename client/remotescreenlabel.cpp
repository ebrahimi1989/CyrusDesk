#include "remotescreenlabel.h"
#include <QDebug>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QPolygon>

RemoteScreenLabel::RemoteScreenLabel(QWidget *parent) 
    : QLabel(parent), m_client(nullptr), m_remoteCursorPos(-1, -1), m_remoteCursorVisible(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    
    // Make sure we can accept key events and focus properly
    setFocus(); // Try to get focus initially
}

void RemoteScreenLabel::setRemoteClient(RemoteClient* client) 
{
    m_client = client;
}

void RemoteScreenLabel::mousePressEvent(QMouseEvent *event) 
{
    // Ensure we have focus for keyboard events
    setFocus();
    
    if (m_client && m_client->connected()) {
        QPoint scaledPos = getScaledPosition(event->pos());
        // With standard RDP-style mapping, all positions are valid
        m_client->sendMouseClick(scaledPos.x(), scaledPos.y(), 
                               event->button() == Qt::LeftButton ? 0 :
                               event->button() == Qt::RightButton ? 1 : 2, true);
    }
    QLabel::mousePressEvent(event);
}

void RemoteScreenLabel::mouseReleaseEvent(QMouseEvent *event) 
{
    if (m_client && m_client->connected()) {
        QPoint scaledPos = getScaledPosition(event->pos());
        // With standard RDP-style mapping, all positions are valid
        m_client->sendMouseClick(scaledPos.x(), scaledPos.y(), 
                               event->button() == Qt::LeftButton ? 0 :
                               event->button() == Qt::RightButton ? 1 : 2, false);
    }
    QLabel::mouseReleaseEvent(event);
}

void RemoteScreenLabel::mouseMoveEvent(QMouseEvent *event) 
{
    if (m_client && m_client->connected()) {
        QPoint scaledPos = getScaledPosition(event->pos());
        // With standard RDP-style mapping, all positions are valid
        m_client->sendMouseMove(scaledPos.x(), scaledPos.y());
        // Hide system cursor when connected to show only remote cursor
        setCursor(Qt::BlankCursor);
    }
    QLabel::mouseMoveEvent(event);
}

void RemoteScreenLabel::keyPressEvent(QKeyEvent *event)
{
    if (m_client && m_client->connected()) {
        // Prevent auto-repeat from sending duplicate press events
        if (event->isAutoRepeat()) {
            event->accept();
            return;
        }

        int qtKey = event->key();
        int modifiers = event->modifiers();

        // Debug to see which keys we're receiving
        qDebug() << "Key pressed:" << qtKey << "text:" << event->text() << "modifiers:" << modifiers;

        // Send key event to remote server
        m_client->sendKeyPress(qtKey, true, modifiers);

        // CRITICAL: Accept the event to prevent propagation to parent widgets
        // This prevents the key event from being processed multiple times
        event->accept();
        return;
    }

    // If not connected, pass to parent
    QLabel::keyPressEvent(event);
}

void RemoteScreenLabel::keyReleaseEvent(QKeyEvent *event)
{
    if (m_client && m_client->connected()) {
        // Prevent auto-repeat from sending duplicate release events
        if (event->isAutoRepeat()) {
            event->accept();
            return;
        }

        int qtKey = event->key();
        int modifiers = event->modifiers();

        qDebug() << "Key released:" << qtKey << "modifiers:" << modifiers;

        // Send key release event to remote server
        m_client->sendKeyPress(qtKey, false, modifiers);

        // CRITICAL: Accept the event to prevent propagation to parent widgets
        event->accept();
        return;
    }

    // If not connected, pass to parent
    QLabel::keyReleaseEvent(event);
}

QPoint RemoteScreenLabel::getScaledPosition(const QPoint& pos) 
{
    // Qt5 returns const QPixmap*, Qt6 returns QPixmap by value
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (pixmap().isNull()) {
#else
    if (!pixmap() || pixmap()->isNull()) {
#endif
        return QPoint(-1, -1);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QSize remoteScreenSize = pixmap().size();
#else
    QSize remoteScreenSize = pixmap()->size();
#endif
    QSize clientWidgetSize = size(); // Client widget size
    
    if (remoteScreenSize.isEmpty() || clientWidgetSize.isEmpty()) {
        return QPoint(-1, -1);
    }
    
    // Standard remote desktop coordinate transformation:
    // Simple linear scaling without aspect ratio consideration
    // This is how RDP, VNC, and most remote desktop solutions work
    
    double scaleX = static_cast<double>(remoteScreenSize.width()) / clientWidgetSize.width();
    double scaleY = static_cast<double>(remoteScreenSize.height()) / clientWidgetSize.height();
    
    int remoteX = qRound(pos.x() * scaleX);
    int remoteY = qRound(pos.y() * scaleY);
    
    // Ensure coordinates are within remote screen bounds
    remoteX = qBound(0, remoteX, remoteScreenSize.width() - 1);
    remoteY = qBound(0, remoteY, remoteScreenSize.height() - 1);
    
    qDebug() << "Standard RDP-style coordinate mapping:";
    qDebug() << "  Client click:" << pos;
    qDebug() << "  Client size:" << clientWidgetSize;
    qDebug() << "  Remote size:" << remoteScreenSize;
    qDebug() << "  Scale factors:" << scaleX << "x" << scaleY;
    qDebug() << "  Remote coords:" << QPoint(remoteX, remoteY);
    
    return QPoint(remoteX, remoteY);
}

void RemoteScreenLabel::updateRemoteCursor(int x, int y, bool visible)
{
    QPoint oldPos = m_remoteCursorPos;
    m_remoteCursorPos = QPoint(x, y);
    m_remoteCursorVisible = visible;
    
    // Debug cursor updates
    if (oldPos != m_remoteCursorPos || !visible) {
        qDebug() << "Remote cursor updated:" << m_remoteCursorPos << "visible:" << visible;
    }
    
    update(); // Trigger repaint to show/hide cursor
}

QPoint RemoteScreenLabel::getDisplayPosition(const QPoint& remotePos) 
{
    // Qt5 returns const QPixmap*, Qt6 returns QPixmap by value
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (pixmap().isNull() || remotePos.x() < 0) {
#else
    if (!pixmap() || pixmap()->isNull() || remotePos.x() < 0) {
#endif
        return QPoint(-1, -1);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QSize remoteScreenSize = pixmap().size();
#else
    QSize remoteScreenSize = pixmap()->size();
#endif
    QSize clientWidgetSize = size(); // Client widget size
    
    if (remoteScreenSize.isEmpty() || clientWidgetSize.isEmpty()) {
        return QPoint(-1, -1);
    }
    
    // Standard remote desktop coordinate transformation (reverse):
    // Convert remote screen coordinates back to client widget coordinates
    
    double scaleX = static_cast<double>(clientWidgetSize.width()) / remoteScreenSize.width();
    double scaleY = static_cast<double>(clientWidgetSize.height()) / remoteScreenSize.height();
    
    int clientX = qRound(remotePos.x() * scaleX);
    int clientY = qRound(remotePos.y() * scaleY);
    
    // Ensure coordinates are within client widget bounds
    clientX = qBound(0, clientX, clientWidgetSize.width() - 1);
    clientY = qBound(0, clientY, clientWidgetSize.height() - 1);
    
    return QPoint(clientX, clientY);
}

void RemoteScreenLabel::paintEvent(QPaintEvent *event)
{
    // First paint the normal label content
    QLabel::paintEvent(event);
    
    // Then paint the remote cursor if visible
    if (m_remoteCursorVisible && m_remoteCursorPos.x() >= 0) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QPoint displayPos = getDisplayPosition(m_remoteCursorPos);
        if (displayPos.x() >= 0 && displayPos.y() >= 0) {
            // Draw exactly like real system cursor with perfect pixel accuracy
            painter.setRenderHint(QPainter::Antialiasing, false); // Disable for pixel-perfect rendering
            
            int hotX = displayPos.x();
            int hotY = displayPos.y();
            
            // Standard system cursor shape - exact replica
            QPolygon cursorArrow;
            cursorArrow << QPoint(hotX, hotY)           // 0: tip
                       << QPoint(hotX, hotY + 15)      // 1: bottom of main shaft
                       << QPoint(hotX + 4, hotY + 12)  // 2: inner left of bottom
                       << QPoint(hotX + 7, hotY + 12)  // 3: inner right of bottom  
                       << QPoint(hotX + 11, hotY + 16) // 4: right wing tip
                       << QPoint(hotX + 12, hotY + 15) // 5: right wing outer
                       << QPoint(hotX + 8, hotY + 11)  // 6: right wing inner
                       << QPoint(hotX + 8, hotY + 8)   // 7: right side of shaft
                       << QPoint(hotX, hotY);          // 8: back to tip
            
            // Draw black outline/shadow (1px wider)
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(Qt::black);
            QPolygon shadowCursor = cursorArrow.translated(1, 1);
            painter.drawPolygon(shadowCursor);
            
            // Draw main white cursor
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(Qt::white);
            painter.drawPolygon(cursorArrow);
            
            // Add thin black border inside white area for definition
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawPolygon(cursorArrow);
            
            // Add subtle highlight on left edge (like real cursor)
            painter.setPen(QPen(QColor(250, 250, 250), 1));
            painter.drawLine(hotX + 1, hotY + 1, hotX + 1, hotY + 7);
            painter.drawLine(hotX + 2, hotY + 2, hotX + 2, hotY + 6);
        }
    }
}