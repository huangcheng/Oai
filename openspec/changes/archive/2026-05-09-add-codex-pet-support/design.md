## Context

Oai's character system is built around `CharacterPack` (manifest + assets) and `CharacterPackManager` (discovery + lifecycle). Packs are either directories or `.opk` ZIP archives. Three animation engines exist: Lottie (rlottie), SpriteSheet (QPixmap), and Live2D (Cubism).

Codex pets are `.codex-pet` ZIP archives containing:
- `pet.json` — metadata (`id`, `displayName`, `description`, `spritesheetPath`, `kind`)
- `spritesheet.webp` — a **fixed 8×9 sprite atlas** (1536×1872 pixels, 192×208 cells)

The Codex spritesheet uses a rigid grid where **each row is a distinct animation state**:

| Row | State | Frames | Durations |
|-----|-------|--------|-----------|
| 0 | idle | 6 | 280, 110, 110, 140, 140, 320 ms |
| 1 | running-right | 8 | 120×7, 220 ms |
| 2 | running-left | 8 | 120×7, 220 ms |
| 3 | waving | 4 | 140×3, 280 ms |
| 4 | jumping | 5 | 140×4, 280 ms |
| 5 | failed | 8 | 140×7, 240 ms |
| 6 | waiting | 6 | 150×5, 260 ms |
| 7 | running | 6 | 120×5, 220 ms |
| 8 | review | 6 | 150×5, 280 ms |

The Codex format is simpler than Oai's native manifest — it has no `eventMap` or `idlePool`. The animation states are hardcoded to the 9 rows.

## Goals / Non-Goals

**Goals:**
- Enable Oai to load `.codex-pet` files transparently alongside `.opk` packs
- Display Codex pet metadata correctly in the pack list
- Animate Codex pets using the existing `SpriteAnimationEngine`
- Map Codex animation states (9 rows) to Oai's animation system
- Support both built-in (shipped with app) and user-installed Codex pets

**Non-Goals:**
- Converting Codex pets to `.opk` format (load directly, no export)
- Editing or repacking Codex pets
- Supporting non-standard Codex pet variants (only the official 8×9 atlas format)
- Adding Lottie or Live2D support for Codex pets (they are sprite-sheet only)

## Decisions

### 1. Load `.codex-pet` as a `CharacterPack` on-the-fly
**Decision**: Parse `pet.json` and synthesize a `CharacterPack::Metadata` + `CharacterConfig` + `AnimationDef` list at load time, rather than converting to `.opk`.
**Rationale**: Conversion adds complexity and disk overhead. Codex pets are read-only in Oai, so ephemeral parsing is sufficient. The existing `loadFromDirectory()` path can be extended with a new `loadFromCodexPet(archivePath)` method.
**Alternative considered**: Extract to temp directory and load as normal pack — rejected because it adds I/O and cleanup complexity.

### 2. Re-use `SpriteAnimationEngine` without modifications
**Decision**: Feed synthesized `AnimationDef` frames into `SpriteAnimationEngine` via `loadFromCharacterPack()`.
**Rationale**: Codex pets are pure sprite sheets with atlas coordinates. The existing engine already supports atlas-mode `FrameDef` (`x`, `y`, `w`, `h`). No engine changes means less risk.

### 3. Fixed grid frame extraction (8×9, 192×208)
**Decision**: Use the known Codex atlas geometry: `COLUMNS = 8`, `ROWS = 9`, `CELL_WIDTH = 192`, `CELL_HEIGHT = 208`. Each row produces one `AnimationDef` with the row's used frames.
**Rationale**: The Codex format is rigid. All official pets use this exact grid. Hardcoding the geometry is reliable and avoids fragile image-analysis heuristics.
**Trade-off**: Non-standard pets (if any exist) won't load correctly. The format is officially fixed, so this is acceptable.

### 4. Map Codex rows to Oai animation names
**Decision**: Map each row index to a kebab-case animation name:
`idle`, `running-right`, `running-left`, `waving`, `jumping`, `failed`, `waiting`, `running`, `review`.
**Rationale**: Preserves Codex semantics while fitting Oai's animation naming conventions.
**Oai event mapping**: For MVP, set `idlePool` to `[{"name": "idle", "weight": 1}]` and do not map Codex states to Oai's 17 canonical events. Codex pets will idle by default. Future work can map `failed` → `session.error`, `running` → `tool.before`, etc.

### 5. Discover `.codex-pet` in `CharacterPackManager`
**Decision**: Extend `discoverPacks()` and `autoInstallBuiltInPacks()` to scan for `*.codex-pet` files alongside `*.opk`.
**Rationale**: Users should see Codex pets in the same list as native packs. The manager already handles ZIP extraction (via miniz) for `.opk`.
**Implementation**: In `discoverPacks()`, if a file ends with `.codex-pet`, call `CharacterPack::loadFromCodexPet()` directly instead of looking for a `manifest.json` in a subdirectory.

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| Codex spritesheet geometry changes in future versions | Format is officially fixed at 8×9/192×208; if it changes, update constants and re-release |
| No event mapping means pets won't react to specific Oai events | Acceptable for MVP — pet idles continuously. Future work adds event-to-row mapping |
| `spritesheet.webp` decoding performance | QImage auto-detects WebP via Qt image plugins; test on all platforms |
| pet.json schema changes in future Codex versions | Version-agnostic parsing: read known fields, ignore extras |
| Row frame counts vary (4–8 frames) | Use the hardcoded per-row frame counts from the Codex spec; ignore unused trailing cells |

## Migration Plan

No migration needed — this is a pure addition. Existing `.opk` packs and user configs are untouched.

## Open Questions

1. **Event mapping**: Should `failed` row auto-trigger on `session.error`? Should `running` trigger on `tool.before`? (Decision: defer to post-MVP; for now only idle animation plays.)
2. **Display scale**: Codex cells are 192×208. Oai's window is 124×200. Should we scale down? (Decision: let `SpriteAnimationEngine` scale via `displayScale` in `CharacterConfig`, default 1.0.)
3. **Running-left mirroring**: Some Codex pets mirror `running-right` for `running-left`. Oai should play the row as-is without additional mirroring logic.
