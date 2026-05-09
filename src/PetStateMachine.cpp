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

    // One-shots
    if (eventName == "session.error"
        || eventName == "tool.failed"
        || eventName == "permission.denied") {
        enterOneShot(State::Failed, NOTIFICATION_ONESHOT_MS);
        return;
    }

    if (eventName == "notification.sent") {
        enterOneShot(State::Reviewing, NOTIFICATION_ONESHOT_MS);
        return;
    }

    if (eventName == "todo.updated") {
        if (payload.value("status").toString() == "completed") {
            enterOneShot(State::Celebrating, NOTIFICATION_ONESHOT_MS);
        }
        return;
    }

    if (eventName == "session.start") {
        if (m_baseState == State::Idle && m_overlayState == State::Idle) {
            enterOneShot(State::Greeting, NOTIFICATION_ONESHOT_MS);
        }
        return;
    }

    if (eventName == "session.end" || eventName == "session.idle") {
        m_workingGrace.stop();
        m_thinkingTimeout.stop();
        m_reviewingTimeout.stop();
        enterBase(State::Idle, NormalPriority);
        return;
    }
}
void PetStateMachine::onSyntheticEvent(const QString &eventName)
{
    if (eventName == "user.click" || eventName == "user.doubleclick") {
        if (m_baseState == State::Idle && m_overlayState == State::Idle) {
            enterOneShot(State::Greeting, NOTIFICATION_ONESHOT_MS);
        }
    }
    // Hover events (user.hoverEnter / user.hoverLeave) are accepted but
    // intentionally ignored: they're not state-changing per the spec.
    // MainWindow still emits them so a future hover-feedback feature can
    // hook in here without touching the call sites.
}
void PetStateMachine::onPositionChanged(const QPoint &oldPos, const QPoint &newPos, bool isUserDrag)
{
    if (isUserDrag) return;  // user is dragging; no walk overlay

    const int dx = newPos.x() - oldPos.x();
    const int dy = newPos.y() - oldPos.y();
    if (qAbs(dx) < WALK_DELTA_THRESHOLD_PX && qAbs(dy) < WALK_DELTA_THRESHOLD_PX) {
        return;
    }

    WalkDir dir = (dx >= 0) ? WalkDir::Right : WalkDir::Left;
    if (m_walking && dir == m_walkDir) {
        return;  // already walking that direction
    }
    m_walking = true;
    m_walkDir = dir;

    QStringList chain;
    chain << ((dir == WalkDir::Right) ? "running-right" : "running-left");
    chain << "walk" << "Walk";
    if (!chain.contains(m_idleFallback)) {
        chain.append(m_idleFallback);
    }
    emit animationRequested(chain, static_cast<int>(HighPriority));
    // NOTE: m_walking is intentionally not reset here. There is no
    // "motion stopped" detector today; the next event-driven state
    // transition will emit a fresh chain and override the walking
    // animation visually. When a real producer (gaming-mode return,
    // wander timer) lands, add a short-debounce timer that flips
    // m_walking back to false and re-emits the underlying base state's
    // chain.
}
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
void PetStateMachine::onOneShotFinished()
{
    if (m_overlayState == State::Idle) return;
    m_overlayState = State::Idle;
    // Restore from saved-sustained because m_baseState may have changed
    // during the overlay (e.g. a tool.before arrived while Failed was
    // showing). m_savedSustained holds whatever was active when the
    // one-shot started.
    if (m_baseState != m_savedSustained) {
        m_baseState = m_savedSustained;
    }
    emit stateChanged(activeState());
    emitChainFor(m_baseState, NormalPriority);
}

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
void PetStateMachine::enterOneShot(State s, int durationMs)
{
    // Save the sustained state so we can restore on completion. Grace
    // timers keep ticking under the overlay so the underlying state's
    // exit conditions still apply.
    if (m_overlayState == State::Idle) {
        m_savedSustained = m_baseState;
    }
    m_overlayState = s;
    m_oneShotTimer.start(durationMs);
    emit stateChanged(activeState());
    emitChainFor(s, HighPriority);
}
void PetStateMachine::emitChainFor(State s, Priority priority)
{
    QStringList chain = resolveChain(s);
    if (!m_idleFallback.isEmpty() && !chain.contains(m_idleFallback)) {
        chain.append(m_idleFallback);
    }
    emit animationRequested(chain, static_cast<int>(priority));
}
void PetStateMachine::rebuildChainsFromNameMap(const QMap<QString, QString> &nameMap)
{
    rebuildChainsFromMaps({}, nameMap);
}

void PetStateMachine::rebuildChainsFromMaps(const QMap<QString, QStringList> &stateMap,
                                            const QMap<QString, QString> &nameMap)
{
    struct Mapping { State state; const char *stateKey; QStringList canonicalCandidates; };
    const QVector<Mapping> mappings = {
        {State::Idle,        "Idle",        {"idle", "rest"}},
        {State::Greeting,    "Greeting",    {"greet", "wave", "greeting"}},
        {State::Thinking,    "Thinking",    {"think", "thinking"}},
        {State::Working,     "Working",     {"work", "processing", "writing", "searching"}},
        {State::Reviewing,   "Reviewing",   {"attention", "gettechy", "getwizardy"}},
        {State::Failed,      "Failed",      {"alert"}},
        {State::Celebrating, "Celebrating", {"celebrate", "congratulate"}},
    };

    // Stash engine defaults before we overwrite, so we can re-append.
    QMap<State, QStringList> engineDefaults;
    for (const auto &m : mappings) {
        engineDefaults[m.state] = m_chains.value(m.state);
    }

    for (const auto &m : mappings) {
        QStringList chain;

        // 1. Pack-author explicit stateMap (highest priority).
        const QStringList override = stateMap.value(m.stateKey);
        for (const QString &name : override) {
            if (!name.isEmpty() && !chain.contains(name)) chain.append(name);
        }

        // 2. Translated nameMap entries.
        for (const QString &canonical : m.canonicalCandidates) {
            const QString actual = nameMap.value(canonical);
            if (!actual.isEmpty() && !chain.contains(actual)) {
                chain.append(actual);
            }
        }

        // 3. Engine defaults.
        for (const QString &name : engineDefaults.value(m.state)) {
            if (!chain.contains(name)) chain.append(name);
        }

        m_chains[m.state] = chain;
    }

    const QString packIdle = nameMap.value("idle", nameMap.value("rest"));
    if (!packIdle.isEmpty()) {
        m_idleFallback = packIdle;
    }
}

void PetStateMachine::rebuildChainsFromPack(const CharacterPack *pack)
{
    if (!pack || !pack->isValid()) return;
    rebuildChainsFromMaps(pack->stateMap(), pack->nameMap());
}

void PetStateMachine::onActivePackChanged(const CharacterPack *pack)
{
    rebuildChainsFromPack(pack);
}

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
