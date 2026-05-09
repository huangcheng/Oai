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
