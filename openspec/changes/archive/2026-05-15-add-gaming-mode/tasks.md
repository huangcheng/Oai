## 1. Config & Settings

- [x] 1.1 Add `gamingModeEnabled` bool property, getter, setter, and `gamingModeEnabledChanged` signal to `ConfigManager` (`.h` + `.cpp`)
- [x] 1.2 Load `gamingMode` key from QSettings in `ConfigManager::load()` with default `false`
- [x] 1.3 Save `gamingMode` key in `ConfigManager::save()`

## 2. FullscreenWatcher Core

- [x] 2.1 Create `src/FullscreenWatcher.h` — QObject subclass with `bool isFullscreenAppActive()` pure-virtual-like method and `QTimer` polling; signals `fullscreenAppStarted()` and `fullscreenAppStopped()`
- [x] 2.2 Create `src/FullscreenWatcher.cpp` — shared polling loop: start/stop timer, track previous state, emit signals on transitions only
- [x] 2.3 Create `src/FullscreenWatcher_win.cpp` — Windows implementation using `GetForegroundWindow()`, `GetWindowRect()`, `MonitorFromWindow()`, `GetMonitorInfo()`, excluding Oai's own HWND
- [x] 2.4 Create `src/FullscreenWatcher_mac.mm` — macOS implementation using `CGWindowListCopyWindowInfo` to detect a window at fullscreen level covering the full display, excluding Oai's `NSApp` windows
- [x] 2.5 Create `src/FullscreenWatcher_x11.cpp` — Linux/X11 implementation using `_NET_ACTIVE_WINDOW` root property + `_NET_WM_STATE_FULLSCREEN` atom check; return `false` gracefully on Wayland
- [x] 2.6 Wire platform files in `CMakeLists.txt` using `if(WIN32)` / `elseif(APPLE)` / `else()` conditionals

## 3. MainWindow Integration

- [x] 3.1 Instantiate `FullscreenWatcher` in `MainWindow` constructor when `ConfigManager::gamingModeEnabled()` is true; store as `m_fullscreenWatcher`
- [x] 3.2 Connect `FullscreenWatcher::fullscreenAppStarted` → slot `onFullscreenStarted()`: hide MainWindow + TipBubbleWidget + EcgWidget (only if visible and not already user-hidden); set `m_hiddenByGamingMode = true`; show tray message "Gaming Mode: Oai is hiding while you play"
- [x] 3.3 Connect `FullscreenWatcher::fullscreenAppStopped` → slot `onFullscreenStopped()`: if `m_hiddenByGamingMode`, show all windows and clear flag; show tray message "Gaming Mode: Oai is back!"
- [x] 3.4 Add `bool m_hiddenByGamingMode = false` member to `MainWindow` to distinguish Gaming Mode hides from user-initiated hides
- [x] 3.5 Connect `ConfigManager::gamingModeEnabledChanged` in `MainWindow`: start/stop `FullscreenWatcher` and restore windows if Gaming Mode is disabled while pet is hidden by it

## 4. System Tray Menu

- [x] 4.1 Add a checkable "Gaming Mode" `QAction` to the tray context menu in `SystemTray`
- [x] 4.2 Initialize the action's checked state from `ConfigManager::gamingModeEnabled()` on menu construction
- [x] 4.3 Connect action `toggled` → `ConfigManager::setGamingModeEnabled()`
- [x] 4.4 Connect `ConfigManager::gamingModeEnabledChanged` → update the tray action's checked state (in case it's changed from another UI surface)
- [x] 4.5 Add i18n string for "Gaming Mode" using `tr()` and add to `Oai_zh_CN.ts` translation file

## 5. Settings Panel (optional toggle)

- [x] 5.1 Add a `QCheckBox` for "Gaming Mode" in `SettingsPanelWidget` UI, bound to `ConfigManager::gamingModeEnabled`
- [x] 5.2 Connect checkbox `stateChanged` → `ConfigManager::setGamingModeEnabled()`
- [x] 5.3 Connect `ConfigManager::gamingModeEnabledChanged` → update checkbox state

## 6. Tests

- [x] 6.1 Add unit test for `ConfigManager`: verify `gamingMode` key round-trips correctly (default `false`, set `true`, save, reload, read back `true`)
- [x] 6.2 Add unit test for `FullscreenWatcher` polling logic: mock `isFullscreenAppActive()` returning a sequence of values and verify `fullscreenAppStarted`/`fullscreenAppStopped` signals fire exactly once per transition
