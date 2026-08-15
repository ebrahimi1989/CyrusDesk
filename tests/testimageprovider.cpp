#include "testimageprovider.h"
#include "../client/remoteclient.h"

void TestImageProvider::initTestCase()
{
}

void TestImageProvider::cleanupTestCase()
{
}

void TestImageProvider::testSetPixmap()
{
    ImageProvider provider;
    QImage img(640, 480, QImage::Format_RGB32);
    img.fill(Qt::red);
    QPixmap pixmap = QPixmap::fromImage(img);

    provider.setPixmap(pixmap);

    QSize size;
    QPixmap result = provider.requestPixmap("test", &size, QSize());
    QCOMPARE(size, QSize(640, 480));
    QCOMPARE(result.size(), QSize(640, 480));
}

void TestImageProvider::testRequestPixmapWhenEmpty()
{
    ImageProvider provider;
    QSize size;
    QPixmap result = provider.requestPixmap("test", &size, QSize());
    QVERIFY(result.isNull());
}

void TestImageProvider::testRequestPixmapAfterSet()
{
    ImageProvider provider;
    QImage img(320, 240, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QPixmap pixmap = QPixmap::fromImage(img);

    provider.setPixmap(pixmap);

    QSize size;
    QPixmap result = provider.requestPixmap("test", &size, QSize());
    QCOMPARE(result.width(), 320);
    QCOMPARE(result.height(), 240);
    QCOMPARE(result.toImage().pixel(160, 120), QColor(Qt::blue).rgb());
}

void TestImageProvider::testSetPixmapOverwrite()
{
    ImageProvider provider;

    QImage img1(100, 100, QImage::Format_RGB32);
    img1.fill(Qt::red);
    provider.setPixmap(QPixmap::fromImage(img1));

    QImage img2(200, 200, QImage::Format_RGB32);
    img2.fill(Qt::green);
    provider.setPixmap(QPixmap::fromImage(img2));

    QSize size;
    QPixmap result = provider.requestPixmap("test", &size, QSize());
    QCOMPARE(result.width(), 200);
    QCOMPARE(result.height(), 200);
}
