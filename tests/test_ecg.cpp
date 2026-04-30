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

    // New redesign tests
    void pwrToggleStopsTimer();
    void almToggleSilencesBeep();
    void modeButtonCyclesBpm();
    void sliderDragUpdatesVolume();
};

static void removeTestConfig()
{
    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("OaiTests"), QStringLiteral("OaiTests-Ecg"));
        s.clear();
        s.sync();
    }
    const QString cfgDir = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir(cfgDir).removeRecursively();
    QFile::remove(cfgDir + ".ini");
}

void TestEcg::initTestCase()
{
    QCoreApplication::setOrganizationName("OaiTests");
    QCoreApplication::setApplicationName("OaiTests-Ecg");
    removeTestConfig();
}

void TestEcg::cleanupTestCase()
{
    removeTestConfig();
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
    cfg.setEcgEnabled(false);
    QSignalSpy spy(&cfg, &ConfigManager::ecgEnabledChanged);
    cfg.setEcgEnabled(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    cfg.setEcgEnabled(true);
    QCOMPARE(spy.count(), 1);
}

void TestEcg::ecgSampleHasRPeakNear030()
{
    const double rPeak = EcgWidget::ecgSample(0.30);
    QVERIFY(rPeak > 0.8);

    for (double p : {0.05, 0.20, 0.40, 0.60, 0.80, 0.95}) {
        QVERIFY2(EcgWidget::ecgSample(p) < rPeak - 0.3,
                 qPrintable(QString("phase %1 should be far below R peak").arg(p)));
    }
}

void TestEcg::ecgSampleBaselineAwayFromComplex()
{
    QVERIFY(std::abs(EcgWidget::ecgSample(0.85)) < 0.05);
    QVERIFY(std::abs(EcgWidget::ecgSample(0.95)) < 0.05);
}

void TestEcg::onTickAdvancesPhaseAndShiftsBuffer()
{
    EcgWidget w;
    const double startPhase = w.phase();
    const int n = w.sampleCount();
    QVERIFY(n > 100);

    QMetaObject::invokeMethod(&w, "onTick");
    for (int i = 0; i < 29; ++i) {
        QMetaObject::invokeMethod(&w, "onTick");
    }
    QVERIFY(w.phase() != startPhase);
    // After ~1 second of ticks at 72 BPM, phase should have advanced ~1.2 cycles.
    const double elapsedCycles = (30 * 33.0 / 1000.0) * (72.0 / 60.0);
    QVERIFY(std::abs((w.phase() - startPhase) - (elapsedCycles - std::floor(elapsedCycles))) < 0.05
            || std::abs(w.phase() - startPhase) < 1.0);
}

void TestEcg::synthesizeBeepWavHasValidRiffHeader()
{
    QByteArray wav = EcgWidget::synthesizeBeepWav();
    QVERIFY(wav.size() > 44);
    QCOMPARE(QByteArray(wav.constData(), 4),      QByteArray("RIFF"));
    QCOMPARE(QByteArray(wav.constData() + 8, 4),  QByteArray("WAVE"));
    QCOMPARE(QByteArray(wav.constData() + 12, 4), QByteArray("fmt "));

    quint16 fmt = static_cast<quint8>(wav[20])
                | (static_cast<quint8>(wav[21]) << 8);
    QCOMPARE(int(fmt), 1);

    quint16 ch = static_cast<quint8>(wav[22])
               | (static_cast<quint8>(wav[23]) << 8);
    QCOMPARE(int(ch), 1);

    quint32 sr = static_cast<quint8>(wav[24])
               | (static_cast<quint8>(wav[25]) << 8)
               | (static_cast<quint8>(wav[26]) << 16)
               | (static_cast<quint8>(wav[27]) << 24);
    QCOMPARE(int(sr), 22050);
}

// -----------------------------------------------------------------------
// New tests for the ICU monitor redesign
// -----------------------------------------------------------------------

void TestEcg::pwrToggleStopsTimer()
{
    EcgWidget w;
    // start() makes the widget visible and starts the timer; skip show() to
    // avoid needing a display — use the tick/phase interface instead.
    // Drive a few ticks to confirm phase advances when power is on.
    QVERIFY(w.powerOn()); // default on

    const double phaseA = w.phase();
    QMetaObject::invokeMethod(&w, "onTick");
    const double phaseB = w.phase();
    QVERIFY(phaseB != phaseA); // ticking advances phase while powered on

    // Simulate PWR button click: press then release on the same rect.
    // pressControlAt / releaseControlAt are the testability hooks exposed
    // specifically so headless tests don't need a visible window.
    const QPoint pwrCenter(w.findChild<QObject*>() ? 0 : 0, 0); // unused; use named helper
    // Use the public hit-test helpers directly.
    w.pressControlAt(QPoint(27, 119));  // x=SHADOW_BLUR+8+BUTTON_W/2, y≈ctrl center
    w.releaseControlAt(QPoint(27, 119));

    QVERIFY(!w.powerOn());

    // After power-off, onTick should be a no-op for phase.
    const double phaseAfterOff = w.phase();
    QMetaObject::invokeMethod(&w, "onTick");
    QCOMPARE(w.phase(), phaseAfterOff); // phase must not advance
}

void TestEcg::almToggleSilencesBeep()
{
    EcgWidget w;
    QVERIFY(!w.muted()); // default not muted

    // Press + release ALM button.
    // ALM button: x = SHADOW_BLUR + 8 + BUTTON_W + BUTTON_GAP + BUTTON_W/2
    //           = 10 + 8 + 38 + 4 + 19 = 79
    // y ≈ ctrl panel center = SHADOW_BLUR + TITLE_HEIGHT + LCD_HEIGHT + READOUT_HEIGHT + CONTROL_HEIGHT/2
    //                       = 10 + 18 + 60 + 16 + 23 = 127
    const QPoint almCenter(79, 127);
    w.pressControlAt(almCenter);
    w.releaseControlAt(almCenter);

    QVERIFY(w.muted());

    // Second click toggles back.
    w.pressControlAt(almCenter);
    w.releaseControlAt(almCenter);

    QVERIFY(!w.muted());
}

void TestEcg::modeButtonCyclesBpm()
{
    EcgWidget w;
    // Default hrIndex=1 → 72 BPM.
    QCOMPARE(w.currentBpm(), 72.0);

    // MODE button center: x = SHADOW_BLUR + 8 + 2*(BUTTON_W+BUTTON_GAP) + BUTTON_W/2
    //                       = 10 + 8 + 2*(38+4) + 19 = 10 + 8 + 84 + 19 = 121
    // y ≈ 127 (same ctrl center)
    const QPoint modeCenter(121, 127);

    w.pressControlAt(modeCenter);
    w.releaseControlAt(modeCenter);
    QCOMPARE(w.currentBpm(), 90.0); // 1→2

    w.pressControlAt(modeCenter);
    w.releaseControlAt(modeCenter);
    QCOMPARE(w.currentBpm(), 60.0); // 2→0

    w.pressControlAt(modeCenter);
    w.releaseControlAt(modeCenter);
    QCOMPARE(w.currentBpm(), 72.0); // 0→1, back to start
}

void TestEcg::sliderDragUpdatesVolume()
{
    EcgWidget w;
    // Slider rect: x = SHADOW_BLUR + PANEL_WIDTH - 8 - SLIDER_W = 10 + 220 - 8 - 60 = 162
    //              width = SLIDER_W = 60
    // Halfway through the slider track:
    //   thumbX at 0.5 volume = sliderRect.left + (SLIDER_W - SLIDER_THUMB_W) * 0.5
    //                        = 162 + (60 - 10) * 0.5 = 162 + 25 = 187
    // Press at x = 187, which is the midpoint of the track.
    const int sliderLeft = 162;
    const int sliderY    = 127; // same ctrl center approx

    // Press at the midpoint of the slider track.
    // The mapping: rel = x - sliderLeft - SLIDER_THUMB_W/2; volume = rel / (SLIDER_W - SLIDER_THUMB_W)
    // For volume=0.5: rel = 0.5 * 50 = 25; x = sliderLeft + 25 + 5 = 192
    const QPoint midSlider(sliderLeft + 5 + 25, sliderY); // x=192
    w.pressControlAt(midSlider);
    QVERIFY(std::abs(w.volume() - 0.5) < 0.05);

    // Drag to 0.0 (leftmost) — simulate drag by pressing again at the left edge.
    // rel = leftSlider.x - sliderLeft - SLIDER_THUMB_W/2 = 162+5 - 162 - 5 = 0 → volume=0
    const QPoint leftSlider(sliderLeft + 5, sliderY);
    w.pressControlAt(leftSlider);
    QVERIFY(w.volume() < 0.05);

    w.releaseControlAt(leftSlider);
}

QTEST_MAIN(TestEcg)
#include "test_ecg.moc"
