## Context

The project has i18n scaffolding (`QTranslator`, `qt_add_translations`, a `.ts` file, `ConfigManager::language()`) but it is incomplete:
- `Qlippy_zh_CN.ts` is empty — `lupdate` has never populated it
- `SettingsPanelWidget.cpp` contains hardcoded English strings without `tr()`
- `main.cpp` loads translations based on system locale, ignoring the user's config preference
- Changing language in settings requires an app restart, and even then main.cpp doesn't read the saved value

## Goals / Non-Goals

**Goals:**
- Make all user-facing strings translatable
- Populate the `.ts` file with extractable source strings
- Respect user's language preference from config at startup
- Support runtime language switching without restart

**Non-Goals:**
- Adding new languages beyond zh_CN ( translators can add more `.ts` files later)
- Translating debug/log messages
- Translating animation names or IPC event names (internal identifiers)

## Decisions

### Decision 1: Add `languageChanged` signal to `ConfigManager`
**Rationale:** `ConfigManager` already persists language. Adding a `languageChanged(QString)` signal lets `MainWindow` react to changes without polling. `SettingsPanelWidget` emits the change through `ConfigManager`, which then broadcasts it.

### Decision 2: Runtime translator reload in `MainWindow`
**Rationale:** `QCoreApplication::removeTranslator()` + `installTranslator()` works at runtime, but already-visible widgets don't auto-retranslate. The simplest robust approach:
1. Remove old translator, load new one, install it
2. Emit a custom `retranslateUi()` signal or call a retranslation method on all UI components
3. `MainWindow`, `SettingsPanelWidget`, `SystemTray` implement retranslation

`MainWindow` connects `ConfigManager::languageChanged` to a slot that reloads the translator and calls `retranslateUi()` on itself and child widgets.

### Decision 3: Use `tr()` in `SettingsPanelWidget` and retranslate dynamically
**Rationale:** `SettingsPanelWidget` creates labels and combo box items in its constructor. For runtime switching, it needs a `retranslateUi()` method that updates all visible text. This mirrors the pattern Qt Designer generates.

### Decision 4: Run `lupdate` via CMake / build step
**Rationale:** `qt_add_translations` in CMakeLists.txt already wires up `lupdate` and `lrelease`. After adding `tr()` to `SettingsPanelWidget`, a clean build will auto-update the `.ts` file. The developer then edits `Qlippy_zh_CN.ts` to add translations.

### Decision 5: `main.cpp` reads config language first, falls back to system locale
**Rationale:** User preference should override system default. If config language is "en" or empty, fall back to `QLocale::system()`.

```cpp
QString lang = config.language();
if (lang.isEmpty() || lang == "en") {
    // Try system locale
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    ...
} else {
    // Load user preference
    translator.load(":/i18n/Qlippy_" + lang);
    a.installTranslator(&translator);
}
```

## Risks / Trade-offs

- **[Risk]** Runtime retranslation of `TipBubbleWidget` / `SpeechBubble` is tricky because they render text in `paintEvent` using stored strings.  
  → **Mitigation:** These bubbles are transient (auto-dismiss). Only persistent UI (`MainWindow`, `SystemTray`, `SettingsPanelWidget`) needs dynamic retranslation. Bubble text comes from `TipsEngine` / `RandomSayingsEngine` which generate content at show-time, so they naturally pick up the current translator.

- **[Risk]** `lupdate` may not find all strings if files aren't listed in CMake correctly.  
  → **Mitigation:** `qt_add_translations` scans all target sources automatically. Verify by checking the generated `.ts` file after build.

## Migration Plan

No migration needed. Existing config files without a `language` field will gracefully fall back to system locale behavior.
