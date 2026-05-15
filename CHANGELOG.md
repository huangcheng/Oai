# Changelog

All notable changes to Oai are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Code-quality / architecture sweep after the 1.2.6 stability + security pass. No
user-visible behavior changes; everything below is internal cleanup that future
feature work will lean on.

### Added
- `src/AnimationEngine.h` — abstract base implemented by SpriteAnimationEngine,
  LottieAnimationEngine, and Live2DAnimationEngine. Shared `Priority` enum
  replaces three near-identical per-engine enums; common methods
  (`playAnimation`, `stop`, `paint`, `isPlaying`, `hasAnimations`,
  `lastPaintSuccessful`, `loadFromCharacterPack`) are now `override`. Engine-
  specific extras (Live2D's `playAnimationChain` / `setPointerTarget`) stay on
  the concrete class.
- `src/PlatformWindow.{h,cpp}` — central home for the Windows-only DWM
  frameless-window dance (`applyDwmFramelessAttributes`, `refreshComposition`).
  No-op on non-Windows. Six widgets shed their per-file `<dwmapi.h>` includes
  and DWMWA_* fallback macros.
- `src/AutoStartManager.{h,cpp}` — extracted from ConfigManager. Stateless
  `setEnabled(bool)` covering Windows HKCU\…\Run, macOS launchd plist, and
  Linux XDG `.desktop` autostart.
- `src/CanonicalEvents.h` — typed constexpr names for the 17 canonical events.
  Replaces literal `"session.start"` strings in EventRouter / TipsEngine /
  EcgWidget; wire format unchanged so gateways stay compatible.
- `src/PackDropHandler.{h,cpp}` — drag-and-drop install logic extracted from
  MainWindow into a stateless namespace. Sanitization (H13) carries over.
- `src/CharacterPackLoader.{h,cpp}` — common archive-probe helper
  (`readJsonEntryFromArchive`, `isValidCodexPet`, `readOpkPackId`). Three sites
  that duplicated the miniz + JSON parse dance now delegate.
- `CharacterPackManager::lastError()` — surfaces real install/uninstall failure
  reasons in the pack-manager UI (missing manifest, oversized JSON, unsafe ZIP
  entry, disk full, etc.) instead of generic "Failed".
- Three new unit tests: `test_autostart_manager`, `test_platform_window`,
  `test_character_pack_manager_errors`.

### Changed
- `TipsEngine` is engine-agnostic. Emits `animationRequested(QString)` instead
  of calling directly into SpriteAnimationEngine; MainWindow fans out across
  the same Live2D > Lottie > Sprite priority chain EventRouter and
  PetStateMachine already use. Fixes silent drop of tip animations on Lottie /
  Live2D packs.
- `MainWindow::refreshAllDwmAttributes()` consolidates the 30 s DWM-refresh
  timer body and the WM_DISPLAYCHANGE handler — one place owns the recovery
  sequence.
- `main.cpp`'s three engine-fan-out lambdas (TipsEngine, IPC tips,
  PetStateMachine chain) collapsed into `dispatchAnimation` /
  `dispatchAnimationChain` helpers.
- `SettingsPanelWidget::setupTtsTabContents` carved out of the 580-line
  `setupUi` so the TTS-tab construction sits next to its retranslate /
  event-handling slots.
- `ConfigManager::save()` is debounced at 500 ms (was synchronous per setter).
  A window drag now collapses ~60 disk writes/sec into one.

### Fixed
- TTS provider precheck before HTTP request — missing `token` / `voice` /
  `model` surfaces an immediate localized error pointing at the TTS settings
  tab, instead of a cryptic provider 401 several seconds later.
- TipsEngine reentrancy contract documented (main-thread-only).
- `CharacterPackManager::cleanupFileWatcher` uses `disconnect(this) +
  deleteLater()` instead of `delete`, so a directoryChanged signal already in
  the queue can't land on a freed object during rapid hot-reload toggles.
- `EcgWidget` `QTemporaryFile`s now parent to `this`; the manual delete block
  in `~EcgWidget` goes away. No more leaked beep / flatline WAV files.
- `PackManagerWidget` + `SystemTray` alert dialogs use `QPointer` +
  `WA_DeleteOnClose`. User-close auto-deletes and re-creates lazily; explicit
  teardown uses `deleteLater`.
- `IpcServer::parseMessage` guards against `m_worker == nullptr` on ping
  during shutdown — silently drops the pong instead of queuing a callback to
  a destroyed worker.
- `PetStateMachine` walking overlay clears after 500 ms of no position
  changes (`onWalkIdleExpired`); previously stuck `m_walking=true` forever.
- `CharacterPackManager` map reads use `value()` instead of `operator[]` so
  a missed `contains()` check can't quietly default-insert.

### Performance
- `fileMessageHandler` only `flush()`es on Critical / Fatal levels. Routine
  Debug / Info / Warning rely on the OS write-back; eliminates a per-message
  fsync that serialized every logging thread.

### Build / Tooling
- Replaced `.clang-format` with one tuned to the existing house style (4-space
  indent, Allman-for-functions / K&R-for-control-flow braces, right-aligned
  pointers, 100-col limit). Existing files were intentionally not mass-
  reformatted; the config is for on-touch use going forward.
- `.gitignore` now covers `.tmux-ide/` and `ide.yml` so local IDE state
  doesn't accidentally land in commits.
- Live2D `#ifdef OAI_LIVE2D_SUPPORT` reduced from 18 sites to 15 (header
  forward decl + member pointer + accessor are unconditional now).

