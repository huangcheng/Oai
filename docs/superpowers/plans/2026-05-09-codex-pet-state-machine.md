# Codex Pet State Machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `EventRouter`'s direct animation dispatch and `EmotionEngine`'s mood fan-out with a single `PetStateMachine` that owns the pet's logical state, with per-state fallback chains so all pack types (Codex, native `.opk`, Live2D, Lottie) render correctly.

**Architecture:** New `PetStateMachine` Qt class with eight states (`Idle`, `Greeting`, `Thinking`, `Working`, `Reviewing`, `Failed`, `Celebrating`) plus a `Walking` overlay driven by position deltas. `EventRouter` shrinks to validation + tip-text. `EmotionEngine` is deleted. `CharacterPack` gains an optional `stateMap` field; `loadFromCodexPet` synthesizes one automatically from the existing nameMap.

**Tech Stack:** C++17, Qt6 (Core, Test), CMake, Qt signals/slots, `QTimer` for grace windows.

**Spec:** `docs/superpowers/specs/2026-05-09-codex-pet-state-machine-design.md`

---

## Task 1: Add `PetStateMachine` skeleton — header only

**Files:**
- Create: `src/PetStateMachine.h`
- Modify: `CMakeLists.txt:318` (add `src/PetStateMachine.h` after `src/CharacterPackManager.h`)
- Modify: `tests/CMakeLists.txt` (add `${CMAKE_SOURCE_DIR}/src/PetStateMachine.h` to `SEELIEPET_LIB_SOURCES`)

- [ ] **Step 1: Create `src/PetStateMachine.h` with the public surface**

```cpp
#ifndef PETSTATEMACHINE_H
#define PETSTATEMACHINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPoint>
#include <QMap>
#include <QTimer>
#include <QJsonObject>

class CharacterPack;

/**
 * @brief Owns "what is the pet doing right now."
 *
 * Eight abstract states map gateway events onto an animation chain that all
 * pack types resolve into their own animations. Sustained states (Working,
 * Thinking, Reviewing) hold across event bursts via grace timers; one-shot
 * states (Greeting, Failed, Celebrating) play once and return to the saved
 * sustained state. Walking is an overlay driven by position deltas.
 */
class PetStateMachine : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Greeting,
        Thinking,
        Working,
        Reviewing,
        Failed,
        Celebrating,
    };
    Q_ENUM(State)

    enum class WalkDir { None, Left, Right };
    Q_ENUM(WalkDir)

    enum Priority {
        HighPriority,
        NormalPriority,
    };

    explicit PetStateMachine(QObject *parent = nullptr);

    State baseState() const { return m_baseState; }
    State activeState() const;  // overlay if set, else baseState

public slots:
    void onCanonicalEvent(const QString &eventName, const QJsonObject &payload = {});
    void onSyntheticEvent(const QString &eventName);
    void onPositionChanged(const QPoint &oldPos, const QPoint &newPos, bool isUserDrag);
    void onActivePackChanged(const CharacterPack *pack);

signals:
    void animationRequested(const QStringList &chain, int priority);
    void stateChanged(PetStateMachine::State newState);

private slots:
    void onWorkingGraceExpired();
    void onThinkingTimeout();
    void onReviewingTimeout();
    void onOneShotFinished();

private:
    void enterBase(State s, Priority priority);
    void enterOneShot(State s, int durationMs);
    void emitChainFor(State s, Priority priority);
    void rebuildChainsFromPack(const CharacterPack *pack);
    QStringList resolveChain(State s) const;

    static QString stateName(State s);

    State m_baseState = State::Idle;
    State m_overlayState = State::Idle;     // Idle == "no overlay"
    State m_savedSustained = State::Idle;
    bool m_walking = false;
    WalkDir m_walkDir = WalkDir::None;

    QTimer m_workingGrace;
    QTimer m_thinkingTimeout;
    QTimer m_reviewingTimeout;
    QTimer m_oneShotTimer;

    QMap<State, QStringList> m_chains;
    QString m_idleFallback = QStringLiteral("idle");

    static constexpr int WORKING_GRACE_MS = 1500;
    static constexpr int THINKING_TIMEOUT_MS = 15000;
    static constexpr int REVIEWING_TIMEOUT_MS = 60000;
    static constexpr int NOTIFICATION_ONESHOT_MS = 2000;
    static constexpr int WALK_DELTA_THRESHOLD_PX = 32;
};

#endif // PETSTATEMACHINE_H
```

- [ ] **Step 2: Add the file to the main app build**

Edit `CMakeLists.txt`. Find line 334 (`src/CharacterPackManager.h`) and add immediately after it:

```cmake
    src/PetStateMachine.cpp
    src/PetStateMachine.h
```

- [ ] **Step 3: Add the file to the test library sources**

Edit `tests/CMakeLists.txt`. Find the `SEELIEPET_LIB_SOURCES` block (line 1). Add these two entries before the `${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.c` line:

```cmake
    ${CMAKE_SOURCE_DIR}/src/PetStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/PetStateMachine.h
```

- [ ] **Step 4: Commit (header only — code follows)**

```bash
git add src/PetStateMachine.h CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(fsm): add PetStateMachine header skeleton"
```

---

## Task 2: Write the failing test — Idle is the default state

**Files:**
- Create: `tests/test_pet_state_machine.cpp`
- Modify: `tests/CMakeLists.txt` (add to `TEST_SOURCES`)

- [ ] **Step 1: Create `tests/test_pet_state_machine.cpp` with the first test**

```cpp
/**
 * test_pet_state_machine.cpp
 *
 * Unit tests for PetStateMachine — no UDP, no engines, no widgets.
 * Drives the FSM via slots and observes animationRequested / stateChanged.
 */

#include <QTest>
#include <QSignalSpy>
#include <QJsonObject>

#include "PetStateMachine.h"

class TestPetStateMachine : public QObject
{
    Q_OBJECT

private slots:
    void testInitialStateIsIdle();

private:
    PetStateMachine *m_fsm = nullptr;

    void initFsm() {
        delete m_fsm;
        m_fsm = new PetStateMachine(this);
    }
};

void TestPetStateMachine::testInitialStateIsIdle()
{
    initFsm();
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Idle);
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Idle);
}

QTEST_MAIN(TestPetStateMachine)
#include "test_pet_state_machine.moc"
```

