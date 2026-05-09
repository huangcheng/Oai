## 1. Core Codex Pet Parsing

- [x] 1.1 Add `CharacterPack::loadFromCodexPet(const QString &archivePath)` method that uses miniz to open the `.codex-pet` ZIP
- [x] 1.2 Parse `pet.json` from the archive and map fields to `CharacterPack::Metadata` (`id` → `id`, `displayName` → `name`, `description` → `description`)
- [x] 1.3 Set `CharacterConfig::engineType = EngineType::SpriteSheet` and populate `spriteSheet` path pointing to extracted `spritesheet.webp`
- [x] 1.4 Extract `spritesheet.webp` to a temporary/cache location so `SpriteAnimationEngine` can load it as a `QPixmap`

## 2. Spritesheet Frame Extraction

- [x] 2.1 Define Codex atlas constants: `COLUMNS = 8`, `ROWS = 9`, `CELL_WIDTH = 192`, `CELL_HEIGHT = 208`
- [x] 2.2 Implement per-row frame counts: `[6, 8, 8, 4, 5, 8, 6, 6, 6]` mapping to rows 0–8
- [x] 2.3 Implement per-row frame durations in ms: map each row's durations from the Codex spec
- [x] 2.4 Generate `AnimationDef` for each row with atlas `FrameDef` entries (`x = col * 192`, `y = row * 208`, `w = 192`, `h = 208`)
- [x] 2.5 Validate spritesheet dimensions (1536×1872); if mismatch, log error and abort load

## 3. Animation State Mapping

- [x] 3.1 Map row indices to animation names: `idle`, `running-right`, `running-left`, `waving`, `jumping`, `failed`, `waiting`, `running`, `review`
- [x] 3.2 Set `idlePool` to `[{"name": "idle", "weight": 1}]` for default idle behavior
- [x] 3.3 Add `eventMap` placeholder (optional MVP): map `session.error` → `failed`

## 4. Pack Discovery Integration

- [x] 4.1 Extend `CharacterPackManager::discoverPacks()` to scan for `*.codex-pet` files in both built-in and user pack directories
- [x] 4.2 Extend `CharacterPackManager::autoInstallBuiltInPacks()` to include `*.codex-pet` in the file filter
- [x] 4.3 When a `.codex-pet` file is found, call `CharacterPack::loadFromCodexPet()` and populate a `PackInfo` entry
- [x] 4.4 Ensure `switchPack()` correctly lazy-loads Codex pets into `m_loadedPacks`

## 5. Animation Engine Integration

- [x] 5.1 Verify `SpriteAnimationEngine::loadFromCharacterPack()` consumes the synthesized `CharacterPack` with atlas-mode `FrameDef` entries
- [x] 5.2 Ensure `hasAnimations()` returns true after loading a Codex pet
- [x] 5.3 Confirm `SpriteAnimationEngine::paint()` renders frames from the spritesheet using atlas coordinates
- [x] 5.4 Test that `idle` animation loops with correct per-frame durations

## 6. Drag & Drop Support

- [x] 6.1 Extend `dragEnterEvent` to accept `.codex-pet` files alongside `.opk`
- [x] 6.2 Add `isValidCodexPet()` validation to reject malformed `.codex-pet` files
- [x] 6.3 Extend `dropEvent` to copy valid `.codex-pet` files to user packs directory
- [x] 6.4 Show install success/failure feedback via tip bubble for `.codex-pet` drops

## 7. Testing & Validation

- [x] 7.1 Test loading a local `.codex-pet` file and verify metadata appears correctly
- [x] 7.2 Test frame extraction and verify 9 animations are generated with correct frame counts
- [x] 7.3 Test that the pet animates in the main window when selected
- [x] 7.4 Verify existing `.opk` packs still load and animate correctly (no regression)
- [x] 7.5 Build the project (`cmake --build .`) and ensure no compilation errors
