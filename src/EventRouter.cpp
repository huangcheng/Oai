#include "EventRouter.h"
#include "SpriteAnimationEngine.h"
#include "LottieAnimationEngine.h"
#ifdef OAI_LIVE2D_SUPPORT
#include "Live2DAnimationEngine.h"
#endif
#include "CharacterPack.h"
#include "TipBubbleWidget.h"
#include "TipsEngine.h"
#include "TipsCatalog.h"

#include <QJsonObject>
#include <QDebug>

// Canonical 17 unified events (from D10)
const QSet<QString> EventRouter::s_validEvents = {
    "session.start", "session.end", "session.idle", "session.error",
    "prompt.submitted",
    "tool.before", "tool.after", "tool.failed",
    "permission.requested", "permission.denied", "permission.response",
    "subagent.started", "subagent.stopped",
    "notification.sent",
    "file.edited", "file.watched",
    "todo.updated"
};

// Events that trigger a reactive animation (tap-style). Everything else is
// treated as a passive state change and queued at NormalPriority so it
// doesn't interrupt an ongoing reaction — without this, a burst of idle-
// mapped events mid-tap cuts the tap motion short every ~1.8s.
static const QSet<QString> s_activeEvents = {
    "session.start", "session.error",
    "tool.after", "tool.failed",
    "permission.requested", "permission.denied",
    "notification.sent", "todo.updated",
};

const QSet<QString> EventRouter::s_validSources = {
    "opencode", "claude-code", "codex"
};

EventRouter::EventRouter(QObject *parent)
    : QObject(parent)
{
    initEventMap();
}

void EventRouter::routeEvent(const QJsonObject &event)
{
    if (!validateEvent(event)) {
        return;
    }

    const QString eventName = event.value("event").toString();
    const QString source = event.value("source").toString();
    const QString session = event.value("session").toString();
    const QString sourceLabel = session.isEmpty() ? source : source + " · " + session;

    // Feed to tips engine
    if (m_tips) {
        m_tips->processEvent(eventName, event);
    }

    // Look up action mapping
    if (!m_eventMap.contains(eventName)) {
        qDebug() << "EventRouter: No action for event:" << eventName;
        return;
    }

    const EventAction action = m_eventMap.value(eventName);

    // Trigger animation — HighPriority for active events (tap-style reactions
    // that should feel snappy), NormalPriority for passive/state events so
    // they queue rather than truncate an ongoing reaction.
    if (!action.animation.isEmpty()) {
        const bool active = s_activeEvents.contains(eventName);
        // Use the engine that has animations loaded (Live2D > Lottie > Sprite).
        // Each engine accepts a fallback chain (QStringList) and plays the
        // first group that has motions for the loaded model.
#ifdef OAI_LIVE2D_SUPPORT
        if (m_live2dEngine && m_live2dEngine->hasAnimations()) {
            m_live2dEngine->playAnimationChain(action.animation,
                active ? Live2DAnimationEngine::HighPriority
                       : Live2DAnimationEngine::NormalPriority);
        } else
#endif
        if (m_lottieEngine && m_lottieEngine->hasAnimations()) {
            // Lottie + sprite engines take a single name — try the first
            // entry in the chain; they don't have the multi-group concept.
            m_lottieEngine->playAnimation(action.animation.first(),
                active ? LottieAnimationEngine::HighPriority
                       : LottieAnimationEngine::NormalPriority);
        } else if (m_engine) {
            m_engine->playAnimation(action.animation.first(),
                active ? SpriteAnimationEngine::HighPriority
                       : SpriteAnimationEngine::NormalPriority);
        }
    }

    // Tip text — pulled from TipsCatalog (per-locale JSON). Empty title means
    // this event doesn't surface a bubble (e.g. session.idle, file.watched).
    const TipsCatalog::Tip tip = TipsCatalog::instance().eventTip(eventName);
    if (!tip.title.isEmpty() && m_tipBubble) {
        m_tipBubble->showBubble(tip.title, tip.body, TipBubbleWidget::TipBubble, sourceLabel);
    }
}

void EventRouter::triggerEvent(const QString &eventName)
{
    // Synthetic local event (e.g. "user.click") — bypass IPC validation
    // and dispatch directly. Wraps the same animation-fallback logic as
    // routeEvent() so mouse interaction respects the manifest's eventMap.
    if (!m_eventMap.contains(eventName)) {
        return;
    }
    const EventAction action = m_eventMap.value(eventName);
    if (action.animation.isEmpty()) return;

#ifdef OAI_LIVE2D_SUPPORT
    if (m_live2dEngine && m_live2dEngine->hasAnimations()) {
        m_live2dEngine->playAnimationChain(action.animation, Live2DAnimationEngine::HighPriority);
        return;
    }
#endif
    if (m_lottieEngine && m_lottieEngine->hasAnimations()) {
        m_lottieEngine->playAnimation(action.animation.first(), LottieAnimationEngine::HighPriority);
    } else if (m_engine) {
        m_engine->playAnimation(action.animation.first(), SpriteAnimationEngine::HighPriority);
    }
}

