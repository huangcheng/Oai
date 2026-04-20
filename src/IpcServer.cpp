#include "IpcServer.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QDebug>

IpcServer::IpcServer(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection,
            this, &IpcServer::onNewConnection);
}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start(const QString &endpoint, bool isNamedPipe)
{
    if (isNamedPipe) {
        // ── Windows named pipe ──────────────────────────────────────────
        // QLocalServer::listen() accepts the pipe name directly.
        // Pass just the pipe name (without the \\.\pipe\ prefix) or the
        // full path — Qt normalises it internally.
        if (!m_server->listen(endpoint)) {
            qWarning() << "IPC: Failed to listen on named pipe:" << endpoint
                        << m_server->errorString();
            return false;
        }
        qDebug() << "IPC: Named pipe listening on:" << endpoint;
    } else {
        // ── Unix domain socket (Linux / macOS) ─────────────────────────
        // Remove any stale socket file left by a previous crash.
        QFile::remove(endpoint);

        // Ensure parent directory exists.
        const QString dir = QFileInfo(endpoint).absolutePath();
        QDir().mkpath(dir);

        if (!m_server->listen(endpoint)) {
            qWarning() << "IPC: Failed to listen on Unix socket:" << endpoint
                        << m_server->errorString();
            return false;
        }
        qDebug() << "IPC: Unix socket listening on:" << endpoint;
    }

    return true;
}

void IpcServer::stop()
{
    m_server->close();
    m_buffers.clear();
}

void IpcServer::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());

        connect(socket, &QLocalSocket::readyRead,
                this, &IpcServer::onReadyRead);
        connect(socket, &QLocalSocket::disconnected,
                this, &IpcServer::onDisconnected);

        qDebug() << "IPC client connected";
    }
}

void IpcServer::onReadyRead()
{
    QLocalSocket *socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    QByteArray &buffer = m_buffers[socket];
    buffer.append(socket->readAll());

    // Parse newline-delimited JSON messages
    int newlinePos;
    while ((newlinePos = buffer.indexOf('\n')) != -1) {
        QByteArray line = buffer.left(newlinePos).trimmed();
        buffer = buffer.mid(newlinePos + 1);

        if (!line.isEmpty()) {
            parseMessage(line, socket);
        }
    }
}

void IpcServer::onDisconnected()
{
    QLocalSocket *socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    m_buffers.remove(socket);
    socket->deleteLater();

    qDebug() << "IPC client disconnected";
}

void IpcServer::parseMessage(const QByteArray &data, QLocalSocket *socket)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "IPC: Malformed JSON:" << error.errorString()
                    << "data:" << data.left(200);
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "IPC: Expected JSON object, got:" << data.left(200);
        return;
    }

    QJsonObject msg = doc.object();
    const QString type = msg.value("type").toString();

    if (type == "event") {
        emit eventReceived(msg);
    } else if (type == "tip") {
        emit tipReceived(msg);
    } else if (type == "ping") {
        // Respond with pong
        QJsonObject pong;
        pong["type"] = "pong";
        QJsonDocument pongDoc(pong);
        socket->write(pongDoc.toJson(QJsonDocument::Compact) + "\n");
        socket->flush();
        emit pingReceived(socket);
    } else {
        qWarning() << "IPC: Unknown message type:" << type;
    }
}
