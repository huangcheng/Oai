## ADDED Requirements

### Requirement: Fullscreen detection
The system SHALL poll the OS at a 2-second interval to determine whether a fullscreen non-Seelie application is the active foreground window on any connected display, using platform-native APIs (Windows: `GetForegroundWindow`/`GetMonitorInfo`; macOS: `CGWindowListCopyWindowInfo`; Linux/X11: `_NET_ACTIVE_WINDOW` + `_NET_WM_STATE_FULLSCREEN`).

#### Scenario: Fullscreen game detected on Windows
- **WHEN** Gaming Mode is enabled AND `GetForegroundWindow()` returns an HWND whose window rect matches the monitor's work area and is not owned by Seelie
- **THEN** the system transitions to the hidden state

#### Scenario: Fullscreen game detected on macOS
- **WHEN** Gaming Mode is enabled AND `CGWindowListCopyWindowInfo` reports a window at fullscreen level covering the full display and not belonging to the Seelie process
- **THEN** the system transitions to the hidden state

#### Scenario: Fullscreen game detected on Linux/X11
- **WHEN** Gaming Mode is enabled AND `_NET_ACTIVE_WINDOW` has `_NET_WM_STATE_FULLSCREEN` in its `_NET_WM_STATE` property
- **THEN** the system transitions to the hidden state

#### Scenario: No fullscreen app active
- **WHEN** Gaming Mode is enabled AND no fullscreen non-Seelie window is detected
- **THEN** the system remains in (or transitions to) the visible state

#### Scenario: Wayland environment
- **WHEN** Gaming Mode is enabled AND the platform is Wayland (X11 atoms not available)
- **THEN** `isFullscreenAppActive()` SHALL return `false` and the pet SHALL remain visible (no crash, no undefined behavior)

### Requirement: Auto-hide on fullscreen
The system SHALL hide all Seelie UI windows (MainWindow, TipBubbleWidget, EcgWidget, and any active LottieEffectOverlay) when transitioning to the hidden state due to fullscreen detection.

#### Scenario: Pet hidden when game launches
- **WHEN** a fullscreen app is detected AND Seelie windows are currently visible
- **THEN** all Seelie windows SHALL call `hide()` within one polling cycle (≤ 2 seconds)

#### Scenario: No double-hide
- **WHEN** a fullscreen app is detected AND Seelie windows are already hidden (e.g., user manually hid them)
- **THEN** the system SHALL NOT call `hide()` again or alter the user's manual visibility state

### Requirement: Auto-restore after fullscreen
The system SHALL restore Seelie window visibility when the fullscreen application exits or loses foreground status and Gaming Mode is still enabled.

#### Scenario: Pet restored when game exits
- **WHEN** no fullscreen app is detected AND Seelie was hidden by Gaming Mode (not manually by user)
- **THEN** all Seelie windows SHALL call `show()` within one polling cycle (≤ 2 seconds)

#### Scenario: Manual hide respected
- **WHEN** the user has manually hidden the pet via the tray icon AND Gaming Mode hides the pet AND the fullscreen app exits
- **THEN** the pet SHALL remain hidden (Gaming Mode does not override the user's manual hide)

### Requirement: Gaming Mode toggle persistence
The system SHALL persist the Gaming Mode enabled/disabled state in `ConfigManager` under the key `gamingMode` (boolean, default `false`) and restore it on next launch.

#### Scenario: Gaming Mode enabled and app restarts
- **WHEN** the user enables Gaming Mode AND restarts Seelie
- **THEN** Gaming Mode SHALL be enabled on launch and the fullscreen poller SHALL start automatically

#### Scenario: Gaming Mode disabled by default
- **WHEN** Seelie is launched for the first time (no config file)
- **THEN** Gaming Mode SHALL be disabled and no fullscreen polling SHALL occur

### Requirement: Tray menu toggle
The system SHALL provide a checkable "Gaming Mode" action in the system tray context menu that reflects the current `gamingModeEnabled` state and toggles it when activated.

#### Scenario: Tray action reflects state
- **WHEN** the tray context menu is opened
- **THEN** the "Gaming Mode" action SHALL be checked if Gaming Mode is enabled, unchecked if disabled

#### Scenario: Toggle via tray
- **WHEN** the user activates the "Gaming Mode" tray action
- **THEN** `gamingModeEnabled` SHALL be toggled and saved, and the fullscreen poller SHALL start or stop accordingly

### Requirement: Tray notification on hide/restore
The system SHALL show a system tray tooltip message when the pet is hidden or restored by Gaming Mode.

#### Scenario: Notification on hide
- **WHEN** Gaming Mode hides the pet
- **THEN** a tray tooltip SHALL display a message such as "Gaming Mode: Seelie is hiding while you play"

#### Scenario: Notification on restore
- **WHEN** Gaming Mode restores the pet
- **THEN** a tray tooltip SHALL display a message such as "Gaming Mode: Seelie is back!"
