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

    // Audio pipeline (one decoder + sink reused across utterances).
    QBuffer *m_audioBuffer = nullptr;
    QAudioDecoder *m_decoder = nullptr;
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioSinkDevice = nullptr;

    static constexpr int kMaxRetries = 2;
};

#endif
