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

    m_voiceCache = std::make_unique<oai::tts::TtsVoiceCache>();
    if (m_config) {
        connect(m_config, &ConfigManager::ttsCacheInvalidated,
                m_voiceCache.get(), &oai::tts::TtsVoiceCache::wipeAll,
                Qt::QueuedConnection);
    }

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

void TTSEngine::clearVoiceCache()
{
    // Hop to the engine thread; m_voiceCache is constructed there in
    // initOnThread() and not safe to touch from elsewhere.
    QMetaObject::invokeMethod(this, [this]() {
        if (m_voiceCache) m_voiceCache->wipeAll();
    }, Qt::QueuedConnection);
}

void TTSEngine::speakWithOptions(const QString &text, SpeakOptions opts)
{
    if (!m_config || !m_config->ttsEnabled() || text.isEmpty()) {
        qCDebug(lcTts) << "speak() ignored — enabled=" << (m_config && m_config->ttsEnabled())
                       << "textEmpty=" << text.isEmpty();
        return;
    }

    QMetaObject::invokeMethod(this, [this, text, opts]() {
        // Debounce: if we're already speaking (or have a request in flight),
        // drop the new one rather than racing the audio pipeline through a
        // teardown. Rapid clicks on the pet stay responsive visually; only
        // the speech is gated.
        if (m_speaking || m_inFlight) {
            qCDebug(lcTts) << "speak() dropped — busy (speaking=" << m_speaking
                           << " inFlight=" << m_inFlight << ")";
            return;
        }
        qCInfo(lcTts) << "speak:" << text.left(60);
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

    if (m_voiceCache && m_config) {
        const QString providerId = m_config->ttsActiveProvider();
        const QString voiceId = m_config->ttsProviderField(providerId, QStringLiteral("voice"));
        const QString modelId = m_config->ttsProviderField(providerId, QStringLiteral("model"));
        m_pendingCacheKey = oai::tts::TtsVoiceCache::cacheKey(
            providerId, voiceId, modelId, opts, text);

        if (m_voiceCache->hasCachedAudio(m_pendingCacheKey)) {
            QByteArray cachedAudio = m_voiceCache->getCachedAudio(m_pendingCacheKey);
            if (!cachedAudio.isEmpty()) {
                qCInfo(lcTts) << "cache hit, returning cached audio for key:"
                              << m_pendingCacheKey;
                SynthesisResult result;
                result.audio = std::move(cachedAudio);
                QString cachedMime = m_voiceCache->getCachedMimeType(m_pendingCacheKey);
                result.mimeType = cachedMime.isEmpty()
                    ? QStringLiteral("audio/mpeg")
                    : cachedMime;
                onSynthesisSuccess(std::move(result));
                return;
            }
        }
    } else {
        m_pendingCacheKey.clear();
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

    if (m_voiceCache && !m_pendingCacheKey.isEmpty()) {
        m_voiceCache->writeCachedAudio(m_pendingCacheKey, result.audio, result.mimeType);
    }

    m_speaking = true;
    startDecode(result.audio, result.mimeType);
    emit speakingStarted();
}

void TTSEngine::onSynthesisError(TtsError err)
{
    m_inFlight = 0;
    if (err.kind == TtsErrorKind::Cancelled) {
        qCDebug(lcTts) << "synthesis cancelled";
        m_speaking = false;
        return;
    }
    qCWarning(lcTts) << "synthesis error kind=" << int(err.kind)
                     << "http=" << err.httpStatus
                     << "msg=" << err.message;
    if (err.kind == TtsErrorKind::AuthFailed) {
        m_speaking = false;
        emit authFailed(m_currentProviderStableId);
        emit error(tr("TTS authentication failed (HTTP %1)").arg(err.httpStatus));
        return;
    }
    if ((err.kind == TtsErrorKind::Network || err.kind == TtsErrorKind::RateLimited)
        && m_retryCount < kMaxRetries) {
        scheduleRetry();
        return;
    }
    m_speaking = false;
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
        if (!m_pcmFormat.isValid()) {
            m_pcmFormat = buf.format();
            qCInfo(lcTts) << "decoded audio format:" << m_pcmFormat;
        }
        m_pcm.append(buf.constData<char>(), buf.byteCount());
    }
}

void TTSEngine::onDecoderFinished()
{
    qCInfo(lcTts) << "decoder finished, pcm size=" << m_pcm.size();
    if (m_pcm.isEmpty() || !m_pcmFormat.isValid()) {
        qCWarning(lcTts) << "no PCM to play";
        m_speaking = false;
        emit speakingFinished();
        return;
    }

    // Hand the assembled PCM to the sink in pull mode. The sink reads at
    // its own pace; no short-write loss.
    m_pcmBuffer = new QBuffer(this);
    m_pcmBuffer->setData(m_pcm);
    m_pcmBuffer->open(QIODevice::ReadOnly);

    m_audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(),
                                  m_pcmFormat, this);
    connect(m_audioSink, &QAudioSink::stateChanged, this,
            [this](QAudio::State s) {
                qCDebug(lcTts) << "sink state ->" << s;
                // IdleState after Active means the source ran out — playback
                // is done. StoppedState means we called stop() ourselves.
                if (s == QAudio::IdleState) {
                    m_speaking = false;
                    emit speakingFinished();
                }
            });

    qCInfo(lcTts) << "sink start (pull mode), device="
                  << QMediaDevices::defaultAudioOutput().description();
    m_audioSink->start(m_pcmBuffer);
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
                m_speaking = false;
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
    }
    if (m_pcmBuffer) {
        m_pcmBuffer->close();
        m_pcmBuffer->deleteLater();
        m_pcmBuffer = nullptr;
    }
    if (m_audioBuffer) {
        m_audioBuffer->close();
        m_audioBuffer->setData(QByteArray());
    }
    m_pcm.clear();
    m_pcmFormat = QAudioFormat{};
}
