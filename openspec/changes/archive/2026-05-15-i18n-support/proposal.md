## Why

The project already has i18n infrastructure (`QTranslator`, `qt_add_translations`, `ConfigManager::language()`, a language dropdown in settings) but it is non-functional: the `.ts` file is empty, `SettingsPanelWidget` has hardcoded English strings, and `main.cpp` ignores the user's language preference. Users who select a non-English language in settings see no effect. This change makes the existing i18n scaffolding actually work.

## What Changes

- Wrap all hardcoded UI strings in `SettingsPanelWidget.cpp` with `tr()`
- Run `lupdate` to populate `Qlippy_zh_CN.ts` with source strings
- Make `main.cpp` read `ConfigManager::language()` at startup instead of only checking system locale
- Add runtime language switching: changing language in settings immediately reloads the translator without requiring restart
- Add `tr()` calls to any other user-facing strings discovered during audit
- Provide initial Chinese translations for all extracted strings

## Capabilities

### New Capabilities
- `runtime-language-switch`: Change UI language without restarting the app

### Modified Capabilities
- `config-persistence`: Language config change now triggers translator reload
- `settings-ui`: Settings panel strings become translatable

## Impact

- `src/SettingsPanelWidget.cpp` — add `tr()` wrappers
- `src/main.cpp` — respect `config.language()`, add translator reload logic
- `src/ConfigManager.cpp/h` — add `languageChanged` signal
- `src/mainwindow.cpp` — connect language change to retranslation
- `Qlippy_zh_CN.ts` — populated with source strings and initial translations
- No protocol or API changes
