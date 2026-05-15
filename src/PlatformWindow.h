#ifndef OAI_PLATFORMWINDOW_H
#define OAI_PLATFORMWINDOW_H

class QWidget;

// Cross-platform wrappers for the Windows-specific DWM massage every
// frameless widget in the project needs. On non-Windows builds these are
// no-ops so call sites don't need `#ifdef Q_OS_WIN` blocks around them.
//
// Background: a frameless transparent Qt window on Windows ships with DWM's
// rounded-corner + system-backdrop + non-client-rendering defaults, all of
// which leave visible chrome the user can see through (drop shadows, mica
// gradients, a 1 px white border around the pet). Each of the 6 widgets
// previously open-coded the same three DwmSetWindowAttribute calls plus
// fallback DWMWA_* macros for older SDKs. Centralized here.
namespace PlatformWindow {

/// Make the widget's native window look like a "plain" frameless window:
///   - DWMWA_WINDOW_CORNER_PREFERENCE  → DWMWCP_DONOTROUND
///   - DWMWA_SYSTEMBACKDROP_TYPE       → DWMSBT_NONE
///   - DWMWA_NCRENDERING_POLICY        → DWMNCRP_DISABLED
///
/// Safe to call before the widget has a HWND (no-op until winId() resolves).
void applyDwmFramelessAttributes(QWidget *widget);

/// Force the OS to throw away cached chrome / composition state. Pair with
/// applyDwmFramelessAttributes when a recovery path (display sleep/wake,
/// DWM restart, GPU reset) needs to re-establish a stale window's visuals.
/// Calls SetWindowPos(SWP_FRAMECHANGED) and schedules a Qt repaint.
void refreshComposition(QWidget *widget);

} // namespace PlatformWindow

#endif // OAI_PLATFORMWINDOW_H
