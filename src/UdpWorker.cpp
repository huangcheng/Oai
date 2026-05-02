#include "UdpWorker.h"

#include <QUdpSocket>
#include <QHostAddress>
#include <QDebug>

UdpWorker::UdpWorker(QObject *parent)
    : QObject(parent)
{
}

UdpWorker::~UdpWorker()
{
    stop();
}

void UdpWorker::start(const QString &endpoint)
{
    if (m_socket) {
        emit errorOccurred("UdpWorker already started");
        return;
    }

    int colonPos = endpoint.lastIndexOf(':');
    if (colonPos <= 0) {
        emit errorOccurred("Invalid endpoint format (expected host:port): " + endpoint);
        return;
    }

    QString host = endpoint.left(colonPos);
    bool portOk = false;
    quint16 port = endpoint.mid(colonPos + 1).toUShort(&portOk);
    if (!portOk || port == 0) {
        emit errorOccurred("Invalid port (must be 1-65535): " + endpoint.mid(colonPos + 1));
        return;
    }

    QHostAddress address;
    if (!address.setAddress(host)) {
        emit errorOccurred("Invalid host address: " + host);
        return;
    }

    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpWorker::onReadyRead);

    if (!m_socket->bind(address, port)) {
        emit errorOccurred("Failed to bind UDP on " + endpoint + ": " + m_socket->errorString());
        delete m_socket;
        m_socket = nullptr;
        return;
    }

    qDebug() << "IPC: UDP worker listening on:" << endpoint;
    emit started();
}

void UdpWorker::stop()
{
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
        emit stopped();
    }
}

void UdpWorker::sendDatagram(const QByteArray &data, const QHostAddress &host, quint16 port)
{
    if (m_socket) {
        m_socket->writeDatagram(data, host, port);
    }
}

void UdpWorker::onReadyRead()
{
    if (!m_socket) return;

    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        if (!datagram.isEmpty()) {
            emit datagramReceived(datagram.trimmed(), sender, senderPort);
        }
    }
}