- [ ] **Step 2: Add the test to `tests/CMakeLists.txt`**

Edit `tests/CMakeLists.txt`. Find the `TEST_SOURCES` block (around line 35):

```cmake
set(TEST_SOURCES
    test_ipc_animations.cpp
    test_ecg.cpp
    test_gaming_mode.cpp
    test_pet_state_machine.cpp
)
```

- [ ] **Step 3: Run the test to verify it fails (no .cpp implementation yet)**

```bash
cd build && cmake --build . --target test_pet_state_machine 2>&1 | tail -20
```

Expected: link error — `undefined reference to PetStateMachine::PetStateMachine`. This is correct; we have only the header.

- [ ] **Step 4: Commit (failing test in place, no impl yet — TDD red phase)**

```bash
git add tests/test_pet_state_machine.cpp tests/CMakeLists.txt
git commit -m "test(fsm): failing test for default Idle state"
```

---

## Task 3: Implement the minimal `PetStateMachine.cpp` to pass Task 2's test

**Files:**
- Create: `src/PetStateMachine.cpp`

- [ ] **Step 1: Create `src/PetStateMachine.cpp` with the constructor and accessors**

```cpp
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

void PetStateMachine::onCanonicalEvent(const QString &, const QJsonObject &) {}
void PetStateMachine::onSyntheticEvent(const QString &) {}
void PetStateMachine::onPositionChanged(const QPoint &, const QPoint &, bool) {}
void PetStateMachine::onActivePackChanged(const CharacterPack *) {}

void PetStateMachine::onWorkingGraceExpired() {}
void PetStateMachine::onThinkingTimeout() {}
void PetStateMachine::onReviewingTimeout() {}
void PetStateMachine::onOneShotFinished() {}

void PetStateMachine::enterBase(State, Priority) {}
void PetStateMachine::enterOneShot(State, int) {}
void PetStateMachine::emitChainFor(State, Priority) {}
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
```

- [ ] **Step 2: Build and run the test**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: test passes — initial state is Idle.

- [ ] **Step 3: Commit**

```bash
git add src/PetStateMachine.cpp
git commit -m "feat(fsm): minimal PetStateMachine to pass initial-state test"
```

---

## Task 4: Sustained transition — `tool.before` enters Working

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`

- [ ] **Step 1: Add the failing test**

Add to the `private slots:` section in `tests/test_pet_state_machine.cpp`:

```cpp
    void testToolBeforeEntersWorking();
```

Add the test method body before `QTEST_MAIN`:

```cpp
void TestPetStateMachine::testToolBeforeEntersWorking()
{
    initFsm();
    QSignalSpy chainSpy(m_fsm, &PetStateMachine::animationRequested);
    QSignalSpy stateSpy(m_fsm, &PetStateMachine::stateChanged);

    m_fsm->onCanonicalEvent("tool.before");

    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Working);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.first().at(0).value<PetStateMachine::State>(),
             PetStateMachine::State::Working);
    QCOMPARE(chainSpy.count(), 1);
    const QStringList chain = chainSpy.first().at(0).toStringList();
    QVERIFY(chain.startsWith("running"));
}
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -15
```

Expected: `testToolBeforeEntersWorking` fails — `baseState` is still Idle because `onCanonicalEvent` is a stub.

- [ ] **Step 3: Implement the sustained-event branch**

Replace the stubs in `src/PetStateMachine.cpp` with these implementations:

```cpp
void PetStateMachine::onCanonicalEvent(const QString &eventName, const QJsonObject &payload)
{
    Q_UNUSED(payload);

    // Sustained: tool.before / subagent.started / file.edited → Working
    if (eventName == "tool.before"
        || eventName == "subagent.started"
        || eventName == "file.edited") {
        enterBase(State::Working, HighPriority);
        m_workingGrace.start();
        return;
    }
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

void PetStateMachine::emitChainFor(State s, Priority priority)
{
    QStringList chain = resolveChain(s);
    if (!m_idleFallback.isEmpty() && !chain.contains(m_idleFallback)) {
        chain.append(m_idleFallback);
    }
    emit animationRequested(chain, static_cast<int>(priority));
}
```

- [ ] **Step 4: Run the test**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: both tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp
git commit -m "feat(fsm): tool.before sustained transition to Working"
```

---

## Task 5: Working grace window expires after 1500 ms of silence

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`

- [ ] **Step 1: Add the failing test**

Add to `private slots:`:

```cpp
    void testWorkingGraceExpiresToIdle();
```

Add the body:

```cpp
void TestPetStateMachine::testWorkingGraceExpiresToIdle()
{
    initFsm();
    m_fsm->onCanonicalEvent("tool.before");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Working);

    QSignalSpy stateSpy(m_fsm, &PetStateMachine::stateChanged);
    QTest::qWait(1700);  // > WORKING_GRACE_MS (1500)
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Idle);
    QCOMPARE(stateSpy.count(), 1);
}
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: `testWorkingGraceExpiresToIdle` fails — `onWorkingGraceExpired` is still a stub.

- [ ] **Step 3: Implement the grace expiry**

Replace the `onWorkingGraceExpired` stub in `src/PetStateMachine.cpp`:

```cpp
void PetStateMachine::onWorkingGraceExpired()
{
    if (m_baseState == State::Working) {
        enterBase(State::Idle, NormalPriority);
    }
}
```

- [ ] **Step 4: Run the test**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all three tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp
git commit -m "feat(fsm): Working grace window drops to Idle after 1500ms"
```

---

## Task 6: Working grace extends across `tool.after` → `tool.before` gaps

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`

- [ ] **Step 1: Add the failing test**

Add to `private slots:`:

```cpp
    void testWorkingGraceExtendsAcrossGaps();
```

Add the body:

