/**
 * test_gaming_mode.cpp
 *
 * Unit tests for Gaming Mode:
 *   - ConfigManager gamingMode key round-trip
 *   - FullscreenWatcher signal emission on state transitions
 */

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QCoreApplication>

#include "ConfigManager.h"
#include "FullscreenWatcher.h"

// ── Testable subclass that simulates checkFullscreen() output ────────────────

class MockFullscreenWatcher : public FullscreenWatcher
{
public:
    explicit MockFullscreenWatcher(QObject *parent = nullptr)
        : FullscreenWatcher(parent) {}

    void setNextResult(bool result) { m_nextResult = result; }

    // Expose onPoll() for direct invocation in tests
    void poll() { QMetaObject::invokeMethod(this, "onPoll", Qt::DirectConnection); }

protected:
    bool checkFullscreen() override { return m_nextResult; }

private:
    bool m_nextResult = false;
};

// ── Test class ────────────────────────────────────────────────────────────────

class TestGamingMode : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // ConfigManager tests
    void testGamingModeDefaultFalse();
    void testGamingModeRoundTrip();
    void testGamingModeSignalEmitted();

    // FullscreenWatcher tests
    void testWatcherStartStop();
    void testFullscreenStartedSignal();
    void testFullscreenStoppedSignal();
    void testNoDoubleSignal();

private:
    QTemporaryDir m_tmpDir;
};

void TestGamingMode::initTestCase()
{
    // Redirect QSettings to a throw-away temp dir so tests don't pollute
    // the developer's real Oai config file.
    QVERIFY(m_tmpDir.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tmpDir.path());
}

// ── ConfigManager tests ───────────────────────────────────────────────────────

void TestGamingMode::testGamingModeDefaultFalse()
{
    ConfigManager cfg;
    cfg.load();
    QCOMPARE(cfg.gamingModeEnabled(), false);
}

void TestGamingMode::testGamingModeRoundTrip()
{
    // Enable Gaming Mode, save, reload in a fresh manager, verify it's true
    {
        ConfigManager cfg;
        cfg.load();
        QCOMPARE(cfg.gamingModeEnabled(), false);
        cfg.setGamingModeEnabled(true);
        QCOMPARE(cfg.gamingModeEnabled(), true);
        // save() is called implicitly by setGamingModeEnabled
    }
    {
        ConfigManager cfg2;
        cfg2.load();
        QCOMPARE(cfg2.gamingModeEnabled(), true);
    }
    // Disable and verify again
    {
        ConfigManager cfg3;
        cfg3.load();
        cfg3.setGamingModeEnabled(false);
    }
    {
        ConfigManager cfg4;
        cfg4.load();
        QCOMPARE(cfg4.gamingModeEnabled(), false);
    }
}

void TestGamingMode::testGamingModeSignalEmitted()
{
    ConfigManager cfg;
    cfg.load();

    QSignalSpy spy(&cfg, &ConfigManager::gamingModeEnabledChanged);
    cfg.setGamingModeEnabled(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    cfg.setGamingModeEnabled(false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);

    // Setting the same value should NOT emit again
    cfg.setGamingModeEnabled(false);
    QCOMPARE(spy.count(), 2);
}

// ── FullscreenWatcher tests ───────────────────────────────────────────────────

void TestGamingMode::testWatcherStartStop()
{
    MockFullscreenWatcher watcher;
    QVERIFY(!watcher.isRunning());
    watcher.start();
    QVERIFY(watcher.isRunning());
    watcher.stop();
    QVERIFY(!watcher.isRunning());
}

void TestGamingMode::testFullscreenStartedSignal()
{
    MockFullscreenWatcher watcher;
    QSignalSpy startedSpy(&watcher, &FullscreenWatcher::fullscreenAppStarted);
    QSignalSpy stoppedSpy(&watcher, &FullscreenWatcher::fullscreenAppStopped);

    watcher.setNextResult(false);
    watcher.poll(); // false → false: no signal
    QCOMPARE(startedSpy.count(), 0);

    watcher.setNextResult(true);
    watcher.poll(); // false → true: emit fullscreenAppStarted
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(stoppedSpy.count(), 0);
}

void TestGamingMode::testFullscreenStoppedSignal()
{
    MockFullscreenWatcher watcher;
    QSignalSpy startedSpy(&watcher, &FullscreenWatcher::fullscreenAppStarted);
    QSignalSpy stoppedSpy(&watcher, &FullscreenWatcher::fullscreenAppStopped);

    // Advance to fullscreen=true first
    watcher.setNextResult(true);
    watcher.poll();
    QCOMPARE(startedSpy.count(), 1);

    // Then fullscreen ends
    watcher.setNextResult(false);
    watcher.poll();
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(startedSpy.count(), 1); // no extra started signal
}

void TestGamingMode::testNoDoubleSignal()
{
    MockFullscreenWatcher watcher;
    QSignalSpy startedSpy(&watcher, &FullscreenWatcher::fullscreenAppStarted);
    QSignalSpy stoppedSpy(&watcher, &FullscreenWatcher::fullscreenAppStopped);

    // Poll true three times in a row — should only emit once
    watcher.setNextResult(true);
    watcher.poll();
    watcher.poll();
    watcher.poll();
    QCOMPARE(startedSpy.count(), 1);

    // Poll false three times in a row — should only emit once
    watcher.setNextResult(false);
    watcher.poll();
    watcher.poll();
    watcher.poll();
    QCOMPARE(stoppedSpy.count(), 1);
}

QTEST_MAIN(TestGamingMode)
#include "test_gaming_mode.moc"
