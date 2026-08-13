#ifndef REMOTESCREENLABEL_H
#define REMOTESCREENLABEL_H

#include <QLabel>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPoint>
#include "remoteclient.h"

class RemoteScreenLabel : public QLabel
{
    Q_OBJECT
public:
    RemoteScreenLabel(QWidget *parent = nullptr);
    void setRemoteClient(RemoteClient* client);

public slots:
    void updateRemoteCursor(int x, int y, bool visible);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint getScaledPosition(const QPoint& pos);
    QPoint getDisplayPosition(const QPoint& remotePos);
    
    RemoteClient* m_client;
    QPoint m_remoteCursorPos;
    bool m_remoteCursorVisible;
};

#endif // REMOTESCREENLABEL_H