```cpp
void TestPetStateMachine::testWorkingGraceExtendsAcrossGaps()
{
    initFsm();
    m_fsm->onCanonicalEvent("tool.before");
    QTest::qWait(800);                       // halfway through grace
    m_fsm->onCanonicalEvent("tool.after");   // resets grace via passive handler
    QTest::qWait(800);                       // total 1600ms wall but grace was reset
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Working);

    m_fsm->onCanonicalEvent("tool.before");  // new tool, extends again
    QTest::qWait(800);
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Working);
}
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: pet drops to Idle after 1500ms because `tool.after` is unhandled.

- [ ] **Step 3: Add passive handlers that reset the grace timer**

Append to the body of `onCanonicalEvent` in `src/PetStateMachine.cpp`, after the sustained block:

```cpp
    // Passive: extend Working grace if currently Working.
    if (eventName == "tool.after"
        || eventName == "subagent.stopped"
        || eventName == "file.watched") {
        if (m_baseState == State::Working) {
            m_workingGrace.start();  // restart from 0
        }
        return;
    }
```

- [ ] **Step 4: Run the test**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: passes.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp
git commit -m "feat(fsm): tool.after extends Working grace timer"
```

---

## Task 7: `prompt.submitted` enters Thinking; first `tool.before` takes over

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`

- [ ] **Step 1: Add two failing tests**

Add to `private slots:`:

```cpp
    void testPromptSubmittedEntersThinking();
    void testToolBeforeTakesOverFromThinking();
```

Add the bodies:

```cpp
void TestPetStateMachine::testPromptSubmittedEntersThinking()
{
    initFsm();
    m_fsm->onCanonicalEvent("prompt.submitted");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Thinking);
}

void TestPetStateMachine::testToolBeforeTakesOverFromThinking()
{
    initFsm();
    m_fsm->onCanonicalEvent("prompt.submitted");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Thinking);
    m_fsm->onCanonicalEvent("tool.before");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Working);
}
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: both new tests fail — `prompt.submitted` is unhandled.

- [ ] **Step 3: Add the Thinking branch**

Insert into `onCanonicalEvent` in `src/PetStateMachine.cpp`, **before** the existing `tool.before` block (so `tool.before` always wins as the takeover):

```cpp
    if (eventName == "prompt.submitted") {
        m_thinkingTimeout.start();
        enterBase(State::Thinking, HighPriority);
        return;
    }
```

Implement the timeout:

```cpp
void PetStateMachine::onThinkingTimeout()
{
    if (m_baseState == State::Thinking) {
        enterBase(State::Idle, NormalPriority);
    }
}
```

Stop the Thinking timer when Working takes over. Modify the existing Working block:

```cpp
    if (eventName == "tool.before"
        || eventName == "subagent.started"
        || eventName == "file.edited") {
        m_thinkingTimeout.stop();
        enterBase(State::Working, HighPriority);
        m_workingGrace.start();
        return;
    }
```

- [ ] **Step 4: Run the tests**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all five tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp
git commit -m "feat(fsm): Thinking state and Working takeover"
```

---

## Task 8: `permission.requested` enters Reviewing; `permission.response` exits

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`

- [ ] **Step 1: Add the failing tests**

Add to `private slots:`:

```cpp
    void testPermissionRequestedEntersReviewing();
    void testPermissionResponseExitsReviewing();
```

Add the bodies:

```cpp
void TestPetStateMachine::testPermissionRequestedEntersReviewing()
{
    initFsm();
    m_fsm->onCanonicalEvent("permission.requested");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Reviewing);
}

void TestPetStateMachine::testPermissionResponseExitsReviewing()
{
    initFsm();
    m_fsm->onCanonicalEvent("permission.requested");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Reviewing);
    m_fsm->onCanonicalEvent("permission.response");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Idle);
}
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: both fail — `permission.requested` unhandled.

- [ ] **Step 3: Implement Reviewing**

Add to `onCanonicalEvent`, after the `prompt.submitted` block:

```cpp
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
```

Implement the timeout:

```cpp
void PetStateMachine::onReviewingTimeout()
{
    if (m_baseState == State::Reviewing) {
        enterBase(State::Idle, NormalPriority);
    }
}
```

- [ ] **Step 4: Run the tests**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all seven tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp
git commit -m "feat(fsm): Reviewing state from permission.requested"
```

---

## Task 9: One-shot states (Failed, Celebrating, Greeting) with restoration

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`

- [ ] **Step 1: Add the failing tests**

Add to `private slots:`:

```cpp
    void testFailedOneShotReturnsToWorking();
    void testGreetingOnlyFromIdle();
    void testCelebratingOnTodoUpdated();
    void testSessionErrorOneShotFromIdle();
```

Add the bodies:

```cpp
void TestPetStateMachine::testFailedOneShotReturnsToWorking()
{
    initFsm();
    m_fsm->onCanonicalEvent("tool.before");
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Working);

    m_fsm->onCanonicalEvent("tool.failed");
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Failed);
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Working);  // base preserved

    // After the one-shot completes, overlay should clear.
    QTest::qWait(2200);  // > NOTIFICATION_ONESHOT_MS, used as default duration
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Working);
}

void TestPetStateMachine::testGreetingOnlyFromIdle()
{
    initFsm();
    m_fsm->onCanonicalEvent("session.start");
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Greeting);

    // Now mid-session: a second session.start should NOT preempt Working.
    initFsm();
    m_fsm->onCanonicalEvent("tool.before");
    m_fsm->onCanonicalEvent("session.start");  // ignored — not Idle
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Working);
}

void TestPetStateMachine::testCelebratingOnTodoUpdated()
{
    initFsm();
    QJsonObject payload;
    payload["status"] = "completed";
    m_fsm->onCanonicalEvent("todo.updated", payload);
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Celebrating);

    // Updates without "completed" should NOT celebrate.
    initFsm();
    QJsonObject other;
    other["status"] = "in_progress";
    m_fsm->onCanonicalEvent("todo.updated", other);
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Idle);
}

