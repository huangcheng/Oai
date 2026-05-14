#ifndef TTSENGINE_H
#define TTSENGINE_H

#include <QObject>
#include <QThread>
#include <QWebSocket>
#include <QAudioSink>
#include <QAudioFormat>
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
    void retryConnection();

private:
    void setupWebSocket();
    void sendTtsCreate();
    void sendTtsText(const QString &text);
    void ensureAudioSink();
    void writePcm(const QByteArray &pcm);
    void clearRetryTimer();

    ConfigManager *m_config;
    QThread *m_thread = nullptr;
    QWebSocket *m_webSocket = nullptr;
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioSinkDevice = nullptr;  // owned by m_audioSink
    QTimer *m_retryTimer = nullptr;
    QTimer *m_heartbeatTimer = nullptr;

    QString m_sessionId;
    QString m_pendingText;

    int m_retryCount = 0;
    static constexpr int MAX_RETRIES = 5;
    static constexpr int INITIAL_RETRY_DELAY_MS = 1000;
    static constexpr int HEARTBEAT_INTERVAL_MS = 15000;  // server idle timeout is typically 30-60s
};

#endif // TTSENGINE_H
