---
name: import-live2d-models
description: Import Live2D character packs from the upstream Eikanya/Live2d-model archive (or any local clone of it) into assets/packs/. Use when the user wants to add new characters, expand a category, import a specific shipgirl/idol/etc, or refresh the imported pack lineup. Also covers re-running imports after updating the upstream submodule pin.
license: MIT
metadata:
  author: seelie
  version: "1.0"
---

Import Live2D Cubism 3+ packs from the upstream community archive into `assets/packs/`. Imports are gitignored build artifacts — only the 21 first-party packs are tracked.

## When to use

Triggers:
- "import more characters", "add another shipgirl", "pull X from the upstream"
- "regenerate packs", "refresh the live2d lineup"
- "extend PICKS", "add a new category"
- after `git pull` brings a new submodule pin

Do NOT use when the user wants to add a *first-party* original pack (Live2D Free Material samples, hand-authored content) — those live committed in `assets/packs/` and bypass the import pipeline. For those, write the manifest by hand following `schemas/character-pack-v1.schema.json`.

## Pre-flight

Confirm three things before starting:

1. **Upstream available locally.** Either:
   - `thirdparty/upstream-live2d/` exists and has files in it (`ls thirdparty/upstream-live2d` shows folders like `碧蓝航线 Azue Lane`, `少女前线 girls Frontline`, etc.). Init with `git submodule update --init --depth=1 thirdparty/upstream-live2d` if not.
   - OR the user has an external local clone (e.g. `F:/Live2d-model`). The import script's `--local <path>` accepts any directory.
2. **What category.** Map their request to one of the whitelisted categories in `scripts/import_live2d.py:LOCAL_CATEGORIES`. If they want a folder not in that whitelist, ask whether to add it (small edit) or skip.
3. **Naming:** for arbitrary picks the import generates pinyin-derived names (`Sd 042` / `Ks 1124100`). For nicer English+Chinese names the user has to either (a) extend `PICKS` in the script, or (b) accept the pinyin and edit manifests post-hoc. If they want polish, add to `PICKS`.

## Steps

### 1. Update submodule pin (only if user asked for "newest" or "latest")

```bash
cd thirdparty/upstream-live2d && git pull && cd -
git add thirdparty/upstream-live2d
```

Skip otherwise. The pinned commit is fine for most imports.

### 2. (Optional) Extend `PICKS` for curated names

`scripts/import_live2d.py` has two name sources:
- `PICKS = [(upstream_path, local_id, name_en, name_zh), ...]` — hand-curated, gets nice names
- Bulk per-category sweep — auto-pinyin fallback

To add curated entries, append tuples to `PICKS` like:
```python
("碧蓝航线 Azue Lane/Azue Lane(JP)/lafei", "laffey", "Laffey", "拉菲"),
```

The path must exist under the upstream root. Verify before writing:
```bash
ls "thirdparty/upstream-live2d/碧蓝航线 Azue Lane/Azue Lane(JP)/lafei"
```

### 3. Run the import

```bash
python scripts/import_live2d.py --local thirdparty/upstream-live2d --cap 50
```

Flags worth knowing:
- `--cap N` — max packs per category (default 50; keeps each menu submenu navigable). Safe to bump for sparse categories like Konosuba where dedup leaves few entries.
- `--out-dir <path>` — defaults to `assets/packs/` which is what you want.
- Specific ids: `python scripts/import_live2d.py id1 id2 ...` runs only the named PICKS (network mode, network required). For local-clone use `--local`.

The script:
- Skips packs that already exist on disk (idempotent re-runs are cheap).
- Wipes pack dirs that look half-finished (<100 KB total) so a previous failure doesn't get respected forever.
- Patches `model3.json` motion groups via `patch_model3()` — handles three upstream layouts (single anonymous group, many per-file groups, missing `Motions` key with files on disk).
- Rewrites motion3.json `TotalSegmentCount` / `TotalPointCount` via `sanitize_motion()` — Yostar's exporter writes wrong counts which cause heap overrun in Cubism's `LoadMotion`.
- Stamps `category` into the manifest based on the upstream folder.

### 4. Verify

