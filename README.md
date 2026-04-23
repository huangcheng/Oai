# Orai Desktop Pet

A native Qt6/C++ desktop pet that reacts to AI coding tool events. A lightweight (<10MB RAM) native application with a sprite pack engine for customizable characters.

## Features

- **Transparent, frameless, always-on-top** window with character animations
- **Sprite pack engine** — load custom characters via `.opk` packs
- **Lottie animations** via Samsung's rlottie library — smooth 60fps playback
- **Windows 98-style speech bubble** with auto-dismiss tips
- **Framework-agnostic** — works with OpenCode, Claude Code, Codex, or any tool that can send IPC messages
- **Proactive tips engine** — detects coding patterns and shows contextual suggestions
- **Cross-platform** — macOS, Windows, Linux
- **<10MB RAM** at idle

## Sprite Packs

Orai supports customizable characters through sprite packs (`.opk` files). Each pack contains:
- Sprite sheet or Lottie animations
- Animation definitions
- Event-to-animation mappings
- Preview image

### Installing Packs

1. **Drag-and-drop**: Drop `.opk` file onto the pet window
2. **Manual**: Copy `.opk` to `~/.config/Orai/packs/`
3. **Built-in**: Official packs are generated during build

### Creating Packs

See `schemas/sprite-pack-v1.schema.json` for the pack format specification.

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
macdeployqt Orai.app

# Run
open Orai.app
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
windeployqt Release\Orai.exe

# Run
Release\Orai.exe
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
windeployqt Orai.exe

# Run
Orai.exe
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
./Orai
```

## IPC Protocol

Orai accepts IPC messages over UDP localhost:

| Transport | Default Endpoint |
|---|---|
| **UDP** | `127.0.0.1:52847` |

All Node.js gateways send to this endpoint automatically. Override with `--endpoint <host:port>` on any gateway CLI command.

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

## Gateway

Install the CLI gateway globally from GitHub Packages:

```bash
npm install -g @huangcheng/orai-gateway --registry=https://npm.pkg.github.com
```

Configure your npm to use GitHub Packages for `@huangcheng` scope (one-time setup):

```bash
echo "@huangcheng:registry=https://npm.pkg.github.com" >> ~/.npmrc
```

### Claude Code

Add hooks to `~/.claude/settings.json`:

```json
{
  "hooks": {
    "SessionStart": [{
      "hooks": [{
        "type": "command",
        "command": "orai-gateway --source claude-code --event session.start",
        "timeout": 3,
        "async": true
      }]
    }],
    "Stop": [{
      "hooks": [{
        "type": "command",
        "command": "orai-gateway --source claude-code --event session.idle",
        "timeout": 3,
        "async": true
      }]
    }],
    "PreToolUse": [{
      "hooks": [{
        "type": "command",
        "command": "orai-gateway --source claude-code --event tool.before",
        "timeout": 5,
        "async": true
      }]
    }],
    "PostToolUse": [{
      "hooks": [{
        "type": "command",
        "command": "orai-gateway --source claude-code --event tool.after",
        "timeout": 3,
        "async": true
      }]
    }]
  }
}
```

### Health Check

Check if Orai is running:

```bash
orai-gateway --ping
# Exit code 0 = alive, 1 = not responding
```

### Manual Testing

Send a test event:

```bash
orai-gateway --source claude-code --event session.start
```

## Project Structure

```
orai/
├── CMakeLists.txt              # Build configuration
├── src/
│   ├── main.cpp                # Application entry point
│   ├── mainwindow.h/cpp        # Transparent frameless pet window
│   ├── SpriteAnimationEngine.h/cpp  # Sprite sheet animation playback
│   ├── LottieAnimationEngine.h/cpp  # Lottie animation playback
│   ├── LottieEffectOverlay.h/cpp    # Visual effects overlay
│   ├── SpritePack.h/cpp        # Sprite pack data structure
│   ├── SpritePackManager.h/cpp # Pack discovery and management
│   ├── IpcServer.h/cpp         # UDP IPC server
│   ├── EventRouter.h/cpp       # Event dispatch and validation
│   ├── TipsEngine.h/cpp        # Pattern-matching tips engine
│   ├── ConfigManager.h/cpp     # JSON config persistence
│   └── SystemTray.h/cpp        # System tray integration
├── assets/
│   ├── packs/
│   │   └── clippy/             # Built-in Clippy pack
│   │       ├── manifest.json
│   │       ├── sprites/
│   │       │   ├── map.png     # Sprite sheet
│   │       │   └── animations.json
│   │       └── preview.png
│   └── lottie/
│       └── effects/            # Visual effects (6 Lottie files)
├── schemas/
│   └── sprite-pack-v1.schema.json  # Sprite pack format schema
├── installer/                  # Qt Installation Framework config
├── gateways/
│   ├── shared/                 # Platform-aware IPC transport (Node.js)
│   │   └── ipc.mjs             # UDP client for 127.0.0.1:52847
│   └── orai-gateway/           # CLI gateway tool
└── Orai_zh_CN.ts               # Simplified Chinese translations
```

## Configuration

Config file: `~/.config/Orai/config.json`

```json
{
  "windowX": 100,
  "windowY": 500,
  "language": "en",
  "autoStart": false,
  "ipcEndpoint": "127.0.0.1:52847"
}
```

The `ipcEndpoint` field defaults to `127.0.0.1:52847` and can be overridden.

## Asset Attribution

- Character animations: Clippy Classic pack (Microsoft Office Assistant)
- Visual effects: Custom-designed Lottie effect animations (MIT license)
- Animation engine: [Samsung rlottie](https://github.com/Samsung/rlottie) (MIT license)

## License

MIT © HUANG Cheng
