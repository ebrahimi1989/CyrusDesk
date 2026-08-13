#include <QApplication>
#include <QDebug>
#include "remoteserver.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Get port from command line argument, default to 5555
    int port = 5555;
    if (argc > 1) {
        bool ok;
        int argPort = QString::fromLocal8Bit(argv[1]).toInt(&ok);
        if (ok && argPort > 0 && argPort < 65536) {
            port = argPort;
        } else {
            qWarning() << "Invalid port argument, using default port 5555";
        }
    }
    
    RemoteServer server;
    if (!server.start(port)) {
        qDebug() << "Failed to start server on port" << port;
        return -1;
    }
    
    qDebug() << "CyrusDesk server started on port" << port;
    
    return app.exec();
}