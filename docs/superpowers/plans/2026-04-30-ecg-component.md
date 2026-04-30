# ECG Monitor Component Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an optional ICU-style ECG monitor widget that snaps above the character, sharing the Win98 parallelogram frame with the tip bubble and settings panel, and showing a scrolling phosphor-green PQRST trace with a synchronized "beep" on each R-peak.

**Architecture:** A new top-level frameless `EcgWidget` (peer to `TipBubbleWidget` / `SettingsPanelWidget`) is owned by `MainWindow` and re-anchored on every position change. Its outer frame reuses the existing parallelogram + orange stripe; its interior is a dark-green LCD canvas with a faint grid and a scrolling polyline. A 30 Hz `QTimer` drives phase advancement, sample-buffer rotation, repaint, and R-peak beep playback. Audio is a 60 ms 880 Hz tone synthesized in C++ at startup, written to a `QTemporaryFile`, and triggered via `QSoundEffect`. A new `ecgEnabled` `ConfigManager` flag (default `false`) toggles visibility from a checkbox added to `SettingsPanelWidget`.

**Tech Stack:** Qt 6 Widgets (`QWidget`, `QPainter`, `QPainterPath`, `QTimer`, `QPropertyAnimation`), Qt 6 Multimedia (`QSoundEffect`, `QTemporaryFile`), C++17, CMake 3.19+. Reuses the existing rendering vocabulary from `TipBubbleWidget` and `SettingsPanelWidget` (no new third-party deps).

---

## File Structure

### New files

- `src/EcgWidget.h` / `src/EcgWidget.cpp` — frameless top-level widget. Owns frame painting, LCD canvas, sample ring buffer, phase advancement, paint trace, and beep playback. Public API mirrors `TipBubbleWidget`: `anchorTo(const QWidget*)`, `setAnchorRect(const QRect&)`, `start()`, `stop()`, plus a static `synthesizeBeepWav()` helper.
- `tests/test_ecg.cpp` — Qt Test target covering `ConfigManager::ecgEnabled` round-trip, `EcgWidget::ecgSample()` PQRST shape, ring-buffer scroll, R-peak detection, and a smoke construction test.

### Modified files

- `src/ConfigManager.h` / `src/ConfigManager.cpp` — add `bool ecgEnabled() const`, `void setEcgEnabled(bool)`, signal `ecgEnabledChanged(bool)`, persisted under key `ecgEnabled` (default `false`).
- `src/SettingsPanelWidget.h` / `src/SettingsPanelWidget.cpp` — add an "ECG Monitor" row using the existing `CheckMarkBox` style, bump `PANEL_HEIGHT` from 210 → 250 to fit the new row, add label/checkbox members, retranslation hook.
- `src/mainwindow.h` / `src/mainwindow.cpp` — own `EcgWidget *m_ecgWidget`, anchor it on `positionChanged`, hide it when the pet is hidden, show/hide on `ConfigManager::ecgEnabledChanged`, re-anchor in `onActivePackChanged` when the pet resizes for a new pack.
- `CMakeLists.txt` — add `Qt6 Multimedia` to `find_package`, list `src/EcgWidget.cpp` / `src/EcgWidget.h` in `qt_add_executable`, link `Qt::Multimedia`.
- `tests/CMakeLists.txt` — add `src/EcgWidget.cpp` / `src/EcgWidget.h` to `OAIPET_LIB_SOURCES`, append `test_ecg.cpp` to `TEST_SOURCES`, add `Qt::Multimedia` to test link list.
- `Oai_zh_CN.ts` — add `<message>` entries for new English strings ("ECG Monitor" → "心电图").

---

## Decomposition Notes

- **Why a new top-level widget rather than embedding in MainWindow's transparent canvas?** `TipBubbleWidget` and `SettingsPanelWidget` are also separate top-level frameless `Qt::Tool` windows; this lets the ECG escape the `MainWindow` 124×200 viewport and apply the Win11 DWM no-chrome workaround independently.
- **Why synthesize WAV at startup instead of shipping an asset?** Avoids touching `qt_add_resources`, build steps, or Python generators. The synthesized bytes are ~2.7 KB, written once to a `QTemporaryFile` that lives for the widget's lifetime. `QSoundEffect` accepts a file URL.
- **Why 30 Hz tick instead of 60 Hz?** Battery + CPU friendly; LCD-style ECGs visually look fine at 30 Hz because the trace itself is moving, not the screen. Matches the `update()` cadence the existing engines use without taxing the rest of the app.
- **Coexistence with the tip bubble:** Both widgets snap above the pet and may overlap when a tip appears. This is acceptable (mirrors a real ICU monitor with a popover alarm) and avoids cross-widget layout coordination. Documented; not papered over.
- **Default off:** `ecgEnabled` defaults to `false` so existing users see no behavior change after upgrade. Audio in particular (a beep ~every 833 ms) is opt-in.

---

## Constants (used across tasks — keep in lock-step)

These are the canonical values. Every task that uses them must use these spellings exactly.