void TestPetStateMachine::testSessionErrorOneShotFromIdle()
{
    initFsm();
    m_fsm->onCanonicalEvent("session.error");
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Failed);
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Idle);
}
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all four new tests fail — none of `session.error`/`session.start`/`tool.failed`/`todo.updated` are handled.

- [ ] **Step 3: Implement one-shot transitions**

Add to `onCanonicalEvent`, after the existing handlers:

```cpp
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
```

Replace the `enterOneShot` and `onOneShotFinished` stubs:

```cpp
void PetStateMachine::enterOneShot(State s, int durationMs)
{
    // Save the sustained state so we can restore on completion.
    if (m_overlayState == State::Idle) {
        m_savedSustained = m_baseState;
    }
    m_overlayState = s;
    m_oneShotTimer.start(durationMs);
    emit stateChanged(activeState());
    emitChainFor(s, HighPriority);
}

void PetStateMachine::onOneShotFinished()
{
    if (m_overlayState == State::Idle) return;
    m_overlayState = State::Idle;
    emit stateChanged(activeState());
    emitChainFor(m_baseState, NormalPriority);
}
```

- [ ] **Step 4: Run the tests**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all eleven tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp
git commit -m "feat(fsm): one-shot states (Failed, Celebrating, Greeting) with restoration"
```

---

## Task 10: Idle-fallback append on every emitted chain

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`

- [ ] **Step 1: Add the failing test**

Add to `private slots:`:

```cpp
    void testIdleFallbackAppendedToEveryChain();
```

Add the body:

```cpp
void TestPetStateMachine::testIdleFallbackAppendedToEveryChain()
{
    initFsm();
    QSignalSpy chainSpy(m_fsm, &PetStateMachine::animationRequested);

    m_fsm->onCanonicalEvent("tool.before");
    m_fsm->onCanonicalEvent("session.error");

    QVERIFY(chainSpy.count() >= 2);
    for (const QList<QVariant> &emission : chainSpy) {
        const QStringList chain = emission.at(0).toStringList();
        QVERIFY2(chain.contains("idle"),
                 qPrintable(QString("chain missing idle fallback: %1").arg(chain.join(','))));
    }
}
```

- [ ] **Step 2: Run the test**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: passes immediately — the `emitChainFor` implementation in Task 4 already appends `m_idleFallback` (`"idle"`). This test exists to lock in the contract.

- [ ] **Step 3: Commit**

```bash
git add tests/test_pet_state_machine.cpp
git commit -m "test(fsm): lock in idle-fallback chain contract"
```

---

## Task 11: Walking overlay from position deltas

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`
- Modify: `src/PetStateMachine.h` (add Walking chain init)

- [ ] **Step 1: Add the failing tests**

Add to `private slots:`:

```cpp
    void testPositionChangeFiresWalkingChain();
    void testWalkingChainHasDirection();
    void testUserDragDoesNotTriggerWalking();
```

Add the bodies:

```cpp
void TestPetStateMachine::testPositionChangeFiresWalkingChain()
{
    initFsm();
    QSignalSpy chainSpy(m_fsm, &PetStateMachine::animationRequested);

    m_fsm->onPositionChanged(QPoint(100, 100), QPoint(200, 100), false);

    QVERIFY(chainSpy.count() >= 1);
    const QStringList chain = chainSpy.last().at(0).toStringList();
    QVERIFY(chain.contains("running-right"));
}

void TestPetStateMachine::testWalkingChainHasDirection()
{
    initFsm();
    QSignalSpy chainSpy(m_fsm, &PetStateMachine::animationRequested);

    m_fsm->onPositionChanged(QPoint(200, 100), QPoint(100, 100), false);  // leftward

    QVERIFY(chainSpy.count() >= 1);
    const QStringList chain = chainSpy.last().at(0).toStringList();
    QVERIFY(chain.contains("running-left"));
}

void TestPetStateMachine::testUserDragDoesNotTriggerWalking()
{
    initFsm();
    QSignalSpy chainSpy(m_fsm, &PetStateMachine::animationRequested);

    m_fsm->onPositionChanged(QPoint(100, 100), QPoint(200, 100), true);  // user drag
    QCOMPARE(chainSpy.count(), 0);
}
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: walking tests fail — `onPositionChanged` is a stub.

- [ ] **Step 3: No constructor change needed**

Walking chains are computed at emit-time in `onPositionChanged` because they depend on direction. Skip ahead to step 4.

- [ ] **Step 4: Implement `onPositionChanged`**

Replace the stub:

```cpp
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
}
```

- [ ] **Step 5: Run the tests**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all fourteen tests pass.

- [ ] **Step 6: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp
git commit -m "feat(fsm): Walking overlay from non-drag position deltas"
```

---

## Task 12: Pack-aware chain rebuild — Codex pets translate `nameMap`

**Files:**
- Modify: `tests/test_pet_state_machine.cpp`
- Modify: `src/PetStateMachine.cpp`
- Read: `src/CharacterPack.h` (no changes — consume existing `nameMap()`)

- [ ] **Step 1: Add the failing test using a stub pack**

Add a tiny test helper at the top of `tests/test_pet_state_machine.cpp` (after the includes):

```cpp
#include "CharacterPack.h"

// Build a CharacterPack that pretends to be a Codex pet by injecting a
// minimal nameMap. We rely on the fact that loadFromCodexPet leaves these
// public via nameMap() — the FSM consumes that map directly.
//
// Since nameMap is private and only populated by loaders, we forge one by
// loading from a real codex-pet archive in fixtures/. If the fixture is
// missing, this test is skipped.
```

Actually — `nameMap` is exposed via `pack->nameMap()` (public, `CharacterPack.h:188`), but populating it requires loading a `.codex-pet`. For the unit test, we need a synthetic fixture. Use the simpler approach: **make the FSM take a raw `QMap<QString,QString>` overload for testing**, AND wire `onActivePackChanged` to call into it. This keeps the test self-contained without filesystem fixtures.

Add to `src/PetStateMachine.h`, after the existing slots:

```cpp
    /// Test seam: rebuild chains directly from a (canonical → pack-actual)
    /// name map, the same shape as CharacterPack::nameMap().
    void rebuildChainsFromNameMap(const QMap<QString, QString> &nameMap);
