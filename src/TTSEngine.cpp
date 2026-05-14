#include "TTSEngine.h"
#include "ConfigManager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>

TTSEngine::TTSEngine(ConfigManager *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_thread = new QThread(this);
    moveToThread(m_thread);

    connect(m_thread, &QThread::started, this, [this]() {
        setupWebSocket();
        if (m_config && m_config->ttsEnabled()) {
            connectToProvider();
        }
    });
}

TTSEngine::~TTSEngine()
{
    stop();
}

void TTSEngine::start()
{
    if (m_thread && !m_thread->isRunning()) {
        m_thread->start();
    }
}

void TTSEngine::stop()
{
    clearRetryTimer();
    disconnectFromProvider();

    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
    }
}

void TTSEngine::setupWebSocket()
{
    if (m_webSocket) return;

    m_webSocket = new QWebSocket();
    m_webSocket->moveToThread(m_thread);

    connect(m_webSocket, &QWebSocket::connected,
            this, &TTSEngine::onConnected);
    connect(m_webSocket, &QWebSocket::disconnected,
            this, &TTSEngine::onDisconnected);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &TTSEngine::onError);
    connect(m_webSocket, &QWebSocket::textMessageReceived,
            this, &TTSEngine::onTextMessageReceived);
    connect(m_webSocket, &QWebSocket::binaryMessageReceived,
            this, &TTSEngine::onBinaryMessageReceived);
}

void TTSEngine::connectToProvider()
{
    if (!m_webSocket || !m_config) return;

    QString baseUrl = m_config->ttsBaseUrl();
    if (baseUrl.isEmpty()) {
        emit error(tr("TTS base URL not configured"));
        return;
    }

    m_retryCount = 0;

    QUrl url(baseUrl);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_config->ttsToken().toUtf8());

    m_webSocket->open(request);
}

void TTSEngine::disconnectFromProvider()
{
    clearRetryTimer();
    if (m_webSocket) {
        m_webSocket->close();
    }
}

void TTSEngine::speak(const QString &text)
{
    if (!m_config || !m_config->ttsEnabled()) return;
    if (text.isEmpty()) return;

    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        connectToProvider();
    }

    sendTtsRequest(text);
}

void TTSEngine::sendTtsRequest(const QString &text)
{
    if (!m_webSocket || !m_config) return;

    QJsonObject request;
    request["text"] = text;
    request["model"] = m_config->ttsModel();
    request["language"] = m_config->ttsLanguage();

    QJsonDocument doc(request);
    m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    emit speakingStarted();
}

void TTSEngine::playAudio(const QByteArray &audioData)
{
    if (!m_mediaPlayer) {
        m_mediaPlayer = new QMediaPlayer(this);
        m_audioOutput = new QAudioOutput(this);
        m_mediaPlayer->setAudioOutput(m_audioOutput);

        connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
                this, &TTSEngine::onMediaStatusChanged);
        connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
                this, &TTSEngine::onPlaybackStateChanged);
    }

    QBuffer *buffer = new QBuffer();
    buffer->setData(audioData);
    buffer->open(QIODevice::ReadOnly);

    m_mediaPlayer->setSourceDevice(buffer);
    m_mediaPlayer->play();
}

void TTSEngine::onConnected()
{
    m_retryCount = 0;
    clearRetryTimer();
    emit connected();
    qDebug() << "TTSEngine: connected to provider";
}

void TTSEngine::onDisconnected()
{
    emit disconnected();
    qDebug() << "TTSEngine: disconnected from provider";
}

void TTSEngine::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    QString errorMsg = m_webSocket ? m_webSocket->errorString() : tr("Unknown WebSocket error");
    emit error(errorMsg);
    qWarning() << "TTSEngine: WebSocket error:" << errorMsg;

    if (m_retryCount < MAX_RETRIES) {
        retryConnection();
    }
}

void TTSEngine::onTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject obj = doc.object();

    if (obj.contains("error")) {
        emit error(obj["error"].toString());
        return;
    }

    if (obj.contains("status") && obj["status"].toString() == "finished") {
        emit speakingFinished();
    }
}

void TTSEngine::onBinaryMessageReceived(const QByteArray &data)
{
    playAudio(data);
}

void TTSEngine::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        emit speakingFinished();
    }
}

void TTSEngine::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    Q_UNUSED(state)
}

void TTSEngine::retryConnection()
{
    if (!m_retryTimer) {
        m_retryTimer = new QTimer(this);
        m_retryTimer->setSingleShot(true);
        connect(m_retryTimer, &QTimer::timeout, this, &TTSEngine::connectToProvider);
    }

    int delay = INITIAL_RETRY_DELAY_MS * (1 << m_retryCount);
    m_retryCount++;
    m_retryTimer->start(delay);
}

void TTSEngine::clearRetryTimer()
{
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
}
