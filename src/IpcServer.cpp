#include "IpcServer.h"

#include <QUdpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QDebug>

IpcServer::IpcServer(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead,
            this, &IpcServer::onReadyRead);
}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start(const QString &endpoint)
{
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

    if (!m_socket->bind(address, port)) {
        qWarning() << "IPC: Failed to bind UDP on" << endpoint
                   << m_socket->errorString();
        return false;
    }
    qDebug() << "IPC: UDP server listening on:" << endpoint;
    return true;
}

void IpcServer::stop()
{
    m_socket->close();
}

bool IpcServer::restart(const QString &endpoint)
{
    stop();
    return start(endpoint);
}

void IpcServer::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        if (!datagram.isEmpty()) {
            parseMessage(datagram.trimmed(), sender, senderPort);
        }
    }
}

void IpcServer::parseMessage(const QByteArray &data, const QHostAddress &sender, quint16 port)
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
        QJsonObject pong;
        pong["type"] = "pong";
        QJsonDocument pongDoc(pong);
        m_socket->writeDatagram(pongDoc.toJson(QJsonDocument::Compact) + "\n",
                                sender, port);
        emit pingReceived(sender, port);
    } else {
        qWarning() << "IPC: Unknown message type:" << type;
    }
}
