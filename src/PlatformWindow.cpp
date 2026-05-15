#include "PlatformWindow.h"

#include <QWidget>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dwmapi.h>

// SDK fallbacks. These constants ship in newer Windows SDKs; redefine
// when the build host has an older one so we keep one code path.
#  ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#    define DWMWA_WINDOW_CORNER_PREFERENCE 33
#  endif
#  ifndef DWMWA_SYSTEMBACKDROP_TYPE
#    define DWMWA_SYSTEMBACKDROP_TYPE 38
#  endif
#endif

namespace PlatformWindow {

void applyDwmFramelessAttributes(QWidget *widget)
{
#ifdef Q_OS_WIN
    if (!widget) return;
    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd) return;

    const int doNotRound = 1;          // DWMWCP_DONOTROUND
    ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                            &doNotRound, sizeof(doNotRound));
    const int backdropNone = 1;        // DWMSBT_NONE
    ::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                            &backdropNone, sizeof(backdropNone));
    const int ncRenderingDisabled = 1; // DWMNCRP_DISABLED
    ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                            &ncRenderingDisabled, sizeof(ncRenderingDisabled));
#else
    (void)widget;
#endif
}

void refreshComposition(QWidget *widget)
{
#ifdef Q_OS_WIN
    if (!widget) return;
    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd) return;
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                   SWP_FRAMECHANGED | SWP_NOACTIVATE);
    widget->update();
#else
    (void)widget;
#endif
}

} // namespace PlatformWindow
