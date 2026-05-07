#include "EmotionEngine.h"
#include <QDateTime>
#include <QDebug>

const QMap<QString, EmotionEngine::EventScore> EmotionEngine::s_eventScores = {
    {"session.start",        { 0.3f,  0.4f}},
    {"session.end",          {-0.1f, -0.2f}},
    {"session.idle",         {-0.1f, -0.3f}},
    {"session.error",        {-0.5f,  0.5f}},
    {"prompt.submitted",     { 0.2f,  0.2f}},
    {"tool.before",          { 0.1f,  0.1f}},
    {"tool.after",           { 0.2f, -0.1f}},
    {"tool.failed",          {-0.4f,  0.4f}},
    {"permission.requested", { 0.0f,  0.2f}},
    {"permission.denied",    {-0.3f,  0.3f}},
    {"permission.response",  { 0.1f,  0.0f}},
    {"subagent.started",     { 0.2f,  0.2f}},
    {"subagent.stopped",     { 0.1f, -0.1f}},
    {"notification.sent",    { 0.1f,  0.1f}},
    {"file.edited",          { 0.2f,  0.1f}},
    {"file.watched",         { 0.0f,  0.0f}},
    {"todo.updated",         { 0.4f,  0.3f}},
};

const QMap<QString, QString> EmotionEngine::s_moodAnimations = {
    {"happy",     "celebrate"},
    {"frustrated","alert"},
    {"bored",     "rest"},
    {"focused",   "work"},
    {"stressed",  "alert"},
    {"neutral",   "rest"},
};

EmotionEngine::EmotionEngine(QObject *parent)
    : QObject(parent)
{
    m_decayTimer.setInterval(DECAY_INTERVAL_MS);
    connect(&m_decayTimer, &QTimer::timeout, this, &EmotionEngine::onDecayTick);
    m_decayTimer.start();
}

void EmotionEngine::processEvent(const QString &eventName)
{
    const auto it = s_eventScores.constFind(eventName);
    if (it == s_eventScores.constEnd()) return;

    const EventScore score = it.value();

    float multiplier = 1.0f;
    if (detectStreak(eventName)) {
        multiplier = 2.0f;
        qDebug() << "EmotionEngine: Streak detected for" << eventName;
    }

    m_state.valence = qBound(-1.0f, m_state.valence + score.valence * multiplier, 1.0f);
    m_state.arousal = qBound(-1.0f, m_state.arousal + score.arousal * multiplier, 1.0f);

    m_recentEvents.append({eventName, QDateTime::currentMSecsSinceEpoch()});

    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - STREAK_WINDOW_MS;
    while (!m_recentEvents.isEmpty() && m_recentEvents.first().timestamp < cutoff) {
        m_recentEvents.removeFirst();
    }

    const QString oldMood = m_state.mood;
    updateMood();

    if (m_state.mood != oldMood) {
        const auto animIt = s_moodAnimations.constFind(m_state.mood);
        const QString anim = (animIt != s_moodAnimations.constEnd())
                             ? animIt.value() : QStringLiteral("rest");
        qDebug() << "EmotionEngine: Mood changed to" << m_state.mood
                 << "-> animation:" << anim
                 << "(v=" << m_state.valence << "a=" << m_state.arousal << ")";
        emit moodChanged(anim);
    }
}

void EmotionEngine::onDecayTick()
{
    m_state.valence *= DECAY_FACTOR;
    m_state.arousal *= DECAY_FACTOR;

    if (qAbs(m_state.valence) < 0.01f) m_state.valence = 0.0f;
    if (qAbs(m_state.arousal) < 0.01f) m_state.arousal = 0.0f;

    const QString oldMood = m_state.mood;
    updateMood();

    if (m_state.mood != oldMood) {
        const auto animIt = s_moodAnimations.constFind(m_state.mood);
        const QString anim = (animIt != s_moodAnimations.constEnd())
                             ? animIt.value() : QStringLiteral("rest");
        qDebug() << "EmotionEngine: Decay mood change to" << m_state.mood;
        emit moodChanged(anim);
    }
}

void EmotionEngine::updateMood()
{
    const float v = m_state.valence;
    const float a = m_state.arousal;

    if (v > 0.3f && a > 0.3f) {
        m_state.mood = QStringLiteral("happy");
    } else if (v < -0.3f && a > 0.4f) {
        m_state.mood = QStringLiteral("frustrated");
    } else if (v < -0.2f && a < 0.2f) {
        m_state.mood = QStringLiteral("bored");
    } else if (v > 0.0f && a < 0.3f) {
        m_state.mood = QStringLiteral("focused");
    } else if (v < -0.1f && a > 0.3f) {
        m_state.mood = QStringLiteral("stressed");
    } else {
        m_state.mood = QStringLiteral("neutral");
    }
}

bool EmotionEngine::detectStreak(const QString &eventName)
{
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - STREAK_WINDOW_MS;
    int count = 0;
    for (int i = m_recentEvents.size() - 1; i >= 0; --i) {
        if (m_recentEvents[i].timestamp < cutoff) break;
        if (m_recentEvents[i].name == eventName) ++count;
    }
    return count >= STREAK_THRESHOLD;
}
