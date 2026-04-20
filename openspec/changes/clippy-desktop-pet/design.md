## Context

We are rebuilding opencode-clippy — an Electron desktop pet that reacts to coding events — as a native Qt6/C++ application. The existing app works but suffers from Electron's 300-500MB memory footprint for what is essentially a transparent overlay with sprite animations. The new version must:

- Work with any AI coding tool (not just OpenCode) via a generic IPC protocol
- Use <10MB RAM at idle
- Deliver smooth 60fps Lottie animations (character + visual effects)
- Maintain full compatibility with the existing OpenCode plugin's event format
- Be cross-platform (macOS primary, Windows + Linux supported)

The new project already has a Qt6/C++ skeleton (`CMakeLists.txt`, `mainwindow.*`) but no actual desktop pet functionality. All code must be written from scratch.

## Goals / Non-Goals

**Goals:**
- Native desktop pet overlay with transparent, frameless, always-on-top window
- <10MB RAM at idle, <20MB during animation
- All character animations + visual effects rendered as Lottie JSON files via rlottie
- Windows 98-style speech bubble with auto-dismiss
- Visual effects (sparkles, confetti, alert-pulse, etc.) as Lottie JSON animations via rlottie
- Generic IPC event bridge — works with OpenCode plugin, CLI, or any event source
- Proactive tips engine with pattern matching and cooldowns
- System tray, single instance, config persistence, i18n
- Drag-to-move, click/double-click interactions
- macOS primary, cross-platform build support

**Non-Goals:**
- Built-in AI/LLM capabilities (Clippy is reactive, not generative)
- Multiple pet characters (Clippy only for v1)
- Network connectivity or cloud features
- Mobile platforms
- Plugin marketplace or extension system

## Decisions

### D0: Framework Selection (Research-Based)

**Choice**: Qt6/C++ native application.

**Rationale**: Deep research into desktop pet frameworks revealed that Qt6 is the only viable path for the <10MB memory constraint:

| Framework | Idle RAM | macOS Overlay | Notes |
|-----------|----------|---------------|-------|
| **Qt6 Native** | **<10 MB** | Native NSWindow via Qt | ✅ Meets all constraints |
| **Tauri v2** | 20-110 MB | Needs tauri-nspanel plugin | ❌ Exceeds memory budget |
| **Flutter Desktop** | ~50 MB | No selective click-through | ❌ Memory too high, overlay issues |
| **Electron** | 300-500 MB | Possible but heavy | ❌ Explicitly excluded |

Research validated that frame-by-frame sprite sheets (used by Desktop Goose, Shimeji, CrabNebula koi-pond) are the industry standard for desktop pets. Qt's `QPainter::drawPixmap()` with source-rect clipping is the most efficient rendering path for small bitmap frames.

**Alternatives considered**:
- *Tauri v2*: Real-world desktop pets (WindowPet, Peekoo AI) idle at 20-110MB depending on OS WebView version. macOS Tahoe WebView regressed to 110MB. The `tauri-nspanel` plugin is needed for fullscreen overlay, adding complexity. Rejected: cannot guarantee <10MB.
- *Native SwiftUI/AppKit*: Would be <20MB and have perfect macOS overlay support, but requires separate Windows/Linux implementations. Rejected: violates cross-platform goal.
- *Raw OpenGL/Vulkan*: Maximum performance but enormous complexity for a desktop pet — not justified.

### D1: Pure Qt6 Widgets + QPainter architecture with rlottie

**Choice**: Main window is a `QWidget` (transparent, frameless). All rendering — Lottie character animations, visual effects, and speech bubbles — uses `QPainter` directly. Lottie animations are rendered via Samsung's **rlottie** C++ library into a pixel buffer, converted to `QImage`, and painted with `QPainter::drawImage()`. No QML or Qt Quick dependency.

**Rationale**: Pure `QWidget` + `QPainter` + rlottie handles everything we need with minimal dependencies and no JavaScript runtime overhead:
- Lottie animation rendering via rlottie: `rlottie::Animation::loadFromData()` → `render(frame, surface)` → `QImage` → `QPainter::drawImage()`
- Speech bubble rendering via `QPainter` path drawing with rounded rectangles
- Visual effects (sparkles, confetti, etc.) as separate Lottie JSON files, rendered identically to character animations
- Zero QML/Qt Quick dependency — smaller binary, lower memory, faster startup
- All C++ — no JavaScript engine, no declarative layer overhead
- rlottie is mature (used by Telegram, WhatsApp), cross-platform, and MIT-licensed

