#include "EventRouter.h"
#include "SpriteAnimationEngine.h"
#include "TipBubbleWidget.h"
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

    // Trigger animation — always use HighPriority so event animations
    // immediately interrupt idle/previous animations
    if (!action.animation.isEmpty() && m_engine) {
        m_engine->playAnimation(action.animation, SpriteAnimationEngine::HighPriority);
    }

    // Trigger visual effect
    if (!action.effect.isEmpty() && m_effects) {
        m_effects->triggerEffect(action.effect);
    }

    // Show tip
    if (!action.tipTitle.isEmpty() && m_tipBubble) {
        m_tipBubble->showBubble(action.tipTitle, action.tipBody, TipBubbleWidget::TipBubble);
    }
}

void EventRouter::initEventMap()
{
    // Session events
    m_eventMap["session.start"] = {"wave", "", tr("Session started"), tr("Let's get to work!")};
    m_eventMap["session.end"] = {"rest", "", tr("Session ended"), tr("Good job today!")};
    m_eventMap["session.idle"] = {"rest", "", "", ""};
    m_eventMap["session.error"] = {"alert", "alert-pulse", tr("Oops!"), tr("Something went wrong. Check the logs!")};

    // Prompt
    m_eventMap["prompt.submitted"] = {"thinking", "thinking-dots", tr("Thinking..."), tr("Give me a moment to process that.")};

    // Tool events
    m_eventMap["tool.before"] = {"explain", "", tr("Tool running"), tr("Executing command...")};
    m_eventMap["tool.after"] = {"", "sparkles", tr("Done!"), tr("Command completed successfully.")};
    m_eventMap["tool.failed"] = {"alert", "alert-pulse", tr("Tool failed"), tr("The command didn't work. Try again?")};

    // Permission events
    m_eventMap["permission.requested"] = {"getattentionyawn", "alert-pulse", tr("Permission needed"), tr("Please approve the requested action.")};
    m_eventMap["permission.denied"] = {"alert", "", tr("Denied"), tr("Permission was denied.")};
    m_eventMap["permission.response"] = {"", "", "", ""};

    // Subagent events
    m_eventMap["subagent.started"] = {"explain", "", tr("Subagent started"), tr("A helper is working on a task.")};
    m_eventMap["subagent.stopped"] = {"", "sparkles", tr("Subagent done"), tr("The helper has finished.")};

    // Notification
    m_eventMap["notification.sent"] = {"", "speech-pop", tr("Notification"), tr("You have a new message!")};

    // File events
    m_eventMap["file.edited"] = {"sendmail", "sparkles", tr("File saved"), tr("Your changes have been saved.")};
    m_eventMap["file.watched"] = {"", "", "", ""};

    // Todo
    m_eventMap["todo.updated"] = {"congratulate", "confetti", tr("Task complete!"), tr("Nice work checking off that todo!")};
}

void EventRouter::retranslateUi()
{
    initEventMap();
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
