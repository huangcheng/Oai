## ADDED Requirements

### Requirement: System tray icon
The system SHALL display a system tray icon (menu bar on macOS, system tray on Windows/Linux). Clicking the icon SHALL toggle Clippy's visibility. The tray menu SHALL provide: Show/Hide, Settings, Quit.

#### Scenario: Toggle visibility from tray
- **WHEN** the user clicks the tray icon
- **THEN** Clippy toggles between visible and hidden states

#### Scenario: Tray menu options
- **WHEN** the user right-clicks the tray icon
- **THEN** a menu appears with Show/Hide, Settings, and Quit options

### Requirement: Single instance enforcement
The system SHALL enforce a single running instance. If a second instance is launched, it SHALL bring the existing instance to the foreground and exit immediately.

#### Scenario: Second instance launch
- **WHEN** the user attempts to launch Clippy while it is already running
- **THEN** the existing instance's window is brought to the front, and the new instance exits

#### Scenario: Stale lock recovery
- **WHEN** Clippy crashed previously and left a stale lock file
- **THEN** the new instance detects the stale lock (PID not running), removes it, and starts normally

### Requirement: Configuration persistence
The system SHALL persist configuration to `~/.config/Clippy/config.json` (macOS/Linux) or `%APPDATA%/Clippy/config.json` (Windows). Supported settings include:
- `enabled`: Boolean — whether Clippy is visible
- `idleTimeoutMs`: Integer — milliseconds before idle animations trigger (default: 3000)
- `speechBubbleTimeoutMs`: Integer — milliseconds before tips auto-dismiss (default: 8000)
- `statusBubbleTimeoutMs`: Integer — milliseconds before status messages auto-dismiss (default: 4000)
- `position`: Object — `{x, y}` last known screen position
- `soundEnabled`: Boolean — whether sound effects play (default: false)
- `language`: String — locale code for i18n (default: system locale)

#### Scenario: Config load on startup
- **WHEN** the application starts and a config file exists
- **THEN** the system reads the config and restores the previous position, settings, and state

#### Scenario: Config save on change
- **WHEN** the user changes a setting via the settings dialog
- **THEN** the config file is updated within 1 second (debounced)

### Requirement: i18n support
The system SHALL support internationalization via Qt's `QTranslator`. At minimum, English and Simplified Chinese translations SHALL be provided. The UI SHALL detect the system locale on startup and load the appropriate translation.

#### Scenario: System locale detection
- **WHEN** the application starts on a Chinese macOS system
- **THEN** the UI displays in Simplified Chinese (`zh_CN`)

#### Scenario: Fallback to English
- **WHEN** the application starts on a system with an unsupported locale
- **THEN** the UI falls back to English

### Requirement: Auto-start on login (cross-platform)
The system SHALL optionally start at user login, using platform-native mechanisms. This SHALL be toggleable in settings.
- **macOS**: Register with `SMLoginItemSetEnabled` via a helper bundle in `Library/LoginItems`
- **Windows**: Add/remove entry in `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run` via QSettings
- **Linux**: Create/remove `.desktop` file in `~/.config/autostart/Clippy.desktop` (XDG autostart spec)

#### Scenario: Enable auto-start (macOS)
- **WHEN** the user enables "Start at Login" in settings on macOS
- **THEN** Clippy registers as a login item via SMLoginItemSetEnabled and starts automatically on next boot

#### Scenario: Enable auto-start (Windows)
- **WHEN** the user enables "Start at Login" in settings on Windows
- **THEN** Clippy adds a registry entry under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` and starts automatically on next boot

#### Scenario: Enable auto-start (Linux)
- **WHEN** the user enables "Start at Login" in settings on Linux
- **THEN** Clippy creates `~/.config/autostart/Clippy.desktop` with `Type=Application` and `Exec=<path-to-clippy>` and starts automatically on next login

#### Scenario: Disable auto-start
- **WHEN** the user disables "Start at Login" in settings on any platform
- **THEN** the platform-specific auto-start entry is removed

### Requirement: Taskbar/Dock/Alt-Tab exclusion
The system SHALL NOT appear in the taskbar (Windows), Dock (macOS), or taskbar/pager (Linux) application lists. Clippy SHALL also not appear in the Alt+Tab (Windows), Cmd+Tab (macOS), or Alt+Tab (Linux) application switcher.

Platform implementation:
- **macOS**: `Qt::Tool` window type hides from Dock and Cmd+Tab. Optionally set `activationPolicy` to `NSApplicationActivationPolicyAccessory` for process-level exclusion.
- **Windows**: `Qt::Tool` window type maps to `WS_EX_TOOLWINDOW` which excludes from Alt+Tab and taskbar. `WS_EX_NOACTIVATE` prevents stealing focus.
- **Linux (X11)**: `Qt::Tool` sets `_NET_WM_STATE_SKIP_TASKBAR` and `_NET_WM_STATE_SKIP_PAGER` via EWMH.
- **Linux (Wayland)**: No standard protocol for taskbar exclusion; `Qt::Tool` is best-effort. Compositor-specific behavior may vary.

#### Scenario: Clippy hidden from application switcher (macOS)
- **WHEN** the user presses Cmd+Tab on macOS
- **THEN** Clippy does not appear in the application switcher

#### Scenario: Clippy hidden from taskbar (Windows)
- **WHEN** Clippy is running on Windows
- **THEN** Clippy does not appear in the Windows taskbar

#### Scenario: Clippy hidden from taskbar (Linux)
- **WHEN** Clippy is running on Linux with X11
- **THEN** Clippy does not appear in the panel/taskbar or window list
