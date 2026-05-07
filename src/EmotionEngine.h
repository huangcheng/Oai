#ifndef EMOTIONENGINE_H
#define EMOTIONENGINE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QTimer>

class EmotionEngine : public QObject
{
    Q_OBJECT

public:
    struct EmotionState {
        float valence = 0.0f;
        float arousal = 0.0f;
        QString mood = QStringLiteral("neutral");
    };

    explicit EmotionEngine(QObject *parent = nullptr);

    EmotionState state() const { return m_state; }

    void processEvent(const QString &eventName);

signals:
    void moodChanged(const QString &animationName);

private slots:
    void onDecayTick();

private:
    void updateMood();
    bool detectStreak(const QString &eventName);

    EmotionState m_state;

    struct EventRecord {
        QString name;
        qint64 timestamp;
    };
    QList<EventRecord> m_recentEvents;
    static constexpr int STREAK_THRESHOLD = 3;
    static constexpr int STREAK_WINDOW_MS = 30000;

    QTimer m_decayTimer;
    static constexpr int DECAY_INTERVAL_MS = 10000;
    static constexpr float DECAY_FACTOR = 0.85f;

    struct EventScore {
        float valence;
        float arousal;
    };
    static const QMap<QString, EventScore> s_eventScores;

    static const QMap<QString, QString> s_moodAnimations;
};

#endif // EMOTIONENGINE_H