```

Add the test:

```cpp
void TestPetStateMachine::testCodexNameMapRebuildsChains()
{
    initFsm();
    QMap<QString, QString> nameMap;
    nameMap["work"] = "running";          // Working → "running"
    nameMap["alert"] = "failed";          // Failed  → "failed"
    nameMap["greet"] = "waving";          // Greeting → "waving"
    nameMap["think"] = "waiting";         // Thinking → "waiting"
    nameMap["attention"] = "review";      // Reviewing → "review"
    nameMap["celebrate"] = "jumping";     // Celebrating → "jumping"
    nameMap["rest"] = "idle";             // idle fallback

    m_fsm->rebuildChainsFromNameMap(nameMap);

    QSignalSpy chainSpy(m_fsm, &PetStateMachine::animationRequested);
    m_fsm->onCanonicalEvent("tool.before");
    QVERIFY(chainSpy.count() >= 1);
    const QStringList chain = chainSpy.last().at(0).toStringList();
    QCOMPARE(chain.first(), QStringLiteral("running"));
}
```

Add to `private slots:`:

```cpp
    void testCodexNameMapRebuildsChains();
```

- [ ] **Step 2: Run to verify failure**

```bash
cd build && cmake --build . --target test_pet_state_machine 2>&1 | tail -10
```

Expected: link error — `rebuildChainsFromNameMap` not implemented.

- [ ] **Step 3: Implement the method**

In `src/PetStateMachine.cpp`, replace the `rebuildChainsFromPack` stub and add the new method:

```cpp
void PetStateMachine::rebuildChainsFromNameMap(const QMap<QString, QString> &nameMap)
{
    // Canonical-name → State mapping. Each State takes the first matching
    // canonical name from the pack's nameMap, keeps engine defaults as
    // fallbacks behind it.
    struct Mapping { State state; QStringList canonicalCandidates; };
    const QVector<Mapping> mappings = {
        {State::Idle,        {"idle", "rest"}},
        {State::Greeting,    {"greet", "wave", "greeting"}},
        {State::Thinking,    {"think", "thinking"}},
        {State::Working,     {"work", "processing", "writing", "searching"}},
        {State::Reviewing,   {"attention", "gettechy", "getwizardy"}},
        {State::Failed,      {"alert"}},
        {State::Celebrating, {"celebrate", "congratulate"}},
    };

    for (const auto &m : mappings) {
        QStringList chain;
        for (const QString &canonical : m.canonicalCandidates) {
            const QString actual = nameMap.value(canonical);
            if (!actual.isEmpty() && !chain.contains(actual)) {
                chain.append(actual);
            }
        }
        // Append engine defaults as later fallbacks.
        const QStringList defaults = m_chains.value(m.state);
        for (const QString &name : defaults) {
            if (!chain.contains(name)) chain.append(name);
        }
        m_chains[m.state] = chain;
    }

    // Update idle fallback if pack has one.
    const QString packIdle = nameMap.value("idle", nameMap.value("rest"));
    if (!packIdle.isEmpty()) {
        m_idleFallback = packIdle;
    }
}

void PetStateMachine::rebuildChainsFromPack(const CharacterPack *pack)
{
    if (!pack || !pack->isValid()) return;
    rebuildChainsFromNameMap(pack->nameMap());
}
```

Wire `onActivePackChanged` to call it:

```cpp
void PetStateMachine::onActivePackChanged(const CharacterPack *pack)
{
    rebuildChainsFromPack(pack);
}
```

- [ ] **Step 4: Run the tests**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all fifteen tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pet_state_machine.cpp src/PetStateMachine.cpp src/PetStateMachine.h
git commit -m "feat(fsm): rebuild chains from pack nameMap (Codex/native)"
```

---

## Task 13: Optional `stateMap` field in the manifest schema

**Files:**
- Modify: `src/CharacterPack.h` (add accessor)
- Modify: `src/CharacterPack.cpp` (add parser, populate from `loadFromCodexPet`)
- Modify: `src/PetStateMachine.cpp` (consume `stateMap` if present, prefer over `nameMap`)
- Modify: `tests/test_pet_state_machine.cpp` (add coverage)

- [ ] **Step 1: Add the failing test for explicit stateMap**

Add to `private slots:`:

```cpp
    void testExplicitStateMapOverridesNameMap();
```

Add the body:

```cpp
void TestPetStateMachine::testExplicitStateMapOverridesNameMap()
{
    initFsm();
    QMap<QString, QStringList> stateMap;
    stateMap["Working"] = QStringList{"my-busy-anim"};
    stateMap["Failed"]  = QStringList{"my-error-anim"};

    QMap<QString, QString> nameMap;
    nameMap["work"] = "ignored-by-explicit-statemap";

    m_fsm->rebuildChainsFromMaps(stateMap, nameMap);

    QSignalSpy chainSpy(m_fsm, &PetStateMachine::animationRequested);
    m_fsm->onCanonicalEvent("tool.before");
    QCOMPARE(chainSpy.last().at(0).toStringList().first(),
             QStringLiteral("my-busy-anim"));
}
```

- [ ] **Step 2: Add the new public method to `src/PetStateMachine.h`**

After `rebuildChainsFromNameMap`:

```cpp
    /// Test seam / production entry: `stateMap` (if non-empty) supplies the
    /// pack-author override for each State; `nameMap` provides canonical-name
    /// fallbacks as in rebuildChainsFromNameMap. Engine defaults are appended
    /// last so a partial override still degrades cleanly.
    void rebuildChainsFromMaps(const QMap<QString, QStringList> &stateMap,
                               const QMap<QString, QString> &nameMap);
```

- [ ] **Step 3: Implement it in `src/PetStateMachine.cpp`**

Replace `rebuildChainsFromNameMap` and add the new entry:

```cpp
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
```

- [ ] **Step 4: Run the tests**

