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
#include <QDir>
#include <QUrl>
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

    const int lcdW = PANEL_WIDTH - LCD_PADDING * 2;
    m_samples.resize(lcdW);
    m_samples.fill(0.0);

    m_tickTimer.setInterval(TICK_INTERVAL_MS);
    connect(&m_tickTimer, &QTimer::timeout, this, &EcgWidget::onTick);

    // Lazy: skip the audio subsystem until ECG is first enabled.
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
    initAudio();
    // Sync prevPhase so the first tick after re-show can't fire a false R-peak.
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

    painter.save();
    painter.setOpacity(0.35);
    painter.setPen(Qt::NoPen);
    QPainterPath shadowPath = panelPath;
    shadowPath.translate(3, 4);
    painter.setBrush(Qt::black);
    painter.drawPath(shadowPath);
    painter.restore();

    painter.setPen(QPen(Qt::black, BORDER_WIDTH, Qt::SolidLine,
                        Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(Qt::white);
    painter.drawPath(panelPath);

    painter.save();
    painter.setClipPath(panelPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xF3, 0x6F, 0x1A));
    painter.drawRect(QRectF(body.left(), body.top(), body.width() + sk, 4));
    painter.restore();

    const QRect lcd(SHADOW_BLUR + LCD_PADDING,
                    SHADOW_BLUR + LCD_PADDING,
                    PANEL_WIDTH  - LCD_PADDING * 2,
                    PANEL_HEIGHT - LCD_PADDING * 2);

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x05, 0x1F, 0x0A));
    painter.drawRect(lcd);

    painter.setPen(QPen(QColor(0x00, 0x10, 0x05), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(lcd.adjusted(0, 0, -1, -1));

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
        painter.setPen(QPen(QColor(0x4F, 0xFF, 0x7A, 90), 4));
        painter.drawPath(trace);
        painter.setPen(QPen(QColor(0x4F, 0xFF, 0x7A), 1.6));
        painter.drawPath(trace);
    }
    painter.restore();
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
    const double periodMs = 60.0 * 1000.0 / HEART_RATE_BPM;
    const double dPhase = static_cast<double>(TICK_INTERVAL_MS) / periodMs;

    m_prevPhase = m_phase;
    m_phase += dPhase;
    if (m_phase >= 1.0) m_phase -= 1.0;

    m_samples[m_writeHead] = ecgSample(m_phase);
    m_writeHead = (m_writeHead + 1) % m_samples.size();

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

    update();
}

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
}

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
