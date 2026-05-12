#include "EventRouter.h"
#include "TipWidget.h"
#include "TipsEngine.h"
#include "TipsCatalog.h"

#include <QJsonObject>
#include <QDebug>

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

EventRouter::EventRouter(QObject *parent) : QObject(parent) {}

void EventRouter::routeEvent(const QJsonObject &event)
{
    if (!validateEvent(event)) return;

    const QString eventName = event.value("event").toString();
    const QString source = event.value("source").toString();
    const QString session = event.value("session").toString();
    const QString sourceLabel = session.isEmpty() ? source : source + " · " + session;

    if (m_tips) {
        m_tips->processEvent(eventName, event);
    }

    emit eventProcessed(eventName, event);

    const TipsCatalog::Tip tip = TipsCatalog::instance().eventTip(eventName);
    if (!tip.title.isEmpty() && m_tipWidget) {
        m_tipWidget->showBubble(tip.title, tip.body, TipWidget::TipBubble, sourceLabel);
    }
}

bool EventRouter::validateEvent(const QJsonObject &event) const
{
    if (event.value("type").toString() != "event") {
        qWarning() << "EventRouter: Not an event message";
        return false;
    }
    const QString source = event.value("source").toString();
    if (source.isEmpty()) {
        qWarning() << "EventRouter: Missing source field";
        return false;
    }
    if (!s_validSources.contains(source)) {
        qDebug() << "EventRouter: Unknown source (accepted):" << source;
    }
    const QString eventName = event.value("event").toString();
    if (!s_validEvents.contains(eventName)) {
        qWarning() << "EventRouter: Unknown event name:" << eventName;
        return false;
    }
    return true;
}
