#include "TTSEngine.h"
#include "ConfigManager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QUrlQuery>

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

    // Construct TTS WebSocket URL from base URL + model
    // If baseUrl is wss://api.stepfun.com/step_plan/v1/realtime
    // we need wss://api.stepfun.com/step_plan/v1/realtime/audio?model=MODEL
    QUrl url(baseUrl);
    QString path = url.path();
    if (!path.endsWith("/audio")) {
        if (path.endsWith("/")) {
            path += "audio";
        } else {
            path += "/audio";
        }
        url.setPath(path);
    }

    QUrlQuery query;
    query.addQueryItem("model", m_config->ttsModel());
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_config->ttsToken().toUtf8());

    qDebug() << "TTSEngine: connecting to" << url.toString();
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
        // Queue the text to speak after connection
        m_pendingText = text;
        connectToProvider();
    } else if (!m_sessionId.isEmpty()) {
        sendTtsText(text);
    } else {
        // Waiting for session, queue it
        m_pendingText = text;
    }
}

void TTSEngine::sendTtsCreate()
{
    if (!m_webSocket || m_sessionId.isEmpty()) return;

    QJsonObject data;
    data["session_id"] = m_sessionId;
    data["voice_id"] = "cixingnansheng";  // Default voice
    data["response_format"] = "mp3";
    data["volume_ratio"] = 1.0;
    data["speed_ratio"] = 1.0;
    data["sample_rate"] = 24000;

    QJsonObject request;
    request["type"] = "tts.create";
    request["data"] = data;

    QJsonDocument doc(request);
    m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    qDebug() << "TTSEngine: sent tts.create for session" << m_sessionId;
}

void TTSEngine::sendTtsText(const QString &text)
{
    if (!m_webSocket || m_sessionId.isEmpty()) return;

    QJsonObject data;
    data["session_id"] = m_sessionId;
    data["text"] = text;

    QJsonObject request;
    request["type"] = "tts.text.delta";
    request["data"] = data;

    QJsonDocument doc(request);
    m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    emit speakingStarted();
    qDebug() << "TTSEngine: sent tts.text.delta:" << text;

    // Send flush to force immediate generation
    QJsonObject flushData;
    flushData["session_id"] = m_sessionId;

    QJsonObject flushRequest;
    flushRequest["type"] = "tts.text.flush";
    flushRequest["data"] = flushData;

    QJsonDocument flushDoc(flushRequest);
    m_webSocket->sendTextMessage(QString::fromUtf8(flushDoc.toJson(QJsonDocument::Compact)));

    // Send done
    QJsonObject doneData;
    doneData["session_id"] = m_sessionId;

    QJsonObject doneRequest;
    doneRequest["type"] = "tts.text.done";
    doneRequest["data"] = doneData;

    QJsonDocument doneDoc(doneRequest);
    m_webSocket->sendTextMessage(QString::fromUtf8(doneDoc.toJson(QJsonDocument::Compact)));
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

    QBuffer *buffer = new QBuffer(this);
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
    qDebug() << "TTSEngine: WebSocket connected, waiting for session...";
}

void TTSEngine::onDisconnected()
{
    m_sessionId.clear();
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

    QString type = obj["type"].toString();
    QJsonObject data = obj["data"].toObject();

    qDebug() << "TTSEngine: received" << type;

    if (type == "tts.connection.done") {
        m_sessionId = data["session_id"].toString();
        qDebug() << "TTSEngine: session established:" << m_sessionId;
        sendTtsCreate();
    }
    else if (type == "tts.response.created") {
        // Session ready, send pending text if any
        if (!m_pendingText.isEmpty()) {
            sendTtsText(m_pendingText);
            m_pendingText.clear();
        }
    }
    else if (type == "tts.response.audio.delta") {
        QString audioBase64 = data["audio"].toString();
        if (!audioBase64.isEmpty()) {
            QByteArray audioData = QByteArray::fromBase64(audioBase64.toUtf8());
            if (!audioData.isEmpty()) {
                playAudio(audioData);
            }
        }
    }
    else if (type == "tts.response.audio.done") {
        QString audioBase64 = data["audio"].toString();
        if (!audioBase64.isEmpty()) {
            QByteArray audioData = QByteArray::fromBase64(audioBase64.toUtf8());
            if (!audioData.isEmpty()) {
                playAudio(audioData);
            }
        }
        emit speakingFinished();
    }
    else if (type == "tts.response.error") {
        QString errorMsg = data["message"].toString();
        emit error(errorMsg);
        qWarning() << "TTSEngine: server error:" << errorMsg;
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
