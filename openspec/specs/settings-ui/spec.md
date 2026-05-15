## ADDED Requirements

### Requirement: All settings panel text is translatable
The system SHALL wrap every user-visible string in `SettingsPanelWidget` with `tr()` so that `lupdate` can extract it for translation.

#### Scenario: lupdate extracts settings strings
- **WHEN** `lupdate` runs against the project
- **THEN** strings such as "Settings", "Language", and "Launch at Login" appear in the `.ts` file

### Requirement: Settings panel supports dynamic retranslation
The system SHALL implement a `retranslateUi()` method in `SettingsPanelWidget` that updates all labels and combo box items when called.

#### Scenario: Language changes while settings is visible
- **GIVEN** the settings panel is open and showing English labels
- **WHEN** `retranslateUi()` is called after a language switch
- **THEN** all labels and combo box items update to the new language
