#include "EventRouter.h"
#include "SpriteAnimationEngine.h"
#include "SpeechBubble.h"
#include "LottieEffectOverlay.h"
#include "TipsEngine.h"

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

    // Trigger animation
    if (!action.animation.isEmpty() && m_engine) {
        m_engine->playAnimation(action.animation,
            eventName.contains("permission") || eventName.contains("error")
            ? SpriteAnimationEngine::HighPriority
            : SpriteAnimationEngine::NormalPriority);
    }

    // Trigger visual effect
    if (!action.effect.isEmpty() && m_effects) {
        m_effects->triggerEffect(action.effect);
    }

    // Show tip
    if (!action.tipTitle.isEmpty() && m_bubble) {
        m_bubble->showMessage(action.tipTitle, action.tipBody, SpeechBubble::TipBubble);
    }
}

void EventRouter::initEventMap()
{
    // Session events
    m_eventMap["session.start"] = {"wave", "", "", ""};
    m_eventMap["session.end"] = {"rest", "", "", ""};
    m_eventMap["session.idle"] = {"rest", "", "", ""};
    m_eventMap["session.error"] = {"alert", "alert-pulse", "", ""};

    // Prompt
    m_eventMap["prompt.submitted"] = {"thinking", "thinking-dots", "", ""};

    // Tool events
    m_eventMap["tool.before"] = {"explain", "", "", ""};
    m_eventMap["tool.after"] = {"", "sparkles", "", ""};
    m_eventMap["tool.failed"] = {"alert", "alert-pulse", "", ""};

    // Permission events
    m_eventMap["permission.requested"] = {"getattentionyawn", "alert-pulse", "", ""};
    m_eventMap["permission.denied"] = {"alert", "", "", ""};
    m_eventMap["permission.response"] = {"", "", "", ""};

    // Subagent events
    m_eventMap["subagent.started"] = {"explain", "", "", ""};
    m_eventMap["subagent.stopped"] = {"", "sparkles", "", ""};

    // Notification
    m_eventMap["notification.sent"] = {"", "speech-pop", "", ""};

    // File events
    m_eventMap["file.edited"] = {"sendmail", "sparkles", "", ""};
    m_eventMap["file.watched"] = {"", "", "", ""};

    // Todo
    m_eventMap["todo.updated"] = {"congratulate", "confetti", "", ""};
}

bool EventRouter::validateEvent(const QJsonObject &event) const
{
    // Check message type
    if (event.value("type").toString() != "event") {
        qWarning() << "EventRouter: Not an event message";
        return false;
    }

    // Check source field
    const QString source = event.value("source").toString();
    if (!s_validSources.contains(source)) {
        qWarning() << "EventRouter: Invalid or missing source:" << source;
        return false;
    }

    // Check event name
    const QString eventName = event.value("event").toString();
    if (!s_validEvents.contains(eventName)) {
        qWarning() << "EventRouter: Unknown event name:" << eventName;
        return false;
    }

    return true;
}
