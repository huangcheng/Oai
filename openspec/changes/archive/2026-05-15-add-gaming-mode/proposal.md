## Why

Oai's pet window uses `Qt::WindowStaysOnTopHint` unconditionally, which causes it to render on top of fullscreen games (e.g., Genshin Impact, Steam titles). This breaks immersion and forces users to manually hide the pet before gaming and re-show it afterward — a frustrating UX gap for the core audience of developers who also game.

## What Changes

- Add a **Gaming Mode** setting that, when enabled, automatically hides the pet window whenever a fullscreen application is detected as the foreground window.
- When the fullscreen app exits or loses focus, the pet automatically reappears.
- Gaming Mode is toggled via the system tray context menu and persisted in config.
- A tray icon tooltip/notification confirms when Gaming Mode hides or restores the pet.

## Capabilities

### New Capabilities

- `gaming-mode`: Fullscreen detection poller and auto-hide/restore logic. Polls the foreground window state at a configurable interval; hides all Oai windows (MainWindow, TipBubbleWidget, EcgWidget) when a fullscreen non-Oai app is active and restores them when it is not.

### Modified Capabilities

- `settings`: Gaming Mode toggle added to the tray menu and persisted via ConfigManager.

## Impact

- **src/ConfigManager** — new `gamingModeEnabled` bool property + signal
- **src/mainwindow** — new `FullscreenWatcher` (QTimer-based poller) that calls `hide()`/`show()` on MainWindow and its child overlays
- **src/SettingsPanelWidget** — optional toggle in settings panel UI
- **src/SystemTray** — tray menu "Gaming Mode" checkable action
- Platform fullscreen detection: `GetForegroundWindow` + `GetWindowRect` / `MonitorFromWindow` (Windows), `CGWindowListCopyWindowInfo` (macOS), `_NET_ACTIVE_WINDOW` + `_NET_WM_STATE_FULLSCREEN` via Xlib/xcb (Linux)
- No new third-party dependencies; uses platform APIs via Qt's `QProcess`/native calls or `QWindow::fromWinId`
