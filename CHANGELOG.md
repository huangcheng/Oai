# Changelog

All notable changes to Oai are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- TTS voice cache — synthesised audio is cached on disk under `~/.cache/Oai/tts_voice_cache/`, keyed by SHA-256 of `(provider, voice, model, options, normalized text)`. Cache hits replay instantly without a network round-trip. Bounded at 100 MB with LRU eviction. Cleared automatically when voice/model/active provider changes; users can also click **Clear voice cache** in the TTS settings tab.

### Fixed
- Daily log rotation now uses a `yyyy-MM-dd_HH-mm-ss` suffix, so a chatty crash loop on the same day no longer overwrites earlier archives. Archive cap is enforced strictly above the limit instead of at-or-above.

## [1.2.5] — 2026-05-10

### Added
- Pet state machine (`PetStateMachine`) — seven abstract states (`Idle`, `Greeting`, `Thinking`, `Working`, `Reviewing`, `Failed`, `Celebrating`) plus a `Walking` overlay. Sustained states hold across event bursts via grace timers; one-shot states play once and restore the saved sustained state.
- Codex pet support — drag-drop `.codex-pet` archives; the 8×9 atlas (192×208 cells) renders directly without conversion. Codex pets are scaled to 124 px wide for visual parity with other packs.
- Optional `stateMap` field in the pack manifest schema — pack-author override that beats `nameMap` for per-state animation chain resolution.
- Tip-bubble toggle in Settings — independent suppression channel from mode-driven (ECG / gaming auto-hide) suppression, persisted as `tipBubblesEnabled`.

### Changed
- `EventRouter` is now validation + tip-text only; animation dispatch moved to the state machine. Eliminates the dual-path race between event-driven animations and emotion-driven mood changes.
- README, README_CN, and the landing page document `.codex-pet` support.

### Removed
- `EmotionEngine` — its mood-to-animation fan-out duplicated what the state machine now expresses. Streak amplification can return as an `m_intensity` field on the FSM if the behavior is missed in practice.
- Gaming-mode tray balloon notifications (`Oai is hiding while you play` / `Oai is back!`) — the visibility change is already obvious from the missing window.

### Fixed
- One-shot states (Failed/Celebrating/Greeting) restore the sustained state from `m_savedSustained` rather than reading the current `m_baseState`, so a `tool.before` arriving during a `Failed` overlay no longer corrupts restoration.
- Codex pets render at a sensible window size (124 × 134 px) instead of native 192 × 208.

## [1.2.4] — 2026-05-08

### Added
- Gaming Mode — auto-hide the pet when a fullscreen application is active.
- Mouse tracking — pet reacts to hover-enter / hover-leave events.
- Global hotkey — show/hide the pet from anywhere via `Ctrl+Shift+O` (configurable).
- Emotion engine — accumulates valence/arousal scores from gateway events and surfaces a mood-driven animation. (Removed in 1.2.5; see above.)
- Codex pet format support — initial implementation of `.codex-pet` parsing.

### Fixed
- i18n synchronization, Windows-specific build issues, and Gaming Mode checkbox styling.

### Removed
- Dead `gateways/shared/` module — never used at runtime.

## [1.2.3] — 2026-05-07

### Added
- Multilingual installer — English and Simplified Chinese for Qt Installer Framework chrome and package metadata.
- Open-source release prep — license headers, contribution guide, public GitHub structure.

### Fixed
- Windows DWM recomposition after display sleep/wake events.
- Live2D OpenGL context recovery on Windows (lost-context handling).
- Tip bubble suppression while pet is hidden.
- Five additional findings from a second-round audit pass.
- Lottie `frameRate=0` guard and rejection of invalid UDP ports.
- About dialog now shows as a modal when the tip bubble is suppressed.
- Latent bugs from the first audit report.

### Changed
- `SpriteAnimationEngine::loadAssets` returns `bool` so callers can detect failure.

## [1.2.2] — 2026-05-01

### Added
- ECG (electrocardiogram) display mode — alternative to character mode; pet appears as an ICU-style monitor whose heart rate and alarm state are driven by gateway events.
  - Asystole (flatline) state after 60 seconds of no events.
  - Continuous flatline tone during asystole.
  - Single 2-second flatline beep variant.
  - Heart rate driven by gateway events; alarm flash on errors.
  - Chassis drag, exclusive-display-mode toggle.
- `DisplayMode` enum, migrating from `ecgEnabled` boolean.
- Mode dropdown in Settings (replacing the ECG checkbox).

### Fixed
- Flatline tone leaking into Character mode.
- ECG mode skips unconditional `w.show()`.
- Stray `>` characters in translations; context menu on ECG widget.

### Changed
- ECG palette refresh — white + orange, fixed button gaps.
- `compile_commands.json` exported for clangd-based editors.

## [1.2.1] — 2026-04-27

### Added
- Erlang/OTP UDP update server (`server/`) — version-check protocol over UDP with CRC16-CCITT validation. Reads version from the deployed `CMakeLists.txt`.
- Live2D pack import — `scripts/import_live2d.py` populates `assets/packs/` from the upstream `Eikanya/Live2d-model` archive.

### Fixed
- macOS deployment — hand-deploy Qt frameworks (macdeployqt's pass is broken on bundled QtQml frameworks).
- macOS `Contents/MacOS/` cleanliness so the app actually launches.
- Build helper accepts `check=` override.
- Tip bubble no longer steals keyboard focus.
- Auto-start checkbox draws its checkmark.
- macOS DMG builds — clean QML frameworks, sign bottom-up, add `/Applications` symlink.

### Changed
- Single source of truth for version: bump `CMakeLists.txt` and everything else (installer, server, in-app About) follows.

[Unreleased]: https://github.com/huangcheng/Oai/compare/v1.2.5...HEAD
[1.2.5]: https://github.com/huangcheng/Oai/compare/v1.2.4...v1.2.5
[1.2.4]: https://github.com/huangcheng/Oai/compare/v1.2.3...v1.2.4
[1.2.3]: https://github.com/huangcheng/Oai/compare/v1.2.2...v1.2.3
[1.2.2]: https://github.com/huangcheng/Oai/compare/v1.2.1...v1.2.2
[1.2.1]: https://github.com/huangcheng/Oai/releases/tag/v1.2.1
