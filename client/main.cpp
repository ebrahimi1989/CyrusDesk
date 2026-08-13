#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QTime>
#include <QDebug>
#include <QSettings>
#include <QUuid>
#include "remoteclient.h"
#include "remotescreenlabel.h"

class CursorLabel : public QLabel
{
    Q_OBJECT
public:
    CursorLabel(QWidget *parent = nullptr) : QLabel(parent) {}
};


class CyrusDeskWindow : public QWidget
{
    Q_OBJECT

public:
    CyrusDeskWindow(const QString &instanceId = QString(), QWidget *parent = nullptr)
        : QWidget(parent), m_instanceId(instanceId) {
        if (m_instanceId.isEmpty()) {
            m_instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                  "CyrusDesk", QString("CyrusDesk_%1").arg(m_instanceId), this);
        setupUI();
        connectSignals();
        loadSettings();
    }

    ~CyrusDeskWindow() {
        saveSettings();
    }

    QString getInstanceId() const {
        return m_instanceId;
    }

private slots:
    void onConnectClicked() {
        if (m_client->connected()) {
            m_client->disconnect();
        } else {
            QString host = m_hostEdit->text().isEmpty() ? "127.0.0.1" : m_hostEdit->text();
            int port = m_portSpin->value();
            m_client->connectToServer(host, port);
        }
    }

    void onConnectedChanged() {
        m_connectBtn->setText(m_client->connected() ? "Disconnect" : "Connect");
    }

     void onStatusChanged() {
        m_statusLabel->setText("Status: " + m_client->status());
    }

    void onPacketRateChanged() {
        m_packetRateLabel->setText(QString("Packets: %1 pkt/s").arg(m_client->packetRate(), 0, 'f', 0));
    }

    void onFrameReceived() {
        m_frameCount++;
        QPixmap pixmap = m_client->imageProvider()->requestPixmap("", nullptr, QSize());
        if (!pixmap.isNull()) {
            // Store the original pixmap without scaling - let the label handle scaling
            m_imageLabel->setPixmap(pixmap);
            m_imageLabel->setText(""); // Clear "Not connected" text
        } else {
            qDebug() << "Received null pixmap";
        }

        // Update FPS every second
        static QTime lastTime = QTime::currentTime();
        QTime currentTime = QTime::currentTime();
        int elapsed = lastTime.msecsTo(currentTime);
        if (elapsed >= 1000) {
            double fps = m_frameCount / (elapsed / 1000.0);
            m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
            m_frameCount = 0;
            lastTime = currentTime;
        }
    }

    void onRemoteCursorMoved(int x, int y) {
        // Handle remote cursor movement - placeholder implementation
        Q_UNUSED(x)
        Q_UNUSED(y)
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        // Trigger frame update when window is resized
        if (m_client && m_client->connected()) {
            onFrameReceived();
        }
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_F11) {
            if (isFullScreen()) {
                showNormal();
            } else {
                showFullScreen();
            }
        }
        QWidget::keyPressEvent(event);
    }

