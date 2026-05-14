#ifndef TTSENGINE_H
#define TTSENGINE_H

#include <QObject>
#include <QThread>
#include <QWebSocket>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTimer>
#include <QUrl>

class ConfigManager;

class TTSEngine : public QObject
{
    Q_OBJECT

public:
    explicit TTSEngine(ConfigManager *config, QObject *parent = nullptr);
    ~TTSEngine();

    void start();
    void stop();

public slots:
    void speak(const QString &text);
    void connectToProvider();
    void disconnectFromProvider();

signals:
    void connected();
    void disconnected();
    void error(const QString &message);
    void speakingStarted();
    void speakingFinished();

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &data);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void retryConnection();

private:
    void setupWebSocket();
    void sendTtsRequest(const QString &text);
    void playAudio(const QByteArray &audioData);
    void clearRetryTimer();

    ConfigManager *m_config;
    QThread *m_thread = nullptr;
    QWebSocket *m_webSocket = nullptr;
    QMediaPlayer *m_mediaPlayer = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QTimer *m_retryTimer = nullptr;

    int m_retryCount = 0;
    static constexpr int MAX_RETRIES = 5;
    static constexpr int INITIAL_RETRY_DELAY_MS = 1000;
};

#endif // TTSENGINE_H
