# Qlippy Desktop Pet

A native Qt6/C++ desktop pet that reacts to AI coding tool events. A lightweight (<10MB RAM) native application.

## Features

- **Transparent, frameless, always-on-top** window with Qlippy animations
- **Lottie animations** via Samsung's rlottie library — smooth 60fps playback
- **Windows 98-style speech bubble** with auto-dismiss tips
- **Visual effects** — sparkles, confetti, alert-pulse, thinking-dots, wave-lines, speech-pop
- **Framework-agnostic** — works with OpenCode, Claude Code, Codex, or any tool that can send IPC messages
- **Proactive tips engine** — detects coding patterns and shows contextual suggestions
- **Cross-platform** — macOS, Windows, Linux
- **<10MB RAM** at idle

## Building

### Prerequisites

- **Qt 6.5+** (Widgets, Gui, Network, LinguistTools)
- **CMake 3.19+**
- **C++17 compiler** (GCC 10+, Clang 12+, MSVC 2019+)

### macOS

```bash
# Install Qt via Homebrew
brew install qt@6

# Build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build .

# Create .app bundle
macdeployqt Qlippy.app

# Run
open Qlippy.app
```

### Windows (MSVC)

```powershell
# Install Qt via the Qt installer or vcpkg
# vcpkg: vcpkg install qt6-base qt6-tools

# Build
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
cmake --build . --config Release

# Bundle Qt DLLs
windeployqt Release\Qlippy.exe

# Run
Release\Qlippy.exe
```

### Windows (MinGW)

```powershell
# Install Qt via the Qt installer (select MinGW)

# Build
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\mingw_64"
cmake --build .

# Bundle Qt DLLs
windeployqt Qlippy.exe

# Run
Qlippy.exe
```

### Linux

```bash
# Install Qt development packages
# Ubuntu/Debian:
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential

# Fedora:
sudo dnf install qt6-qtbase-devel qt6-qttools-devel cmake gcc-c++

# Build
mkdir build && cd build
cmake ..
cmake --build .

# Run
./Qlippy
```

### AppImage (Linux)

```bash
# After building
linuxdeploy --appdir AppDir --plugin qt
appimagetool AppDir Qlippy-x86_64.AppImage
```

## IPC Protocol

Qlippy accepts IPC messages over a platform-appropriate transport:

| Platform | Transport | Default Endpoint |
|---|---|---|
| **Linux** | Unix domain socket | `~/.qlippy/qlippy.sock` |
| **macOS** | Unix domain socket | `~/.qlippy/qlippy.sock` |
| **Windows** | Named pipe | `\\.\pipe\qlippy` |

The desktop app and gateway adapters auto-detect the platform. All Node.js gateways use the shared `gateways/shared/ipc.mjs` module which connects to the correct endpoint. Override with `--endpoint <path>` on any gateway CLI command.

The C++ IpcServer uses Qt's `QLocalServer`, which abstracts Unix sockets and named pipes behind a single cross-platform API.

### Message Format

Newline-delimited JSON. Each message is a single JSON object terminated by `\n`.

### Message Types

#### Event

```json
{
  "type": "event",
  "source": "opencode|claude-code|codex",
  "event": "session.start",
  "toolName": "write",
  "filePath": "src/main.cpp"
}
```

#### Tip

```json
{
  "type": "tip",
  "title": "Having trouble?",
  "body": "It looks like you're running into repeated errors.",
  "animation": "alert"
}
```

#### Ping/Pong

```json
// Request
{ "type": "ping" }

// Response
{ "type": "pong" }
```

### Unified Event Names (17 events)

| Event | Description |
|---|---|
| `session.start` | Session/turn started |
| `session.end` | Session/turn ended |
| `session.idle` | Session idle (no activity) |
| `session.error` | Session error occurred |
| `prompt.submitted` | User submitted a prompt |
| `tool.before` | About to execute a tool |
| `tool.after` | Tool execution completed |
| `tool.failed` | Tool execution failed |
| `permission.requested` | Permission requested from user |
| `permission.denied` | Permission denied |
| `permission.response` | Permission response received |
| `subagent.started` | Subagent started |
| `subagent.stopped` | Subagent stopped |
| `notification.sent` | Notification displayed |
| `file.edited` | File was edited |
| `file.watched` | Watched file changed |
| `todo.updated` | Todo list updated |

## Gateway Adapters

### OpenCode

```bash
cd ~/.config/opencode
npm install @eastlake/qlippy-opencode
```

Then add `"@eastlake/qlippy-opencode"` to the `plugin` array in `opencode.json`.

### Claude Code

First, install `qlippy-gateway` globally:

```bash
npm install -g @eastlake/qlippy-gateway
```

```bash
npm install -g @eastlake/qlippy-claude-code
npx @eastlake/qlippy-claude-code
```

### Codex

```bash
npm install -g @eastlake/qlippy-codex
npx @eastlake/qlippy-codex
```

For non-interactive mode:
```bash
codex exec --json "your prompt" | npx @eastlake/qlippy-codex parser.mjs
```

### Health Check

Check if Qlippy is running:

```bash
npx @eastlake/qlippy-gateway --ping
# Exit code 0 = alive, 1 = not responding
```

### Manual Testing

Send a test event (auto-retries up to 2 times if Qlippy is temporarily unreachable):

```bash
npx @eastlake/qlippy-gateway --source opencode --event session.start
```

## Project Structure

```
qlippy/
├── CMakeLists.txt              # Build configuration
├── src/
│   ├── main.cpp                # Application entry point
│   ├── mainwindow.h/cpp        # Transparent frameless pet window
│   ├── LottieAnimationEngine.h/cpp  # rlottie animation playback
│   ├── SpeechBubble.h/cpp      # Win98-style speech bubble
│   ├── LottieEffectOverlay.h/cpp    # Visual effects overlay
│   ├── IpcServer.h/cpp         # QLocalServer IPC bridge
│   ├── EventRouter.h/cpp       # Event dispatch and validation
│   ├── TipsEngine.h/cpp        # Pattern-matching tips engine
│   ├── ConfigManager.h/cpp     # JSON config persistence
│   └── SystemTray.h/cpp        # System tray integration
├── assets/
│   └── lottie/
│       ├── character/          # Character animations (20 Lottie files)
│       └── effects/            # Visual effects (6 Lottie files)
├── gateways/
│   ├── shared/                 # Platform-aware IPC transport (Node.js)
│   │   └── ipc.mjs             # Auto-detects Unix socket vs named pipe
│   ├── qlippy-gateway/         # CLI gateway tool (Node.js)
│   ├── opencode-plugin/        # OpenCode plugin
│   ├── claude-code-hooks/      # Claude Code hooks config
│   ├── codex-hooks/            # Codex hooks config
│   └── codex-jsonl-parser/     # Codex JSONL stream parser
└── Qlippy_zh_CN.ts             # Simplified Chinese translations
```

## Configuration

Config file: `~/.config/Qlippy/config.json`

```json
{
  "windowX": 100,
  "windowY": 500,
  "language": "en",
  "autoStart": false,
  "ipcEndpoint": "~/.qlippy/qlippy.sock"
}
```

The `ipcEndpoint` field auto-detects the platform default (Unix socket on Linux/macOS, named pipe on Windows) and can be overridden.

## Asset Attribution

- Character animations: Placeholder Lottie files (to be replaced with production assets)
- Visual effects: Custom-designed Lottie effect animations (MIT license)
- Animation engine: [Samsung rlottie](https://github.com/Samsung/rlottie) (MIT license)

## License

MIT
