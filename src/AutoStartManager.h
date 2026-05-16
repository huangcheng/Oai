#ifndef SEELIE_AUTOSTARTMANAGER_H
#define SEELIE_AUTOSTARTMANAGER_H

// Reflect the user's "launch at login" preference into the OS's per-user
// auto-start facility. Stateless namespace — call setEnabled() whenever
// the toggle flips; the function reads QCoreApplication::applicationFilePath()
// itself.
//
// The OS is the source of truth for autostart state. We don't shadow it in
// QSettings: that historically caused the Windows installer's "Launch at
// startup" task to silently disagree with the in-app toggle, since both
// targeted the same registry key but the app rewrote it from a stale INI
// on every launch. isEnabled() reads the canonical OS state directly.
//
// Per-platform:
//   Windows: HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Seelie
//   macOS:   ~/Library/LaunchAgents/im.cheng.seelie.plist (launchd LaunchAgent)
//   Linux:   ~/.config/autostart/seelie.desktop (XDG autostart spec)
//
// On AppImage builds the Linux path uses $APPIMAGE rather than the
// transient FUSE-mount path returned by applicationFilePath().
//
// Other Unix variants (BSD, Haiku, etc.) are a no-op (isEnabled returns false).
namespace AutoStartManager {

/// Enable or disable launch-at-login for the current user.
/// Disables next-launch behaviour by removing the registry value /
/// plist / .desktop file. The "next login" semantics match across all
/// three platforms — toggling does not affect the currently running
/// process.
void setEnabled(bool enabled);

/// Returns true iff the OS-level autostart entry currently exists. Cheap
/// (one registry read or stat per call). Source of truth — call sites
/// MUST NOT cache this in a separate INI field; defer to the OS.
bool isEnabled();

} // namespace AutoStartManager

#endif // SEELIE_AUTOSTARTMANAGER_H
