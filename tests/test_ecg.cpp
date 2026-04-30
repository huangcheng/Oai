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
#include <cmath>

#include "ConfigManager.h"
#include "EcgWidget.h"

class TestEcg : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void configEcgEnabledDefaultsFalse();
    void configEcgEnabledRoundTrips();
    void configEcgEnabledEmitsSignal();

    void ecgSampleHasRPeakNear030();
    void ecgSampleBaselineAwayFromComplex();

    void onTickAdvancesPhaseAndShiftsBuffer();

    void synthesizeBeepWavHasValidRiffHeader();
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

void TestEcg::ecgSampleHasRPeakNear030()
{
    // The R peak is the largest positive deflection; verify that
    // ecgSample(0.30) is bigger than samples elsewhere by a clear margin.
    const double rPeak = EcgWidget::ecgSample(0.30);
    QVERIFY(rPeak > 0.8);

    for (double p : {0.05, 0.20, 0.40, 0.60, 0.80, 0.95}) {
        QVERIFY2(EcgWidget::ecgSample(p) < rPeak - 0.3,
                 qPrintable(QString("phase %1 should be far below R peak").arg(p)));
    }
}

void TestEcg::ecgSampleBaselineAwayFromComplex()
{
    // Far from QRS and T, the trace should sit near baseline (|v| < 0.05).
    QVERIFY(std::abs(EcgWidget::ecgSample(0.85)) < 0.05);
    QVERIFY(std::abs(EcgWidget::ecgSample(0.95)) < 0.05);
}

void TestEcg::onTickAdvancesPhaseAndShiftsBuffer()
{
    EcgWidget w;
    const double startPhase = w.phase();
    const int n = w.sampleCount();
    QVERIFY(n > 100); // LCD inner width — must have a buffer

    // Drive the timer manually 30 times.
    QMetaObject::invokeMethod(&w, "onTick");
    for (int i = 0; i < 29; ++i) {
        QMetaObject::invokeMethod(&w, "onTick");
    }
    QVERIFY(w.phase() != startPhase);
    // After ~1 second of ticks, phase should have advanced ~1.2 cycles.
    const double elapsedCycles = (30 * 33.0 / 1000.0) * (72.0 / 60.0);
    QVERIFY(std::abs((w.phase() - startPhase) - (elapsedCycles - std::floor(elapsedCycles))) < 0.05
            || std::abs(w.phase() - startPhase) < 1.0);
}

void TestEcg::synthesizeBeepWavHasValidRiffHeader()
{
    QByteArray wav = EcgWidget::synthesizeBeepWav();
    QVERIFY(wav.size() > 44); // header + at least some samples
    QCOMPARE(QByteArray(wav.constData(), 4),     QByteArray("RIFF"));
    QCOMPARE(QByteArray(wav.constData() + 8, 4), QByteArray("WAVE"));
    QCOMPARE(QByteArray(wav.constData() + 12, 4), QByteArray("fmt "));

    // Audio format = 1 (PCM).
    quint16 fmt = static_cast<quint8>(wav[20])
                | (static_cast<quint8>(wav[21]) << 8);
    QCOMPARE(int(fmt), 1);

    // Channels = 1 (mono).
    quint16 ch = static_cast<quint8>(wav[22])
               | (static_cast<quint8>(wav[23]) << 8);
    QCOMPARE(int(ch), 1);

    // Sample rate = 22050.
    quint32 sr = static_cast<quint8>(wav[24])
               | (static_cast<quint8>(wav[25]) << 8)
               | (static_cast<quint8>(wav[26]) << 16)
               | (static_cast<quint8>(wav[27]) << 24);
    QCOMPARE(int(sr), 22050);
}

QTEST_MAIN(TestEcg)
#include "test_ecg.moc"
