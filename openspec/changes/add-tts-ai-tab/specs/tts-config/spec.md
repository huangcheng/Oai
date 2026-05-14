## ADDED Requirements

### Requirement: ConfigManager persists TTS settings
The ConfigManager SHALL persist five TTS-related settings: enabled state, base URL, API token, model identifier, and language/voice identifier.

#### Scenario: TTS settings saved
- **WHEN** the user changes any TTS setting in the AI tab
- **THEN** ConfigManager saves the value to QSettings
- **AND** the value persists across application restarts

#### Scenario: TTS settings loaded
- **WHEN** the application starts
- **THEN** ConfigManager loads TTS settings from QSettings
- **AND** default values are used if settings are missing:
  - `ttsEnabled`: false
  - `ttsBaseUrl`: "" (empty)
  - `ttsToken`: "" (empty)
  - `ttsModel`: "" (empty)
  - `ttsLanguage`: "zh-CN"

### Requirement: ConfigManager emits TTS setting change signals
The ConfigManager SHALL emit signals when TTS settings change, allowing UI components to react.

#### Scenario: Signal emission
- **WHEN** `setTtsEnabled(true)` is called
- **THEN** `ttsEnabledChanged(true)` signal is emitted
- **AND** SettingsPanelWidget updates the checkbox state

### Requirement: TTS settings are isolated from existing settings
The TTS settings SHALL use distinct QSettings keys and SHALL NOT interfere with existing configuration keys.

#### Scenario: Key isolation
- **WHEN** TTS settings are saved
- **THEN** they use keys prefixed with `tts/` (e.g., `tts/enabled`, `tts/baseUrl`)
- **AND** existing settings (language, autoStart, etc.) remain unchanged
