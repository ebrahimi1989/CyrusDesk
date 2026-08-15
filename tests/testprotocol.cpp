#include "testprotocol.h"
#include "../common/protocol.h"

void TestProtocol::initTestCase()
{
}

void TestProtocol::cleanupTestCase()
{
}

void TestProtocol::testSerializeDeserializeScreenData()
{
    QByteArray payload = "fake_jpeg_data";
    Message msg;
    msg.type = MessageType::ScreenData;
    msg.data = payload;
    msg.size = payload.size();

    QByteArray serialized = msg.serialize();
    QCOMPARE(serialized.size(), (int)(sizeof(quint8) + sizeof(quint32) + payload.size()));

    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::ScreenData);
    QCOMPARE(deserialized.size, (quint32)payload.size());
    QCOMPARE(deserialized.data, payload);
}

void TestProtocol::testSerializeDeserializeMouseMove()
{
    MouseEvent event;
    event.x = 100;
    event.y = 200;
    event.button = -1;
    event.pressed = false;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << event.x << event.y << event.button << event.pressed;

    Message msg;
    msg.type = MessageType::MouseMove;
    msg.data = data;
    msg.size = data.size();

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::MouseMove);
    QCOMPARE(deserialized.size, (quint32)data.size());

    QDataStream ds(deserialized.data);
    int x, y, button;
    bool pressed;
    ds >> x >> y >> button >> pressed;
    QCOMPARE(x, 100);
    QCOMPARE(y, 200);
    QCOMPARE(button, -1);
    QCOMPARE(pressed, false);
}

void TestProtocol::testSerializeDeserializeMouseClick()
{
    MouseEvent event;
    event.x = 50;
    event.y = 75;
    event.button = 1;
    event.pressed = true;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << event.x << event.y << event.button << event.pressed;

    Message msg;
    msg.type = MessageType::MouseClick;
    msg.data = data;
    msg.size = data.size();

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::MouseClick);

    QDataStream ds(deserialized.data);
    int x, y, button;
    bool pressed;
    ds >> x >> y >> button >> pressed;
    QCOMPARE(x, 50);
    QCOMPARE(y, 75);
    QCOMPARE(button, 1);
    QCOMPARE(pressed, true);
}

void TestProtocol::testSerializeDeserializeKeyEvent()
{
    KeyEvent event;
    event.key = 65; // 'A'
    event.pressed = true;
    event.modifiers = 0;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << event.key << event.pressed << event.modifiers;

    Message msg;
    msg.type = MessageType::KeyEvent;
    msg.data = data;
    msg.size = data.size();

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::KeyEvent);

    QDataStream ds(deserialized.data);
    int key, modifiers;
    bool pressed;
    ds >> key >> pressed >> modifiers;
    QCOMPARE(key, 65);
    QCOMPARE(pressed, true);
    QCOMPARE(modifiers, 0);
}

void TestProtocol::testSerializeDeserializePing()
{
    Message msg;
    msg.type = MessageType::Ping;
    msg.size = 0;

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::Ping);
    QCOMPARE(deserialized.size, (quint32)0);
    QVERIFY(deserialized.data.isEmpty());
}

void TestProtocol::testSerializeDeserializePong()
{
    Message msg;
    msg.type = MessageType::Pong;
    msg.size = 0;

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::Pong);
    QCOMPARE(deserialized.size, (quint32)0);
}

void TestProtocol::testSerializeDeserializeVideoData()
{
    QByteArray payload(1024, 0xAB);
    Message msg;
    msg.type = MessageType::VideoData;
    msg.data = payload;
    msg.size = payload.size();

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::VideoData);
    QCOMPARE(deserialized.size, (quint32)payload.size());
    QCOMPARE(deserialized.data, payload);
}

void TestProtocol::testSerializeDeserializeCodecInfo()
{
    QByteArray spsPps;
    spsPps.append(static_cast<char>(0x01));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    spsPps.append(static_cast<char>(0x00));
    Message msg;
    msg.type = MessageType::CodecInfo;
    msg.data = spsPps;
    msg.size = spsPps.size();

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::CodecInfo);
    QCOMPARE(deserialized.size, (quint32)spsPps.size());
    QCOMPARE(deserialized.data, spsPps);
}

void TestProtocol::testSerializeDeserializeCursorUpdate()
{
    int x = 300, y = 400;
    int hotspotX = 5, hotspotY = 10;
    bool visible = true;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << x << y << visible << hotspotX << hotspotY;

    Message msg;
    msg.type = MessageType::CursorUpdate;
    msg.data = data;
    msg.size = data.size();

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::CursorUpdate);

    QDataStream ds(deserialized.data);
    int rx, ry, hx, hy;
    bool vis;
    ds >> rx >> ry >> vis >> hx >> hy;
    QCOMPARE(rx, 300);
    QCOMPARE(ry, 400);
    QCOMPARE(vis, true);
    QCOMPARE(hx, 5);
    QCOMPARE(hy, 10);
}

void TestProtocol::testDeserializeInvalidType()
{
    quint8 typeValue = static_cast<quint8>(MessageType::CursorUpdate) + 10;
    quint32 size = 0;

    QByteArray raw;
    QDataStream stream(&raw, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << typeValue << size;

    Message deserialized = Message::deserialize(raw);
    QCOMPARE(static_cast<int>(deserialized.type), static_cast<int>(typeValue));
    QCOMPARE(deserialized.size, (quint32)0);
}

void TestProtocol::testSerializeEmptyData()
{
    Message msg;
    msg.type = MessageType::Ping;
    msg.data = QByteArray();
    msg.size = 0;

    QByteArray serialized = msg.serialize();
    QCOMPARE(serialized.size(), (int)(sizeof(quint8) + sizeof(quint32)));

    Message deserialized = Message::deserialize(serialized);
    QVERIFY(deserialized.data.isEmpty());
}

void TestProtocol::testDeserializeEmptyData()
{
    Message msg;
    msg.type = MessageType::Pong;
    msg.size = 0;

    QByteArray serialized = msg.serialize();
    Message deserialized = Message::deserialize(serialized);
    QCOMPARE(deserialized.type, MessageType::Pong);
    QCOMPARE(deserialized.size, (quint32)0);
    QVERIFY(deserialized.data.isEmpty());
}

void TestProtocol::testDeserializePartialData()
{
    quint8 typeValue = static_cast<quint8>(MessageType::VideoData);
    quint32 size = 100;

    QByteArray raw;
    QDataStream stream(&raw, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << typeValue << size;

    Message deserialized = Message::deserialize(raw);
    QCOMPARE(deserialized.type, MessageType::VideoData);
    QCOMPARE(deserialized.size, (quint32)100);
    QVERIFY(deserialized.data.isEmpty());
}