```bash
cd build && cmake --build . --target test_pet_state_machine && ctest -R test_pet_state_machine -V 2>&1 | tail -10
```

Expected: all sixteen tests pass.

- [ ] **Step 5: Add `stateMap` to `CharacterPack`**

Edit `src/CharacterPack.h`. After the `eventMap()` accessor (line 181):

```cpp
    /**
     * @brief Per-state explicit animation chains (optional, pack-author override).
     *        Keyed by abstract state name ("Working", "Failed", "Greeting", ...).
     *        When non-empty, takes precedence over `nameMap` for that state in
     *        PetStateMachine chain resolution.
     */
    const QMap<QString, QStringList> &stateMap() const { return m_stateMap; }
```

Add the member after `m_eventMap` (line 265):

```cpp
    QMap<QString, QStringList> m_stateMap;
```

Add the parser declaration after `parseEventMap` (line 256):

```cpp
    bool parseStateMap(const QJsonObject &map);
```

- [ ] **Step 6: Implement the parser in `src/CharacterPack.cpp`**

After `parseEventMap` (line 725), add:

```cpp
bool CharacterPack::parseStateMap(const QJsonObject &map)
{
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QString stateName = it.key();
        const QJsonValue value = it.value();
        QStringList chain;
        if (value.isArray()) {
            for (const QJsonValue &v : value.toArray()) {
                const QString s = v.toString();
                if (!s.isEmpty()) chain.append(s);
            }
        } else {
            const QString s = value.toString();
            if (!s.isEmpty()) chain.append(s);
        }
        m_stateMap[stateName] = chain;
    }
    return true;
}
```

Wire it into `parseManifest`. After the `parseEffectTriggers(pickObject("effectTriggers"))` block (around line 506), add:

```cpp
    if (!parseStateMap(pickObject("stateMap"))) {
        qWarning() << "CharacterPack: Failed to parse state map";
        return false;
    }
```

- [ ] **Step 7: Update `PetStateMachine::rebuildChainsFromPack` to consume `stateMap`**

In `src/PetStateMachine.cpp`, replace `rebuildChainsFromPack`:

```cpp
void PetStateMachine::rebuildChainsFromPack(const CharacterPack *pack)
{
    if (!pack || !pack->isValid()) return;
    rebuildChainsFromMaps(pack->stateMap(), pack->nameMap());
}
```

- [ ] **Step 8: Build the whole tree to make sure CharacterPack still compiles for all consumers**

```bash
cd build && cmake --build . 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 9: Run all tests**

```bash
cd build && ctest 2>&1 | tail -15
```

Expected: all four test executables pass.

- [ ] **Step 10: Commit**

```bash
git add src/CharacterPack.h src/CharacterPack.cpp src/PetStateMachine.h src/PetStateMachine.cpp tests/test_pet_state_machine.cpp
git commit -m "feat(pack): optional stateMap manifest field (FSM override)"
```

---

## Task 14: Wire `PetStateMachine` into `main.cpp` and rip out `EmotionEngine`

**Files:**
- Modify: `src/main.cpp`
- Delete: `src/EmotionEngine.cpp`, `src/EmotionEngine.h`
- Modify: `CMakeLists.txt` (drop EmotionEngine entries)

- [ ] **Step 1: Modify `src/main.cpp` — replace EmotionEngine wiring with PetStateMachine**

Edit `src/main.cpp`. Replace line 19:

```cpp
#include "PetStateMachine.h"
```

(removing the `#include "EmotionEngine.h"`).

Add an `#include <optional>` near the other system headers (after `#include <QTimer>`):

```cpp
#include <optional>
```

Replace the entire `--- Emotion engine ---` block (lines 380-397) with:

```cpp
    // --- Pet state machine ----------------------------------------------------
    PetStateMachine stateMachine;

    // Gateway events flow EventRouter → FSM. EventRouter still owns tip-text.
    QObject::connect(&eventRouter, &EventRouter::eventProcessed,
                     &stateMachine,
                     [&stateMachine](const QString &name) {
                         stateMachine.onCanonicalEvent(name);
                     });

    // Window-position deltas drive the Walking overlay.
    QObject::connect(&w, &MainWindow::positionChanged,
                     &stateMachine,
                     [&stateMachine, lastPos = std::optional<QPoint>{}](const QPoint &p) mutable {
                         if (lastPos.has_value()) {
                             stateMachine.onPositionChanged(*lastPos, p, /*isUserDrag=*/false);
                         }
                         lastPos = p;
                     });

    // Active pack changes rebuild per-state chains.
    QObject::connect(packManager, &CharacterPackManager::activePackChanged,
                     &stateMachine,
                     [&stateMachine, packManager]() {
                         stateMachine.onActivePackChanged(packManager->activePack());
                     });

    // FSM-emitted chain → engine fan-out (same shape as the old moodChanged lambda).
    QObject::connect(&stateMachine, &PetStateMachine::animationRequested,
                     &w,
                     [&w](const QStringList &chain, int priority) {
        if (chain.isEmpty()) return;
#ifdef SEELIE_LIVE2D_SUPPORT
        if (w.live2dEngine() && w.live2dEngine()->hasAnimations()) {
            w.live2dEngine()->playAnimationChain(chain,
                priority == PetStateMachine::HighPriority
                    ? Live2DAnimationEngine::HighPriority
                    : Live2DAnimationEngine::NormalPriority);
            return;
        }
#endif
        if (w.lottieEngine() && w.lottieEngine()->hasAnimations()) {
            w.lottieEngine()->playAnimation(chain.first(),
                priority == PetStateMachine::HighPriority
                    ? LottieAnimationEngine::HighPriority
                    : LottieAnimationEngine::NormalPriority);
            return;
        }
        w.animationEngine()->playAnimation(chain.first(),
            priority == PetStateMachine::HighPriority
                ? SpriteAnimationEngine::HighPriority
                : SpriteAnimationEngine::NormalPriority);
    });
```

