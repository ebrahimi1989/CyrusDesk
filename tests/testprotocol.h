#ifndef TESTPROTOCOL_H
#define TESTPROTOCOL_H

#include <QObject>
#include <QtTest/QtTest>

class TestProtocol : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSerializeDeserializeScreenData();
    void testSerializeDeserializeMouseMove();
    void testSerializeDeserializeMouseClick();
    void testSerializeDeserializeKeyEvent();
    void testSerializeDeserializePing();
    void testSerializeDeserializePong();
    void testSerializeDeserializeVideoData();
    void testSerializeDeserializeCodecInfo();
    void testSerializeDeserializeCursorUpdate();

    void testDeserializeInvalidType();
    void testSerializeEmptyData();
    void testDeserializeEmptyData();
    void testDeserializePartialData();
};

#endif // TESTPROTOCOL_H