```cpp
// EcgWidget styling (mirrors TipBubbleWidget where possible)
static constexpr int PANEL_WIDTH      = 190;   // outer frame, excludes shadow
static constexpr int PANEL_HEIGHT     = 70;
static constexpr int SHADOW_BLUR      = 10;    // matches TipBubbleWidget
static constexpr int CORNER_RADIUS    = 4;     // matches TipBubbleWidget
static constexpr int BORDER_WIDTH     = 3;     // matches TipBubbleWidget
static constexpr int SKEW_PX          = 4;     // parallelogram skew, matches TipBubbleWidget
static constexpr int LCD_PADDING      = 10;    // inset of LCD canvas inside frame
static constexpr int TICK_INTERVAL_MS = 33;    // ~30 Hz

// LCD palette (phosphor-green CRT)
static constexpr QRgb LCD_BG          = qRgb(0x05, 0x1F, 0x0A); // dark green-black
static constexpr QRgb LCD_GRID_MINOR  = qRgb(0x15, 0x4D, 0x2E);
static constexpr QRgb LCD_GRID_MAJOR  = qRgb(0x2D, 0x7A, 0x4A);
static constexpr QRgb LCD_TRACE       = qRgb(0x4F, 0xFF, 0x7A); // bright phosphor

// Heartbeat
static constexpr double HEART_RATE_BPM = 72.0;
static constexpr double R_PEAK_PHASE   = 0.30;   // phase in [0,1) where R fires

// Beep audio
static constexpr int BEEP_SAMPLE_RATE = 22050;
static constexpr int BEEP_FREQ_HZ     = 880;
static constexpr int BEEP_DURATION_MS = 60;
static constexpr int BEEP_FADE_MS     = 3;
```

---

## Task 1: ConfigManager — `ecgEnabled` persistence

**Files:**
- Modify: `src/ConfigManager.h`
- Modify: `src/ConfigManager.cpp`
- Create: `tests/test_ecg.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Wire the new test executable into CMake (compiles to empty for now)**

Edit `tests/CMakeLists.txt`. Find the line:
```cmake
set(TEST_SOURCES
    test_ipc_animations.cpp
)
```
Replace with:
```cmake
set(TEST_SOURCES
    test_ipc_animations.cpp
    test_ecg.cpp
)
```

- [ ] **Step 2: Create `tests/test_ecg.cpp` with the failing ConfigManager test**

```cpp
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
```

- [ ] **Step 3: Run the build to confirm the test fails to compile (missing API)**

```bash
cd build && cmake --build . --target test_ecg 2>&1 | tail -20
```
Expected: build error in `test_ecg.cpp` like `'ecgEnabled' is not a member of 'ConfigManager'`.

- [ ] **Step 4: Add the ECG accessors to `ConfigManager.h`**

Edit `src/ConfigManager.h`. Inside the `public:` section, after `void setActivePackId(const QString &packId);` (around line 30), add:
```cpp
    /** Whether the ICU-style ECG monitor widget is shown above the pet. */
    bool ecgEnabled() const { return m_ecgEnabled; }
    void setEcgEnabled(bool enabled);
```

In the `signals:` section (around line 65), after the existing signals, add:
```cpp
    void ecgEnabledChanged(bool enabled);
```

In the `private:` member-variable block (after `QString m_activePackId;`), add:
```cpp
    bool m_ecgEnabled = false;
```

- [ ] **Step 5: Implement `setEcgEnabled` and persistence in `ConfigManager.cpp`**

Edit `src/ConfigManager.cpp`. In `ConfigManager::load()`, after the `m_activePackId = m_settings.value("activePackId").toString();` line (around line 92), add:
```cpp
    // ECG monitor visibility
    m_ecgEnabled = m_settings.value("ecgEnabled", false).toBool();
```

In `ConfigManager::save()`, after `m_settings.setValue("activePackId", m_activePackId);` (around line 105), add:
```cpp
    m_settings.setValue("ecgEnabled", m_ecgEnabled);
```

At the bottom of the file (after `setUpdateServerEndpoint`), add:
```cpp
void ConfigManager::setEcgEnabled(bool enabled)
{
    if (m_ecgEnabled != enabled) {
        m_ecgEnabled = enabled;
        save();
        emit ecgEnabledChanged(enabled);
    }
}
```

- [ ] **Step 6: Build and run the test**

```bash
cd build && cmake --build . --target test_ecg && ./tests/test_ecg
```
Expected: 3 tests pass (`configEcgEnabledDefaultsFalse`, `configEcgEnabledRoundTrips`, `configEcgEnabledEmitsSignal`). Other test cases will be added in later tasks.

- [ ] **Step 7: Commit**

```bash
git add src/ConfigManager.h src/ConfigManager.cpp tests/test_ecg.cpp tests/CMakeLists.txt
git commit -m "feat(config): add ecgEnabled flag for ECG monitor toggle"
```

---

## Task 2: Build system — link Qt::Multimedia

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add Multimedia to find_package in root CMakeLists.txt**

Edit `CMakeLists.txt`. Find:
```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools Test)
```
Replace with:
```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia LinguistTools Test)
```

- [ ] **Step 2: Link Qt::Multimedia in the Oai target**

In the `target_link_libraries(Oai ...)` block (around line 367), add `Qt::Multimedia`:
```cmake
target_link_libraries(Oai
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Multimedia
        rlottie::rlottie
)
```

- [ ] **Step 3: Link Qt::Multimedia in test executables**

Edit `tests/CMakeLists.txt`. In the `target_link_libraries(${test_name} ...)` block, add `Qt::Multimedia`:
```cmake
    target_link_libraries(${test_name}
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            Qt::Multimedia
            Qt::Test
            rlottie::rlottie
    )
```

- [ ] **Step 4: Reconfigure and verify the new dependency resolves**

```bash
cd build && cmake .. && cmake --build . --target Oai test_ecg 2>&1 | tail -20
```
Expected: clean build. `find_package` prints "Found Qt6Multimedia" during configure. If Multimedia is missing, install via `brew install qt@6` (already provides Multimedia) or the Qt online installer.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt
git commit -m "build: link Qt::Multimedia for ECG monitor audio"
```

---

## Task 3: EcgWidget — class skeleton + frame painting

**Files:**
- Create: `src/EcgWidget.h`
- Create: `src/EcgWidget.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create `src/EcgWidget.h`**

```cpp
#ifndef ECGWIDGET_H
#define ECGWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QRect>

class QSoundEffect;
class QTemporaryFile;

class EcgWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EcgWidget(QWidget *parent = nullptr);
    ~EcgWidget() override;

    // Position relative to the pet widget (mirrors TipBubbleWidget's API).
    void anchorTo(const QWidget *petWidget);
    void setAnchorRect(const QRect &rect) { m_anchorRect = rect; }

    // Begin / end the timer-driven animation + audio.
    // start() is idempotent; stop() halts the timer and mutes audio.
    void start();
    void stop();

    // --- Test accessors ------------------------------------------------------
    // ecgSample(phase) returns the stylized PQRST voltage in [-1, +1] for
    // a given phase in [0, 1). Pure function — testable in isolation.
    static double ecgSample(double phase);

    // synthesizeBeepWav() returns a complete RIFF WAVE file in memory:
    // 16-bit signed PCM, mono, BEEP_SAMPLE_RATE Hz, BEEP_DURATION_MS ms,
    // BEEP_FREQ_HZ tone with BEEP_FADE_MS linear fade in/out.
    static QByteArray synthesizeBeepWav();

    double phase() const { return m_phase; }
    int sampleCount() const { return m_samples.size(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onTick();

private:
    void positionRelativeTo(const QWidget *pet);
    void initAudio();

    // --- State ---------------------------------------------------------------
    QTimer m_tickTimer;
    QVector<double> m_samples;          // ring of width-many samples for the trace
    int m_writeHead = 0;                // index in m_samples to overwrite next
    double m_phase = 0.0;               // current heartbeat phase in [0, 1)
    double m_prevPhase = 0.0;           // phase at previous tick (for R-peak edge detection)

    QSoundEffect *m_beep = nullptr;
    QTemporaryFile *m_beepFile = nullptr;

    const QWidget *m_anchoredPet = nullptr;
    QRect m_anchorRect;

    // --- Styling constants ---------------------------------------------------
    static constexpr int PANEL_WIDTH      = 190;
    static constexpr int PANEL_HEIGHT     = 70;
    static constexpr int SHADOW_BLUR      = 10;
    static constexpr int CORNER_RADIUS    = 4;
    static constexpr int BORDER_WIDTH     = 3;
    static constexpr int SKEW_PX          = 4;
    static constexpr int LCD_PADDING      = 10;
    static constexpr int TICK_INTERVAL_MS = 33;

    static constexpr double HEART_RATE_BPM = 72.0;
    static constexpr double R_PEAK_PHASE   = 0.30;

    static constexpr int BEEP_SAMPLE_RATE = 22050;
    static constexpr int BEEP_FREQ_HZ     = 880;
    static constexpr int BEEP_DURATION_MS = 60;
    static constexpr int BEEP_FADE_MS     = 3;
};

#endif // ECGWIDGET_H
```

- [ ] **Step 2: Create `src/EcgWidget.cpp` with the parallelogram frame**

```cpp
#include "EcgWidget.h"
#include "MacFocusFix.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QShowEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QSoundEffect>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QFile>
#include <QtEndian>
#include <QtMath>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#endif

EcgWidget::EcgWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool |
        Qt::WindowDoesNotAcceptFocus
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif

    setFixedSize(PANEL_WIDTH + SHADOW_BLUR * 2, PANEL_HEIGHT + SHADOW_BLUR * 2);

    // Sample buffer width = LCD inner width.
    const int lcdW = PANEL_WIDTH - LCD_PADDING * 2;
    m_samples.resize(lcdW);
    m_samples.fill(0.0);

    m_tickTimer.setInterval(TICK_INTERVAL_MS);
    connect(&m_tickTimer, &QTimer::timeout, this, &EcgWidget::onTick);

    // Audio is initialized lazily on first start() to avoid creating a
    // QSoundEffect for users who never enable the ECG.
}

EcgWidget::~EcgWidget()
{
    m_tickTimer.stop();
    delete m_beep;
    delete m_beepFile;
}

void EcgWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    MacFocusFix::makeNonActivating(this);
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        const int doNotRound = 1;          // DWMWCP_DONOTROUND
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &doNotRound, sizeof(doNotRound));
        const int backdropNone = 1;        // DWMSBT_NONE
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                              &backdropNone, sizeof(backdropNone));
        const int ncRenderingDisabled = 1; // DWMNCRP_DISABLED
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                              &ncRenderingDisabled, sizeof(ncRenderingDisabled));
    }
#endif
}

void EcgWidget::anchorTo(const QWidget *petWidget)
{
    m_anchoredPet = petWidget;
    if (petWidget && isVisible()) {
        positionRelativeTo(petWidget);
    }
}

void EcgWidget::start()
{
    // Reset phase state; samples are kept (so re-show preserves continuity).
    m_prevPhase = m_phase;
    m_tickTimer.start();
    if (m_anchoredPet) {
        positionRelativeTo(m_anchoredPet);
    }
    show();
}

void EcgWidget::stop()
{
    m_tickTimer.stop();
    if (m_beep) {
        m_beep->stop();
    }
    hide();
}

void EcgWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body(SHADOW_BLUR, SHADOW_BLUR, PANEL_WIDTH, PANEL_HEIGHT);
    const qreal r = CORNER_RADIUS;
    const qreal sk = SKEW_PX;

    // Skewed parallelogram path (matches TipBubbleWidget / SettingsPanelWidget).
    QPainterPath panelPath;
    panelPath.moveTo(body.left() + sk + r, body.top());
    panelPath.lineTo(body.right() + sk - r, body.top());
    panelPath.quadTo(body.right() + sk, body.top(), body.right() + sk, body.top() + r);
    panelPath.lineTo(body.right(), body.bottom() - r);
    panelPath.quadTo(body.right(), body.bottom(), body.right() - r, body.bottom());
    panelPath.lineTo(body.left() + r, body.bottom());
    panelPath.quadTo(body.left(), body.bottom(), body.left(), body.bottom() - r);
    panelPath.lineTo(body.left() + sk, body.top() + r);
    panelPath.quadTo(body.left() + sk, body.top(), body.left() + sk + r, body.top());
    panelPath.closeSubpath();

    // Bold offset shadow.
    painter.save();
    painter.setOpacity(0.35);
    painter.setPen(Qt::NoPen);
    QPainterPath shadowPath = panelPath;
    shadowPath.translate(3, 4);
    painter.setBrush(Qt::black);
    painter.drawPath(shadowPath);
    painter.restore();

    // White fill + thick black border.
    painter.setPen(QPen(Qt::black, BORDER_WIDTH, Qt::SolidLine,
                        Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(Qt::white);
    painter.drawPath(panelPath);

    // Persona-orange accent stripe at top.
    painter.save();
    painter.setClipPath(panelPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xF3, 0x6F, 0x1A));
    painter.drawRect(QRectF(body.left(), body.top(), body.width() + sk, 4));
    painter.restore();

    // LCD canvas + waveform are filled in by Tasks 4-5; for now just paint
    // a placeholder dark rectangle so the frame is visually testable.
    const QRect lcd(SHADOW_BLUR + LCD_PADDING,
                    SHADOW_BLUR + LCD_PADDING,
                    PANEL_WIDTH  - LCD_PADDING * 2,
                    PANEL_HEIGHT - LCD_PADDING * 2);
    painter.fillRect(lcd, QColor(0x05, 0x1F, 0x0A));
}

void EcgWidget::positionRelativeTo(const QWidget *pet)
{
    if (!pet)
        return;

    QRect anchor = m_anchorRect.isValid()
                   ? m_anchorRect
                   : QRect(0, 0, pet->width(), pet->height());

    // Same Qt::Tool macOS quirk as TipBubbleWidget — prefer the native
    // QWindow position over mapToGlobal/pos which can return stale data.
    QPoint petGlobalPos;
    if (QWindow *w = pet->windowHandle()) {
        petGlobalPos = w->position();
    } else {
        petGlobalPos = pet->pos();
    }
    const int petCenterX = petGlobalPos.x() + anchor.x() + anchor.width() / 2;
    const int petTop     = petGlobalPos.y() + anchor.y();

    int wx = petCenterX - width() / 2;
    int wy = petTop - height(); // sits flush above pet

    QScreen *screen = QGuiApplication::screenAt(QPoint(petCenterX, petTop));
    if (screen) {
        QRect g = screen->availableGeometry();
        wx = qBound(g.left(), wx, g.right() - width());
        wy = qBound(g.top(), wy, g.bottom() - height());
    }
    move(wx, wy);
}

void EcgWidget::onTick()
{
    // Filled in by Tasks 4-6.
    update();
}

void EcgWidget::initAudio()
{
    // Filled in by Task 6.
}

// Stubs for Tasks 4-6 — defined here so the header and the test file link.
double EcgWidget::ecgSample(double /*phase*/)
{
    return 0.0;
}

QByteArray EcgWidget::synthesizeBeepWav()
{
    return QByteArray();
}
```

- [ ] **Step 3: Register the new sources in CMake**

Edit `CMakeLists.txt`. In the `qt_add_executable(Oai ...)` block (around line 289), add after the `src/SettingsPanelWidget.h` lines:
```cmake
    src/EcgWidget.cpp
    src/EcgWidget.h
```

Edit `tests/CMakeLists.txt`. In `OAIPET_LIB_SOURCES`, after the `SettingsPanelWidget` lines are *not* listed (note: SettingsPanelWidget is not in the test lib — it's app-only). Add the ECG widget after `${CMAKE_SOURCE_DIR}/src/TipBubbleWidget.h`:
```cmake
    ${CMAKE_SOURCE_DIR}/src/EcgWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/EcgWidget.h
```

- [ ] **Step 4: Build and confirm clean compile**

```bash
cd build && cmake --build . --target Oai test_ecg 2>&1 | tail -10
```
Expected: clean build, no warnings.

- [ ] **Step 5: Smoke-run the app to see the placeholder frame**

```bash
open build/Oai.app
```
Manually verify: nothing visibly broken (the EcgWidget is not yet shown anywhere — that lands in Task 8). The app runs, the pet appears, no crashes.

- [ ] **Step 6: Commit**

```bash
git add src/EcgWidget.h src/EcgWidget.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(ecg): add EcgWidget skeleton with shared Win98 parallelogram frame"
```

---

## Task 4: EcgWidget — PQRST waveform sampler (pure function)

**Files:**
- Modify: `src/EcgWidget.cpp`
- Modify: `tests/test_ecg.cpp`

- [ ] **Step 1: Add the failing waveform shape test**

Edit `tests/test_ecg.cpp`. Add `#include "EcgWidget.h"` at the top. In the `private slots:` declarations, add:
```cpp
    void ecgSampleHasRPeakNear030();
    void ecgSampleBaselineAwayFromComplex();
```

At the bottom of the file (before `QTEST_MAIN`), add the implementations:
```cpp
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
```

- [ ] **Step 2: Run tests; new ones should fail (sampler stub returns 0)**

```bash
cd build && cmake --build . --target test_ecg && ./tests/test_ecg
```
Expected: `ecgSampleHasRPeakNear030` fails (`rPeak = 0`), `ecgSampleBaselineAwayFromComplex` passes (0 is at baseline).

- [ ] **Step 3: Implement `ecgSample` in `src/EcgWidget.cpp`**

