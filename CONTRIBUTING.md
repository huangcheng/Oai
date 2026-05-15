# Contributing to Seelie

## Quick Start

```bash
git clone --recurse-submodules <repo>
# That pulls the two small Cubism submodules (~50 MB total).
# The 16 GB Live2D asset archive is NOT a submodule — see below to opt in.
```

Then follow the platform-specific build instructions in [README.md](README.md#building).

## Live2D Cubism SDK Core

The build expects the **Cubism Core** native library under `thirdparty/CubismNativeSamples/Core/`. This directory is **not** included in the git submodules because Live2D redistributes it under a separate EULA.

If the directory is missing, CMake prints:

```
-- Live2D Cubism SDK not found — building without Live2D support
```

The build succeeds, but the resulting binary **cannot render Live2D models** (only legacy sprite-sheet packs work).

### How to get it

1. Go to <https://www.live2d.com/en/sdk/download/native/>
2. Download the **Cubism SDK for Native** (requires a free Live2D account)
3. Extract the archive and copy the `Core/` folder into `thirdparty/CubismNativeSamples/`:

```
thirdparty/
  CubismNativeSamples/
    Core/                  ← drop it here
      include/
        Live2DCubismCore.h
      lib/
        macos/…
        windows/…
        linux/…
```

The directory must contain `include/Live2DCubismCore.h` — that's what CMake probes at configure time.

## GLEW / OpenGL Dependencies

On first build, GLEW must be set up inside the Cubism Samples tree:

```bash
cd thirdparty/CubismNativeSamples/Samples/OpenGL/thirdParty
bash scripts/setup_glew_glfw
```

On Windows, run the equivalent in a bash-capable shell (Git Bash, MSYS2) or download GLEW manually.

## Live2D Community Packs

The upstream Live2D model archive ([Eikanya/Live2d-model](https://github.com/Eikanya/Live2d-model)) is available as an opt-in clone into `thirdparty/upstream-live2d/`. It's ~16 GB packed.

```bash
git clone https://github.com/Eikanya/Live2d-model thirdparty/upstream-live2d --depth=1
cmake --build build --target import_packs
cmake --build build --target generate_packs
```

Asset rights belong to the original game studios. Treat imports as **personal-use only**.

## Code Style

- **C++17** with Qt6 idioms (signals/slots, `QString`, `tr()` for i18n)
- Animation names: PascalCase in C++, snake_case over IPC
- All user-visible strings use `tr()` for translation
- Tests use the Qt Test framework on UDP port 52848

### Formatting

The project ships a `.clang-format` tuned to the existing style: 4-space
indent, function braces on their own line (Allman), control-flow braces
attached, pointers right-aligned (`QObject *parent`), 100-char column
limit.

```bash
clang-format -i src/Foo.cpp           # reformat one file
clang-format --dry-run --Werror src/  # check without writing
```

Most editors auto-pick up the config (VS Code, Qt Creator, CLion) when
the file lives at the project root. Existing files were *not* mass-
reformatted — apply on touch when you edit a file rather than running
it across the whole tree.

## Pull Requests

- Keep changes focused — one concern per PR
- Run `cd build && ctest` before pushing
- If you change IPC behavior, update both this repo and the gateway (`gateways/seelie-gateway/`)
