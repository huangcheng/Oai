#ifndef TIPSENGINE_H
#define TIPSENGINE_H

#include <QObject>
#include <QVector>
#include <QDateTime>
#include <QMap>
#include <QJsonObject>

class SpriteAnimationEngine;
class TipWidget;

class TipsEngine : public QObject
{
    Q_OBJECT

public:
    explicit TipsEngine(QObject *parent = nullptr);

    void setAnimationEngine(SpriteAnimationEngine *engine) { m_engine = engine; }
    void setTipWidget(TipWidget *bubble) { m_tipWidget = bubble; }

public slots:
    void processEvent(const QString &eventName, const QJsonObject &eventData);
    void retranslateUi();

private:
    struct PatternMatcher {
        QString name;
        std::function<bool(const QVector<QPair<QString, QDateTime>>&)> matcher;
        QString tipTitle;
        QString tipBody;
        QString animation;
    };

    void initMatchers();
    bool isInCooldown(const QString &patternName) const;

    struct EventEntry {
        QString name;
        QDateTime timestamp;
        QJsonObject data;
    };

    QVector<EventEntry> m_eventWindow;
    int m_windowSize = 20;
    int m_windowDurationSec = 30;

    QMap<QString, QDateTime> m_lastTriggered;
    int m_cooldownMinMs = 5 * 60 * 1000; // 5 minutes

    QVector<PatternMatcher> m_matchers;

    SpriteAnimationEngine *m_engine = nullptr;
    TipWidget *m_tipWidget = nullptr;
};

#endif // TIPSENGINE_H