The `lastPos` capture mutable lambda tracks previous window position so we can compute deltas. `packManager` must be available in scope; check around line 350-380 of `main.cpp` for its existing variable name and adjust if it's `packMgr` or similar.

- [ ] **Step 2: Verify the variable name for the pack manager**

```bash
grep -n "CharacterPackManager\|packManager\|packMgr" src/main.cpp | head -10
```

Expected: shows the actual variable name used for the pack manager. If it differs from `packManager`, adjust the lambda above accordingly.

- [ ] **Step 3: Delete the EmotionEngine sources**

```bash
git rm src/EmotionEngine.cpp src/EmotionEngine.h
```

- [ ] **Step 4: Drop EmotionEngine from `CMakeLists.txt`**

Delete lines 364-365 of `CMakeLists.txt`:

```
src/EmotionEngine.cpp
src/EmotionEngine.h
```

- [ ] **Step 5: Build the app**

```bash
cd build && cmake --build . --target Seelie 2>&1 | tail -20
```

Expected: clean build. If link errors mention `EmotionEngine`, check that no other file `#include`s it.

- [ ] **Step 6: Verify no stragglers**

```bash
grep -rn "EmotionEngine" src/ tests/ 2>&1 | head -5
```

Expected: empty output.

- [ ] **Step 7: Run all tests**

```bash
cd build && ctest 2>&1 | tail -15
```

Expected: all four test executables pass.

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp CMakeLists.txt
git commit -m "feat: wire PetStateMachine, delete EmotionEngine"
```

---

## Task 15: Strip animation dispatch from `EventRouter`

**Files:**
- Modify: `src/EventRouter.h`
- Modify: `src/EventRouter.cpp`
- Modify: `src/main.cpp` (remove engine-setter calls on EventRouter)
- Modify: `src/mainwindow.cpp` (route synthetic events to FSM, not EventRouter)
- Modify: `src/mainwindow.h` (add FSM pointer, drop EventRouter setter for triggerEvent path)
- Modify: `tests/test_ipc_animations.cpp` (remove EventRouter→engine wiring; add a no-op FSM)

- [ ] **Step 1: Strip animation surface from `src/EventRouter.h`**

Replace the existing setters (lines 24-31) with:

```cpp
    void setTipBubble(TipBubbleWidget *bubble) { m_tipBubble = bubble; }
    void setTipsEngine(TipsEngine *tips) { m_tips = tips; }
```

Delete:
- `setAnimationEngine`, `setLottieEngine`, `setLive2dEngine` setters
- `loadFromCharacterPack` declaration
- `triggerEvent` declaration
- The `EventAction` struct (lines 53-57)
- The `m_eventMap` member (line 59)
- The `m_engine`, `m_lottieEngine`, `m_live2dEngine` members (lines 61-65)
- The `initEventMap()` declaration (line 50)
- The `s_validSources` may stay; the `s_validEvents` stays.

The minimal header should now read (replace whole file):

```cpp
#ifndef EVENTROUTER_H
#define EVENTROUTER_H

#include <QObject>
#include <QSet>

class TipBubbleWidget;
class TipsEngine;

/**
 * Validates incoming canonical events, fires tip-text bubbles, and emits
 * `eventProcessed` for downstream consumers (PetStateMachine). It no longer
 * dispatches animations directly — that responsibility moved to the FSM.
 */
class EventRouter : public QObject
{
    Q_OBJECT

public:
    explicit EventRouter(QObject *parent = nullptr);

    void setTipBubble(TipBubbleWidget *bubble) { m_tipBubble = bubble; }
    void setTipsEngine(TipsEngine *tips) { m_tips = tips; }

public slots:
    void routeEvent(const QJsonObject &event);
    void retranslateUi() {}  // no-op — kept for signal-slot compatibility

signals:
    /** Emitted after any canonical event is validated and tip-routed. */
    void eventProcessed(const QString &eventName, const QJsonObject &payload);

private:
    bool validateEvent(const QJsonObject &event) const;

    TipBubbleWidget *m_tipBubble = nullptr;
    TipsEngine *m_tips = nullptr;

    static const QSet<QString> s_validEvents;
    static const QSet<QString> s_validSources;
};

