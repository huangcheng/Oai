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
