#include "EventRouter.h"
#include "SpriteAnimationEngine.h"
#include "LottieAnimationEngine.h"
#ifdef OAI_LIVE2D_SUPPORT
#include "Live2DAnimationEngine.h"
#endif
#include "CharacterPack.h"
#include "TipBubbleWidget.h"
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

    // Trigger animation — always use HighPriority so event animations
    // immediately interrupt idle/previous animations
    if (!action.animation.isEmpty()) {
        // Use the engine that has animations loaded (Live2D > Lottie > Sprite)
#ifdef OAI_LIVE2D_SUPPORT
        if (m_live2dEngine && m_live2dEngine->hasAnimations()) {
            m_live2dEngine->playAnimation(action.animation, Live2DAnimationEngine::HighPriority);
        } else
#endif
        if (m_lottieEngine && m_lottieEngine->hasAnimations()) {
            m_lottieEngine->playAnimation(action.animation, LottieAnimationEngine::HighPriority);
        } else if (m_engine) {
            m_engine->playAnimation(action.animation, SpriteAnimationEngine::HighPriority);
        }
    }

    // Show tip with source label
    if (!action.tipTitle.isEmpty() && m_tipBubble) {
        m_tipBubble->showBubble(action.tipTitle, action.tipBody, TipBubbleWidget::TipBubble, sourceLabel);
    }
}

void EventRouter::initEventMap()
{
    // Canonical animation names (skin-agnostic):
    //   greet, idle, think, work, alert, celebrate, rest, send, attention
    // Each skin maps these to actual animation names in SpriteAnimationEngine.

    // Session events
    m_eventMap["session.start"] = {"greet", tr("Session started"), tr("Let's get to work!")};
    m_eventMap["session.end"] = {"rest", tr("Session ended"), tr("Good job today!")};
    m_eventMap["session.idle"] = {"rest", "", ""};
    m_eventMap["session.error"] = {"alert", tr("Oops!"), tr("Something went wrong. Check the logs!")};

    // Prompt
    m_eventMap["prompt.submitted"] = {"think", tr("Thinking..."), tr("Give me a moment to process that.")};

    // Tool events
    m_eventMap["tool.before"] = {"work", tr("Tool running"), tr("Executing command...")};
    m_eventMap["tool.after"] = {"", tr("Done!"), tr("Command completed successfully.")};
    m_eventMap["tool.failed"] = {"alert", tr("Tool failed"), tr("The command didn't work. Try again?")};

    // Permission events
    m_eventMap["permission.requested"] = {"attention", tr("Permission needed"), tr("Please approve the requested action.")};
    m_eventMap["permission.denied"] = {"alert", tr("Denied"), tr("Permission was denied.")};
    m_eventMap["permission.response"] = {"", "", ""};

    // Subagent events
    m_eventMap["subagent.started"] = {"work", tr("Subagent started"), tr("A helper is working on a task.")};
    m_eventMap["subagent.stopped"] = {"", tr("Subagent done"), tr("The helper has finished.")};

    // Notification
    m_eventMap["notification.sent"] = {"", tr("Notification"), tr("You have a new message!")};

    // File events
    m_eventMap["file.edited"] = {"send", tr("File saved"), tr("Your changes have been saved.")};
    m_eventMap["file.watched"] = {"", "", ""};

    // Todo
    m_eventMap["todo.updated"] = {"celebrate", tr("Task complete!"), tr("Nice work checking off that todo!")};
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
        const QString animationName = it.value();

        if (m_eventMap.contains(eventName)) {
            // Update animation but keep existing tip text
            m_eventMap[eventName].animation = animationName;
        } else {
            // New event not in defaults — add with animation only
            EventAction action;
            action.animation = animationName;
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
