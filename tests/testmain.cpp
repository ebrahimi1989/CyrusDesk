#include <QtTest/QtTest>
#include <QApplication>
#include "testprotocol.h"
#include "testlatencymonitor.h"
#include "testhwdecoder.h"
#include "testhwencoder.h"
#include "testimageprovider.h"
#include "testremoteclient.h"
#include "testscreenlabel.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("CyrusDesk Tests");

    int result = 0;

    {
        TestProtocol t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestLatencyMonitor t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestHWDecoder t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestHWEncoder t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestImageProvider t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestRemoteClient t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestScreenLabel t;
        result |= QTest::qExec(&t, argc, argv);
    }

    return result;
}
