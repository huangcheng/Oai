## ADDED Requirements

### Requirement: Settings panel uses left-side vertical tabs
The SettingsPanelWidget SHALL display two tabs on the left side: "General" and "AI".

#### Scenario: Tab navigation
- **WHEN** the user clicks the "General" tab button
- **THEN** the General tab content becomes visible
- **AND** the "General" tab button shows active state (orange accent)

#### Scenario: Default tab
- **WHEN** the settings panel is opened
- **THEN** the "General" tab is active by default

### Requirement: General tab contains existing settings
The General tab SHALL contain all pre-existing settings: language, auto-start, display mode, IPC port, sprite pack selection, global shortcut, gaming mode, and tip bubbles.

#### Scenario: Existing settings preserved
- **WHEN** the user opens the General tab
- **THEN** all existing settings are present and functional
- **AND** their values are bound to ConfigManager as before

### Requirement: AI tab contains TTS configuration
The AI tab SHALL contain TTS configuration fields: enable toggle, provider base URL, API token, model selection, and language/voice selection.

#### Scenario: TTS settings visible
- **WHEN** the user clicks the "AI" tab
- **THEN** TTS configuration fields are visible
- **AND** the fields are bound to ConfigManager TTS properties

### Requirement: Tab buttons match visual design
The left-side tab buttons SHALL match the existing visual design language: black borders, white background, orange accent for active state, and HarmonyOS Sans SC font.

#### Scenario: Active tab styling
- **WHEN** a tab is selected
- **THEN** the tab button displays an orange left border or background highlight
- **AND** inactive tabs show a subtle gray border
