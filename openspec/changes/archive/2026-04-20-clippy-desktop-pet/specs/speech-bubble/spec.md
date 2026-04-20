## ADDED Requirements

### Requirement: Windows 98-style speech bubble rendering
The system SHALL render speech bubbles in the classic Windows 98 tooltip style: `#FFFFE1` yellow background, 1px black border, 2px `#808080` drop shadow, Tahoma-equivalent 11px font, and a triangular tail pointing toward Clippy.

#### Scenario: Display speech bubble with title
- **WHEN** the system receives a tip or event with a title string
- **THEN** a speech bubble appears adjacent to Clippy with the title text rendered in the Windows 98 style

#### Scenario: Display speech bubble with title and body
- **WHEN** the system receives a tip with both title and body strings
- **THEN** the speech bubble shows the title in bold and the body in regular weight, both in the Windows 98 style

### Requirement: Auto-dismiss timers
The system SHALL automatically dismiss speech bubbles after a configurable timeout:
- Status messages: 4 seconds
- Tip messages: 8 seconds

#### Scenario: Status bubble auto-dismiss
- **WHEN** a status-type speech bubble is displayed
- **THEN** it automatically fades out and is removed after 4 seconds

#### Scenario: Tip bubble auto-dismiss
- **WHEN** a tip-type speech bubble is displayed
- **THEN** it automatically fades out and is removed after 8 seconds

### Requirement: Speech bubble positioning
The system SHALL position the speech bubble above or to the side of Clippy, ensuring it stays within screen bounds. If the bubble would extend beyond the screen edge, it SHALL be repositioned to stay visible.

#### Scenario: Bubble above Clippy
- **WHEN** Clippy is in the lower half of the screen and a speech bubble is triggered
- **THEN** the bubble appears above Clippy with the tail pointing down

#### Scenario: Bubble repositioned to stay on screen
- **WHEN** Clippy is near the right edge of the screen and a speech bubble would extend beyond the screen
- **THEN** the bubble is shifted left to stay fully visible

### Requirement: Bubble enter/exit animation
The system SHALL animate the speech bubble appearance with a brief scale-up animation (0 to full size over 200ms) and disappearance with a scale-down animation (full to 0 over 150ms).

#### Scenario: Bubble enter animation
- **WHEN** a new speech bubble is triggered
- **THEN** the bubble scales from 0% to 100% over 200ms with an ease-out curve

#### Scenario: Bubble exit animation
- **WHEN** a speech bubble is dismissed (auto or replaced)
- **THEN** the bubble scales from 100% to 0% over 150ms with an ease-in curve
