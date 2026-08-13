#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QScrollBar>
#include <QFrame>
#include <QPalette>
#include <QDateTime>
#include <QDebug>
#include "remoteserver.h"

class ServerWindow : public QWidget
{
    Q_OBJECT

public:
    ServerWindow(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_server(new RemoteServer(this))
        , m_portSpin(nullptr)
        , m_startStopBtn(nullptr)
        , m_statusLabel(nullptr)
        , m_clientLabel(nullptr)
        , m_logEdit(nullptr)
    {
        setupUI();
        connectSignals();
    }

private slots:
    void onStartStopClicked() {
        if (!m_server->serverRunning()) {
            quint16 port = static_cast<quint16>(m_portSpin->value());
            if (m_server->start(port)) {
                logMessage(QString("Server started on port %1").arg(port));
            } else {
                logMessage("Failed to start server");
            }
        } else {
            m_server->stop();
            logMessage("Server stopped");
        }
    }

    void onServerRunningChanged() {
        m_startStopBtn->setText(m_server->serverRunning() ? "Stop Server" : "Start Server");
        updateStatusStyle();
    }

    void onStatusChanged() {
        m_statusLabel->setText(m_server->status());
        updateStatusStyle();
    }

    void onClientConnected(const QString& clientAddress) {
        m_clientLabel->setText("Client: " + clientAddress);
        m_clientLabel->setStyleSheet("color: #00ff88;");
        logMessage(QString("Client connected from %1").arg(clientAddress));
    }

    void onClientDisconnected() {
        m_clientLabel->setText("Client: None");
        m_clientLabel->setStyleSheet("color: #ff4444;");
        logMessage("Client disconnected");
    }

private:
    void setupUI() {
        setWindowTitle("CyrusDesk Server");
        resize(500, 350);
        setMinimumSize(400, 300);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // -- Control panel --
        QHBoxLayout *controlLayout = new QHBoxLayout();

        m_portSpin = new QSpinBox();
        m_portSpin->setRange(1, 65535);
        m_portSpin->setValue(5555);
        m_portSpin->setFixedWidth(100);

        m_startStopBtn = new QPushButton("Start Server");
        m_startStopBtn->setFixedWidth(120);

        controlLayout->addWidget(new QLabel("Port:"));
        controlLayout->addWidget(m_portSpin);
        controlLayout->addWidget(m_startStopBtn);
        controlLayout->addStretch();

        mainLayout->addLayout(controlLayout);

        // -- Status section --
        QFrame *statusFrame = new QFrame();
        statusFrame->setFrameShape(QFrame::StyledPanel);
        statusFrame->setFrameShadow(QFrame::Sunken);

        QVBoxLayout *statusLayout = new QVBoxLayout(statusFrame);

        m_statusLabel = new QLabel("Status: Stopped");
        m_statusLabel->setStyleSheet("font-weight: bold;");

        m_clientLabel = new QLabel("Client: None");
        m_clientLabel->setStyleSheet("color: #ff4444;");

        statusLayout->addWidget(m_statusLabel);
        statusLayout->addWidget(m_clientLabel);

        mainLayout->addWidget(statusFrame);

        // -- Log area --
        mainLayout->addWidget(new QLabel("Event Log:"));

        m_logEdit = new QTextEdit();
        m_logEdit->setReadOnly(true);
        m_logEdit->setFont(QFont("monospace"));
        m_logEdit->document()->setMaximumBlockCount(500);
        m_logEdit->setPlaceholderText("Press 'Start Server' to begin...");

        mainLayout->addWidget(m_logEdit);
    }

    void connectSignals() {
        connect(m_startStopBtn, &QPushButton::clicked, this, &ServerWindow::onStartStopClicked);
        connect(m_server, &RemoteServer::serverRunningChanged, this, &ServerWindow::onServerRunningChanged);
        connect(m_server, &RemoteServer::statusChanged, this, &ServerWindow::onStatusChanged);
        connect(m_server, &RemoteServer::clientConnected, this, &ServerWindow::onClientConnected);
        connect(m_server, &RemoteServer::clientDisconnected, this, &ServerWindow::onClientDisconnected);
    }

    void updateStatusStyle() {
        bool running = m_server->serverRunning();
        QColor bgColor = running ? QColor(255, 255, 255, 20) : QColor(255, 0, 0, 20);
        QString style = QString("QLabel { background-color: %1; padding: 4px; border-radius: 4px; }")
                            .arg(bgColor.name(QColor::HexArgb));
        m_statusLabel->setStyleSheet(style + " font-weight: bold;");
    }

    void logMessage(const QString& message) {
        QString timestamp = QTime::currentTime().toString("hh:mm:ss");
        m_logEdit->append(QString("[%1] %2").arg(timestamp).arg(message));
        m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
    }

    RemoteServer *m_server;
    QSpinBox *m_portSpin;
    QPushButton *m_startStopBtn;
    QLabel *m_statusLabel;
    QLabel *m_clientLabel;
    QTextEdit *m_logEdit;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Check for CLI mode (--cli flag or no display)
    bool cliMode = false;
    int port = 5555;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--cli") {
            cliMode = true;
        } else if (arg == "--port" && i + 1 < argc) {
            bool ok;
            port = QString::fromLocal8Bit(argv[i + 1]).toInt(&ok);
            if (!ok || port <= 0 || port > 65535) {
                qWarning() << "Invalid port argument, using default port 5555";
                port = 5555;
            }
        }
    }

    if (cliMode) {
        // Backward-compatible CLI mode
        RemoteServer server;
        if (!server.start(static_cast<quint16>(port))) {
            qDebug() << "Failed to start server on port" << port;
            return -1;
        }
        qDebug() << "CyrusDesk server started on port" << port;
        return app.exec();
    }

    // GUI mode
    ServerWindow window;
    window.show();
    return app.exec();
}

#include "main.moc"
