# Artworks

Original source artwork the Seelie project was built from. Everything in
this directory is an **input** — committed so the project's visual
identity is reproducible and re-deriveable. Build outputs (`.icns`,
`.ico`, `.qrc`-packed PNGs, the codex-pet atlas) live under `assets/` and
are derived from these sources.

Provenance: nearly all images here were generated 2026-05-17 via the
MiniMax `image-01` API using the `minimax-multimodal-toolkit` skill;
voice samples via MiniMax `t2a_v2`. The Codex-pet atlas was generated
separately via OpenAI's `hatch-pet` skill, using `mascot/seelie-avatar_001.jpg`
as its reference image.

## Layout

```
artworks/
├── mascot/             persona references and full-body illustrations
├── icon-source/        high-res icon PNGs and the macOS .iconset
├── voice-samples/      MiniMax TTS samples for picking the in-app voice
└── hatch-pet-qa/       diagnostic outputs from the hatch-pet skill run
```

## `mascot/`

The Seelie persona — pale blue-to-lavender twintail hair, large warm
cyan eyes, subtly pointed elf ears, oversized white hoodie with lavender
trim, glowing cyan crystal pendant.

- **`seelie-avatar_001.jpg`** — the canonical reference avatar. This is
  the image fed to `hatch-pet` as the row-base for every Codex-pet
  animation row, so identity stays locked across the 8×9 atlas. Don't
  delete or regenerate without re-running hatch-pet — drift will show
  up across the in-app sprite pack.
- **`seelie-idle_001.jpg`** — alternate close-up portrait, idle pose.
- **`seelie-coding_001.jpg`** — portrait, focused/coding mood (dark
  backdrop with star particles).
- **`seelie-celebrate_001.jpg`** — celebrating pose, used as a brand
  reference for the celebrate sprite row.
- **`seelie-splash_001.jpg`** + **`seelie-splash-v2_001.jpg`** — full
  body sitting on an ice/crystal cube with a floating laptop. Kept as
  brand-reference / promotional candidate art; not currently shipped.

## `icon-source/`

High-resolution PNG masters and the macOS iconset that the built icon
bundles (`assets/seelie.icns`, `assets/seelie.ico`,
`assets/icons/seelie.png`) are derived from.

- **`seelie-icon-rounded_1024.png`** + **`_512.png`** + **`_256.png`** —
  rounded-corner squircle variants of the avatar, sized for iOS-style
  app-icon canvases. The 1024 is the master.
- **`seelie-icon-square_1024.png`** — square (un-rounded) variant.
- **`Seelie.iconset/`** — the macOS iconset folder that `iconutil` was
  run against to produce `assets/seelie.icns`. Contains the standard
  Apple sizes (16/32/64/128/256/512 at @1x and @2x). To regenerate the
  .icns after editing any size:
  ```
  iconutil -c icns artworks/icon-source/Seelie.iconset \
           -o assets/seelie.icns
  ```

## `voice-samples/`

MiniMax `t2a_v2` outputs from the persona-voice selection. These are
**samples to pick from**, not assets the app loads. Once a final voice
is picked it's referenced by ID inside `ConfigManager`, not bundled.

- **`seelie-voice-cute-spirit.mp3`** — playful, higher-pitched, fae vibe.
- **`seelie-voice-warm-girl.mp3`** — warmer mid-range, encouraging tone.

## `hatch-pet-qa/`

Diagnostic images from the OpenAI Codex `hatch-pet` skill run that
produced `assets/packs/seelie/spritesheet.webp`. Kept for future
regenerations so we can compare a new atlas against this baseline.

- **`preview-grid.png`** — 9-row × 8-col contact sheet of the final
  atlas; visual sanity check for identity drift between rows.
- **`running-gif.png`** — single-row strip for the `running` state,
  decoded prior to atlas composition.

The full per-row run dir lives under `codex-pet-runs/seelie/` and is
gitignored — see `.claude/skills/seelie-codex-pet/SKILL.md` for how to
re-run.

## What does NOT belong here

- The built sprite atlas (`assets/packs/seelie/spritesheet.webp`) — that
  is derived from `hatch-pet-qa/` source rows and the hatch-pet run.
- The compiled icon bundles (`assets/seelie.icns`, `assets/seelie.ico`,
  `assets/icons/seelie.png`) — those are built from `icon-source/` and
  shipped to Qt's resource system.
- Per-run hatch-pet intermediates (`codex-pet-runs/seelie/decoded/`,
  `outputs/`, prompts) — gitignored. Diagnostic only.
