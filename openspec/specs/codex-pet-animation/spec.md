# codex-pet-animation Specification

## Purpose
TBD - created by archiving change add-codex-pet-support. Update Purpose after archive.
## Requirements
### Requirement: Codex pets render through SpriteAnimationEngine
The system SHALL render Codex pets using the existing `SpriteAnimationEngine` without modifying the engine's core frame-advance or paint logic.

#### Scenario: Frame rendering
- **WHEN** the animation engine ticks
- **THEN** the current frame is extracted from the loaded spritesheet QPixmap using the atlas coordinates from `FrameDef`
- **AND** painted to the main window via `SpriteAnimationEngine::paint()`

#### Scenario: Engine fallback compatibility
- **WHEN** the `SpriteAnimationEngine` loads a Codex pet
- **THEN** the engine's `hasAnimations()` returns true
- **AND** `lastPaintSuccessful()` reflects the actual render state

### Requirement: Codex pet animations loop according to row spec
The system SHALL configure each synthesized `AnimationDef` with `loop = true` so that each row animation loops indefinitely while active.

#### Scenario: Idle loop
- **WHEN** the `idle` animation reaches its final frame (frame 5)
- **THEN** the animation restarts from frame 0
- **AND** the frame durations are respected on each cycle

### Requirement: Codex pet frame geometry is fixed
The system SHALL use the hardcoded Codex atlas geometry for all `.codex-pet` files: 8 columns, 9 rows, 192×208 pixel cells.

#### Scenario: Atlas decomposition
- **WHEN** a Codex pet spritesheet is loaded
- **THEN** row 0 produces frames at y=0 with heights of 208 pixels
- **AND** row 1 produces frames at y=208
- **AND** each frame within a row is offset by x = frameIndex × 192

