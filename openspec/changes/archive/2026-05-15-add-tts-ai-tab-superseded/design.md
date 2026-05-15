> **STATUS: Superseded** by
> `docs/superpowers/specs/2026-05-14-tts-provider-abstraction-design.md`.
> The "WebSocket as Primary Protocol" decision was reversed; the new
> implementation is HTTP-only across all providers. Voice and provider
> handling now live behind ITtsProvider rather than inside TTSEngine.

## Context

The Seelie desktop pet currently has a single-page settings panel (`SettingsPanelWidget`) with a vertical grid layout showing all configuration options. The pet communicates visually through tip bubbles only. Users have requested adding Text-to-Speech (TTS) capabilities so the pet can speak tips and greetings aloud.

The settings panel uses a custom Win98-style design with skewed parallelogram shapes, orange accents (`#F36F1A`), and `HarmonyOS Sans SC` font. All existing settings are managed through `ConfigManager` with QSettings persistence.

Current settings include: language, auto-start, display mode, IPC endpoint, active sprite pack, global shortcut, gaming mode, and tip bubbles. The panel dimensions are fixed at 230×310 pixels.

## Goals / Non-Goals

**Goals:**
- Refactor settings panel into a left-side tabbed interface with "General" and "AI" tabs
- Add TTS configuration UI (enable toggle, base URL, API token, model, language/voice)
- Implement a WebSocket-based TTS engine supporting StepFun and MiniMax providers
- Integrate TTS triggers into the tip display flow
- Maintain existing visual design language (orange accent, black borders, skewed shapes)

**Non-Goals:**
- Offline/local TTS (no on-device synthesis)
- Speech recognition or voice commands
- Multiple concurrent TTS streams
- TTS queue management with priority/pause
- Non-WSS protocols (REST polling as fallback only)

## Decisions

### Left-Side Tabs vs Top Tabs
**Decision:** Use left-side vertical tab buttons instead of traditional top tabs.
**Rationale:** The settings panel is narrow (230px). Top tabs would consume precious vertical space and limit content area. Left-side tabs (≈60px wide) preserve horizontal space for form fields and match modern IDE/sidebar patterns.
**Alternative considered:** Top tabs with icons only — rejected because text labels improve discoverability.

### WebSocket as Primary Protocol
**Decision:** WSS (WebSocket Secure) is the primary TTS streaming protocol.
**Rationale:** Both StepFun Audio API and MiniMax speech-t2a-websocket support WebSocket for real-time streaming with low latency. WSS provides encrypted transport and supports streaming audio chunks as they generate.
**Alternative considered:** REST polling — simpler but higher latency, no streaming. Will be available as fallback.

### Provider-Agnostic Engine
**Decision:** Create a single `TTSEngine` class with provider-specific adapters internally.
**Rationale:** StepFun and MiniMax have different WebSocket message formats and authentication. A unified engine with internal providers keeps the public API simple (`speak(text)`).
**Alternative considered:** Separate engine classes per provider — rejected because callers (TipWidget, PetStateMachine) shouldn't need to know which provider is configured.

### Config Storage
**Decision:** Store TTS settings alongside existing config in QSettings.
**Rationale:** Consistent with existing pattern. TTS settings are user preferences that should persist across sessions.
**Alternative considered:** Separate TTS config file — rejected; unnecessary complexity.

### QtWebSockets Dependency
**Decision:** Add Qt6 WebSockets module (`Qt6::WebSockets`).
**Rationale:** Native Qt integration, supports WSS, integrates with existing event loop. Homebrew Qt6 package includes WebSockets.
**Alternative considered:** Third-party WebSocket library (websocketpp, etc.) — rejected to minimize dependencies.

### TTSEngine Threading
**Decision:** Run `TTSEngine` on a dedicated `QThread` via `moveToThread()`.
**Rationale:** Prevents WebSocket I/O, audio decoding, and buffer management from competing with the main thread's animation loop. Follows the existing `UdpWorker` pattern in the codebase. Cross-thread communication via Qt signals/slots.
**Alternative considered:** Run on main thread (QtWebSocket is async) — rejected because audio buffer preparation and decoding could still cause frame drops during heavy TTS operations.

## Risks / Trade-offs

- **[Risk]** QtWebSockets may not be available in all build environments → **Mitigation:** Make TTS compilation conditional via CMake option `SEELIE_TTS_ENABLED` (default ON if QtWebSockets found)
- **[Risk]** WebSocket connection failures in restricted networks → **Mitigation:** Implement connection retry with exponential backoff; surface connection status in UI
- **[Risk]** Audio playback conflicts with system sounds → **Mitigation:** Use Qt Multimedia `QMediaPlayer` at low volume; allow volume control in settings
- **[Risk]** API tokens stored in plain text QSettings → **Mitigation:** Use Qt Keychain (`qtkeychain`) or macOS Keychain for token storage (future enhancement)
- **[Risk]** Settings panel becomes too tall with TTS fields → **Mitigation:** Left tabs allow scrolling per-tab; AI tab can be taller than General tab

## Migration Plan

1. Add QtWebSockets to CMake `find_package` (optional)
2. Extend ConfigManager with TTS fields
3. Refactor SettingsPanelWidget layout to left-tab structure
4. Add TTS configuration UI to AI tab
5. Implement TTSEngine with WebSocket support
6. Integrate TTS calls into TipWidget show flow
7. Update translations (`Seelie_zh_CN.ts`)

Rollback: Remove `SEELIE_TTS_ENABLED` CMake flag or disable at compile time. Existing settings remain backward compatible.

## Open Questions

1. Should TTS speak all tips or only specific types (status vs tips)?
2. Volume control: global slider or per-message?
3. Interrupt behavior: should new speech cancel previous speech?
4. MiniMax WSS requires binary protocol frames — should we support both text and binary WebSocket frames?
