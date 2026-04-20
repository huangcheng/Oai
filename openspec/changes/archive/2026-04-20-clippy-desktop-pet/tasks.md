## 1. Project Setup & Window Foundation

- [x] 1.1 Update CMakeLists.txt to add `Qt6::Gui` and `Qt6::Network` dependencies (remove `Qt6::QuickWidgets`/`Qt6::Qml`), create `src/` directory structure, remove `.ui` file
- [x] 1.2 Copy Lottie animation assets into `assets/lottie/character/` (character animations) and `assets/lottie/effects/` (6 effect files: sparkles, confetti, alert-pulse, thinking-dots, wave-lines, speech-pop from opencode-clippy) and add them to CMake as resources
- [x] 1.3 Rewrite `mainwindow.h/cpp` to create transparent, frameless, always-on-top, non-activating QWidget window (spec: system-integration "Transparent frameless always-on-top window")
- [x] 1.4 Remove the `.ui` file dependency — MainWindow no longer uses Qt Designer UI

## 2. Lottie Animation Engine

- [x] 2.1 Add rlottie as CMake dependency via FetchContent — fetch from https://github.com/Samsung/rlottie, pin to release tag, build as static library (design: D1)
- [x] 2.2 Create `src/LottieAnimationEngine` class: load all Lottie JSON files from `assets/lottie/character/` at startup via `rlottie::Animation::loadFromFile()`, store as `std::shared_ptr<rlottie::Animation>` in name-keyed map, allocate reusable render buffer (spec: lottie-animation-engine "Lottie animation loading")
- [x] 2.3 Implement frame-driven playback loop: QTimer at 16ms, advance frame counter based on rlottie's frameRate() and per-animation speed multiplier, render via `animation->render(frame, surface)` → QImage(Format_ARGB32_Premultiplied) → QPainter::drawImage() (spec: lottie-animation-engine "Named animation playback")
- [x] 2.4 Implement priority-based animation queue: high-priority interrupts current animation, normal-priority queues after current (spec: lottie-animation-engine "Priority-based animation queue")
- [x] 2.5 Implement idle animation pool: idle variants with weighted random selection, triggered after 3 seconds of inactivity (spec: lottie-animation-engine "Idle animation pool")
- [x] 2.6 Implement rest pose: display first frame of designated rest animation when no animation is active (spec: lottie-animation-engine "Rest pose")
- [x] 2.7 Integrate LottieAnimationEngine into MainWindow paintEvent — call engine's paint() method in paintEvent to draw current frame onto the transparent widget

## 3. Speech Bubble

- [x] 3.1 Create `src/SpeechBubble` widget class rendering Windows 98-style tooltip: `#FFFFE1` background, 1px black border, 2px `#808080` drop shadow, Tahoma-equivalent 11px font, triangular tail (spec: speech-bubble "Windows 98-style rendering")
- [x] 3.2 Implement auto-dismiss timers: 4 seconds for status bubbles, 8 seconds for tip bubbles (spec: speech-bubble "Auto-dismiss timers")
- [x] 3.3 Implement bubble positioning logic: above Clippy by default, flip if near screen edges, tail direction follows Clippy position (spec: speech-bubble "Positioning")
- [x] 3.4 Implement enter/exit animations: 200ms scale-up ease-out on appear, 150ms scale-down ease-in on dismiss (spec: speech-bubble "Enter/exit animation")
- [x] 3.5 Wire SpeechBubble into MainWindow as a child widget, shown/hidden via animation engine signals

## 4. Lottie Visual Effects

- [x] 4.1 Create `src/LottieEffectOverlay` class: loads 6 Lottie effect JSON files from `assets/lottie/effects/` at startup via rlottie, manages active effect instances with independent frame counters (spec: lottie-effects "Lottie effect types")
- [x] 4.2 Implement effect rendering: each active effect rendered via same rlottie pipeline as character — `render(frame, surface)` → QImage → QPainter::drawImage(), composited on top of character in same paint pass (spec: lottie-effects "rlottie rendering of effects")
- [x] 4.3 Implement effect lifecycle: once-mode effects auto-cleanup after last frame, loop-mode effects (thinking-dots) play until explicitly stopped, multiple simultaneous effects supported (spec: lottie-effects "Effect lifecycle management")
- [x] 4.4 Wire effect triggers: map animation names and event types to their corresponding Lottie effects (e.g., Congratulate → confetti.json, tool.success → sparkles.json, permission.requested → alert-pulse.json) (spec: lottie-effects "Lottie effect types")
- [x] 4.5 Implement effect positioning: configurable per-effect offset relative to pet widget (e.g., confetti above, sparkles centered) (spec: lottie-effects "Effect positioning")

## 5. IPC Event Bridge

- [x] 5.1 Create `src/IpcServer` class wrapping `QLocalServer`, listening on `~/.opencode-clippy/clippy.sock` (spec: event-bridge "IPC server with QLocalServer")
- [x] 5.2 Implement newline-delimited JSON parser handling three message types: event, tip, ping (with pong response) (spec: event-bridge "Newline-delimited JSON protocol")
- [x] 5.3 Implement connection lifecycle: accept multiple simultaneous connections, clean up on disconnect, log connections (spec: event-bridge "Connection lifecycle")
- [x] 5.4 Add resilient parsing: log warnings for malformed JSON, incomplete messages, unknown types — never crash (spec: event-bridge "Resilient parsing")
- [x] 5.5 Create `src/EventRouter` class that receives parsed IPC messages, validates unified event names against the 17-event table, and dispatches Qt signals to AnimationEngine, SpeechBubble, TipsEngine, and ParticleOverlay (design: D5, D10)
- [x] 5.6 Add unified event name validation: reject messages with event names not in the canonical table, log warning with the invalid name (spec: event-bridge "Unified event naming scheme")
- [x] 5.7 Add source field validation: reject event messages without a `source` field (one of `opencode`, `claude-code`, `codex`), log warning (spec: event-bridge "Event message enrichment")

