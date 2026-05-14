#include "TTSEngine.h"
#include "ConfigManager.h"

#include <QAudioDecoder>
#include <QBuffer>
#include <QDebug>
#include <QLoggingCategory>
#include <QMediaDevices>
#include <QNetworkAccessManager>

Q_LOGGING_CATEGORY(lcTts, "oai.tts")

using namespace oai::tts;

TTSEngine::TTSEngine(ConfigManager *config, QObject *parent)
    : QObject(parent), m_config(config)
{
    m_thread = new QThread(this);
    moveToThread(m_thread);

    connect(m_thread, &QThread::started, this, &TTSEngine::initOnThread);

    if (m_config) {
        connect(m_config, &ConfigManager::ttsActiveProviderChanged,
                this, &TTSEngine::onActiveProviderChanged,
                Qt::QueuedConnection);
        connect(m_config, &ConfigManager::ttsProviderFieldChanged,
                this, &TTSEngine::onProviderFieldChanged,
                Qt::QueuedConnection);
    }
}

TTSEngine::~TTSEngine() { stop(); }

void TTSEngine::start()
{
    if (m_thread && !m_thread->isRunning())
        m_thread->start();
}

void TTSEngine::stop()
{
    if (m_thread && m_thread->isRunning()) {
        QMetaObject::invokeMethod(this, [this]() { resetAudio(); m_provider.reset(); },
                                  Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait(3000);
    }
}

void TTSEngine::initOnThread()
{
    m_nam = new QNetworkAccessManager(this);
    m_audioBuffer = new QBuffer(this);
    // Decoder is built lazily in startDecode() — see header for why.

    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        doSynthesize(m_pendingText, m_pendingOptions);
    });

    qCInfo(lcTts) << "engine init on thread, active provider ="
                  << (m_config ? m_config->ttsActiveProvider() : QStringLiteral("<no config>"));
    rebuildProvider();
}

void TTSEngine::rebuildProvider()
{
    if (!m_config) return;
    if (m_inFlight && m_provider) {
        m_provider->cancel(m_inFlight);
        m_inFlight = 0;
    }
    m_provider.reset();
    m_currentProviderStableId.clear();

    const QString stableId = m_config->ttsActiveProvider();
    const ProviderDescriptor* desc =
        TtsProviderRegistry::findByStableId(stableId);
    if (!desc) {
        emit error(tr("Unknown TTS provider: %1").arg(stableId));
        return;
    }

    ProviderConfig cfg{ m_config->ttsProviderConfig(stableId) };
    try {
        m_provider = desc->factory(cfg, m_nam);
    } catch (const std::exception& e) {
        qCWarning(lcTts) << "factory threw for" << stableId << ":" << e.what();
        emit error(tr("Failed to construct TTS provider: %1")
                       .arg(QString::fromUtf8(e.what())));
        return;
    }
    m_currentProviderStableId = stableId;
    qCInfo(lcTts) << "provider built:" << stableId;
}

void TTSEngine::onActiveProviderChanged(const QString &)
{
    rebuildProvider();
}

void TTSEngine::onProviderFieldChanged(const QString &providerId,
                                        const QString &,
                                        const QString &)
{
    if (providerId == m_currentProviderStableId)
        rebuildProvider();
}

void TTSEngine::speak(const QString &text)
{
    speakWithOptions(text, SpeakOptions{});
}

void TTSEngine::speakWithOptions(const QString &text, SpeakOptions opts)
{
    if (!m_config || !m_config->ttsEnabled() || text.isEmpty()) {
        qCDebug(lcTts) << "speak() ignored — enabled=" << (m_config && m_config->ttsEnabled())
                       << "textEmpty=" << text.isEmpty();
        return;
    }

    QMetaObject::invokeMethod(this, [this, text, opts]() {
        qCInfo(lcTts) << "speak:" << text.left(60);
        if (m_inFlight && m_provider) {
            m_provider->cancel(m_inFlight);
            m_inFlight = 0;
        }
        m_retryTimer->stop();
        m_retryCount = 0;
        m_pendingText = text;
        m_pendingOptions = opts;
        doSynthesize(text, opts);
    }, Qt::QueuedConnection);
}

void TTSEngine::doSynthesize(const QString &text, SpeakOptions opts)
{
    if (!m_provider) {
        qCWarning(lcTts) << "doSynthesize: no provider";
        emit error(tr("No TTS provider configured"));
        return;
    }
    qCDebug(lcTts) << "synthesize via" << m_currentProviderStableId;
    SynthesisRequest req{text, opts};
    m_inFlight = m_provider->synthesize(req,
        [this](SynthesisResult r) { onSynthesisSuccess(std::move(r)); },
        [this](TtsError e)        { onSynthesisError(std::move(e)); });
}

