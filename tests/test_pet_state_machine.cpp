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
    void testToolBeforeEntersWorking();
    void testWorkingGraceExpiresToIdle();
    void testWorkingGraceExtendsAcrossGaps();
    void testPromptSubmittedEntersThinking();
    void testToolBeforeTakesOverFromThinking();
    void testPermissionRequestedEntersReviewing();
    void testPermissionResponseExitsReviewing();
    void testFailedOneShotReturnsToWorking();
    void testGreetingOnlyFromIdle();
    void testCelebratingOnTodoUpdated();
    void testSessionErrorOneShotFromIdle();
    void testIdleFallbackAppendedToEveryChain();
    void testPositionChangeFiresWalkingChain();
    void testWalkingChainHasDirection();
    void testUserDragDoesNotTriggerWalking();

private:
    PetStateMachine *m_fsm = nullptr;

    void initFsm() {
        delete m_fsm;
        m_fsm = new PetStateMachine(this);
        qRegisterMetaType<PetStateMachine::State>("PetStateMachine::State");
    }
};

void TestPetStateMachine::testInitialStateIsIdle()
{
    initFsm();
    QCOMPARE(m_fsm->baseState(), PetStateMachine::State::Idle);
    QCOMPARE(m_fsm->activeState(), PetStateMachine::State::Idle);
}

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
    QVERIFY(!chain.isEmpty());
    QCOMPARE(chain.first(), QStringLiteral("running"));
}

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

QTEST_MAIN(TestPetStateMachine)
#include "test_pet_state_machine.moc"
