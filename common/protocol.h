#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QDataStream>

enum class MessageType : quint8 {
    ScreenData = 0x01,    // Legacy JPEG (deprecated)
    MouseMove = 0x02,
    MouseClick = 0x03,
    KeyEvent = 0x04,
    Ping = 0x05,
    Pong = 0x06,
    VideoData = 0x07,     // H.264 encoded video packet
    CodecInfo = 0x08,     // Codec configuration (SPS/PPS)
    CursorUpdate = 0x09   // Remote cursor position update
};

struct Message {
    MessageType type;
    quint32 size;
    QByteArray data;
    
    QByteArray serialize() const {
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<quint8>(type) << size;
        result.append(data);
        return result;
    }
    
    static Message deserialize(const QByteArray& bytes) {
        Message msg;
        QDataStream stream(bytes);
        stream.setByteOrder(QDataStream::BigEndian);
        quint8 typeValue;
        stream >> typeValue >> msg.size;
        msg.type = static_cast<MessageType>(typeValue);
        msg.data = bytes.mid(sizeof(quint8) + sizeof(quint32));
        return msg;
    }
};

struct MouseEvent {
    int x, y;
    int button;
    bool pressed;
};

struct KeyEvent {
    int key;
    bool pressed;
    int modifiers;
};

struct CursorInfo {
    int x, y;           // Cursor position
    bool visible;       // Is cursor visible
    int hotspotX, hotspotY; // Cursor hotspot
};

#endif // PROTOCOL_H