#include "PetStateMachine.h"

#include "CharacterPack.h"

#include <QDebug>

PetStateMachine::PetStateMachine(QObject *parent)
    : QObject(parent)
{
    m_workingGrace.setSingleShot(true);
    m_workingGrace.setInterval(WORKING_GRACE_MS);
    connect(&m_workingGrace, &QTimer::timeout, this, &PetStateMachine::onWorkingGraceExpired);

    m_thinkingTimeout.setSingleShot(true);
    m_thinkingTimeout.setInterval(THINKING_TIMEOUT_MS);
    connect(&m_thinkingTimeout, &QTimer::timeout, this, &PetStateMachine::onThinkingTimeout);

    m_reviewingTimeout.setSingleShot(true);
    m_reviewingTimeout.setInterval(REVIEWING_TIMEOUT_MS);
    connect(&m_reviewingTimeout, &QTimer::timeout, this, &PetStateMachine::onReviewingTimeout);

    m_oneShotTimer.setSingleShot(true);
    connect(&m_oneShotTimer, &QTimer::timeout, this, &PetStateMachine::onOneShotFinished);

    // Engine-default chains (overridden when a pack loads).
    m_chains[State::Idle]        = {"idle", "Idle"};
    m_chains[State::Greeting]    = {"waving", "greet", "Login", "Tap"};
    m_chains[State::Thinking]    = {"waiting", "think", "Thinking", "TouchHead", "Tap"};
    m_chains[State::Working]     = {"running", "work", "Processing", "Writing", "TouchBody", "Tap"};
    m_chains[State::Reviewing]   = {"review", "attention", "GetAttention", "TouchHead", "Tap"};
    m_chains[State::Failed]      = {"failed", "alert", "Alert", "TouchHead", "Tap"};
    m_chains[State::Celebrating] = {"jumping", "celebrate", "Congratulate", "TouchBody", "Tap"};
}

PetStateMachine::State PetStateMachine::activeState() const
{
    return (m_overlayState != State::Idle) ? m_overlayState : m_baseState;
}

// --- Stubs (filled in by later tasks) ---------------------------------------

void PetStateMachine::onCanonicalEvent(const QString &eventName, const QJsonObject &payload)
{
    Q_UNUSED(payload);

    if (eventName == "prompt.submitted") {
        m_thinkingTimeout.start();
        enterBase(State::Thinking, HighPriority);
        return;
    }

    if (eventName == "permission.requested") {
        m_reviewingTimeout.start();
        enterBase(State::Reviewing, HighPriority);
        return;
    }

    if (eventName == "permission.response") {
        if (m_baseState == State::Reviewing) {
            m_reviewingTimeout.stop();
            enterBase(State::Idle, NormalPriority);
        }
        return;
    }

    // Sustained: tool.before / subagent.started / file.edited → Working
    if (eventName == "tool.before"
        || eventName == "subagent.started"
        || eventName == "file.edited") {
        m_thinkingTimeout.stop();
        enterBase(State::Working, HighPriority);
        m_workingGrace.start();
        return;
    }

    // Passive: extend Working grace if currently Working.
    if (eventName == "tool.after"
        || eventName == "subagent.stopped"
        || eventName == "file.watched") {
        if (m_baseState == State::Working) {
            m_workingGrace.start();  // restart from 0
        }
        return;
    }
}
void PetStateMachine::onSyntheticEvent(const QString &) {}
void PetStateMachine::onPositionChanged(const QPoint &, const QPoint &, bool) {}
void PetStateMachine::onActivePackChanged(const CharacterPack *) {}

void PetStateMachine::onWorkingGraceExpired()
{
    if (m_baseState == State::Working) {
        enterBase(State::Idle, NormalPriority);
    }
}
void PetStateMachine::onThinkingTimeout()
{
    if (m_baseState == State::Thinking) {
        enterBase(State::Idle, NormalPriority);
    }
}
void PetStateMachine::onReviewingTimeout()
{
    if (m_baseState == State::Reviewing) {
        enterBase(State::Idle, NormalPriority);
    }
}
void PetStateMachine::onOneShotFinished() {}

void PetStateMachine::enterBase(State s, Priority priority)
{
    if (m_baseState == s && m_overlayState == State::Idle) {
        // Already there; just refresh timer-driven entries by re-emitting if needed.
        // For now, no-op to avoid spam.
        return;
    }
    m_baseState = s;
    if (m_overlayState != State::Idle) {
        // Save it; will restore when overlay finishes.
        // (Overlay is not active in this code path, but kept for clarity.)
    }
    emit stateChanged(activeState());
    emitChainFor(s, priority);
}
void PetStateMachine::enterOneShot(State, int) {}
void PetStateMachine::emitChainFor(State s, Priority priority)
{
    QStringList chain = resolveChain(s);
    if (!m_idleFallback.isEmpty() && !chain.contains(m_idleFallback)) {
        chain.append(m_idleFallback);
    }
    emit animationRequested(chain, static_cast<int>(priority));
}
void PetStateMachine::rebuildChainsFromPack(const CharacterPack *) {}
QStringList PetStateMachine::resolveChain(State s) const { return m_chains.value(s); }

QString PetStateMachine::stateName(State s)
{
    switch (s) {
        case State::Idle: return "Idle";
        case State::Greeting: return "Greeting";
        case State::Thinking: return "Thinking";
        case State::Working: return "Working";
        case State::Reviewing: return "Reviewing";
        case State::Failed: return "Failed";
        case State::Celebrating: return "Celebrating";
    }
    return "?";
}
