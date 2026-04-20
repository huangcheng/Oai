## ADDED Requirements

### Requirement: Proactive tip detection
The system SHALL monitor incoming IPC events and detect patterns that trigger proactive tips. Pattern rules include:
- **File creation**: New file created → tip about file naming conventions
- **Config edit**: Editing `*.json`, `*.yaml`, `*.toml` → tip about configuration best practices
- **Git error**: `git.error` event → tip about common git fixes
- **Build failure**: `build.failed` event → tip about debugging build errors
- **Long session**: No activity for 30 minutes, then activity resumes → tip about taking breaks
- **Repeated error**: Same error event 3+ times within 5 minutes → tip about the specific error pattern

#### Scenario: Config file tip
- **WHEN** the user creates or edits a `.json` file
- **THEN** after a 5-second cooldown, Clippy displays a tip: "It looks like you're editing configuration! Consider using a schema validator."

#### Scenario: Repeated error tip
- **WHEN** the same error event is received 3 times within 5 minutes
- **THEN** Clippy displays a targeted tip about that error pattern with a link to documentation

### Requirement: Tip cooldowns
Each tip type SHALL have a per-session cooldown to prevent spam. The default cooldown is 5 minutes per tip type.

#### Scenario: Cooldown prevents spam
- **WHEN** a config edit tip was shown 1 minute ago and another config edit event occurs
- **THEN** no tip is displayed because the cooldown has not expired

### Requirement: Tip priority override
Tips SHALL respect the animation priority system (normal priority). If a high-priority animation is playing (e.g., user clicked Clippy), the tip SHALL queue until the high-priority animation completes.

#### Scenario: Tip queues during user interaction
- **WHEN** the user clicks Clippy (high priority) and a tip is triggered simultaneously
- **THEN** the tip speech bubble waits until the click animation completes before appearing

### Requirement: Tip dismissal
The user SHALL be able to dismiss a tip early by clicking an X button on the speech bubble, clicking anywhere outside the bubble, or pressing Escape.

#### Scenario: User dismisses tip
- **WHEN** a tip is displayed and the user clicks the X button
- **THEN** the speech bubble exits immediately and the tip is marked as dismissed
