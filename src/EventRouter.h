#ifndef EVENTROUTER_H
#define EVENTROUTER_H

#include <QObject>
#include <QMap>
#include <QSet>

class SpriteAnimationEngine;
class LottieAnimationEngine;
#ifdef OAI_LIVE2D_SUPPORT
class Live2DAnimationEngine;
#endif
class TipBubbleWidget;
class TipsEngine;
class SpritePack;

class EventRouter : public QObject
{
    Q_OBJECT

public:
    explicit EventRouter(QObject *parent = nullptr);

    void setAnimationEngine(SpriteAnimationEngine *engine) { m_engine = engine; }
    void setLottieEngine(LottieAnimationEngine *engine) { m_lottieEngine = engine; }
#ifdef OAI_LIVE2D_SUPPORT
    void setLive2dEngine(Live2DAnimationEngine *engine) { m_live2dEngine = engine; }
#endif
    void setTipBubble(TipBubbleWidget *bubble) { m_tipBubble = bubble; }
    void setTipsEngine(TipsEngine *tips) { m_tips = tips; }

    /**
     * @brief Load event mappings from a sprite pack
     * @param pack Sprite pack to load from
     */
    void loadFromSpritePack(const SpritePack *pack);

public slots:
    void routeEvent(const QJsonObject &event);
    void retranslateUi();

private:
    void initEventMap();
    bool validateEvent(const QJsonObject &event) const;

    struct EventAction {
        QString animation;  // sprite sheet animation name (snake_case, mapped by engine)
        QString tipTitle;
        QString tipBody;
    };

    QMap<QString, EventAction> m_eventMap;

    SpriteAnimationEngine *m_engine = nullptr;
    LottieAnimationEngine *m_lottieEngine = nullptr;
#ifdef OAI_LIVE2D_SUPPORT
    Live2DAnimationEngine *m_live2dEngine = nullptr;
#endif
    TipBubbleWidget *m_tipBubble = nullptr;
    TipsEngine *m_tips = nullptr;

    static const QSet<QString> s_validEvents;
    static const QSet<QString> s_validSources;
};

#endif // EVENTROUTER_H
