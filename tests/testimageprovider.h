#ifndef TESTIMAGEPROVIDER_H
#define TESTIMAGEPROVIDER_H

#include <QObject>
#include <QtTest/QtTest>

class TestImageProvider : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSetPixmap();
    void testRequestPixmapWhenEmpty();
    void testRequestPixmapAfterSet();
    void testSetPixmapOverwrite();
};

#endif // TESTIMAGEPROVIDER_H
