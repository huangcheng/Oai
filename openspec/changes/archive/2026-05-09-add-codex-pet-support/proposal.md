## Why

Codex CLI introduced `.codex-pet` files — animated companion characters that react to coding events. Seelie currently only supports its native `.opk` character packs (Lottie/SpriteSheet/Live2D). Users want to bring their Codex pets into Seelie, but there's no loading path. Adding Codex pet support expands the character ecosystem without requiring users to repack or convert assets.

## What Changes

- **Add `.codex-pet` discovery and loading** to `CharacterPackManager`
- **Add Codex pet parsing** — read `pet.json` + `spritesheet.webp` from the zip archive
- **Add spritesheet frame extraction** — Codex pets use a vertical strip spritesheet with fixed frame dimensions; build `AnimationDef` frames from the spritesheet metadata
- **Map Codex pet metadata** to Seelie's `CharacterPack::Metadata` and `CharacterConfig`
- **Integrate with existing `SpriteAnimationEngine`** — Codex pets are sprite-sheet based, so they map naturally to the existing engine
- **No breaking changes** — native `.opk` packs continue to work unchanged

## Capabilities

### New Capabilities
- `codex-pet-loading`: Discover, load, and parse `.codex-pet` zip archives into Seelie's `CharacterPack` data model
- `codex-pet-animation`: Extract frames from Codex spritesheets and drive them through `SpriteAnimationEngine`

### Modified Capabilities
- *(none — this is a pure addition with no spec-level behavior changes to existing capabilities)*

## Impact

- **CharacterPackManager**: `.codex-pet` files are discovered alongside `.opk` in built-in and user pack directories
- **CharacterPack**: New loading path for Codex format (zip with `pet.json` + `spritesheet.webp`)
- **SpriteAnimationEngine**: No changes required — Codex pet frames feed into existing sprite-sheet pipeline
- **UI/Settings**: Codex pets appear in the character pack list automatically
- **Dependencies**: `miniz` already used for `.opk` extraction; QImage/WebP support handles `spritesheet.webp`
