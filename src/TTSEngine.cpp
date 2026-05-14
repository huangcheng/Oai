#include "TTSEngine.h"
#include "ConfigManager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QUrlQuery>
#include <QFile>
#include <QDateTime>

static void ttsLog(const QString &msg)
{
    QFile f("/tmp/oai_tts.log");
    if (f.open(QIODevice::WriteOnly | QIODevice::Append)) {
        f.write(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz").toUtf8());
        f.write(" ");
        f.write(msg.toUtf8());
        f.write("\n");
        f.close();
    }
}

TTSEngine::TTSEngine(ConfigManager *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_thread = new QThread(this);
    moveToThread(m_thread);

    ttsLog("TTSEngine constructor, ttsEnabled=" + QString::number(m_config ? m_config->ttsEnabled() : -1));

    connect(m_thread, &QThread::started, this, [this]() {
        setupWebSocket();
        if (m_config && m_config->ttsEnabled()) {
            ttsLog("Thread started, connecting to provider");
            connectToProvider();
        } else {
            ttsLog("Thread started, TTS disabled or no config");
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
    if (!m_webSocket || !m_config) {
        ttsLog("connectToProvider: missing websocket or config");
        return;
    }

    QString baseUrl = m_config->ttsBaseUrl();
    if (baseUrl.isEmpty()) {
        ttsLog("connectToProvider: baseUrl empty");
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

    ttsLog("connectToProvider: opening " + url.toString());
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
    ttsLog("speak called, text=" + text.left(50));
    if (!m_config || !m_config->ttsEnabled()) {
        ttsLog("speak: TTS disabled");
        return;
    }
    if (text.isEmpty()) {
        ttsLog("speak: text empty");
        return;
    }

    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        // Queue the text to speak after connection
        m_pendingText = text;
        ttsLog("speak: not connected, queuing and connecting");
        connectToProvider();
    } else if (!m_sessionId.isEmpty()) {
        ttsLog("speak: connected with session, sending text");
        sendTtsText(text);
    } else {
        // Waiting for session, queue it
        m_pendingText = text;
        ttsLog("speak: connected but no session, queuing");
    }
}

void TTSEngine::sendTtsCreate()
{
    if (!m_webSocket || m_sessionId.isEmpty()) {
        ttsLog("sendTtsCreate: no websocket or session");
        return;
    }

    QString voice = m_config ? m_config->ttsVoice() : QString();
    if (voice.isEmpty()) voice = QStringLiteral("cixingnansheng");

    QJsonObject data;
    data["session_id"] = m_sessionId;
    data["voice_id"] = voice;
    data["response_format"] = "pcm";  // raw 16-bit signed LE mono — no headers, streams cleanly
    data["volume_ratio"] = 1.0;
    data["speed_ratio"] = 1.0;
    data["sample_rate"] = 24000;

    QJsonObject request;
    request["type"] = "tts.create";
    request["data"] = data;

    QJsonDocument doc(request);
    m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    ttsLog("sendTtsCreate: sent tts.create session=" + m_sessionId);
}

void TTSEngine::sendTtsText(const QString &text)
{
    if (!m_webSocket || m_sessionId.isEmpty()) {
        ttsLog("sendTtsText: no websocket or session");
        return;
    }

    QJsonObject data;
    data["session_id"] = m_sessionId;
    data["text"] = text;

    QJsonObject request;
    request["type"] = "tts.text.delta";
    request["data"] = data;

    QJsonDocument doc(request);
    m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    emit speakingStarted();
    ttsLog("sendTtsText: sent tts.text.delta");

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

void TTSEngine::ensureAudioSink()
{
    if (m_audioSink) return;

    QAudioFormat fmt;
    fmt.setSampleRate(24000);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);  // PCM s16le

    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!dev.isFormatSupported(fmt)) {
        ttsLog("ensureAudioSink: 24kHz s16 mono not supported by default device");
    }

    m_audioSink = new QAudioSink(dev, fmt, this);
    m_audioSinkDevice = m_audioSink->start();  // start in push mode
    if (!m_audioSinkDevice) {
        ttsLog("ensureAudioSink: start() returned null");
    } else {
        ttsLog("ensureAudioSink: streaming sink ready");
    }
}

void TTSEngine::writePcm(const QByteArray &pcm)
{
    ensureAudioSink();
    if (!m_audioSinkDevice) return;

    qint64 written = m_audioSinkDevice->write(pcm);
    if (written < pcm.size()) {
        ttsLog("writePcm: short write " + QString::number(written) + "/" + QString::number(pcm.size()));
    }
}

void TTSEngine::onConnected()
{
    m_retryCount = 0;
    clearRetryTimer();

    if (!m_heartbeatTimer) {
        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL_MS);
        connect(m_heartbeatTimer, &QTimer::timeout, this, [this]() {
            if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
                m_webSocket->ping();
            }
        });
    }
    m_heartbeatTimer->start();

    emit connected();
    ttsLog("onConnected: WebSocket connected, heartbeat started");
}

void TTSEngine::onDisconnected()
{
    if (m_heartbeatTimer) m_heartbeatTimer->stop();
    m_sessionId.clear();
    emit disconnected();
    ttsLog("onDisconnected");
}

void TTSEngine::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    QString errorMsg = m_webSocket ? m_webSocket->errorString() : tr("Unknown WebSocket error");
    emit error(errorMsg);
    ttsLog("onError: " + errorMsg);

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

    ttsLog("onTextMessageReceived: type=" + type);

    if (type == "tts.connection.done") {
        m_sessionId = data["session_id"].toString();
        ttsLog("session established: " + m_sessionId);
        sendTtsCreate();
    }
    else if (type == "tts.response.created") {
        if (!m_pendingText.isEmpty()) {
            ttsLog("tts.response.created, sending pending text");
            sendTtsText(m_pendingText);
            m_pendingText.clear();
        }
    }
    else if (type == "tts.response.audio.delta") {
        QString audioBase64 = data["audio"].toString();
        ttsLog("audio delta, base64 len=" + QString::number(audioBase64.length()));
        if (!audioBase64.isEmpty()) {
            writePcm(QByteArray::fromBase64(audioBase64.toUtf8()));
        }
    }
    else if (type == "tts.response.audio.done") {
        QString audioBase64 = data["audio"].toString();
        ttsLog("audio done, base64 len=" + QString::number(audioBase64.length()));
        if (!audioBase64.isEmpty()) {
            writePcm(QByteArray::fromBase64(audioBase64.toUtf8()));
        }
        emit speakingFinished();
    }
    else if (type == "tts.response.error") {
        QString errorMsg = data["message"].toString();
        emit error(errorMsg);
        ttsLog("server error: " + errorMsg);
    }
}

void TTSEngine::onBinaryMessageReceived(const QByteArray &data)
{
    ttsLog("binary message received, size=" + QString::number(data.size()));
    writePcm(data);
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
    ttsLog("retryConnection: attempt " + QString::number(m_retryCount) + " delay=" + QString::number(delay));
    m_retryTimer->start(delay);
}

void TTSEngine::clearRetryTimer()
{
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
}
