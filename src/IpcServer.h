#ifndef IPCSERVER_H
#define IPCSERVER_H

#include <QObject>
#include <QJsonDocument>
#include <QMap>

class QTcpServer;
class QTcpSocket;

class IpcServer : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject *parent = nullptr);
    ~IpcServer() override;

    /**
     * Start listening on the given TCP endpoint.
     *
     * @param endpoint  "host:port" string, e.g. "127.0.0.1:52847"
     */
    bool start(const QString &endpoint);
    void stop();
    bool restart(const QString &endpoint);

signals:
    void eventReceived(const QJsonObject &event);
    void tipReceived(const QJsonObject &tip);
    void pingReceived(QTcpSocket *socket);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void parseMessage(const QByteArray &data, QTcpSocket *socket);

    QTcpServer *m_server = nullptr;
    QMap<QTcpSocket*, QByteArray> m_buffers;
};

#endif // IPCSERVER_H
