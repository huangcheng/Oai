#include "IpcServer.h"
#include "UdpWorker.h"

#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

IpcServer::IpcServer(QObject *parent)
    : QObject(parent)
{
}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start(const QString &endpoint)
{
    if (m_thread) {
        qWarning() << "IPC: Server already running";
        return false;
    }

    m_thread = new QThread(this);
    m_worker = new UdpWorker();
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, [this, endpoint]() {
        m_worker->start(endpoint);
    });

    connect(m_worker, &UdpWorker::started, this, [endpoint]() {
        qDebug() << "IPC: UDP server listening on:" << endpoint;
    });

    connect(m_worker, &UdpWorker::stopped, this, []() {
        qDebug() << "IPC: UDP worker stopped";
    });

    connect(m_worker, &UdpWorker::errorOccurred, this, &IpcServer::onWorkerError);

    connect(m_worker, &UdpWorker::datagramReceived, this, &IpcServer::onDatagramReceived);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start();
    return true;
}

void IpcServer::stop()
{
    if (m_thread) {
        // Invoke stop() on the worker's thread to avoid touching the socket
        // from the main thread (QSocketNotifier is thread-affine).
        QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
        m_thread->quit();
        // Bound the wait so a wedged worker can't freeze the main thread on
        // shutdown. 5s is generous — UDP socket close is normally instant.
        if (!m_thread->wait(5000)) {
            qWarning() << "IPC: worker thread did not stop within 5s; forcing termination";
            m_thread->terminate();
            m_thread->wait(1000);
        }
        delete m_thread;
        m_thread = nullptr;
        m_worker = nullptr;
    }
}

bool IpcServer::restart(const QString &endpoint)
{
    stop();
    return start(endpoint);
}

void IpcServer::onDatagramReceived(const QByteArray &data, const QHostAddress &sender, quint16 port)
{
    parseMessage(data, sender, port);
}

void IpcServer::onWorkerError(const QString &message)
{
    qWarning() << "IPC:" << message;
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
        QByteArray response = pongDoc.toJson(QJsonDocument::Compact) + "\n";
        QMetaObject::invokeMethod(m_worker, "sendDatagram",
                                  Qt::QueuedConnection,
                                  Q_ARG(QByteArray, response),
                                  Q_ARG(QHostAddress, sender),
                                  Q_ARG(quint16, port));
        emit pingReceived(sender, port);
    } else {
        qWarning() << "IPC: Unknown message type:" << type;
    }
}