## 6. Gateway Adapters

- [x] 6.1 Create `gateways/clippy-gateway` CLI tool (Node.js/TypeScript): accepts `--source <tool> --event <unified-name> [--tool-name <name>] [--file-path <path>]`, connects to IPC socket, sends JSON message, disconnects (spec: event-bridge "Gateway adapter architecture")
- [x] 6.2 Create `gateways/opencode-plugin/` — update existing `clippy.ts` plugin to map OpenCode bus events to unified names per D10 table, add `source: "opencode"` to all messages (spec: event-bridge "Unified event naming scheme")
- [x] 6.3 Create `gateways/claude-code-hooks/` — generate `settings.json` snippet with `command` hooks for all 17 applicable Claude Code events, each invoking `clippy-gateway --source claude-code --event <name>` with `async: true` (spec: event-bridge "Gateway adapter architecture")
- [x] 6.4 Create `gateways/codex-hooks/` — generate `hooks.json` for Codex interactive mode (PreToolUse, PostToolUse, SessionStart, Stop, UserPromptSubmit, PermissionRequest) invoking `clippy-gateway --source codex` (spec: event-bridge "Gateway adapter architecture")
- [x] 6.5 Create `gateways/codex-jsonl-parser/` — Node.js script that reads `codex exec --json` JSONL output, maps thread/turn/item events to unified names, forwards to IPC socket (spec: event-bridge "Gateway adapter architecture")
- [x] 6.6 Add gateway installation instructions: `clippy-gateway` to PATH, OpenCode plugin to `~/.config/opencode/plugins/`, Claude Code hooks to `~/.claude/settings.json`, Codex hooks to `~/.codex/hooks.json` with feature flag

## 7. Tips Engine

- [x] 7.1 Create `src/TipsEngine` class with sliding window buffer (20 events, 30-second window) and pattern matcher registry (spec: tips-engine "Event window buffer")
- [x] 7.2 Implement 8 pattern matchers: repeated errors, test file activity, config editing, first edit, rapid changes, git commands, idle after start, permission denials (spec: tips-engine "Pattern matching engine")
- [x] 7.3 Implement 5-minute cooldown per pattern type to prevent tip fatigue (spec: tips-engine "Cooldown management")
- [x] 7.4 Wire TipsEngine signals to SpeechBubble (display tip) and AnimationEngine (play tip animation)

## 8. Pet Interaction

- [x] 8.1 Implement mouse event handling in MainWindow: left click → random animation from click pool, double-click → random from double-click pool, both high-priority (spec: pet-interaction "Click/Double-click")
- [x] 8.2 Implement drag-to-move: track press position, start drag after 5px threshold, play GestureDown on start and LookDown+RestPose on release, move window with cursor (spec: pet-interaction "Drag-to-move")
- [x] 8.3 Implement right-click context menu with Show/Hide, Settings, About, Quit options (spec: pet-interaction "Context menu")

## 9. System Integration

- [x] 9.1 Create `src/ConfigManager` class for reading/writing `~/.config/Clippy/config.json` with QJsonDocument — window position, language, autoStart, ipcSocketPath (spec: system-integration "Configuration persistence")
- [x] 9.2 Implement single instance enforcement with QLockFile in `~/.config/Clippy/` — second instance signals first to show window, then exits (spec: system-integration "Single instance")
- [x] 9.3 Create `src/SystemTray` class with QSystemTrayIcon, context menu, and toggle-visibility on click (spec: system-integration "System tray icon")
- [ ] 9.4 Set up i18n with Qt Linguist: create `Clippy_zh_CN.ts`, wrap all user-facing strings with `tr()`, add Simplified Chinese translations (spec: system-integration "i18n")
- [ ] 9.5 Implement auto-start at login via platform-native APIs (macOS LaunchServices, Windows registry, Linux XDG autostart) (spec: system-integration "Auto-start at login")

## 10. Integration & Wiring

- [x] 10.1 Update `main.cpp` to initialize ConfigManager, check single instance, create MainWindow with all subsystems, start IpcServer, and show system tray
- [x] 10.2 Connect all signal/slot wiring per design D5: IpcServer → EventRouter → LottieAnimationEngine/SpeechBubble/TipsEngine/LottieEffectOverlay, TipsEngine → SpeechBubble/LottieAnimationEngine
- [x] 10.3 Implement config persistence: save window position on drag (debounced 1s), load on startup, restore last position
- [ ] 10.4 Test end-to-end: start app, send IPC event from CLI (`echo '{"type":"event","event":"message.updated"}' | nc -U ~/.opencode-clippy/clippy.sock`), verify animation plays, bubble appears, particles trigger

## 11. Build & Polish

- [ ] 11.1 Verify macOS `.app` bundle builds and runs correctly with `cmake --build` and `macdeployqt`
- [ ] 11.2 Verify Windows build with MSVC/MinGW: produce `.exe`, bundle Qt DLLs via `windeployqt`, test on Windows 10/11
- [ ] 11.3 Verify Linux build: produce AppImage via `linuxdeployqt` or `linuxdeploy`, test on Ubuntu 22.04+ (X11 and Wayland)
- [ ] 11.4 Verify memory usage is <10MB at idle on all platforms using Activity Monitor (macOS) / Task Manager (Windows) / `ps aux` (Linux) (design goal)
- [x] 11.5 Add `README.md` with build instructions for all 3 platforms (macOS: homebrew Qt + cmake, Windows: vcpkg Qt + MSVC, Linux: apt/dnf Qt + cmake), IPC protocol documentation, and asset attribution
