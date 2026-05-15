#ifndef OAI_AUTOSTARTMANAGER_H
#define OAI_AUTOSTARTMANAGER_H

// Reflect the user's "launch at login" preference into the OS's per-user
// auto-start facility. Stateless namespace — call setEnabled() whenever
// the toggle flips; the function reads QCoreApplication::applicationFilePath()
// itself.
//
// Per-platform:
//   Windows: HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Oai
//   macOS:   ~/Library/LaunchAgents/im.cheng.oai.plist (launchd LaunchAgent)
//   Linux:   ~/.config/autostart/oai.desktop (XDG autostart spec)
//
// On AppImage builds the Linux path uses $APPIMAGE rather than the
// transient FUSE-mount path returned by applicationFilePath().
//
// Other Unix variants (BSD, Haiku, etc.) are a no-op.
namespace AutoStartManager {

/// Enable or disable launch-at-login for the current user.
/// Disables next-launch behaviour by removing the registry value /
/// plist / .desktop file. The "next login" semantics match across all
/// three platforms — toggling does not affect the currently running
/// process.
void setEnabled(bool enabled);

} // namespace AutoStartManager

#endif // OAI_AUTOSTARTMANAGER_H
