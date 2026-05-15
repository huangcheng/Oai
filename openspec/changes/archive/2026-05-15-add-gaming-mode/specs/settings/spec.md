## ADDED Requirements

### Requirement: Gaming Mode setting in config
The system SHALL expose `gamingModeEnabled` as a boolean property on `ConfigManager`, persisted as the `gamingMode` key in `~/.config/Seelie/config.json`, with a default value of `false`.

#### Scenario: Read default value
- **WHEN** the config file does not contain a `gamingMode` key
- **THEN** `ConfigManager::gamingModeEnabled()` SHALL return `false`

#### Scenario: Persist enabled state
- **WHEN** `ConfigManager::setGamingModeEnabled(true)` is called
- **THEN** `save()` SHALL write `"gamingMode": true` to the config file AND emit `gamingModeEnabledChanged(true)`

#### Scenario: Persist disabled state
- **WHEN** `ConfigManager::setGamingModeEnabled(false)` is called
- **THEN** `save()` SHALL write `"gamingMode": false` to the config file AND emit `gamingModeEnabledChanged(false)`
