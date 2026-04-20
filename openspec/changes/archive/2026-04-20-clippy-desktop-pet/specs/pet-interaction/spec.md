## ADDED Requirements

### Requirement: Drag-to-move
The system SHALL allow the user to drag Clippy to any position on the screen by clicking and holding on the sprite, then releasing to drop. The drag operation SHALL not steal focus from other applications.

#### Scenario: Drag Clippy to a new position
- **WHEN** the user clicks and holds on Clippy, moves the mouse, and releases
- **THEN** Clippy is repositioned to the new location and no other window loses focus

#### Scenario: Drag during animation
- **WHEN** Clippy is playing an animation and the user begins dragging
- **THEN** the current animation is interrupted (high priority), Clippy switches to a "Drag" animation, and follows the cursor until released

### Requirement: Click interactions
The system SHALL respond to single-click and double-click on Clippy:
- **Single click**: Triggers a random greeting or acknowledgment animation (Greeting, Wave, Acknowledge)
- **Double click**: Opens the configuration/settings dialog

#### Scenario: Single click on Clippy
- **WHEN** the user single-clicks on Clippy
- **THEN** a random greeting animation plays and a brief acknowledgment bubble appears ("What would you like help with?")

#### Scenario: Double click on Clippy
- **WHEN** the user double-clicks on Clippy
- **THEN** the settings dialog opens, allowing configuration of behaviors, timings, and toggle features

### Requirement: Right-click context menu
The system SHALL display a context menu on right-click with options: Settings, Hide, Show Log, Quit.

#### Scenario: Right-click context menu
- **WHEN** the user right-clicks on Clippy
- **THEN** a context menu appears with the specified options

### Requirement: Window transparency and click-through
The system SHALL render Clippy in a transparent, frameless window. The window SHALL be always-on-top. Transparent areas around the sprite SHALL pass mouse events through to underlying applications (click-through).

#### Scenario: Click-through transparent areas
- **WHEN** the user clicks on the transparent area surrounding Clippy
- **THEN** the click passes through to the window beneath, and Clippy does not intercept the event

#### Scenario: Click on sprite captures event
- **WHEN** the user clicks directly on Clippy's visible pixels
- **THEN** the click is captured by Clippy and triggers the appropriate interaction

### Requirement: Application switcher and taskbar exclusion (cross-platform)
The system SHALL NOT appear in the Dock (macOS), taskbar (Windows/Linux), or application switcher (Cmd+Tab/Alt+Tab) on any supported platform. This is achieved via `Qt::Tool` window type which maps to platform-native exclusion mechanisms.

Platform implementation:
- **macOS**: `Qt::Tool` hides from Dock and Cmd+Tab. Optionally set `activationPolicy` to `NSApplicationActivationPolicyAccessory` for process-level exclusion.
- **Windows**: `Qt::Tool` maps to `WS_EX_TOOLWINDOW` style, excluding from Alt+Tab and taskbar. Combined with `WS_EX_NOACTIVATE` to prevent focus stealing.
- **Linux (X11)**: `Qt::Tool` sets `_NET_WM_STATE_SKIP_TASKBAR` and `_NET_WM_STATE_SKIP_PAGER` EWMH properties.
- **Linux (Wayland)**: Best-effort via `Qt::Tool`; compositor behavior varies. No standard exclusion protocol.

#### Scenario: Excluded from application switcher (macOS)
- **WHEN** the user presses Cmd+Tab on macOS
- **THEN** Clippy does not appear in the application switcher

#### Scenario: Excluded from taskbar and Alt+Tab (Windows)
- **WHEN** Clippy is running on Windows
- **THEN** Clippy does not appear in the taskbar or Alt+Tab switcher

#### Scenario: Excluded from taskbar (Linux)
- **WHEN** Clippy is running on Linux (X11)
- **THEN** Clippy does not appear in the panel/taskbar or pager window list
