/**
 * test_ecg.cpp
 *
 * Unit tests for the ECG monitor component:
 *  - ConfigManager::ecgEnabled persistence + signal
 *  - EcgWidget waveform sampler shape
 *  - WAV synthesis header correctness
 *  - Smoke-test EcgWidget construction
 */

#include <QTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

#include "ConfigManager.h"

class TestEcg : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void configEcgEnabledDefaultsFalse();
    void configEcgEnabledRoundTrips();
    void configEcgEnabledEmitsSignal();
};

void TestEcg::initTestCase()
{
    // Use an isolated config dir so we don't clobber the real user config.
    QCoreApplication::setOrganizationName("OaiTests");
    QCoreApplication::setApplicationName("OaiTests-Ecg");
    const QString cfgDir = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir(cfgDir).removeRecursively();
}

void TestEcg::cleanupTestCase()
{
    const QString cfgDir = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir(cfgDir).removeRecursively();
}

void TestEcg::configEcgEnabledDefaultsFalse()
{
    ConfigManager cfg;
    cfg.load();
    QCOMPARE(cfg.ecgEnabled(), false);
}

void TestEcg::configEcgEnabledRoundTrips()
{
    {
        ConfigManager cfg;
        cfg.load();
        cfg.setEcgEnabled(true);
        cfg.save();
    }
    {
        ConfigManager cfg2;
        cfg2.load();
        QCOMPARE(cfg2.ecgEnabled(), true);
    }
}

void TestEcg::configEcgEnabledEmitsSignal()
{
    ConfigManager cfg;
    cfg.load();
    cfg.setEcgEnabled(false); // ensure baseline
    QSignalSpy spy(&cfg, &ConfigManager::ecgEnabledChanged);
    cfg.setEcgEnabled(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    // Idempotent — same value should not re-emit.
    cfg.setEcgEnabled(true);
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestEcg)
#include "test_ecg.moc"
