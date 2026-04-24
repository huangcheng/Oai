#ifndef IPCSERVER_H
#define IPCSERVER_H

#include <QObject>
#include <QJsonDocument>
#include <QHostAddress>

class QThread;
class UdpWorker;

class IpcServer : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject *parent = nullptr);
    ~IpcServer() override;

    bool start(const QString &endpoint);
    void stop();
    bool restart(const QString &endpoint);

signals:
    void eventReceived(const QJsonObject &event);
    void tipReceived(const QJsonObject &tip);
    void pingReceived(const QHostAddress &sender, quint16 port);

private slots:
    void onDatagramReceived(const QByteArray &data, const QHostAddress &sender, quint16 port);
    void onWorkerError(const QString &message);

private:
    void parseMessage(const QByteArray &data, const QHostAddress &sender, quint16 port);

    QThread *m_thread = nullptr;
    UdpWorker *m_worker = nullptr;
};

#endif // IPCSERVER_H
