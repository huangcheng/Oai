#include "IpcServer.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QDebug>

IpcServer::IpcServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &IpcServer::onNewConnection);
}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start(const QString &endpoint)
{
    // Parse "host:port"
    int colonPos = endpoint.lastIndexOf(':');
    if (colonPos <= 0) {
        qWarning() << "IPC: Invalid endpoint format (expected host:port):" << endpoint;
        return false;
    }

    QString host = endpoint.left(colonPos);
    quint16 port = static_cast<quint16>(endpoint.mid(colonPos + 1).toUInt());

    QHostAddress address;
    if (!address.setAddress(host)) {
        qWarning() << "IPC: Invalid host address:" << host;
        return false;
    }

    if (!m_server->listen(address, port)) {
        qWarning() << "IPC: Failed to listen on" << endpoint
                   << m_server->errorString();
        return false;
    }
    qDebug() << "IPC: TCP server listening on:" << endpoint;
    return true;
}

void IpcServer::stop()
{
    // Close active connections first
    const auto sockets = m_buffers.keys();
    for (QTcpSocket *socket : sockets) {
        socket->close();
    }
    m_buffers.clear();
    m_server->close();
}

bool IpcServer::restart(const QString &endpoint)
{
    stop();
    return start(endpoint);
}

void IpcServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());

        connect(socket, &QTcpSocket::readyRead,
                this, &IpcServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &IpcServer::onDisconnected);

        qDebug() << "IPC client connected from" << socket->peerAddress().toString();
    }
}

void IpcServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
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
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    m_buffers.remove(socket);
    socket->deleteLater();

    qDebug() << "IPC client disconnected";
}

void IpcServer::parseMessage(const QByteArray &data, QTcpSocket *socket)
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