void TTSEngine::onSynthesisSuccess(SynthesisResult result)
{
    m_inFlight = 0;
    m_retryCount = 0;
    qCInfo(lcTts) << "synthesis ok:" << result.audio.size() << "bytes"
                  << "mime=" << result.mimeType;
    if (result.audio.isEmpty()) {
        qCWarning(lcTts) << "empty audio buffer — nothing to play";
        emit speakingFinished();
        return;
    }
    startDecode(result.audio, result.mimeType);
    emit speakingStarted();
}

void TTSEngine::onSynthesisError(TtsError err)
{
    m_inFlight = 0;
    if (err.kind == TtsErrorKind::Cancelled) {
        qCDebug(lcTts) << "synthesis cancelled";
        return;
    }
    qCWarning(lcTts) << "synthesis error kind=" << int(err.kind)
                     << "http=" << err.httpStatus
                     << "msg=" << err.message;
    if (err.kind == TtsErrorKind::AuthFailed) {
        emit authFailed(m_currentProviderStableId);
        emit error(tr("TTS authentication failed (HTTP %1)").arg(err.httpStatus));
        return;
    }
    if ((err.kind == TtsErrorKind::Network || err.kind == TtsErrorKind::RateLimited)
        && m_retryCount < kMaxRetries) {
        scheduleRetry();
        return;
    }
    emit error(err.message.isEmpty()
                 ? tr("TTS request failed")
                 : err.message);
}

void TTSEngine::scheduleRetry()
{
    const int delays[] = {250, 1000};
    m_retryTimer->start(delays[m_retryCount]);
    ++m_retryCount;
}

void TTSEngine::onDecoderBufferReady()
{
    if (!m_decoder) return;
    while (m_decoder->bufferAvailable()) {
        QAudioBuffer buf = m_decoder->read();
        if (!buf.isValid()) break;
        if (!m_audioSink) {
            qCInfo(lcTts) << "audio format:" << buf.format()
                          << "device=" << QMediaDevices::defaultAudioOutput().description();
            m_audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(),
                                          buf.format(), this);
            connect(m_audioSink, &QAudioSink::stateChanged, this,
                    [](QAudio::State s) { qCDebug(lcTts) << "sink state ->" << s; });
            m_audioSinkDevice = m_audioSink->start();
            if (!m_audioSinkDevice) {
                qCWarning(lcTts) << "QAudioSink::start() returned null —"
                                 << "error=" << m_audioSink->error();
            }
        }
        if (m_audioSinkDevice) {
            qint64 written = m_audioSinkDevice->write(
                reinterpret_cast<const char*>(buf.constData<char>()),
                buf.byteCount());
            if (written < buf.byteCount()) {
                qCWarning(lcTts) << "short write to sink"
                                 << written << "of" << buf.byteCount();
            }
        }
    }
}

void TTSEngine::onDecoderFinished()
{
    qCInfo(lcTts) << "decoder finished";
    emit speakingFinished();
}

void TTSEngine::startDecode(const QByteArray &audio, const QString &mimeType)
{
    // Recreate the decoder every time. Reusing a single QAudioDecoder across
    // utterances is unreliable on Qt 6.11's WMF backend (Windows): once a
    // decode completes or errors, setSourceDevice() + start() on the same
    // instance frequently produces no bufferReady signals. A fresh decoder
    // sidesteps that entirely.
    resetAudio();

    m_audioBuffer->setData(audio);
    m_audioBuffer->open(QIODevice::ReadOnly);

    m_decoder = new QAudioDecoder(this);
    m_decoder->setSourceDevice(m_audioBuffer);

    connect(m_decoder, &QAudioDecoder::bufferReady,
            this, &TTSEngine::onDecoderBufferReady);
    connect(m_decoder, QOverload<>::of(&QAudioDecoder::finished),
            this, &TTSEngine::onDecoderFinished);
    connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error e) {
                qCWarning(lcTts) << "QAudioDecoder error" << e
                                 << m_decoder->errorString();
                emit speakingFinished();
            });

    qCDebug(lcTts) << "decoder start, audio bytes=" << audio.size()
                   << "mime hint=" << mimeType;
    m_decoder->start();
}

void TTSEngine::resetAudio()
{
    if (m_decoder) {
        m_decoder->stop();
        m_decoder->deleteLater();
        m_decoder = nullptr;
    }
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->deleteLater();
        m_audioSink = nullptr;
        m_audioSinkDevice = nullptr;
    }
    if (m_audioBuffer) {
        m_audioBuffer->close();
        m_audioBuffer->setData(QByteArray());
    }
}
