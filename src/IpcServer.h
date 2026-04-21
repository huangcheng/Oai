#ifndef IPCSERVER_H
#define IPCSERVER_H

#include <QObject>
#include <QJsonDocument>
#include <QMap>

class QLocalServer;
class QLocalSocket;

class IpcServer : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject *parent = nullptr);
    ~IpcServer() override;

    /**
     * Start listening on the given endpoint.
     *
     * Linux/macOS — endpoint is a Unix domain socket path
     *   (e.g. "~/.clippy/clippy.sock").
     *   Stale socket files are cleaned up automatically.
     *
     * Windows — endpoint is a named pipe path
     *   (e.g. "\\.\pipe\clippy").
     *   QLocalServer creates the pipe directly; no file is written.
     *
     * @param endpoint     Platform IPC path
     * @param isNamedPipe  True when endpoint is a Windows named pipe
     */
    bool start(const QString &endpoint, bool isNamedPipe = false);
    void stop();

signals:
    void eventReceived(const QJsonObject &event);
    void tipReceived(const QJsonObject &tip);
    void pingReceived(QLocalSocket *socket);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void parseMessage(const QByteArray &data, QLocalSocket *socket);

    QLocalServer *m_server = nullptr;
    QMap<QLocalSocket*, QByteArray> m_buffers;
};

#endif // IPCSERVER_H
