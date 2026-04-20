## Why

The existing opencode-clippy is an Electron-based desktop pet that only works with OpenCode. Electron brings 30-50MB RAM overhead for what is essentially a sprite animation overlay — a poor trade-off for an always-on desktop companion. The new Clippy rebuilds this as a native Qt6/C++ application that is framework-agnostic (works with any AI coding tool, not just OpenCode), uses a fraction of the resources, and delivers smoother animations via Samsung's rlottie Lottie rendering engine.

## What Changes

- **Native Qt6/C++ desktop widget** replacing the Electron renderer — transparent, frameless, always-on-top overlay window with <10MB RAM footprint
- **Framework-agnostic event bridge** — instead of a hard OpenCode plugin, a local IPC server (Unix socket / named pipe) that accepts events from any source (OpenCode plugin, Claude Code hook, CLI tool, or future integrations)
- **Lottie animation engine in C++** — character animations and visual effects rendered as Lottie JSON files via Samsung's rlottie C++ library, with priority queue (high=interrupt, normal=queue) and weighted idle animation pool
- **Proactive tips engine ported to C++** — pattern-matching engine that detects coding patterns (repeated errors, test file reads, config edits, etc.) and triggers classic "It looks like you're..." speech bubbles
- **Classic Windows 98 speech bubble** — native QPainter-rendered tooltip with `#FFFFE1` background, Tahoma-equivalent font, triangular tail, auto-dismiss timers
- **Lottie visual effects** — sparkle, confetti, alert-pulse, thinking-dots, wave-lines, speech-pop effects as Lottie JSON animations rendered via rlottie, composited on top of the character in the same paint pass
- **System tray integration** — native QSystemTrayIcon with show/hide toggle, settings, quit
- **i18n** — Qt Linguist-based translation system (English + Simplified Chinese, extensible)
- **Single instance enforcement** — QSharedMemory or QLockFile to prevent duplicate processes
- **Config persistence** — `~/.config/Clippy/config.json` for window position, language, auto-start preferences
- **Drag-to-move** — native mouse event handling for dragging the frameless widget
- **Cross-platform** — macOS (primary), Windows, Linux via CMake + Qt6

## Capabilities

### New Capabilities

- `sprite-animation-engine`: Lottie JSON animation rendering via rlottie with priority queue, idle pool, per-animation speed multiplier, and rest pose support
- `speech-bubble`: Windows 98-style tooltip rendering with auto-dismiss, triangular tail, and rich text support
- `particle-effects`: Lottie visual effects (sparkle, confetti, alert-pulse, thinking-dots, wave-lines, speech-pop) rendered via rlottie with lifecycle management and simultaneous playback
- `event-bridge`: Local IPC server (Unix socket / named pipe) for receiving events from external tools, with heartbeat and auto-reconnect
- `tips-engine`: Proactive pattern-matching engine that detects coding patterns and generates contextual suggestions with cooldown management
- `pet-interaction`: Click, double-click, and drag-to-move handlers with animation triggers
- `system-integration`: System tray, single instance, config persistence, auto-start, i18n

### Modified Capabilities

(none — this is a new project, no existing specs)

## Impact

- **Tech stack**: Qt6 (Widgets only), C++17, CMake build system, rlottie (Samsung Lottie C++ library) — replaces Electron/TypeScript/Vite entirely
- **Assets**: Lottie JSON animation files for character animations and visual effects. 6 existing effect files (sparkles, confetti, alert-pulse, thinking-dots, wave-lines, speech-pop) reused from opencode-clippy. Character Lottie animations to be created or sourced separately
- **IPC protocol**: Compatible with existing newline-delimited JSON protocol from opencode-clippy — existing OpenCode plugin can send events to the new widget without modification
- **Build**: Requires Qt6 >= 6.5 development headers, CMake >= 3.19, C++17 compiler
- **Distribution**: macOS `.app` bundle (potentially `.dmg`), Windows installer, Linux AppImage
- **OpenCode plugin**: Remains as a separate npm package (`opencode-clippy`), no changes needed — it already sends events to `~/.opencode-clippy/clippy.sock`