void EventRouter::initEventMap()
{
    // Animation chains only — tip text is pulled per-event from
    // TipsCatalog at dispatch time so the JSON catalog is the single
    // source of truth for tip copy (and authors can add a locale
    // without re-running lupdate / lrelease).
    using SL = QStringList;

    // Sprite-pack defaults: each engine maps these canonical names to
    // its own animations. Live2D pack manifests override with their
    // own chains via CharacterPack::eventMap().
    m_eventMap["session.start"]        = {SL{"greet"},      "", ""};
    m_eventMap["session.end"]          = {SL{"rest"},       "", ""};
    m_eventMap["session.idle"]         = {SL{"rest"},       "", ""};
    m_eventMap["session.error"]        = {SL{"alert"},      "", ""};
    m_eventMap["prompt.submitted"]     = {SL{"think"},      "", ""};
    m_eventMap["tool.before"]          = {SL{"work"},       "", ""};
    m_eventMap["tool.after"]           = {SL{},             "", ""};
    m_eventMap["tool.failed"]          = {SL{"alert"},      "", ""};
    m_eventMap["permission.requested"] = {SL{"attention"},  "", ""};
    m_eventMap["permission.denied"]    = {SL{"alert"},      "", ""};
    m_eventMap["permission.response"]  = {SL{},             "", ""};
    m_eventMap["subagent.started"]     = {SL{"work"},       "", ""};
    m_eventMap["subagent.stopped"]     = {SL{},             "", ""};
    m_eventMap["notification.sent"]    = {SL{},             "", ""};
    m_eventMap["file.edited"]          = {SL{"send"},       "", ""};
    m_eventMap["file.watched"]         = {SL{},             "", ""};
    m_eventMap["todo.updated"]         = {SL{"celebrate"},  "", ""};

    // Synthetic local events for mouse interaction. Live2D pack manifests
    // typically supply chains like ["TouchBody","TouchHead","Tap"]; sprite
    // packs use generic "tap"/"doubleclick" animation names. The chain is
    // declared by the manifest, not the engine.
    m_eventMap["user.click"]       = {SL{"tap"}, "", ""};
    m_eventMap["user.doubleclick"] = {SL{"doubleclick", "tap"}, "", ""};
}

void EventRouter::retranslateUi()
{
    initEventMap();
}

void EventRouter::loadFromCharacterPack(const CharacterPack *pack)
{
    if (!pack || !pack->isValid()) {
        qWarning() << "EventRouter: Invalid sprite pack";
        return;
    }

    // Get event map from pack
    const auto &eventMap = pack->eventMap();
    if (eventMap.isEmpty()) {
        qDebug() << "EventRouter: No event map in sprite pack, using defaults";
        return;
    }

    // Ensure defaults are initialized so tip text is available
    if (m_eventMap.isEmpty()) {
        initEventMap();
    }

    // Merge pack animation names into existing event map, preserving tip text
    for (auto it = eventMap.begin(); it != eventMap.end(); ++it) {
        const QString eventName = it.key();
        const QStringList animationChain = it.value();

        if (m_eventMap.contains(eventName)) {
            // Update animation chain but keep existing tip text
            m_eventMap[eventName].animation = animationChain;
        } else {
            // New event not in defaults — add with animation only
            EventAction action;
            action.animation = animationChain;
            m_eventMap[eventName] = action;
        }
    }

    qDebug() << "EventRouter: Merged" << eventMap.size() << "animation mappings from sprite pack";
}

bool EventRouter::validateEvent(const QJsonObject &event) const
{
    // Check message type
    if (event.value("type").toString() != "event") {
        qWarning() << "EventRouter: Not an event message";
        return false;
    }

    // Check source field (accept any non-empty source for extensibility)
    const QString source = event.value("source").toString();
    if (source.isEmpty()) {
        qWarning() << "EventRouter: Missing source field";
        return false;
    }
    if (!s_validSources.contains(source)) {
        qDebug() << "EventRouter: Unknown source (accepted):" << source;
    }

    // Check event name
    const QString eventName = event.value("event").toString();
    if (!s_validEvents.contains(eventName)) {
        qWarning() << "EventRouter: Unknown event name:" << eventName;
        return false;
    }

    return true;
}