### Internal
- 4 OpenSpec changes archived (`add-tts-voice-cache`, `add-gaming-mode`,
  `i18n-support`, `add-tts-ai-tab` superseded); their delta specs synced into
  `openspec/specs/`.

## [1.2.6] — 2026-05-15

Stability + security pass driven by an end-to-end audit, plus a TTS voice cache and the related UI polish.

### Added
- **TTS voice cache** — synthesised audio is cached on disk under `~/.cache/Oai/tts_voice_cache/`, keyed by SHA-256 of `(provider, voice, model, options, normalized text)`. Cache hits replay instantly without a network round-trip. Bounded at 100 MB with LRU eviction. Cleared automatically when voice/model/active provider changes; users can also click **Clear voice cache** in the TTS settings tab.
- **Project `.clang-format`** matching the existing house style: 4-space indent, Allman-for-functions / K&R-for-control-flow braces, right-aligned pointers, 100-col limit. Editors auto-pick it up; existing files were intentionally not mass-reformatted.

### Fixed
- **TTS engine no longer hangs the GUI on quit.** Cleanup is queued onto the engine thread instead of a `BlockingQueuedConnection`, and the engine moves itself back to the main thread before destruction so its `QObject` children are deleted on the right thread. The 2 s shutdown wait now logs and intentionally leaks if CoreAudio wedges, rather than `terminate()`-ing a thread that may be holding audio device locks.
- **`IpcServer` shutdown** uses the same queued-cleanup pattern for the same reason.
- **TTS HTTP requests time out at 30 s** on every provider (StepFun / MiniMax / Azure / OpenAI). A hung TLS handshake no longer wedges the engine; the existing retry/backoff handles the timeout cleanly.
- **TTS provider cancel race** — `cancel()` now erases the in-flight bookkeeping *before* `reply->abort()` to avoid a use-after-free when `abort()` synchronously re-enters the `finished()` lambda.
- **TTS decoder/sink signals are disconnected** before `deleteLater()` so queued events from a stale decoder cannot fire on the new instance during rapid `speak()` calls.
- **Audio decode is bounded at 50 MB** of accumulated PCM. A malformed or hostile audio header that claims an extreme duration aborts decode rather than OOMing the process.
- **Pack-install ZIP path traversal** — entries with `../`, absolute paths, or Windows drive prefixes are rejected before extraction; defends against `.opk` archives that target `~/Library/LaunchAgents/`.
- **Manifest-relative asset paths** are validated to live under the pack root before `assetPath()` returns them.
- **Drag-and-drop pack filenames** are reduced to base name and rejected if they contain path separators.
- **Manifest / `pet.json` size cap of 10 MB** before allocating from the archive's central directory.
- **Live2D dimension clamping** — `frameWidth` / `frameHeight` from the manifest are clamped to `[1, 4096]` before FBO creation; texture images outside the same range are rejected before `glTexImage2D`.
- **Live2D context-loss recovery** runs on every platform now (was Windows-only). macOS triggers context loss on display sleep / lid close / GPU switch.
- **Live2D `initOpenGL`** routes every failure path through `releaseOpenGL()` so partial init doesn't leak the offscreen surface or context. `recoverOpenGL()` releases the previous renderer before `setupRenderer()` rebuilds it.
- **Sprite-sheet frame coordinates** are widened to `qint64` for the multiply, then intersected with the sheet rect; out-of-bounds frames are rejected, partial overlaps clipped.
- **Pet-rect divide-by-zero guard** in `mouseMoveEvent` for Live2D pointer tracking.
- **UDP datagrams are capped at 65 535 bytes** in both `UdpWorker` and `UpdateChecker`; oversized packets are drained instead of resized.
- **Async DNS** in `UpdateChecker` (was a blocking `QHostInfo::fromName` on the GUI thread).
- **`ConfigManager::save()` is now debounced** at 500 ms — window drags collapse from one disk write per pixel into one per drag. The destructor flushes any pending write.
- **`ipcPort()` validates the parsed port** is in `[1, 65535]` rather than silently truncating via `quint16`.
- **Codex-pet temp dir is removed** in `~CharacterPack` (was leaking ~3 MB to `/tmp` per reload).
- **i18n live-switch** now refreshes the General tab button and the TTS provider field labels (Provider / Token / Voice / Model / BaseUrl) — they were frozen at the language active when the panel was constructed.
- **Daily log rotation** uses a `yyyy-MM-dd_HH-mm-ss` suffix so a chatty crash loop on the same day no longer overwrites earlier archives.
- **TTS settings UI** — Test and Clear voice cache share one row; the Provider combo's dropdown matches the General-tab combos; the Voice label is translated (音色) and the Clear voice cache button is fully localised.
- Plenty of smaller cleanups: `static QFile *` logger → `unique_ptr`, About-dialog `WA_DeleteOnClose`, EcgWidget `flush()` checks, plist XML escape (was HTML escape), TipsCatalog 1 MB read cap, removal of the unused `loadFromArchive` stub, combo-arrow pixmap moved out of `/tmp` into AppLocalData.

### Security

- ZIP path traversal in `installPack` (audit C7).
- Manifest-relative path traversal in `assetPath` (C8).
- Drag-and-drop filename sanitization (H13).
- Pack-archive size caps before allocation (C10).
- Live2D FBO/texture dimension clamping (C13/C14).
- UDP datagram size cap (H15).
- Plist XML-escape correctness (L9).

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
