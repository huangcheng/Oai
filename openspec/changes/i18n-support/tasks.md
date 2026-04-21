## 1. Source String Audit

- [x] 1.1 Wrap all hardcoded strings in `SettingsPanelWidget.cpp` with `tr()`
- [x] 1.2 Audit all other `.cpp` files in `src/` for missed user-facing strings and add `tr()` where needed
- [x] 1.3 Ensure `TipsEngine.cpp` matcher strings use `tr()` consistently

## 2. ConfigManager Signal

- [x] 2.1 Add `languageChanged(QString)` signal to `ConfigManager.h`
- [x] 2.2 Emit `languageChanged` in `ConfigManager::setLanguage()` only when value actually changes

## 3. Startup Language Loading

- [x] 3.1 Modify `main.cpp` to read `config.language()` before loading translator
- [x] 3.2 Load translator for saved language if present; fall back to system locale only if empty/unset
- [x] 3.3 Ensure translator is installed before any UI widgets are created

## 4. Runtime Language Switching

- [x] 4.1 Add `retranslateUi()` method to `MainWindow` that updates context menu items (Hide/Show, About, Settings, Quit)
- [x] 4.2 Add `retranslateUi()` method to `SettingsPanelWidget` that updates labels and combo box text
- [x] 4.3 Add `retranslateUi()` method to `SystemTray` that updates tooltip text
- [x] 4.4 Connect `ConfigManager::languageChanged` to a slot in `MainWindow` that reloads translator and calls all `retranslateUi()` methods

## 5. Translation File

- [x] 5.1 Build the project to trigger `lupdate` and populate `Clippy_zh_CN.ts` with source strings
- [x] 5.2 Add Chinese translations for all extracted strings in `Clippy_zh_CN.ts`
- [x] 5.3 Verify `.qm` file is generated and embedded under `:/i18n/` prefix

## 6. Verification

- [x] 6.1 Build and run, confirm app starts in system locale language
- [x] 6.2 Change language in settings to Chinese, verify UI updates immediately without restart
- [x] 6.3 Restart app, confirm saved language preference is restored
- [x] 6.4 Switch back to English, verify all text returns to English