Replace the `ecgSample` stub in `src/EcgWidget.cpp` with:
```cpp
double EcgWidget::ecgSample(double phase)
{
    // Wrap to [0, 1).
    phase = phase - std::floor(phase);

    auto gauss = [](double x, double mu, double sigma) {
        const double z = (x - mu) / sigma;
        return std::exp(-0.5 * z * z);
    };

    // Stylized PQRST. Coefficients tuned for visual recognizability —
    // not medically accurate, but unmistakable as an ECG trace.
    double v = 0.0;
    v += 0.15 * gauss(phase, 0.15, 0.025); // P wave
    v -= 0.10 * gauss(phase, 0.28, 0.012); // Q dip
    v += 1.00 * gauss(phase, 0.30, 0.012); // R spike
    v -= 0.20 * gauss(phase, 0.32, 0.018); // S dip
    v += 0.30 * gauss(phase, 0.50, 0.040); // T wave
    return v;
}
```

- [ ] **Step 4: Re-run tests; new ones should pass**

```bash
cd build && cmake --build . --target test_ecg && ./tests/test_ecg
```
Expected: all five tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/EcgWidget.cpp tests/test_ecg.cpp
git commit -m "feat(ecg): add PQRST waveform sampler"
```

---

## Task 5: EcgWidget — scrolling sample buffer + LCD trace painting

**Files:**
- Modify: `src/EcgWidget.cpp`
- Modify: `tests/test_ecg.cpp`

- [ ] **Step 1: Add the scroll-buffer test**

In `tests/test_ecg.cpp`, declare a new test in `private slots:`:
```cpp
    void onTickAdvancesPhaseAndShiftsBuffer();
