## 1. Setup and Dependencies

- [ ] 1.1 Add Qt6::WebSockets to CMake find_package (conditional via `OAI_TTS_ENABLED`)
- [ ] 1.2 Add Qt6::WebSockets to target_link_libraries when enabled
- [ ] 1.3 Verify build passes with `OAI_TTS_ENABLED=ON` and `OFF`

## 2. Config Layer

- [ ] 2.1 Add TTS properties to ConfigManager.h (enabled, baseUrl, token, model, language)
- [ ] 2.2 Implement TTS getters/setters in ConfigManager.cpp
- [ ] 2.3 Add TTS change signals (ttsEnabledChanged, ttsBaseUrlChanged, etc.)
- [ ] 2.4 Implement load/save for TTS settings with `tts/` key prefix
- [ ] 2.5 Add default values for TTS settings

## 3. Settings UI Refactor

- [ ] 3.1 Create left-side tab button layout in SettingsPanelWidget
- [ ] 3.2 Create General tab container and migrate existing settings rows
- [ ] 3.3 Create AI tab container with TTS configuration fields
- [ ] 3.4 Implement tab switching logic with active state styling
- [ ] 3.5 Style tab buttons to match existing design (orange accent for active)
- [ ] 3.6 Connect TTS UI fields to ConfigManager signals/slots
- [ ] 3.7 Update retranslateUi() for new TTS labels and tab names

## 4. TTS Engine Core

- [ ] 4.1 Create TTSEngine.h class with speak(), connect(), disconnect() methods
- [ ] 4.2 Implement WebSocket connection management (QWebSocket)
- [ ] 4.3 Implement StepFun Audio API WSS protocol adapter
- [ ] 4.4 Implement MiniMax speech-t2a-websocket protocol adapter
- [ ] 4.5 Add provider selection logic based on ConfigManager settings
- [ ] 4.6 Implement audio playback via QMediaPlayer
- [ ] 4.7 Add error handling (connection failures, auth errors, network issues)
- [ ] 4.8 Implement connection retry with exponential backoff

## 5. TTS Integration

- [ ] 5.1 Instantiate TTSEngine in MainWindow
- [ ] 5.2 Connect TTSEngine to ConfigManager for setting changes
- [ ] 5.3 Trigger TTS when TipWidget shows a tip (if ttsEnabled)
- [ ] 5.4 Trigger TTS on session.start event for greeting (if ttsEnabled)
- [ ] 5.5 Add ttsEnabled guard to prevent TTS when disabled

## 6. Internationalization and Polish

- [ ] 6.1 Add English strings to source files with `tr()`
- [ ] 6.2 Update Oai_zh_CN.ts with Chinese translations for new UI
- [ ] 6.3 Add TTS connection status indicator in AI tab (optional)
- [ ] 6.4 Test tab switching and settings persistence
- [ ] 6.5 Test TTS with StepFun provider
- [ ] 6.6 Test TTS with MiniMax provider
- [ ] 6.7 Verify no regressions in existing settings functionality
