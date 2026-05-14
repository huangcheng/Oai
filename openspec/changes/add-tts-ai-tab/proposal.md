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
