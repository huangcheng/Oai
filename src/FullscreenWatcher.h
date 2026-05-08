#ifndef FULLSCREENWATCHER_H
#define FULLSCREENWATCHER_H

#include <QObject>

class QTimer;

/**
 * FullscreenWatcher polls the OS to detect whether a fullscreen non-Oai
 * application is the foreground window.  When the state changes it emits
 * fullscreenAppStarted() or fullscreenAppStopped().
 *
 * Platform support:
 *   Windows  — GetForegroundWindow + GetMonitorInfo, covers both exclusive
 *               fullscreen and borderless-windowed (e.g. Genshin Impact).
 *   macOS    — CGWindowListCopyWindowInfo; covers borderless fullscreen.
 *               True fullscreen apps live in their own Space, so overlap
 *               is rare but the check still runs correctly.
 *   Linux    — Not yet implemented; isFullscreenAppActive() always returns
 *               false (Gaming Mode is a no-op, no crash).
 */
class FullscreenWatcher : public QObject
{
    Q_OBJECT

public:
    explicit FullscreenWatcher(QObject *parent = nullptr);
    ~FullscreenWatcher() override;

    void start();
    void stop();
    bool isRunning() const;

signals:
    void fullscreenAppStarted();
    void fullscreenAppStopped();

private slots:
    void onPoll();

protected:
    virtual bool checkFullscreen();

    QTimer *m_timer = nullptr;
    bool m_prevState = false;

    static constexpr int POLL_INTERVAL_MS = 2000;
};

#endif // FULLSCREENWATCHER_H
