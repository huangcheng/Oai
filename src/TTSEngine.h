#ifndef TTSENGINE_H
#define TTSENGINE_H

#include "tts/ITtsProvider.h"
#include "tts/TtsProviderRegistry.h"

#include <QAudioDecoder>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <memory>

class ConfigManager;
class QNetworkAccessManager;

class TTSEngine : public QObject
{
    Q_OBJECT

public:
    explicit TTSEngine(ConfigManager *config, QObject *parent = nullptr);
    ~TTSEngine() override;

    void start();
    void stop();

public slots:
    void speak(const QString &text);
    void speakWithOptions(const QString &text, oai::tts::SpeakOptions opts);

signals:
    void speakingStarted();
    void speakingFinished();
    void error(const QString &message);
    void authFailed(QString providerStableId);

private slots:
    void onActiveProviderChanged(const QString &stableId);
    void onProviderFieldChanged(const QString &providerId,
                                const QString &field,
                                const QString &value);
    void onDecoderBufferReady();
    void onDecoderFinished();

private:
    void initOnThread();           // runs on m_thread after start()
    void rebuildProvider();        // tear down, re-instantiate from current config
    void doSynthesize(const QString &text, oai::tts::SpeakOptions opts);
    void onSynthesisSuccess(oai::tts::SynthesisResult result);
    void onSynthesisError(oai::tts::TtsError err);
    void scheduleRetry();
    void resetAudio();
    void startDecode(const QByteArray &audio, const QString &mimeType);

    ConfigManager *m_config = nullptr;
    QThread *m_thread = nullptr;

    QNetworkAccessManager *m_nam = nullptr;
    std::unique_ptr<oai::tts::ITtsProvider> m_provider;
    QString m_currentProviderStableId;

    // Active request bookkeeping.
    oai::tts::RequestHandle m_inFlight = 0;
    QString m_pendingText;
    oai::tts::SpeakOptions m_pendingOptions;
    int m_retryCount = 0;
    QTimer *m_retryTimer = nullptr;

    // Audio pipeline. Decoder is recreated per-utterance (Qt 6.11 WMF
    // backend stops emitting bufferReady on a reused instance after the
    // first decode). The sink runs in pull mode against m_pcmBuffer: we
    // accumulate decoded PCM into the buffer as bufferReady fires, then
    // hand the buffer to QAudioSink::start() once the decoder finishes.
    // Pull mode lets the sink read at its own pace and eliminates the
    // short-write loss that push mode produced on Windows.
    QBuffer *m_audioBuffer = nullptr;        // MP3 input to the decoder
    QAudioDecoder *m_decoder = nullptr;
    QByteArray m_pcm;                        // accumulated decoded PCM
    QAudioFormat m_pcmFormat;                // format of m_pcm
    QBuffer *m_pcmBuffer = nullptr;          // pull-mode source for the sink
    QAudioSink *m_audioSink = nullptr;

    // True between speakingStarted and speakingFinished. Rapid speak() calls
    // arriving while busy are dropped (debounce). Set/cleared on the engine
    // thread only.
    bool m_speaking = false;

    static constexpr int kMaxRetries = 2;
};

#endif
