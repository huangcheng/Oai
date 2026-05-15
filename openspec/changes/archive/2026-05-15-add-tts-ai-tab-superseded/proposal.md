> **STATUS: SUPERSEDED — 2026-05-15**
>
> Every functional outcome of this change has shipped, but in a different
> shape than originally proposed. Replacing this change rather than completing
> the remaining tasks because doing so verbatim would *regress* the design.
>
> | Section | Originally proposed | What actually shipped |
> |---|---|---|
> | TTS protocol | QtWebSockets streaming | HTTP per-provider (`m_nam->post()`, no streaming) — simpler, doesn't need a new Qt module, gives clean per-request error handling |
> | Provider count | StepFun + MiniMax | StepFun + MiniMax + Azure Speech + OpenAI (4 backends), pluggable via `ITtsProvider` interface |
> | Config schema | Flat `ttsBaseUrl/ttsToken/ttsModel` | Multi-provider: `tts/activeProvider` + `tts/providers/<id>/<field>` so adding a 5th backend doesn't rename keys |
> | UI | Left-side tabs (General + AI) | Shipped as designed (General + TTS), plus a hot-swap provider combo, voice ID free-text field, **Test** button, **Clear voice cache** action |
> | Voice cache | (not in original scope) | Bonus: on-disk cache keyed by `(provider, voice, model, options, normalized text)` with 100 MB LRU bound |
>
> Capability specs for the shipped form live in `openspec/specs/tts-voice-cache/`.
> Settings-UI capability is documented in `openspec/specs/settings-ui/`.
>
> **Do not implement the remaining `[ ]` tasks below** — they describe an
> earlier design that is no longer current. The boxes are left unchecked
> only as a record of what was originally planned.

---

## Why

The pet currently communicates through visual tips only. Adding Text-to-Speech (TTS) capabilities will make the pet more interactive and engaging by allowing it to speak tips, greetings, and status messages aloud. This requires refactoring the settings panel to accommodate new AI-related configuration options in a clean, extensible way.

## What Changes

- **Refactor SettingsPanelWidget** from a single-page layout into a left-side tabbed interface
  - **General Tab**: All existing settings (language, auto-start, display mode, IPC port, sprite pack, global shortcut, gaming mode, tip bubbles)
  - **AI Tab**: New TTS configuration fields (base URL, API token, model, language/voice selection)
- **Add TTSEngine** class to handle WebSocket-based TTS streaming
  - Primary protocol: WSS (WebSocket Secure) for real-time streaming
  - Support StepFun Audio API and MiniMax speech-t2a-websocket endpoints
  - Configurable endpoint, model, voice/language parameters
- **Extend ConfigManager** with TTS settings persistence
  - `ttsEnabled`, `ttsBaseUrl`, `ttsToken`, `ttsModel`, `ttsLanguage`
- **Integrate TTS trigger points** in TipWidget and PetStateMachine
  - Speak tips when shown (if TTS enabled)
  - Speak greetings on session start

## Capabilities

### New Capabilities
- `left-tab-settings`: Refactor SettingsPanelWidget into a left-side tabbed interface with General and AI tabs
- `tts-engine`: WebSocket-based TTS streaming engine with StepFun and MiniMax provider support
- `tts-config`: Configuration schema and persistence for TTS parameters

### Modified Capabilities
- (none - existing settings behavior remains unchanged, only UI layout changes)

## Impact

- **UI**: SettingsPanelWidget layout changes from vertical form to left-tab layout
- **Config**: ConfigManager gains 5 new TTS-related fields
- **Network**: New WebSocket client dependency (QtWebSockets) for TTS streaming
- **Dependencies**: Adds QtWebSockets module requirement
- **Architecture**: New TTSEngine class in the event pipeline (TipWidget → TTSEngine → WebSocket)