**rlottie rendering pipeline**:
```
Lottie JSON file → rlottie::Animation::loadFromData()
                                ↓
QTimer (16ms ≈ 60fps) → advance frame number
                                ↓
rlottie::Surface(buffer) → animation->render(frame, surface)
                                ↓
QImage(buffer, Format_ARGB32_Premultiplied) → QPainter::drawImage()
```

**Alternatives considered**:
- *Qt6 LottieAnimation module*: QML-only (`import Qt.labs.lottieqt`), no public C++ QPainter API. Would require QML dependency — rejected per "no QML" constraint.
- *QML hybrid*: Same issue — adds Qt Quick Scene Graph and JavaScript engine. Overkill.
- *Custom QPainter particles*: Manual particle systems for effects. Inferior visual quality compared to designer-authored Lottie effects. More code to maintain.

### D2: rlottie-based Lottie animation rendering

**Choice**: Load Lottie JSON files as `rlottie::Animation` objects at startup. A `QTimer` at 16ms advances the frame counter. Each frame is rendered into an `rlottie::Surface` (uint32_t pixel buffer), converted to a `QImage` with `Format_ARGB32_Premultiplied`, and painted via `QPainter::drawImage()`. Frame timing uses `rlottie::Animation::totalFrame()` and `frameRate()`. Per-animation speed multipliers are supported. Loop/ping-pong behavior is handled by the animation engine wrapping rlottie's linear playback.

**Memory model**: Each `rlottie::Animation` is lightweight (~1–5KB for our JSON files). The render pixel buffer is allocated once at startup and reused every frame — no per-frame allocation.

**Rationale**: rlottie is the most mature C++ Lottie rendering library (used by Telegram, WhatsApp). The `buffer → QImage → QPainter` pipeline is zero-copy compatible (the uint32_t pixel buffer is shared). rlottie's frame-based API maps naturally to a `QTimer` loop. rlottie is MIT-licensed. This approach replaces the sprite-sheet + `QPixmapCache` approach described in D1's alternative analysis.

### D3: QLocalServer for IPC (not raw Unix sockets)

**Choice**: Use Qt's `QLocalServer` / `QLocalSocket` for the event bridge instead of raw `net.Server` (Electron) or POSIX sockets.

**Rationale**: `QLocalServer` is Qt's cross-platform abstraction over Unix domain sockets (macOS/Linux) and named pipes (Windows). Same protocol semantics, zero platform-specific code. This is what the OpenCode plugin already talks to — it connects to `~/.opencode-clippy/clippy.sock` and sends newline-delimited JSON. QLocalServer will listen on the same path.

**Protocol compatibility**: The existing OpenCode plugin sends `{"type":"event","event":"..."}` and `{"type":"tip","title":"...","body":"...","animation":"..."}` messages. The new widget will parse the same JSON format — **zero migration needed** for existing plugin users.

### D4: JSON config with QJsonDocument

**Choice**: Store configuration in `~/.config/Clippy/config.json` using Qt's `QJsonDocument` / `QJsonObject`. Same format as the Electron version's `~/.opencode-clippy/config.json`.

**Rationale**: JSON is human-readable, debuggable, and compatible with the existing config format. QJsonDocument is built into Qt Core — no extra dependency. We'll read on startup, write on change, with a 1-second debounce timer for rapid changes.

### D5: Event-driven architecture with signal/slot pipeline

**Choice**: All components communicate via Qt signals and slots:

```
IPC Server ──[signal]──► Event Router ──[signal]──► Animation Engine
                       ──[signal]──► Speech Bubble
                       ──[signal]──► Tips Engine
                       ──[signal]──► Particle Effects

Tips Engine ──[signal]──► Speech Bubble
                       ──[signal]──► Animation Engine

Animation Engine ──[signal]──► Particle Effects
```

**Rationale**: Qt's signal/slot is the natural event bus for C++/Qt applications. Loose coupling, thread-safe (with `Qt::QueuedConnection` for cross-thread), and debuggable via `QObject::connect` context. The IPC server runs on a secondary thread to avoid blocking the UI.

### D6: Lottie visual effects via rlottie

**Choice**: All visual effects — sparkle, confetti, alert-pulse, thinking-dots, wave-lines, and speech-pop — are implemented as Lottie JSON files rendered via rlottie. Same rendering pipeline as character animations: `rlottie::Animation` → `rlottie::Surface` → `QImage` → `QPainter`. The 6 existing Lottie effect files from `opencode-clippy/assets/lottie/` are reused directly. Effects are positioned relative to the pet widget and composited in the same paint pass. Effect lifecycle (play once or loop until stopped) is managed by the animation engine.