```bash
python -c "
import json
from pathlib import Path
from collections import Counter
counts = Counter()
for m in Path('assets/packs').glob('*/manifest.json'):
    counts[json.load(open(m, encoding='utf-8')).get('category', '<none>')] += 1
for cat, n in sorted(counts.items()): print(f'  {cat:24s} {n:3d}')
"
```

Expect a row per category with non-zero counts. `<none>` rows mean a manifest is missing the `category` field — backfill via the import script (re-running over existing dirs is a no-op except for re-stamping fields).

### 5. Build the .spk archives

```bash
cmake --build build --target generate_packs
```

`CMakeLists.txt` auto-discovers any directory under `assets/packs/` with a `manifest.json` via `file(GLOB ... CONFIGURE_DEPENDS)`. New packs get a `.spk` automatically; no list to maintain.

### 6. Smoke-test in the running app

The app installs `.spk`s from `build/.../packs/` into `<APPDATA_LOCAL>/Seelie/packs/<id>/` on first launch. To force a fresh extraction of a specific pack:

```bash
rm -rf "$LOCALAPPDATA/Seelie/packs/im.cheng.seelie.<id>"   # bash
# or
rmdir /s /q "%LOCALAPPDATA%\Seelie\packs\im.cheng.seelie.<id>"   # cmd
```

Then launch Seelie. Check `<exeDir>/seelie_debug.log` for these markers:
- `CharacterPackManager: Discovered N packs` — should match your category sum
- `SystemTray::refreshPackMenu — populated N packs across M categories` — confirms menu wiring
- For a specific pack: `Switched to pack: "<DisplayName>"` then `Loaded model with N motion groups: QList(...)` (no crash)

If a pack crashes, the log will end at `Live2D: loading motion 0 :` of a specific file. That's almost always either:
1. Wrong `TotalPointCount` in Meta — already handled by `sanitize_motion`, but verify the count matches actual segments
2. UserData mismatch — also handled, verify `UserDataCount==0`
3. A new failure mode — read `thirdparty/CubismNativeFramework/src/Motion/CubismMotion.cpp:678-805` for the parser; the buffer allocations are sized by Meta and the loop writes to those buffers

## Things to tell the user about

- **Imports are gitignored.** A new pack lives only on this machine until they explicitly track it, which they shouldn't — the canonical source is the submodule + script. Don't `git add` imported packs.
- **Repo size discipline.** A normal full import is ~700 MB raw / ~400 MB compressed in .spks. Both stay local. `installer/packages/.../data/` is also gitignored.
- **Menu structure.** New categories appear automatically in tray + settings. Display labels for new categories need entries in `kCategoryOrder[]` (mirrored in `src/SystemTray.cpp` and `src/SettingsPanelWidget.cpp`) — otherwise the raw category id shows up as the menu label. Translations go in `Seelie_zh_CN.ts` for both contexts.
- **NSFW / oversize categories** are deliberately omitted from `LOCAL_CATEGORIES`: 少女咖啡枪 (1.1 GB moc3 only), `galgame live2d` (NSFW), `sin 七大罪`, `凍京Nerco`, `イモコネー`, `Sacred Sword princesses`, `アンノウンブライド`. Don't add them without explicit user consent.

## Reference: where things live

| Concern | Location |
|---|---|
| Import pipeline | `scripts/import_live2d.py` |
| Curated name overrides | `PICKS` list in that file |
| Whitelisted upstream folders | `LOCAL_CATEGORIES` in that file |
| Manifest schema (incl. `category`) | `schemas/character-pack-v1.schema.json` |
| Pack discovery (CMake) | `CMakeLists.txt`, `file(GLOB ... CONFIGURE_DEPENDS)` |
| Tray menu category grouping | `src/SystemTray.cpp:refreshPackMenu` + `kCategoryOrder[]` |
| Settings menu category grouping | `src/SettingsPanelWidget.cpp:refreshPackList` + `kCategoryOrder[]` |
| Live2D engine motion-load path | `src/Live2DAnimationEngine.cpp` (look for `Preloading motions`) |
| Cubism parser (where bad counts crash) | `thirdparty/CubismNativeFramework/src/Motion/CubismMotion.cpp` |
