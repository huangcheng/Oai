#ifndef EVENTROUTER_H
#define EVENTROUTER_H

#include <QObject>
#include <QMap>
#include <QSet>

class SpriteAnimationEngine;
class TipBubbleWidget;
class LottieEffectOverlay;
class TipsEngine;

class EventRouter : public QObject
{
    Q_OBJECT

public:
    explicit EventRouter(QObject *parent = nullptr);

    void setAnimationEngine(SpriteAnimationEngine *engine) { m_engine = engine; }
    void setTipBubble(TipBubbleWidget *bubble) { m_tipBubble = bubble; }
    void setEffectOverlay(LottieEffectOverlay *effects) { m_effects = effects; }
    void setTipsEngine(TipsEngine *tips) { m_tips = tips; }

public slots:
    void routeEvent(const QJsonObject &event);

private:
    void initEventMap();
    bool validateEvent(const QJsonObject &event) const;

    struct EventAction {
        QString animation;  // sprite sheet animation name (snake_case, mapped by engine)
        QString effect;     // Lottie effect name
        QString tipTitle;
        QString tipBody;
    };

    QMap<QString, EventAction> m_eventMap;

    SpriteAnimationEngine *m_engine = nullptr;
    TipBubbleWidget *m_tipBubble = nullptr;
    LottieEffectOverlay *m_effects = nullptr;
    TipsEngine *m_tips = nullptr;

    static const QSet<QString> s_validEvents;
    static const QSet<QString> s_validSources;
};

#endif // EVENTROUTER_H
