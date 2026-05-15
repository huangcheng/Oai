## ADDED Requirements

### Requirement: Language changes apply without restart
The system SHALL reload the active translator and update all visible UI text when the user changes language in settings.

#### Scenario: User switches language in settings
- **GIVEN** the app is running in English
- **WHEN** the user selects "简体中文" from the language dropdown
- **THEN** the translator reloads with Chinese translations and all UI text updates immediately

### Requirement: Translator reload affects persistent UI
The system SHALL retranslate all persistent widgets (main window, system tray menu, settings panel) after a language change.

#### Scenario: Language change while settings panel is open
- **GIVEN** the settings panel is visible
- **WHEN** the user changes language
- **THEN** the settings panel labels update to the new language