private:
    void loadSettings() {
        QString host = m_settings->value("connection/host", "127.0.0.1").toString();
        int port = m_settings->value("connection/port", 5555).toInt();
        
        if (m_hostEdit) {
            m_hostEdit->setText(host);
        }
        if (m_portSpin) {
            m_portSpin->setValue(port);
        }
        
        qDebug() << "Loaded settings for instance" << m_instanceId << "- Host:" << host << "Port:" << port;
    }
    
    void saveSettings() {
        if (m_hostEdit && m_portSpin) {
            m_settings->setValue("connection/host", m_hostEdit->text());
            m_settings->setValue("connection/port", m_portSpin->value());
            m_settings->sync();
            
            qDebug() << "Saved settings for instance" << m_instanceId 
                    << "- Host:" << m_hostEdit->text() 
                    << "Port:" << m_portSpin->value();
        }
    }

    void setupUI() {
        setWindowTitle(QString("CyrusDesk Client - Instance: %1").arg(m_instanceId.left(8)));
        resize(1024, 768);
        setMinimumSize(400, 300); // Allow much smaller window
        
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        
        // Control panel
        QHBoxLayout *controlLayout = new QHBoxLayout();
        
        m_hostEdit = new QLineEdit("127.0.0.1");
        m_hostEdit->setPlaceholderText("Server IP");
        m_hostEdit->setFixedWidth(150);
        
        m_portSpin = new QSpinBox();
        m_portSpin->setRange(1, 65535);
        m_portSpin->setValue(5555);
        m_portSpin->setFixedWidth(80);
        
        m_connectBtn = new QPushButton("Connect");
        m_connectBtn->setFixedWidth(100);
        
        m_statusLabel = new QLabel("Status: Disconnected");
        
        controlLayout->addWidget(new QLabel("Host:"));
        controlLayout->addWidget(m_hostEdit);
        controlLayout->addWidget(new QLabel("Port:"));
        controlLayout->addWidget(m_portSpin);
        controlLayout->addWidget(m_connectBtn);
        controlLayout->addWidget(m_statusLabel);
        controlLayout->addStretch();
        
        mainLayout->addLayout(controlLayout);
        
        // Screen area - with remote control capabilities
        m_imageLabel = new RemoteScreenLabel();
        m_imageLabel->setAlignment(Qt::AlignCenter);
        m_imageLabel->setText("Not connected - CyrusDesk آماده نیست");
        m_imageLabel->setStyleSheet("background-color: #2c2c2c; color: #888; border: 1px solid #555;");
        m_imageLabel->setMinimumSize(320, 240); // Minimal size - allow very small windows
        m_imageLabel->setScaledContents(true); // Enable scaling to fit aspect ratio
        m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_imageLabel->setFocusPolicy(Qt::StrongFocus);

        mainLayout->addWidget(m_imageLabel);
        
        // Status bar
        QHBoxLayout *statusLayout = new QHBoxLayout();
        m_fpsLabel = new QLabel("FPS: 0.0");
        m_packetRateLabel = new QLabel("Packets: 0 pkt/s");
        statusLayout->addWidget(m_fpsLabel);
        statusLayout->addWidget(m_packetRateLabel);
        statusLayout->addStretch();
        statusLayout->addWidget(new QLabel("Press F11 for fullscreen - فشار دهید F11 برای تمام صفحه"));
        
        mainLayout->addLayout(statusLayout);
    }
    
    void connectSignals() {
        m_client = new RemoteClient(this);
        
        // Connect remote client to screen label for input handling
        m_imageLabel->setRemoteClient(m_client);
        
        connect(m_connectBtn, &QPushButton::clicked, this, &CyrusDeskWindow::onConnectClicked);
        connect(m_client, &RemoteClient::connectedChanged, this, &CyrusDeskWindow::onConnectedChanged);
        connect(m_client, &RemoteClient::statusChanged, this, &CyrusDeskWindow::onStatusChanged);
        connect(m_client, &RemoteClient::frameReceived, this, &CyrusDeskWindow::onFrameReceived);
        connect(m_client, &RemoteClient::cursorUpdated, m_imageLabel, &RemoteScreenLabel::updateRemoteCursor);
        connect(m_client, &RemoteClient::packetRateChanged, this, &CyrusDeskWindow::onPacketRateChanged);
        
        // Auto-save settings when they change
        connect(m_hostEdit, &QLineEdit::textChanged, this, &CyrusDeskWindow::saveSettings);
        connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CyrusDeskWindow::saveSettings);
    }

    RemoteClient *m_client;
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_connectBtn;
    QLabel *m_statusLabel;
    RemoteScreenLabel *m_imageLabel;
    QLabel *m_fpsLabel;
    QLabel *m_packetRateLabel;
    int m_frameCount = 0;
    
    QString m_instanceId;
    QSettings *m_settings;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Get instance ID from command line or generate unique one
    QString instanceId;
    if (argc > 1) {
        instanceId = QString::fromLocal8Bit(argv[1]);
    }
    
    CyrusDeskWindow window(instanceId);
    window.show();
    
    qDebug() << "CyrusDesk client started with instance ID:" << window.getInstanceId();
    
    return app.exec();
}

#include "main.moc"