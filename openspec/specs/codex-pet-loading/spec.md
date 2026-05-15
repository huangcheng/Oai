# codex-pet-loading Specification

## Purpose
TBD - created by archiving change add-codex-pet-support. Update Purpose after archive.
## Requirements
### Requirement: System can discover Codex pet files
The system SHALL discover `.codex-pet` files in both the built-in packs directory and the user packs directory.

#### Scenario: Built-in Codex pet discovery
- **WHEN** the application initializes pack discovery
- **THEN** any `.codex-pet` files in the built-in packs directory appear in the pack list

#### Scenario: User Codex pet discovery
- **WHEN** a user places a `.codex-pet` file in `~/.config/Seelie/packs/`
- **THEN** the pet appears in the pack list without restarting the application

### Requirement: System can load Codex pet metadata
The system SHALL parse `pet.json` from a `.codex-pet` archive and expose `id`, `displayName`, and `description` through the `CharacterPack` metadata interface.

#### Scenario: Load pet metadata
- **WHEN** the system loads a `.codex-pet` file
- **THEN** the `CharacterPack::Metadata` contains the `id` from `pet.json`
- **AND** the `displayName` maps to `CharacterPack::Metadata::name`
- **AND** the `description` maps to `CharacterPack::Metadata::description`

### Requirement: System can extract Codex spritesheet frames
The system SHALL read `spritesheet.webp` from the `.codex-pet` archive and decompose the fixed 8×9 grid into individual frames for the `SpriteAnimationEngine`.

#### Scenario: Fixed grid atlas
- **WHEN** the spritesheet is a valid Codex atlas (1536×1872 pixels)
- **THEN** the system uses `CELL_WIDTH = 192` and `CELL_HEIGHT = 208`
- **AND** generates frames for each row according to the hardcoded per-row frame counts
- **AND** ignores unused trailing cells in each row

#### Scenario: Invalid atlas dimensions
- **WHEN** the spritesheet dimensions do not match 1536×1872
- **THEN** the system logs an error and marks the pack as unloadable

### Requirement: System can animate Codex pets
The system SHALL synthesize one `AnimationDef` per row from the Codex spritesheet frames, using the hardcoded row names and frame durations, and play them through `SpriteAnimationEngine`.

#### Scenario: Pet animation starts
- **WHEN** a Codex pet is selected as the active pack
- **THEN** the `SpriteAnimationEngine` loads the spritesheet
- **AND** begins playing the `idle` animation as the default idle loop

#### Scenario: Row frame timing
- **WHEN** the `idle` animation is playing
- **THEN** frame 0 displays for 280 ms
- **AND** frame 1 displays for 110 ms
- **AND** the final frame displays for 320 ms before looping