#endif // EVENTROUTER_H
```

Note: `eventProcessed` now carries the JSON payload too — the FSM needs `payload["status"]` for `todo.updated`.

- [ ] **Step 2: Rewrite `src/EventRouter.cpp` to match**

Replace the file contents:

```cpp
#include "EventRouter.h"
#include "TipBubbleWidget.h"
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
    if (!tip.title.isEmpty() && m_tipBubble) {
        m_tipBubble->showBubble(tip.title, tip.body, TipBubbleWidget::TipBubble, sourceLabel);
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
```

- [ ] **Step 3: Update the FSM's `eventProcessed` consumer to take the payload**

In `src/main.cpp`, change the connection installed in Task 14:

```cpp
    QObject::connect(&eventRouter, &EventRouter::eventProcessed,
                     &stateMachine,
                     [&stateMachine](const QString &name, const QJsonObject &payload) {
                         stateMachine.onCanonicalEvent(name, payload);
                     });
```

- [ ] **Step 4: Remove engine setter calls from `src/main.cpp`**

```bash
grep -n "eventRouter.setAnimationEngine\|eventRouter.setLottieEngine\|eventRouter.setLive2dEngine\|eventRouter.loadFromCharacterPack" src/main.cpp
```

Delete each matching line.

- [ ] **Step 5: Update `src/mainwindow.h` and `src/mainwindow.cpp` to route synthetic events to the FSM**

In `src/mainwindow.h`, replace `m_eventRouter` (line 99) with:

```cpp
    EventRouter *m_eventRouter = nullptr;
    PetStateMachine *m_stateMachine = nullptr;
```

Add a forward declaration near the top:

```cpp
class PetStateMachine;
```

Add a setter:

```cpp
    void setStateMachine(PetStateMachine *sm) { m_stateMachine = sm; }
```

In `src/mainwindow.cpp`, find the four `m_eventRouter->triggerEvent(...)` call sites (around lines 398, 414, 435, 454). Replace each:

```cpp
    if (m_stateMachine) {
        m_stateMachine->onSyntheticEvent(QStringLiteral("user.click"));
    }
```

(Substituting the appropriate event name per call site.)

Add the `#include "PetStateMachine.h"` at the top of `src/mainwindow.cpp`.

- [ ] **Step 6: Implement `PetStateMachine::onSyntheticEvent`**

In `src/PetStateMachine.cpp`, replace the stub:

```cpp
void PetStateMachine::onSyntheticEvent(const QString &eventName)
{
    if (eventName == "user.click" || eventName == "user.doubleclick") {
        if (m_baseState == State::Idle && m_overlayState == State::Idle) {
            enterOneShot(State::Greeting, NOTIFICATION_ONESHOT_MS);
        }
    }
    // user.hoverEnter / user.hoverLeave: no-op for now.
}
```

- [ ] **Step 7: Wire the FSM into `MainWindow` from `src/main.cpp`**

After `PetStateMachine stateMachine;` and before any uses of it, add:

```cpp
    w.setStateMachine(&stateMachine);
```

- [ ] **Step 8: Update `tests/test_ipc_animations.cpp` to compile without the old EventRouter API**

Find the `m_router->setAnimationEngine(m_engine);` call (line 94) and delete it. The `EventRouter` header no longer has that setter.

If the test depends on `EventRouter` triggering animations, the existing assertions in `testEventTriggersAnimation` and `testPriorityQueue` will need to be rewired to drive a `PetStateMachine` in the test setup. Add to `initTestCase` after creating `m_router`:

```cpp
    m_fsm = new PetStateMachine(this);
    connect(m_router, &EventRouter::eventProcessed,
            m_fsm, &PetStateMachine::onCanonicalEvent);
    connect(m_fsm, &PetStateMachine::animationRequested,
            this, [this](const QStringList &chain, int /*priority*/) {
        if (!chain.isEmpty() && m_engine) {
            m_engine->playAnimation(chain.first(), SpriteAnimationEngine::HighPriority);
        }
    });
```

Add `PetStateMachine *m_fsm = nullptr;` to the private section of `TestIpcAnimations`, and `#include "PetStateMachine.h"` at the top.

- [ ] **Step 9: Build everything**

```bash
cd build && cmake --build . 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 10: Run all tests**

```bash
cd build && ctest 2>&1 | tail -15
```

Expected: all four test executables pass. If `test_ipc_animations` has timing-sensitive expectations on animation chains, the FSM's grace timers may shift them — accept short waits in those tests if needed.

- [ ] **Step 11: Commit**

```bash
git add src/EventRouter.h src/EventRouter.cpp src/main.cpp src/mainwindow.h src/mainwindow.cpp src/PetStateMachine.cpp tests/test_ipc_animations.cpp
git commit -m "refactor(events): EventRouter is tip+validation only; FSM owns animation"
```

---

## Task 16: End-to-end smoke test with a real Codex pet

**Files:**
- No code changes — this is a manual verification step. Use the user's existing `.codex-pet` fixtures in `C:/Users/huang/Downloads/`.

- [ ] **Step 1: Build a release binary**

```bash
cd build && cmake --build . --target Seelie --config Release 2>&1 | tail -5
```

- [ ] **Step 2: Drop the Tiny CRT pet into the running app**

Launch `build/Release/Seelie.exe` (or `build/Seelie.exe` for non-multi-config). Drag `C:/Users/huang/Downloads/tiny-crt.codex-pet.zip` (rename to `.codex-pet` first) onto the pet window.

Expected: the CRT pet replaces the current character, idles with subtle motion.

- [ ] **Step 3: Drive events from the gateway and observe state changes**

In a separate terminal:

```bash
seelie-gateway --source claude-code --event session.start
# expect: pet plays the "waving" animation once, returns to idle
seelie-gateway --source claude-code --event tool.before
# expect: pet enters the "running" loop and stays there
seelie-gateway --source claude-code --event tool.failed
# expect: brief "failed" overlay, then back to "running"
seelie-gateway --source claude-code --event session.idle
# expect: drops to idle within 100ms
```

- [ ] **Step 4: Observe Working grace window**

```bash
seelie-gateway --source claude-code --event tool.before
# wait 1 second
seelie-gateway --source claude-code --event tool.after
# wait 1 second
seelie-gateway --source claude-code --event tool.before
# expect: pet stayed in "running" the whole time (no flicker to idle at 1.0s)
```

- [ ] **Step 5: Confirm web-sourced Live2D pack still works**

Switch the active pack back to a Live2D one (via the system tray / settings panel). Repeat Step 3. The pet should react to clicks (TouchHead/Tap fallback) and not error out, even though Live2D models lack `running` / `failed` animations.

- [ ] **Step 6: Commit any tweaks discovered**

If you adjusted timing constants or fallback chains based on what you saw, commit the change with a message like:

```bash
git commit -m "tune(fsm): adjust X based on smoke test"
```

If nothing changed, no commit needed.

---

## Notes for the implementer

- **TDD pace:** Tasks 4–11 follow strict red-green: failing test → minimal impl → commit. Don't batch.
- **DRY:** The `mappings` table in `rebuildChainsFromMaps` is the single source of truth for canonical-name → state translation. If you add a state, add a row there.
- **YAGNI:** Don't add a stack of overlay states in Task 9 — the spec says one Critical at a time, return to base. The simpler model handles 95% of real bursts.
- **Frequent commits:** every step that changes code commits. The plan has 16 tasks ≈ 50+ commits — that's intentional, makes review easy.
- **Don't preserve `EventRouter::triggerEvent`:** the user-facing API change is internal; nothing outside this repo calls it.
- **Open questions deferred to implementation:**
  - `notification.sent` one-shot duration (2 s default — leave it; revisit if it looks wrong in Task 16)
  - Walking trigger threshold (32 px default — leave it; no producer wired today)
  - `todo.updated` payload semantics (relies on `payload["status"] == "completed"` — confirm in Task 16 with a real Claude Code todo flow)