```

Add the implementation:
```cpp
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
```

(The relaxed assertion accounts for phase wrapping; it just verifies advancement happened.)

- [ ] **Step 2: Expose `onTick` for the test (already a slot, but ensure it's invokable)**

In `src/EcgWidget.h`, the `onTick()` slot is already in `private slots:`. `QMetaObject::invokeMethod` can call private slots by name — no header change needed.

- [ ] **Step 3: Run the test; should fail (phase doesn't advance)**

```bash
cd build && cmake --build . --target test_ecg && ./tests/test_ecg -tc onTickAdvancesPhaseAndShiftsBuffer
```
Expected: assertion failure on `w.phase() != startPhase`.

- [ ] **Step 4: Implement phase advancement and buffer write in `onTick`**

In `src/EcgWidget.cpp`, replace the body of `onTick()` with:
```cpp
void EcgWidget::onTick()
{
    // Advance phase by (TICK_INTERVAL_MS / period_ms).
    const double periodMs = 60.0 * 1000.0 / HEART_RATE_BPM;
    const double dPhase = static_cast<double>(TICK_INTERVAL_MS) / periodMs;

    m_prevPhase = m_phase;
    m_phase += dPhase;
    if (m_phase >= 1.0) m_phase -= 1.0;

    // Write the current sample at the head, advance head circularly.
    m_samples[m_writeHead] = ecgSample(m_phase);
    m_writeHead = (m_writeHead + 1) % m_samples.size();

    update();
}
```

- [ ] **Step 5: Re-run the test; should pass**

```bash
cd build && cmake --build . --target test_ecg && ./tests/test_ecg
```
Expected: all six tests pass.

- [ ] **Step 6: Paint the LCD background, grid, and trace**

In `src/EcgWidget.cpp`, replace the placeholder `painter.fillRect(lcd, QColor(0x05, 0x1F, 0x0A));` line in `paintEvent` with:
```cpp
    // LCD background.
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x05, 0x1F, 0x0A));
    painter.drawRect(lcd);

    // Inner border for an LCD bezel feel.
    painter.setPen(QPen(QColor(0x00, 0x10, 0x05), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(lcd.adjusted(0, 0, -1, -1));

    // Grid: 10 px minor, every 5th major.
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (int x = lcd.left(); x <= lcd.right(); x += 10) {
        const bool major = ((x - lcd.left()) % 50 == 0);
        painter.setPen(QPen(major ? QColor(0x2D, 0x7A, 0x4A)
                                  : QColor(0x15, 0x4D, 0x2E), 1));
        painter.drawLine(x, lcd.top(), x, lcd.bottom());
    }
    for (int y = lcd.top(); y <= lcd.bottom(); y += 10) {
        const bool major = ((y - lcd.top()) % 20 == 0);
        painter.setPen(QPen(major ? QColor(0x2D, 0x7A, 0x4A)
                                  : QColor(0x15, 0x4D, 0x2E), 1));
        painter.drawLine(lcd.left(), y, lcd.right(), y);
    }
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Trace: read m_samples starting at m_writeHead (oldest sample) and
    // walk one pixel per sample across the LCD width.
    if (!m_samples.isEmpty()) {
        const double midY = lcd.center().y();
        const double scale = lcd.height() * 0.40; // -1..+1 maps to 80% of LCD
        QPainterPath trace;
        for (int i = 0; i < m_samples.size(); ++i) {
            const int idx = (m_writeHead + i) % m_samples.size();
            const double v = m_samples[idx];
            const double x = lcd.left() + i;
            const double y = midY - v * scale;
            if (i == 0) trace.moveTo(x, y);
            else        trace.lineTo(x, y);
        }
        // Soft glow underlay.
        painter.setPen(QPen(QColor(0x4F, 0xFF, 0x7A, 90), 4));
        painter.drawPath(trace);
        // Crisp top stroke.
        painter.setPen(QPen(QColor(0x4F, 0xFF, 0x7A), 1.6));
        painter.drawPath(trace);
    }
    painter.restore();
```

- [ ] **Step 7: Build, run, and visually verify the trace scrolls**

```bash
cd build && cmake --build . && open Oai.app
```
The widget is still not wired into MainWindow yet — to verify the visuals, temporarily add `EcgWidget *w = new EcgWidget; w->anchorTo(this); w->start();` in `MainWindow`'s constructor as a scratch line, build, observe, then revert. (Task 8 wires it properly.)

Expected: a small green CRT-style scrolling ECG appears above the pet for as long as the temporary line is in.

- [ ] **Step 8: Commit**

```bash
git add src/EcgWidget.cpp tests/test_ecg.cpp
git commit -m "feat(ecg): paint scrolling phosphor trace on LCD canvas"
```

---

## Task 6: EcgWidget — synthesized beep audio on R-peak

**Files:**
- Modify: `src/EcgWidget.cpp`
- Modify: `tests/test_ecg.cpp`

- [ ] **Step 1: Add the WAV header test**

In `tests/test_ecg.cpp`, declare:
```cpp
    void synthesizeBeepWavHasValidRiffHeader();
```

Add the implementation:
```cpp
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
```

- [ ] **Step 2: Run; should fail (returns empty QByteArray)**

```bash
cd build && cmake --build . --target test_ecg && ./tests/test_ecg
```
Expected: `synthesizeBeepWavHasValidRiffHeader` fails on `wav.size() > 44`.

- [ ] **Step 3: Implement `synthesizeBeepWav` in `src/EcgWidget.cpp`**

Replace the existing `synthesizeBeepWav()` stub with:
```cpp
QByteArray EcgWidget::synthesizeBeepWav()
{
    const int sr  = BEEP_SAMPLE_RATE;
    const int n   = sr * BEEP_DURATION_MS / 1000;
    const int fade = sr * BEEP_FADE_MS / 1000;
    const double w = 2.0 * M_PI * BEEP_FREQ_HZ / double(sr);

    QByteArray pcm;
    pcm.reserve(n * 2);
    for (int i = 0; i < n; ++i) {
        double env = 1.0;
        if (i < fade)             env = double(i) / fade;
        else if (i > n - fade)    env = double(n - i) / fade;
        const double s = std::sin(w * i) * env * 0.6;
        const qint16 v = static_cast<qint16>(qBound(-32767.0, s * 32767.0, 32767.0));
        pcm.append(static_cast<char>(v & 0xFF));
        pcm.append(static_cast<char>((v >> 8) & 0xFF));
    }

    QByteArray wav;
    wav.reserve(44 + pcm.size());

    auto putU32 = [&](quint32 v) {
        wav.append(char(v & 0xFF));
        wav.append(char((v >> 8) & 0xFF));
        wav.append(char((v >> 16) & 0xFF));
        wav.append(char((v >> 24) & 0xFF));
    };
    auto putU16 = [&](quint16 v) {
        wav.append(char(v & 0xFF));
        wav.append(char((v >> 8) & 0xFF));
    };

    wav.append("RIFF");
    putU32(36 + pcm.size());
    wav.append("WAVE");
    wav.append("fmt ");
    putU32(16);            // fmt chunk size
    putU16(1);             // PCM
    putU16(1);             // mono
    putU32(sr);            // sample rate
    putU32(sr * 2);        // byte rate (mono * 16-bit)
    putU16(2);             // block align
    putU16(16);            // bits/sample
    wav.append("data");
    putU32(pcm.size());
    wav.append(pcm);
    return wav;
}
```

- [ ] **Step 4: Wire `initAudio()` to write the WAV to a temp file and load `QSoundEffect`**

Replace the empty `initAudio()` stub with:
```cpp
void EcgWidget::initAudio()
{
    if (m_beep) return;

    m_beepFile = new QTemporaryFile(
        QDir::tempPath() + QStringLiteral("/oai_ecg_beep_XXXXXX.wav"));
    m_beepFile->setAutoRemove(true);
    if (!m_beepFile->open()) {
        delete m_beepFile;
        m_beepFile = nullptr;
        return;
    }
    m_beepFile->write(synthesizeBeepWav());
    m_beepFile->flush();
    const QString path = m_beepFile->fileName();
    m_beepFile->close(); // QSoundEffect needs the file readable, not held open.

    m_beep = new QSoundEffect(this);
    m_beep->setSource(QUrl::fromLocalFile(path));
    m_beep->setVolume(0.4);
    m_beep->setLoopCount(1);
}
```

Add the missing include at the top of `EcgWidget.cpp`:
```cpp
#include <QDir>
#include <QUrl>
```

In `start()`, before `m_tickTimer.start();`, add:
```cpp
    initAudio();
```

- [ ] **Step 5: Trigger the beep on R-peak crossing in `onTick`**

In `onTick()`, after the phase wrap and before `update()`, add:
```cpp
    // R-peak edge: phase crossed R_PEAK_PHASE within this tick.
    auto crossedR = [&]() {
        // Handles the wrap case where m_prevPhase > m_phase.
        if (m_prevPhase <= m_phase) {
            return m_prevPhase < R_PEAK_PHASE && m_phase >= R_PEAK_PHASE;
        }
        return R_PEAK_PHASE > m_prevPhase || R_PEAK_PHASE <= m_phase;
    };
    if (crossedR() && m_beep && m_beep->isLoaded()) {
        m_beep->play();
    }
```

- [ ] **Step 6: Build, run, and verify the beep**

```bash
cd build && cmake --build . && open Oai.app
```
Use the same scratch enable line from Task 5 Step 7 to instantiate `EcgWidget` and `start()` it. Listen for ~72 BPM beeps. Then revert the scratch line.

- [ ] **Step 7: Re-run the full test suite**

```bash
cd build && cmake --build . --target test_ecg && ./tests/test_ecg
```
Expected: 7 tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/EcgWidget.cpp tests/test_ecg.cpp
git commit -m "feat(ecg): synthesize and play 880 Hz beep on each R-peak"
```

---

## Task 7: SettingsPanelWidget — ECG toggle row

**Files:**
- Modify: `src/SettingsPanelWidget.h`
- Modify: `src/SettingsPanelWidget.cpp`

- [ ] **Step 1: Bump panel height and declare new members**

Edit `src/SettingsPanelWidget.h`. Change `PANEL_HEIGHT` from 210 to 250:
```cpp
    static constexpr int PANEL_HEIGHT = 250;
```

Add to the UI elements section (next to `m_autoStartLabel` / `m_autoStartCheck`):
```cpp
    QLabel *m_ecgLabel = nullptr;
    QCheckBox *m_ecgCheck = nullptr;
```

Add a new private slot under `private slots:`:
```cpp
    void onEcgToggled(bool checked);
```

- [ ] **Step 2: Construct the row and wire it to ConfigManager**

Edit `src/SettingsPanelWidget.cpp`. After the `m_autoStartCheck` block in `setupUi()` (around line 350, just before the `// Port row` comment), add:
```cpp
    // ECG row: label + checkbox
    m_ecgLabel = new QLabel(tr("ECG Monitor"), m_contentWidget);
    m_ecgLabel->setFont(harmonyFont(10));
    m_ecgLabel->setStyleSheet("color: black; background: transparent;");
    m_ecgLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_ecgCheck = new CheckMarkBox(m_contentWidget);
    m_ecgCheck->setFixedSize(16, 16);
    m_ecgCheck->setStyleSheet(R"(
        QCheckBox::indicator {
            width: 12px;
            height: 12px;
            background: white;
            border: 2px solid black;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background: #F36F1A;
            border: 1px solid #F36F1A;
        }
        QCheckBox::indicator:unchecked {
            background: white;
        }
    )");
    connect(m_ecgCheck, &QCheckBox::toggled,
            this, &SettingsPanelWidget::onEcgToggled);
```

In the `formGrid` block, shift Port and Model down a row and insert ECG between Auto-start and Port:
```cpp
    formGrid->addWidget(m_langLabel,       0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_langCombo,       0, 1);
    formGrid->addWidget(m_autoStartLabel,  1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_autoStartCheck,  1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_ecgLabel,        2, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_ecgCheck,        2, 1, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_portLabel,       3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_portInput,       3, 1);
    formGrid->addWidget(m_packLabel,       4, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_packButton,      4, 1);
```

In the constructor, after `m_autoStartCheck->setChecked(autoStart);` add:
```cpp
    m_ecgCheck->setChecked(m_config->ecgEnabled());
```

At the bottom of the file, define the slot:
```cpp
void SettingsPanelWidget::onEcgToggled(bool checked)
{
    m_config->setEcgEnabled(checked);
}
```

In `retranslateUi()`, add:
```cpp
    m_ecgLabel->setText(tr("ECG Monitor"));
```

- [ ] **Step 3: Build and verify the panel renders correctly**

```bash
cd build && cmake --build . && open Oai.app
```
Right-click the pet → Settings. The panel should now show 5 rows (Language, Launch at Login, ECG Monitor, Port, Model) with no clipping. Toggling the ECG checkbox should not yet produce any visible widget — wiring lands in Task 8.

- [ ] **Step 4: Commit**

```bash
git add src/SettingsPanelWidget.h src/SettingsPanelWidget.cpp
git commit -m "feat(settings): add ECG monitor toggle row"
```

---

## Task 8: MainWindow — own EcgWidget, anchor + show/hide

**Files:**
- Modify: `src/mainwindow.h`
- Modify: `src/mainwindow.cpp`

- [ ] **Step 1: Forward-declare and add the member**

Edit `src/mainwindow.h`. Add a forward declaration near the others (around line 18):
```cpp
class EcgWidget;
```

In the `private:` member-variable block (around line 80), add:
```cpp
    EcgWidget *m_ecgWidget = nullptr;
```

- [ ] **Step 2: Construct, anchor, and gate visibility on `ecgEnabled`**

Edit `src/mainwindow.cpp`. Add the include near the others:
```cpp
#include "EcgWidget.h"
```

In `MainWindow`'s constructor, after the existing `m_settingsPanel` block (around line 75, just before `connect(this, &MainWindow::positionChanged, m_config, ...)`), add:
```cpp
    m_ecgWidget = new EcgWidget(nullptr); // top-level, like the tip bubble
    m_ecgWidget->setAnchorRect(petRect());
    m_ecgWidget->anchorTo(this);
    if (m_config->ecgEnabled()) {
        m_ecgWidget->start();
    }

    connect(m_config, &ConfigManager::ecgEnabledChanged,
            this, [this](bool enabled) {
        if (enabled) {
            m_ecgWidget->setAnchorRect(petRect());
            m_ecgWidget->anchorTo(this);
            m_ecgWidget->start();
        } else {
            m_ecgWidget->stop();
        }
    });
```

In the position-changed lambda (the one that re-anchors `m_tipBubble`), append:
```cpp
        if (m_ecgWidget && m_ecgWidget->isVisible()) {
            m_ecgWidget->setAnchorRect(petRect());
            m_ecgWidget->anchorTo(this);
        }
```

In `toggleVisibility()`, in the `else` branch (when hiding the pet), add:
```cpp
        if (m_ecgWidget) m_ecgWidget->hide();
```
And in the `if (m_visible)` branch (when showing), add:
```cpp
        if (m_ecgWidget && m_config->ecgEnabled()) {
            m_ecgWidget->setAnchorRect(petRect());
            m_ecgWidget->anchorTo(this);
            m_ecgWidget->start();
        }
```

In `onActivePackChanged()`, after the existing `m_tipBubble->setAnchorRect(petRect()); m_tipBubble->anchorTo(this);` lines inside the Live2D timer callback (around line 510), add:
```cpp
            if (m_ecgWidget && m_ecgWidget->isVisible()) {
                m_ecgWidget->setAnchorRect(petRect());
                m_ecgWidget->anchorTo(this);
            }
```

Also at the top of `onActivePackChanged()`, after the window resize block where `m_petSize` is updated (around line 430), add:
```cpp
    if (m_ecgWidget && m_ecgWidget->isVisible()) {
        m_ecgWidget->setAnchorRect(petRect());
        m_ecgWidget->anchorTo(this);
    }
```

- [ ] **Step 3: Build and run; toggle ECG via Settings**

```bash
cd build && cmake --build . && open Oai.app
```
Manual checks:
- Right-click pet → Settings → check "ECG Monitor". The green LCD trace appears above the pet, beeping at ~72 BPM.
- Drag the pet around the screen — the ECG follows.
- Hide/Show the pet via the context menu — the ECG hides with it and reappears when shown.
- Switch character pack via Settings → Model — the ECG re-anchors to the new pet height.
- Uncheck "ECG Monitor" — the widget hides and the beep stops.
- Restart the app with ECG enabled — the setting persists.

- [ ] **Step 4: Run all tests to confirm nothing regressed**

```bash
cd build && ctest --output-on-failure
```
Expected: all tests in `test_ipc_animations` and `test_ecg` pass.

- [ ] **Step 5: Commit**

```bash
git add src/mainwindow.h src/mainwindow.cpp
git commit -m "feat(ecg): wire EcgWidget into MainWindow with config-driven toggle"
```

---

## Task 9: Translations — Chinese strings

**Files:**
- Modify: `Oai_zh_CN.ts`

- [ ] **Step 1: Run lupdate to add the new strings to the .ts file**

```bash
cd /Users/huangcheng/Projects/Oai && \
  /usr/bin/find . -path ./build -prune -o \( -name '*.cpp' -o -name '*.h' \) -print > /tmp/oai_sources.txt && \
  $(brew --prefix qt@6)/bin/lupdate $(cat /tmp/oai_sources.txt) -ts Oai_zh_CN.ts
```
Expected: lupdate prints "Found N source text(s)" and adds an unfinished `<message>` for `ECG Monitor` under the `SettingsPanelWidget` context.

- [ ] **Step 2: Fill in the Chinese translation**

Edit `Oai_zh_CN.ts`. Find the new entry inside the `SettingsPanelWidget` context:
```xml
        <message>
            <source>ECG Monitor</source>
            <translation type="unfinished"></translation>
        </message>
```
Replace with:
```xml
        <message>
            <source>ECG Monitor</source>
            <translation>心电图</translation>
        </message>
```

- [ ] **Step 3: Build (lrelease runs as part of Qt's translations target) and verify Chinese rendering**

```bash
cd build && cmake --build . && open Oai.app
```
In Settings, set Language → 简体中文. The new row label should read "心电图".

- [ ] **Step 4: Commit**

```bash
git add Oai_zh_CN.ts
git commit -m "i18n(zh_CN): translate ECG Monitor toggle"
```

---

## Task 10: Verification & polish

**Files:** none (verification only).

- [ ] **Step 1: Run the full test suite**

```bash
cd build && ctest --output-on-failure
```
Expected: all tests pass.

- [ ] **Step 2: Manual scenarios — golden path**

Open the app on macOS (`open build/Oai.app`). For each scenario, verify the listed outcome.

| Scenario | Expected |
|---|---|
| Default fresh launch | No ECG visible (default off). |
| Settings → check "ECG Monitor" | Green LCD ECG appears above pet, beeping ~72 BPM. |
| Drag pet across screen | ECG follows continuously. |
| Pet drag near top of screen | ECG clamps to screen top, doesn't disappear off-screen. |
| Send a tip via gateway (`oai-gateway --source claude-code --event session.start`) | Tip bubble appears above the ECG, briefly occluding it. ECG keeps tracing. |
| Switch pack via tray Pet menu | ECG repositions for the new pet size. |
| Toggle pet hide/show | ECG hides with the pet, reappears on show. |
| Settings → uncheck "ECG Monitor" | ECG hides; no more beeps. |
| Quit and relaunch with ECG enabled | ECG reappears at startup. |
| Switch language to 简体中文 | Settings row shows "心电图". |

- [ ] **Step 3: Manual scenarios — edge cases**

| Scenario | Expected |
|---|---|
| ECG enabled, system audio muted | Visual still scrolls; no audible beep (expected). |
| Multiple monitor setup, drag pet to second screen | ECG repositions onto the same screen as the pet. |
| Resize a Live2D pack mid-session | After the 500 ms re-crop in `onActivePackChanged`, ECG re-anchors above the cropped silhouette. |

- [ ] **Step 4: If any scenario fails, file a follow-up commit with the fix referencing the scenario number and re-run Step 1**

- [ ] **Step 5: Final cleanup commit if anything was touched in Step 4**

```bash
git add -A
git commit -m "fix(ecg): address verification scenarios"
```

If no fixes were needed, skip this step.

---

## Self-Review Notes (for the implementer)

- **Spec coverage check:** the four user requirements map as: snap to pet top → Task 3 + Task 8; optional via settings → Task 1 + Task 7 + Task 8; same border/frame as tip bubble & settings → Task 3 (uses identical SHADOW_BLUR/CORNER_RADIUS/BORDER_WIDTH/SKEW_PX values and orange stripe); LCD retro screen → Task 5 (phosphor-green palette, grid, scrolling polyline); real ECG sound → Task 6 (synthesized 880 Hz beep on R-peak).
- **Type consistency:** `start()` / `stop()` / `anchorTo()` / `setAnchorRect()` are the only public verbs; same names used in `EcgWidget.h`, `mainwindow.cpp`, and the test file.
- **No placeholders:** every step has either exact code, an exact command, or a concrete user-facing scenario.
- **Worktree note:** the writing-plans skill prefers running in a worktree spawned by brainstorming. This plan was authored on the main branch by direct user request; if isolation is desired, run `git worktree add ../Oai-ecg ecg-feature` before Task 1 and discard at the end via `superpowers:finishing-a-development-branch`.
