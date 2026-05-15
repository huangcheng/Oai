## ADDED Requirements

### Requirement: Startup respects saved language preference
The system SHALL load the translator for the language stored in `config.json` at application startup, falling back to the system locale only if no preference is saved.

#### Scenario: User has Chinese saved in config
- **GIVEN** `config.json` contains `"language": "zh_CN"`
- **WHEN** the app starts
- **THEN** the Chinese translator is loaded before any UI is shown

#### Scenario: No language preference saved
- **GIVEN** `config.json` has no language field
- **WHEN** the app starts
- **THEN** the system falls back to the OS locale for translation selection

### Requirement: ConfigManager emits language change signal
The system SHALL emit a `languageChanged(QString)` signal from `ConfigManager` whenever `setLanguage()` is called with a different value.

#### Scenario: Settings panel changes language
- **WHEN** `ConfigManager::setLanguage("zh_CN")` is called
- **THEN** `ConfigManager` emits `languageChanged("zh_CN")`