**Rationale**: Unified rendering pipeline — the same code path renders both character animations and effects. Designer-authored Lottie effects have superior visual quality versus hand-coded `QPainter` particle systems. No additional dependency: rlottie is already used for character animation (per D2). The rlottie `render()` call handles all effect playback internally, eliminating per-effect animation logic.

### D7: Single instance via QSharedMemory + QLockFile

**Choice**: On startup, check `QLockFile` in `~/.config/Clippy/`. If locked, the app is already running — bring existing window to front and exit.

**Rationale**: `QLockFile` is Qt's native single-instance mechanism. It handles stale locks (process crash cleanup) via PID checking. Simpler and more reliable than the Electron approach (`app.requestSingleInstanceLock()`).

### D8: Priority-based animation queue

**Choice**: Same priority model as the Electron version:
- **High priority** (interrupt): User interactions (click, drag), alerts — immediately cancels current animation
- **Normal priority** (queue): Events from IPC — queued and played in order
- **Idle pool**: 20 idle animations with weighted random selection, triggered after 3 seconds of no activity

**Rationale**: This model worked well in the Electron version. High-priority interrupts ensure click responses feel instant. Normal-priority queuing prevents animation thrashing during rapid event bursts. Idle pool keeps Clippy feeling alive during quiet periods.

### D9: Cross-platform fullscreen overlay support

**Choice**: Use `Qt::WindowStaysOnTopHint` + `Qt::WindowDoesNotAcceptFocus` + `Qt::Tool` for cross-platform fullscreen overlay, with platform-specific fallbacks where Qt's abstraction is insufficient.

**Rationale**: Fullscreen overlay requires three properties: (1) visible above all windows, (2) non-focus-stealing, (3) hidden from taskbar/dock. Qt provides cross-platform flags for all three, but each platform has edge cases that require native intervention:

- **macOS**: `Qt::WindowStaysOnTopHint` + `Qt::WindowDoesNotAcceptFocus` + `Qt::Tool` covers most cases. For fullscreen Spaces, also set `Qt::WA_MacAlwaysShowToolWindow` and use native interface to set `NSFloatingWindowLevel`. Fullscreen apps use `NSWindowCollectionBehaviorFullScreenAuxiliary` via `objc_msgSend` on the `NSWindow` backing `winId()`.

- **Windows**: `Qt::WindowStaysOnTopHint` internally calls `SetWindowPos` with `HWND_TOPMOST`. `Qt::Tool` removes the window from the taskbar (equivalent to `WS_EX_TOOLWINDOW` extended style). For fullscreen games/apps that bypass normal windows, use `winId()` to get the HWND and periodically re-assert topmost via `SetWindowPos(HWND_TOPMOST)` on a timer. Prevent focus stealing with `WS_EX_NOACTIVATE` set via `SetWindowLongPtr` on the HWND from `winId()`.

- **Linux X11**: `Qt::WindowStaysOnTopHint` maps to `_NET_WM_STATE_ABOVE` via ICCCM/EWMH. `Qt::Tool` hides from taskbar/pager via `_NET_WM_STATE_SKIP_TASKBAR` / `_NET_WM_STATE_SKIP_PAGER`. These are well-supported by modern X11 window managers.

- **Linux Wayland**: Layer-shell protocol (via `wlr-layer-shell` or `kde-layer-shell`) is the only reliable way to appear above fullscreen Wayland surfaces. Qt does not expose layer-shell directly, so use `Qt::WindowStaysOnTopHint` + `Qt::Tool` as best-effort; Wayland layer-shell integration is a stretch goal requiring Qt platform plugin or native calls.

**Reference**: Qt window flags `Qt::WindowStaysOnTopHint`, `Qt::WindowDoesNotAcceptFocus`, and `Qt::Tool` are documented in Qt Widgets documentation. macOS: `NSWindowCollectionBehaviorFullScreenAuxiliary` (Apple docs), `NSFloatingWindowLevel` (AppKit). Windows: `SetWindowPos` / `HWND_TOPMOST` (Win32 API), `WS_EX_TOOLWINDOW` / `WS_EX_NOACTIVATE` (Win32 extended window styles). Linux X11: `_NET_WM_STATE_ABOVE`, `_NET_WM_STATE_SKIP_TASKBAR` (EWMH spec). Linux Wayland: layer-shell protocol (wlr-layer-shell-unstable-v1).

### D10: Unified event naming scheme with gateway adapters

