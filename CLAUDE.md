# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Qlippy — a native Qt6/C++ desktop pet that reacts to AI coding tool events (Claude Code, Codex, OpenCode). Lightweight, cross-platform (macOS/Windows/Linux), < 10MB RAM.

## Build Commands

```bash
# macOS
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build .

# Run
open build/Qlippy.app          # macOS
./build/Qlippy                 # Linux

# Tests
cd build && ctest

# Gateway CLI
npm install -g @huangcheng/qlippy-gateway
qlippy-gateway send session.start    # send test event
qlippy-gateway health                # check IPC server
```

## Architecture

### C++ Core (src/)

The app follows a pipeline: **IPC → EventRouter → Animation/Effects/Tips → UI**

- **IpcServer** — TCP server on `127.0.0.1:52847`. Accepts newline-delimited JSON messages (`event`, `tip`, `ping`/`pong`).
- **EventRouter** — Maps 17 canonical event names (e.g. `session.start`, `tool.before`, `file.edited`) to `EventAction` structs containing animation name, effect name, tip title/body. The event set is fixed — gateways normalize tool-specific events into these.
- **SpriteAnimationEngine** — Plays frame-based animations from `assets/map.png` sprite sheet (124×93px frames, 27×34 grid) using definitions in `assets/animations.json`. Animation names are PascalCase internally but mapped from snake_case IPC input.
- **LottieAnimationEngine** — Alternative engine using rlottie to play Lottie JSON character animations from `assets/lottie/character/`.
- **LottieEffectOverlay** — Renders visual effects (sparkles, confetti, alert-pulse, etc.) from `assets/lottie/effects/` with offset positioning above the character.
- **TipBubbleWidget** — Win98-style speech bubble with asymmetric tail, fade animations, auto-dismiss (6s status / 12s tips).
- **TipsEngine** — Pattern matcher on a 30-second event window. Detects behaviors (repeated errors, rapid edits, idle, permission denials) and suggests contextual tips. 5-minute cooldown per tip type.
- **ConfigManager** — Persists to `~/.config/Qlippy/config.json` (window position, language, auto-start, IPC endpoint).
- **MainWindow** — Frameless, always-on-top, transparent 124×200 window. Owns the animation engine, effects overlay, tip bubble, settings panel, and system tray.

### Node.js Gateways (gateways/)

Each gateway adapter normalizes tool-specific events into the 17 canonical events and sends them over TCP IPC:

- **shared/** (`@qlippy/shared`) — Platform-agnostic TCP client used by all gateways.
- **qlippy-gateway/** — CLI tool for sending events and health checks.
- **claude-code/** — 14 hook definitions for Claude Code's `settings.json`. Install: `npx @huangcheng/qlippy-claude-code`.
- **codex/** — JSONL stream parser + 6 hook definitions for Codex.
- **opencode/** — Dynamic plugin that auto-loads in OpenCode.

### IPC Protocol

Transport: TCP, newline-delimited JSON. Fire-and-forget for events (no response expected). Messages:
```json
{"type":"event","source":"claude-code","event":"tool.before","toolName":"Read"}
{"type":"tip","title":"Hello","body":"World","animation":"wave"}
{"type":"ping"}
```

### 17 Canonical Events

`session.start`, `session.end`, `session.idle`, `session.error`, `prompt.submitted`, `tool.before`, `tool.after`, `tool.failed`, `permission.requested`, `permission.denied`, `permission.response`, `subagent.started`, `subagent.stopped`, `notification.sent`, `file.edited`, `file.watched`, `todo.updated`

## Key Conventions

- C++17 with Qt6 (Core, Gui, Widgets, Network, LinguistTools, Test). Qt signals/slots throughout.
- rlottie v0.2 fetched via CMake FetchContent (patched for Apple Silicon NEON).
- Animation names: PascalCase in C++ (`SpriteAnimationEngine`), snake_case over IPC. The engine handles mapping.
- Gateways are pure ES modules (`.mjs`) with zero npm dependencies — only Node.js built-ins.
- i18n: Chinese translation in `Qlippy_zh_CN.ts`. All user-visible strings use `tr()`.
- Tests use Qt Test framework on a separate TCP port (52848).
