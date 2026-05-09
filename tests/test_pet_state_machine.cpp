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

QTEST_MAIN(TestPetStateMachine)
#include "test_pet_state_machine.moc"