**Choice**: Define a unified event namespace with 17 canonical events mapped from OpenCode (22+ bus events), Claude Code (27 hook events), and Codex (6 hook events + 8 JSONL events). Each tool has a gateway adapter that translates tool-specific events to unified names before sending to Clippy's IPC socket.

**Rationale**: Three different AI coding tools have completely different event systems — OpenCode uses an in-process plugin bus, Claude Code uses external command hooks, Codex uses file-based hooks + JSONL stream. A unified event namespace lets Clippy's animation engine, tips engine, and speech bubble respond identically regardless of which tool is driving events. Gateway adapters encapsulate the translation logic per tool.

**Unified event table** (17 events):

| Unified Event | OpenCode | Claude Code | Codex |
|---|---|---|---|
| `session.start` | `session.created` | `SessionStart` | `SessionStart` / `thread.started` |
| `session.end` | `session.deleted` | `SessionEnd` | — |
| `session.idle` | `session.idle` | `Stop` | `turn.completed` |
| `session.error` | `session.error` | `StopFailure` | `turn.failed` |
| `prompt.submitted` | — | `UserPromptSubmit` | `UserPromptSubmit` |
| `tool.before` | `tool.execute.before` | `PreToolUse` | `PreToolUse` |
| `tool.after` | `tool.execute.after` | `PostToolUse` | `PostToolUse` |
| `tool.failed` | — | `PostToolUseFailure` | — |
| `permission.requested` | `permission.asked` | `PermissionRequest` | `PermissionRequest` |
| `permission.denied` | — | `PermissionDenied` | — |
| `permission.response` | `permission.replied` | — | — |
| `subagent.started` | — | `SubagentStart` | — |
| `subagent.stopped` | — | `SubagentStop` | — |
| `notification.sent` | `tui.toast.show` | `Notification` | — |
| `file.edited` | `file.edited` | `FileChanged` | item subtype `FileChange` |
| `file.watched` | `file.watcher.updated` | `FileChanged` | — |
| `todo.updated` | `todo.updated` | `TaskCreated` / `TaskCompleted` | item subtype `TodoList` |

**Gateway adapters**:
- **OpenCode**: In-process plugin (`clippy.ts`) subscribes to `event` hook, maps events, writes to IPC socket. Already exists in opencode-clippy.
- **Claude Code**: External `command` hooks in `~/.claude/settings.json` invoke `clippy-gateway --source claude-code --event <name>` which forwards to IPC socket. Uses `async: true` to avoid blocking.
- **Codex**: Two paths — hooks in `~/.codex/hooks.json` (experimental, feature flag) for interactive mode, plus JSONL stream parser for `codex exec --json` non-interactive mode.

**Alternatives considered**:
- *Per-tool event names*: Would require Clippy to know about every tool's event vocabulary. Fragile and doesn't scale.
- *Passthrough only*: Just forward raw event names. Tips engine and animation mapping would need per-tool logic. Rejected: violates framework-agnostic goal.
- *WebSocket instead of Unix socket*: Adds TCP overhead and port management. Unix domain sockets are faster and simpler for local IPC.

## Risks / Trade-offs

- **Character asset licensing** → The Clippy character animations need to be created or sourced as Lottie JSON files. The original felixrieseberg/clippy sprite sheet (map.png) is MIT-licensed, but the character is a Microsoft trademark. Mitigation: Create new Lottie character animations inspired by the original, include attribution.
- **IPC socket path conflict** → If the old Electron Clippy is still running, both will try to listen on the same socket. Mitigation: Single-instance check prevents this. The new app will detect the old one's lock and prompt.
- **Cross-platform window transparency** → Transparent frameless windows have subtle differences per platform. Mitigation: Platform-specific `#ifdef Q_OS_MAC` / `Q_OS_WIN` / `Q_OS_LINUX` blocks for window flags and hints.
- **rlottie build complexity** → rlottie is a C++ library built via CMake. Integrating via FetchContent adds build complexity (rlottie depends on CMake, a C++ compiler, and optionally FreeType/HarfBuzz for text layers). Mitigation: Use CMake FetchContent to build rlottie from source, pin to a specific release tag, and test builds on all 3 platforms early.
- **Lottie asset creation workflow** → Character animations must be authored as Lottie JSON (e.g., via After Effects + Bodymovin, Figma, or LottieFiles). The original sprite sheet has 42 named animations that need to be recreated. Mitigation: Start with the 6 existing effect Lottie files, use placeholder geometric animations for character during development, and create production Lottie assets as a separate design task.
