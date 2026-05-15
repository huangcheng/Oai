## Context

Oai is a Qt6/C++ desktop pet that always renders above all other windows via `Qt::WindowStaysOnTopHint`. This flag is set unconditionally in `MainWindow`, `TipBubbleWidget`, `EcgWidget`, and `SettingsPanelWidget`. Fullscreen games (e.g., Genshin Impact) render to a fullscreen surface; Qt's always-on-top window still composites above it on Windows and Linux (on macOS the game runs in its own Space so overlap is rare but still possible in windowed-fullscreen mode). There is no existing mechanism for Oai to detect games or yield window priority.

## Goals / Non-Goals

**Goals:**
- Detect when a fullscreen non-Oai application (game) is the foreground window
- Auto-hide all Oai windows (main pet, tip bubble, ECG widget) on fullscreen detection
- Auto-restore Oai windows when the fullscreen app exits or loses focus
- Provide a user toggle (tray menu + config persistence) to enable/disable Gaming Mode
- Show a tray tooltip when Oai hides/restores itself due to Gaming Mode

**Non-Goals:**
- Detecting specific games by name/process (allowlist/blocklist): out of scope for v1
- Adjusting window z-order rather than hiding (would still partially overlay on some platforms)
- Any in-game overlay or HUD integration
- Mobile or web platforms

## Decisions

### Decision 1: Hide vs. lower z-order

**Choice**: **Hide** (`QWidget::hide()`) all Oai windows when fullscreen is detected.

**Rationale**: Simply removing `WindowStaysOnTopHint` at runtime requires `setWindowFlags()` which re-parents the native window handle, causing a visible flash and losing window position on some platforms. Hiding is instant, clean, and fully reversible with `show()`. The pet is not useful while a fullscreen game is running anyway.

**Alternative considered**: `SetWindowPos` with `HWND_NOTOPMOST` (Windows-only) — platform-specific and still leaves the window partially visible under some compositor modes.

### Decision 2: Polling vs. OS event subscription

**Choice**: **QTimer-based polling** at 2-second intervals.

**Rationale**: Cross-platform OS event APIs for "foreground window changed to fullscreen" don't have a unified Qt abstraction. Polling at 2 s is imperceptible latency for a hide/show transition and costs < 0.1 ms per check. A `FullscreenWatcher` class encapsulates the platform calls behind a single `bool isFullscreenAppActive()` method.

**Alternative considered**: Windows `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` — Windows-only, adds complexity, and the latency advantage is unnecessary.

### Decision 3: Platform detection strategy

| Platform | API used |
|---|---|
| Windows | `GetForegroundWindow()` → `GetWindowRect()` + `GetMonitorInfo()` to compare window rect against monitor rect; exclude Oai's own `HWND` |
| macOS | `CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly)` — look for a window at level ≥ `CGWindowLevelForKey(kCGMainMenuWindowLevelKey)` covering the full display, excluding Oai processes |
| Linux/X11 | `_NET_ACTIVE_WINDOW` root property → `_NET_WM_STATE` atom check for `_NET_WM_STATE_FULLSCREEN`; fallback to window geometry vs screen geometry |

Platform-specific code lives in `src/FullscreenWatcher_win.cpp`, `_mac.mm`, `_x11.cpp` selected via `CMakeLists.txt`.

### Decision 4: Config & UI surface

- `ConfigManager` gains `gamingModeEnabled` bool (default: `false`, key `"gamingMode"`).
- SystemTray context menu gets a checkable **"Gaming Mode"** action.
- SettingsPanelWidget gets an optional toggle (same setting, secondary surface).

## Risks / Trade-offs

- [Risk] False positives on non-game fullscreen apps (e.g., YouTube in fullscreen browser) → Oai hides unnecessarily. **Mitigation**: User can disable Gaming Mode; v2 may add an app allowlist.
- [Risk] macOS Space isolation may mean the pet never overlaps a fullscreen game on macOS. **Mitigation**: Gaming Mode still works correctly (it will just hide unnecessarily rarely) — no regression.
- [Risk] Wayland does not expose foreground window or fullscreen state to client apps. **Mitigation**: Linux detection is X11-only; on Wayland, `isFullscreenAppActive()` returns `false` (Gaming Mode is a no-op but doesn't crash). Document this limitation.
- [Risk] 2-second polling wake: minimal CPU impact, but adds a timer to the event loop. **Mitigation**: Timer only runs when Gaming Mode is enabled.

## Migration Plan

1. Feature is off by default (`gamingModeEnabled = false`) — zero behavior change for existing users.
2. Users opt in via tray menu.
3. No data migration needed; config key is additive.
4. Rollback: disable Gaming Mode in tray; no persistent state other than the boolean config key.

## Open Questions

- Should the 2-second poll interval be user-configurable? (Current answer: no — keep it simple for v1.)
- Should a tray notification be shown only once per gaming session or every time? (Current answer: once per hide event, suppressed if tray is not available.)
