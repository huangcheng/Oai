---
name: seelie-build-deploy
description: Bring up the Seelie project on a fresh machine — install Qt + Cubism SDK Core, populate submodules, build with scripts/build_release.py, and produce the platform installer (.exe / .dmg / .AppImage). Use when the user is setting up on a new machine, getting a build error before reaching `Configuring done`, debugging a packaging failure, or asking how to ship a release. Triggers on phrases like "build the project", "make an installer", "deploy", "first build", "Live2DCubismCore not found", "GLEW not found", "package_installer", "windeployqt", "macdeployqt", "appimagetool", "fresh clone", "new machine setup", "pet window invisible", "WEBP not supported", "Codex pet fails to load", "build app crashes", "Qt platform plugin".
license: MIT
metadata:
  author: seelie
  version: "1.1"
---

Seelie is a native Qt6/C++ desktop pet that renders Live2D Cubism characters and reacts to AI-coding-tool events over UDP. Getting from `git clone` to a shippable installer involves five moving parts that bite first-time setups in predictable ways. This skill captures the entire pipeline plus the workarounds for the gotchas we've actually hit.

## When to use

- A fresh clone hits `CMake Error: Live2D Cubism SDK Core is not installed.`
- Configure stops with `GLEW not found. Run: ... bash scripts/setup_glew_glfw`.
- `python scripts/build_release.py` fails partway through.
- The user wants to produce a `.exe`/`.dmg`/`.AppImage` for a release.
- Qt Creator's build dir reports "No rule to make target" for a pack texture.
- A packaging run produced a binary that's missing packs / shows a transparent window.

Skip when the question is about runtime behaviour (event routing, IPC, character animation) — that lives in the codebase, not here.

## Project at a glance

```
F:/Seelie/  (or wherever the repo is)
├── src/                        # C++ sources (Qt 6, OpenGL via GLEW)
├── assets/
│   ├── packs/                  # Live2D character packs (.spk source dirs)
│   └── packs-disabled/         # sprite packs not currently shipped
├── thirdparty/
│   ├── CubismNativeFramework/  # SUBMODULE — Live2D wrapper (open source)
│   ├── CubismNativeSamples/    # SUBMODULE — build glue + GLEW/GLFW staging
│   │   └── Core/               # Cubism Core SDK (PROPRIETARY — manual download)
│   └── upstream-live2d/        # SUBMODULE — Eikanya asset archive (~16 GB, opt-in)
├── installer/                  # Qt Installer Framework config
│   ├── config.xml.in           # rendered to config.xml at configure time
│   └── packages/im.cheng.seelie.desktop/
├── scripts/
│   ├── build_release.py        # one-shot build + package driver
│   └── import_live2d.py        # bake .spks from upstream archive
└── CMakeLists.txt              # entry point for both build flavours
```

Three things ship in the installer: the `Seelie.exe` binary, ~116 Live2D `.spk` packs, and `Live2DCubismCore.dll` (or platform equivalent) next to the executable.

## Prerequisites

| Tool | Min version | Notes |
|------|-------------|-------|
| Qt 6 | 6.5+ | 6.11 known-working. **Install via the official Qt Online Installer or Maintenance Tool** — `brew install qt` is not sufficient on macOS (the Homebrew bottle omits the FFmpeg multimedia backend, breaking TTS — see pitfall #19). Pick the kit that matches your toolchain (mingw_64 on Windows, clang_64/macos on macOS, gcc_64 on Linux). Install location: `/opt/Qt` (recommended), `C:\Qt`, or `~/Qt`. |
| CMake | 3.19 | Bundled in Qt's Tools/CMake_64; the build script auto-discovers it. |
| Ninja | any | Bundled in Qt's Tools/Ninja; preferred over Make. |
| Compiler | matching Qt kit | Windows: `Qt/Tools/mingw1310_64`. macOS: Xcode CLT. Linux: distro gcc/clang. |
| Python | 3.11+ | Drives `build_release.py` and `import_live2d.py`. |
| QtIFW (Win/macOS) | 4.9+ | Qt Maintenance Tool → Tools → Qt Installer Framework. Ships `binarycreator`. |
| appimagetool *or* linuxdeploy | any | Linux only. |

The build script auto-detects all of these — you do not have to put any of them on `PATH` manually. The only thing it cannot auto-install is the Cubism SDK Core.

## Step 1 — Clone and submodules

```bash
git clone git@github.com:huangcheng/Seelie.git
cd Seelie
git submodule update --init thirdparty/CubismNativeFramework thirdparty/CubismNativeSamples
```

Note the explicit submodule list. Plain `--recurse-submodules` would also pull `thirdparty/upstream-live2d` (Eikanya/Live2d-model), which is **~16 GB packed / 33 GB unpacked**. Most workflows only need it when re-running `import_live2d.py` to refresh `assets/packs/` — see the `import-live2d-models` skill for that.

## Step 2 — Install the Cubism SDK Core

This is the only step that cannot be automated. Cubism Core is closed-source, EULA-gated, and not in any public repo.

1. **Download** from <https://www.live2d.com/en/sdk/download/native/>. Free account; accept the EULA.
2. **Extract** the archive's `Core/` directory **over** `thirdparty/CubismNativeSamples/Core/`. The empty `Core/` you got from the submodule contains only `LICENSE.md`/`README.md` placeholders; you're filling in `include/`, `dll/`, `lib/`.

   Verification path that must exist:
   ```
   thirdparty/CubismNativeSamples/Core/include/Live2DCubismCore.h
   ```
3. **Fetch GLEW + GLFW** (a separate one-time script Cubism ships):
   ```bash
   cd thirdparty/CubismNativeSamples/Samples/OpenGL/thirdParty
   bash scripts/setup_glew_glfw
   ```
   This downloads ~10 MB of OpenGL headers/sources next to `Core/`. Without it, configure fails with `GLEW not found`.

CMakeLists has a `FATAL_ERROR` guarding step 2 with the exact instructions above — if you skip it the configure will print the recovery procedure verbatim.

If you already have a Cubism SDK extracted somewhere on the machine (e.g. `~/Downloads/CubismSdkTemp/CubismSdkForNative-5-r.5/`), copy it in directly:

```bash
cp -r "$HOME/Downloads/CubismSdkTemp/CubismSdkForNative-5-r.5/Core/." \
      thirdparty/CubismNativeSamples/Core/
```

## Step 3 — Build + package

```bash
python scripts/build_release.py
```

That's it. The script:

1. Auto-detects Qt prefix, cmake, ninja, the compiler matching the Qt kit, and QtIFW's `binarycreator`. **Qt selection prefers official-installer locations** (`/opt/Qt`, `/Applications/Qt`, `~/Qt`, `C:\Qt`) over `qmake`-on-PATH, since PATH-resolved tools are typically Homebrew/apt and miss components like the FFmpeg multimedia backend (see pitfalls #18 / #19). If everything fails, the script asks you for the kit path interactively rather than guessing.
2. Configures the build into `build/` (or `--build-dir` arg).
3. Runs `cmake --build` with `--parallel`. To build serially (low-memory machines), set `CMAKE_BUILD_PARALLEL_LEVEL=1`.
4. Hands off to the platform packager:
   - **Windows** → `cmake --target package_installer` (which stages files via QtIFW's data dir, runs `windeployqt`, then `binarycreator --offline-only`). Output: `build/SeelieInstaller-<version>.exe` (~2 GB with all 116 packs).
   - **macOS** → `macdeployqt` on `Seelie.app` (with `<qt_prefix>/bin` forced to the front of `PATH` so its internal `qmake` doesn't drift to a different Qt), ad-hoc `codesign`, then `hdiutil create ... -format UDZO`. Output: `build/Seelie-<version>.dmg`.
   - **Linux** → `cmake --install` to `build/AppDir/usr/`, write a `.desktop` file, copy the icon to `hicolor/256x256/apps/`, then `appimagetool` (or `linuxdeploy --output appimage`). Output: `build/Seelie-<version>-<arch>.AppImage`.

Useful flags:
- `--skip-build` — re-run packaging only (binaries already built).
- `--skip-package` — compile only (no installer).
- `--qt-dir <path>` — override Qt prefix when auto-detect picks the wrong kit.
- `--build-dir <name>` — for parallel debug/release trees.

## Step 4 — Direct CMake invocation (Qt Creator / IDE workflow)

If you're driving the build from Qt Creator instead of the script:

```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.0/mingw_64
cmake --build build --parallel
cmake --build build --target package_installer       # Windows installer
```

Qt Creator typically creates its own build dir like `build/Desktop_Qt_6_11_0_<kit>_<config>/`. That dir maintains its own ninja state independent of `build/`. **If you switch between the script and Qt Creator on the same source tree, expect stale-cache surprises** — wipe the unused dir's `CMakeCache.txt` (or the whole dir) when symptoms get weird.

## Common pitfalls

These are the ones we've actually hit in production. In rough order of likelihood:

### 1. "Live2D Cubism SDK Core is not installed"

Configure-time `FATAL_ERROR`. You forgot Step 2. The error message is self-explanatory; follow it. Live2D is required unconditionally — there is no opt-out.

### 2. "GLEW not found. Run: ..."

Configure-time `FATAL_ERROR`. You did Step 2 part 1 (extract `Core/`) but not part 2 (run `setup_glew_glfw`). Re-run the bash script.

### 3. AUTOMOC stale cache after toggling Live2D support

If you build once without the Cubism SDK installed and then install it and rebuild, the linker may emit:
```
undefined reference to `Live2DAnimationEngine::staticMetaObject'
undefined reference to `vtable for Live2DAnimationEngine'
```

Cause: `Q_OBJECT` macros inside `#ifdef SEELIE_LIVE2D_SUPPORT` weren't seen by `moc` on the first pass, and ninja didn't re-run moc when the macro flipped because the header mtime didn't change.

Fix:
```bash
rm -rf build/Seelie_autogen build/CMakeFiles/Seelie.dir
touch src/Live2DAnimationEngine.h src/mainwindow.h src/EventRouter.h
python scripts/build_release.py --skip-package
```

### 4. Stale installer staging — "ships old packs even after re-import"

The `installer_stage` CMake target depends on `Seelie` and `${ALL_PACK_SPKS}`. If you somehow regenerate packs without touching either (rare — manual file edits inside the staging dir, partial cleanups), the installer can ship a stale data dir.

Fix:
```bash
rm -rf installer/packages/im.cheng.seelie.desktop/data build/installer_stage.stamp
python scripts/build_release.py --skip-build
```

### 5. Non-ASCII pack filenames break ninja

After bulk-importing from upstream you may see:
```
ninja: error: No rule to make target 'assets/packs/al_bola/textures/texture_00 - 鍓湰.png'
```

Some upstream "副本" / "Copy of" duplicates carry non-ASCII filenames whose bytes round-trip differently between configure runs. The importer (`scripts/import_live2d.py`, post-`a6e3e82`) now skips any non-ASCII filename at copy time, but a partially-imported tree from before that fix needs cleanup:

```bash
find assets/packs -mindepth 1 | LC_ALL=C grep -P '[^\x00-\x7f]' -z |
  xargs -0 rm -rf
```

Then re-configure. CMake's `file(GLOB)` will pick up the cleaned tree.

### 6. Windows 11 DWM chrome on the pet/bubble window

Qt 6.11+'s `qwindows.dll` realises frameless tool windows in a way that Win11's DWM decorates with rounded corners + drop shadow + Mica backdrop. If `MainWindow::showEvent` or `TipBubbleWidget::showEvent` doesn't call `DwmSetWindowAttribute` to opt out, the pet looks like it's in a soft white frame.

Fix is already committed (`f6513dc`). If it regresses, check that:
- `WA_NoSystemBackground` is set on both windows on all platforms (not just `#ifdef Q_OS_MAC`).
- `showEvent` calls `DwmSetWindowAttribute` for `DWMWA_WINDOW_CORNER_PREFERENCE` (=DWMWCP_DONOTROUND), `DWMWA_SYSTEMBACKDROP_TYPE` (=DWMSBT_NONE), `DWMWA_NCRENDERING_POLICY` (=DWMNCRP_DISABLED).
- `dwmapi` is in `target_link_libraries` for `Seelie` and any test binary that pulls in `TipBubbleWidget.cpp`.

### 7. The path you have entered is not valid \Seelie

The QtIFW wizard's TargetDir line was ending up as a literal `/Seelie`, resolving to root-of-current-drive. Cause: `configure_file`'s @-substitution was eating `@ApplicationsDir@` (which is meant to be substituted by IFW at install time, not by CMake at configure time).

Fix already committed (`ecc27a5`): route through a CMake variable `IFW_TARGET_DIR` whose *value* is `@ApplicationsDir@/Seelie`. `configure_file` writes the `@-token` verbatim; IFW expands it later. If this regresses, check `installer/config.xml.in` says `<TargetDir>@IFW_TARGET_DIR@</TargetDir>` and `CMakeLists.txt` does `set(IFW_TARGET_DIR "@ApplicationsDir@/Seelie")` before the `configure_file` call.

### 8. CMake picks the wrong Qt kit

On Windows it's common to have `C:\Qt\6.11.0\` containing `android_armv7/`, `mingw_64/`, `msvc2022_64/`, `wasm_singlethread/`, etc. side by side. `scripts/build_release.py` resolves Qt in this order, **stopping at the first hit**:

1. `$QT_DIR` / `$QT6_DIR` / `$Qt6_DIR` / `$QT_ROOT` / `$QT_INSTALL_PREFIX` env vars.
2. Filesystem candidates — `/opt/Qt`, `/Applications/Qt`, `~/Qt` on macOS; `/opt/Qt`, `~/Qt` on Linux; `C:\Qt`, `~/Qt` on Windows. Inside each, picks the highest 6.x version, then prefers host kits (macOS: `macos`/`clang_64`; Windows: `mingw`/`llvm-mingw`/`msvc`; Linux: `gcc_64`/`linux`) over cross-compile kits.
3. `qmake` / `qtpaths` on `$PATH` — last-resort, intentionally last because PATH-resolved tools are typically Homebrew/apt and lack the FFmpeg multimedia backend on macOS (see pitfall #19).
4. **Interactive prompt** — if all of the above fail, the script asks the user to paste a kit path rather than silently continue with a broken state.

Override explicitly when needed:
```bash
QT_DIR=/opt/Qt/6.11.1/macos python scripts/build_release.py
# Windows
QT_DIR=C:/Qt/6.11.1/mingw_64 python scripts/build_release.py
```

The active selection prints at the top of every run as `[OK] Qt prefix from <source>: <path>` — check it before assuming a build failure is your code.

### 9. Submodule registration drift

`thirdparty/upstream-live2d` has a `.gitmodules` entry but its gitlink may go missing if someone re-runs `git submodule add` on top of an existing entry. Confirm with:

```bash
git ls-tree HEAD thirdparty/    # should show three '160000 commit' lines
git submodule status            # should show three submodules; '-' prefix = uninitialised
```

If `upstream-live2d` is missing entirely:
```bash
SHA=$(git ls-remote git@github.com:Eikanya/Live2d-model.git HEAD | cut -f1)
git update-index --add --cacheinfo 160000 $SHA thirdparty/upstream-live2d
git commit -m "build: restore upstream-live2d submodule pin"
```

### 10. Installed Seelie.exe crashes with "Live2DCubismCore.dll was not found"

```
Seelie.exe - System Error
The code execution cannot proceed because Live2DCubismCore.dll was not found.
Reinstalling the program may fix this problem.
```

Cause: `windeployqt` only handles **Qt's own DLLs and plugins**. For any third-party runtime DLL (Live2D Cubism Core, OpenCV, FFmpeg, etc.), it logs `Adding local dependency …` on noticing the file in the build dir but does **not** copy it into the install staging tree. Without an explicit `install(FILES …)` rule, `cmake --install` then leaves the DLL behind.

Fix already committed (`7ebadcb`) for Cubism Core specifically. The pattern for *any* third-party runtime DLL on Windows MinGW:

```cmake
# 1. Make the DLL part of the install set so package_installer bundles it.
install(FILES ${PATH_TO_DLL}/Foo.dll
        DESTINATION ${CMAKE_INSTALL_BINDIR})

# 2. Stage it next to Seelie.exe in the build dir so direct-run from
#    Qt Creator / `./Seelie.exe` works without a manual cp.
add_custom_command(TARGET Seelie POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PATH_TO_DLL}/Foo.dll
            $<TARGET_FILE_DIR:Seelie>
    VERBATIM)
```

After applying:

```bash
# Wipe stale install staging — installer_stage's DEPENDS doesn't track
# new install() rules, only Seelie + ${ALL_PACK_SPKS}.
rm -rf installer/packages/im.cheng.seelie.desktop/data build/installer_stage.stamp
python scripts/build_release.py
```

Verify the DLL made it:
```bash
ls installer/packages/im.cheng.seelie.desktop/data/Foo.dll  # at root, NOT data/bin/
```

(The staging step flattens `bin/` into the data root — that matches the installer's flat target layout next to `Seelie.exe`.)

The MSVC Windows path doesn't need this: the `_MD.lib` it links against is the **static** Cubism Core variant, not an import library. Static linkage keeps the runtime DLL out of the picture entirely. macOS / Linux also static-link Cubism (`libLive2DCubismCore.a`) and are similarly unaffected. The install-rule pattern above is specifically for **MinGW + import-library DLLs**.

### 11. Installed Seelie.exe crashes with "no Qt platform plugin could be initialized"

```
Seelie
This application failed to start because no Qt platform plugin could
be initialized. Reinstalling the application may fix this problem.
```

The crash means Qt6Core.dll loaded but couldn't locate `qwindows.dll` (the Win32 platform plugin), which lives at `<install>/plugins/platforms/qwindows.dll`. The crash specifically points at a wrong `qt.conf`.

Cause: `windeployqt` writes
```
[Paths]
Prefix = ..
```
into `qt.conf` because it assumes `Seelie.exe` lives in `<root>/bin/` and Qt should walk up one level to find `<root>/plugins/`. But our staging step (CMakeLists.txt staging custom command) flattens `<root>/bin/` into the data root — the installed layout is `<install>/Seelie.exe`, `<install>/plugins/`, `<install>/qt.conf`. With `Prefix = ..` Qt looks for plugins one level *above* the install dir (e.g. `C:\Program Files\plugins\platforms\`) and finds nothing.

Fix already committed (`238d9c7`): generate `qt.conf.flat` at configure time with `Prefix = .` and copy it over the windeployqt-written file in the staging step, after the bin/-flatten. The CMakeLists pattern:

```cmake
# Configure-time: drop a flat-layout qt.conf in the build dir.
file(WRITE ${CMAKE_BINARY_DIR}/qt.conf.flat "[Paths]\nPrefix = .\n")

# Build-time: in the installer_stage custom_command, after the bin-flatten:
COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/qt.conf.flat
        ${SEELIE_IFW_PKG_DATA}/qt.conf
```

Verify after a rebuild:
```bash
cat installer/packages/im.cheng.seelie.desktop/data/qt.conf
# Expected:
# [Paths]
# Prefix = .
```

If it ever regresses to `Prefix = ..`, the copy step in the staging custom_command was reordered to run *before* the windeployqt-driven `cmake --install`. Move it back to fire after the bin/-flatten. The two related staging fixes (`installer_stage` depending on `${ALL_PACK_SPKS}`, `qt.conf.flat` copy after flatten) both target the same custom_command — keep them grouped.

This pitfall is closely related to #4 and #10: every "ships and crashes immediately" bug we've hit on Windows ultimately came from the bin/-flatten step interacting badly with something cmake-install or windeployqt produced. If you're debugging a *new* runtime-load crash on Windows, your first stop is `installer/packages/.../data/` — diff its contents (especially `qt.conf` and the DLL set) against the bin/-flat layout you'd expect at `<install-dir>/`.

### 12. Edited the icons but the exe still shows the old one

Two distinct caches conspire here.

**a) MinGW windres dependency tracking is incomplete.** `seelie.rc` does
```
IDI_ICON1 ICON "@CMAKE_SOURCE_DIR@/assets/seelie.ico"
```
but `windres` doesn't emit a `.d` deps file listing `seelie.ico` as an input. Ninja therefore doesn't know to rebuild `seelie.rc.obj` when only `seelie.ico` changes — and the linked `Seelie.exe` ships with the previous icon embedded, even though the on-disk `.ico` is fresh.

Fix already committed (`dcdf212`): make the dep explicit.

```cmake
set_source_files_properties(${CMAKE_BINARY_DIR}/seelie.rc PROPERTIES
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/assets/seelie.ico)
```

If this regresses, the manual recovery is one-shot:
```bash
rm build/CMakeFiles/Seelie.dir/seelie.rc.obj
python scripts/build_release.py --skip-package
```

The same caveat applies to any other resource-included file (e.g. a `MANIFEST` referencing an XML, or a `BITMAP` referencing a `.bmp`) — windres + ninja won't track it. When in doubt, add `OBJECT_DEPENDS` for every file the `.rc` references via path.

**b) Windows IconCache survives across rebuilds.** Even with the correct icon embedded, Explorer / taskbar / start menu may still show the old one because Win11 caches per-exe icons in `IconCache.db`. Force a refresh:

```cmd
ie4uinit.exe -show
```

Or restart Explorer:

```cmd
taskkill /f /im explorer.exe & start explorer.exe
```

Or — since the Seelie installer puts the binary at a fixed path (`C:\Program Files\Seelie\Seelie.exe`) — uninstalling and reinstalling tends to invalidate the cache for that path naturally. The in-app tray icon and the SystemTray-rendered icon load from the qrc resource (`seelie.png`), not from the exe's embedded icon, so they refresh as soon as a fresh `Seelie.exe` runs — those aren't affected by IconCache, only Explorer's per-exe thumbnails are.

When debugging an "icon won't update" complaint:
1. `stat` mtimes to check `seelie.rc.obj` is newer than `seelie.ico` — if not, it's (a).
2. `Seelie.exe` mtime is newer than `seelie.rc.obj` — confirms the link picked up the new .obj.
3. If both check out, it's (b) — kick the IconCache.

### 13. macOS: Build-directory app crashes with Qt platform plugin conflict

Running `./build/Seelie.app/Contents/MacOS/Seelie` directly crashes immediately:
```
abort() called
QGuiApplicationPrivate::createPlatformIntegration()
```

Cause: The binary links to Qt via `@rpath`, but the shell environment has `DYLD_LIBRARY_PATH` or `DYLD_FRAMEWORK_PATH` pointing at the Homebrew Qt prefix (`/opt/homebrew/lib`). The dynamic linker loads the **system** QtCore first, then the bundled QtCore, causing symbol conflicts.

**This is expected and not a bug.** The build-directory `.app` is not meant to be run directly. macdeployqt's deployment is only valid when the bundle is launched through LaunchServices (`open`) or installed to `/Applications/`.

Fix: Always test via LaunchServices:
```bash
# Install and test the proper way
rm -rf /Applications/Seelie.app
cp -R build/Seelie.app /Applications/Seelie.app
xattr -cr /Applications/Seelie.app          # remove quarantine
open /Applications/Seelie.app
```

Or use `open -n` for a second instance while one is already running:
```bash
open -n /Applications/Seelie.app
```

### 14. macOS: `open build/Seelie.app` launches the wrong binary

After copying a fresh build to `/Applications/Seelie.app`, running `open build/Seelie.app` still opens the **old** installed version. Both bundles share the same bundle ID (`im.cheng.seelie`), so LaunchServices resolves `build/Seelie.app` to the already-registered `/Applications/Seelie.app`.

Fix: Remove or replace the installed app first, or use absolute path with `-n`:
```bash
rm -rf /Applications/Seelie.app
cp -R build/Seelie.app /Applications/Seelie.app
open /Applications/Seelie.app
```

### 15. macOS: macdeployqt corrupts imageformats plugins and support dylibs

Symptoms in `~/Library/Application Support/Seelie/seelie_debug.log`:
```
[WARN] WEBP image format: NOT supported. Codex pets will fail to load.
[DEBUG] Failed to find metadata in lib .../libqwebp.dylib: '...' is not a valid Mach-O binary (invalid magic cefaedfe)
[DEBUG] libqwebp.dylib cannot load: dlopen(...): Library not loaded: @rpath/libwebpdemux.2.dylib
  Reason: tried: '.../Frameworks/libwebpdemux.2.dylib' (MH_DYLIB is missing LC_ID_DYLIB)
```

`macdeployqt` on Qt 6.11 (Homebrew arm64) corrupts the bundle in two independent ways:

**(a) Plugin Mach-O headers.** Every plugin under `Contents/PlugIns/imageformats/` (and `iconengines/`, `styles/`) gets rewritten with a malformed header — magic byte 0 flipped from `cf` (`MH_MAGIC_64`, 64-bit) to `ce` (`MH_MAGIC`, 32-bit), filetype changed from `BUNDLE` (0x8) to `DYLIB` (0x6), file truncated to roughly half its source size. Qt's own `QMachOParser` then refuses to load them: `invalid magic cefaedfe`. JPEG/PNG appear to keep working only because Qt has built-in raster decoders for those; WEBP has no fallback, so Codex pets are the first visible casualty.

**(b) Support dylib install IDs.** Every bundled `Contents/Frameworks/*.dylib` (libwebp, libjpeg, libpng, libtiff, libsharpyuv, libmng, libjasper, etc.) gets its `LC_ID_DYLIB` stripped. Even after (a) is fixed, plugins now fail at `dlopen` because dyld rejects the support dylibs they link against.

`libsharpyuv.0.dylib` is *also* not copied at all on some Qt versions — it's a transitive dep of libwebp that macdeployqt misses.

**Fix** (already in `scripts/build_release.py`, ~line 484, list `plugin_dirs_to_repair`): post-macdeployqt, recopy every `*.dylib` under each plugin dir we ship — currently `imageformats`, `multimedia`, `networkinformation`, `iconengines`, `tls` — from the Qt source plugin dirs, plus every `Frameworks/*.dylib` from `/opt/homebrew/lib/`. For plugins, rewrite absolute deps to `@rpath` via `install_name_tool -change` and **never** call `install_name_tool -id` on them (which is what corrupts `MH_BUNDLE` files in the first place). For support dylibs, use `-id @rpath/<basename>` because their install IDs were stripped. Re-sign bottom-up. **When you start shipping a new plugin dir (e.g. `platforminputcontexts/`, `imageformats/`, a new media backend), add it to `plugin_dirs_to_repair` — see pitfall #18.**

**Verify after a build:**
```bash
# Plugin should be MH_BUNDLE arm64 with magic cffaedfe, not MH_MAGIC + DYLIB.
xxd Seelie.app/Contents/PlugIns/imageformats/libqwebp.dylib | head -1
# Expected first 4 bytes: cffa edfe (NOT cefa edfe)
file Seelie.app/Contents/PlugIns/imageformats/libqwebp.dylib
# Expected: Mach-O 64-bit bundle arm64

# Support dylibs must have an install ID.
otool -D Seelie.app/Contents/Frameworks/libwebp.7.dylib
# Expected: @rpath/libwebp.7.dylib  (not empty)

# Final smoke test — runtime check via the app's own log.
pkill -f Seelie; open /Applications/Seelie.app && sleep 5
grep "WEBP image format" ~/Library/Application\ Support/Seelie/seelie_debug.log | tail -1
# Expected: [DEBUG] WEBP image format: supported (required for Codex pets)
```

**When investigating a new "Codex pet won't load" report:** always grep the user's `seelie_debug.log` for `WEBP image format:` first. The log is at `~/Library/Application Support/Seelie/seelie_debug.log` and `qInstallMessageHandler` writes everything there — stdout/stderr from the launched app will be empty.

**UX coupling.** A failed `switchPack()` used to silently fail because `QSystemTrayIcon`'s exclusive `QActionGroup` flips the radio indicator on click *before* the slot runs, so the menu lies about the active pack. `SystemTray::onPackActionTriggered` and `SettingsPanelWidget`'s pack-button menu now call `refreshPackMenu` / `refreshPackList` on switchPack failure so the radio reverts to truth. If you add a new pack-switch entry point, mirror that pattern.

**Why Windows isn't affected.** `windeployqt` on Windows just copies plugin DLLs verbatim — the PE format doesn't embed absolute install paths, so there's no `install_name_tool` equivalent and no rewriting step to get wrong. The whole class of "the deploy tool damaged the binary" bugs only exists on macOS because Mach-O binaries must be patched to be relocatable. Expect every macOS deploy regression to look like this one: a fresh build runs fine from `build/Seelie.app` via `open`, then a feature that touches an additional plugin (image format, icon engine, style, multimedia codec) fails silently because the post-macdeployqt redeploy pass didn't cover that file type.

### 16. macOS: Invisible pet window — broken pack loading

The pet window exists but shows nothing (transparent/black). Common causes:

**a) Preferred pack fails to load, no fallback.** If the user's config (`~/.config/Seelie/Seelie.ini`) has `activePackId=dario` but dario can't load (e.g., missing WEBP plugin), the old code left `m_activePack = nullptr` and the pet rendered nothing.

Fix (already committed): `CharacterPackManager::initialize()` now iterates through all available packs as fallback when the preferred pack fails:
```cpp
bool loaded = false;
for (const QString &packId : candidates) {
    if (switchPack(packId)) {
        loaded = true;
        break;
    }
    qWarning() << "Failed to load pack" << packId << "— trying next fallback.";
}
```

**b) Window position saved off-screen.** If the pet was dragged to a now-disconnected monitor, `windowX`/`windowY` in `~/.config/Seelie/Seelie.ini` may place it outside the visible screen.

Fix: Reset window position:
```bash
sed -i '' 's/windowX=.*/windowX=100/' ~/.config/Seelie/Seelie.ini
sed -i '' 's/windowY=.*/windowY=500/' ~/.config/Seelie/Seelie.ini
```

**c) Missing assets directory.** The app searches for `animations.json` as a sentinel. If `findAssetsDir()` returns empty, the app has no animations to render.

Verify:
```bash
# In the installed app
ls /Applications/Seelie.app/Contents/Resources/assets/animations.json
ls /Applications/Seelie.app/Contents/Resources/assets/packs/
```

### 17. macOS: "The application can't be opened" / LaunchServices error 153

```
The application cannot be opened for an unexpected reason,
error=Error Domain=RBSRequestErrorDomain Code=5 "Launch failed."
NSPOSIXErrorDomain Code=153 "Launchd job spawn failed"
```

This is a **generic LaunchServices symptom** with multiple possible root causes. The error text is misleading — it usually means the kernel killed the process during early launch, not that Gatekeeper blocked it.

#### Root cause A: Stale/incremental build producing malformed Mach-O (most common)

After iterative development builds (`cmake --build` without clean), the main binary or a bundled Qt framework can become **corrupted** in a way that the kernel's Mach-O loader rejects it immediately. This produces `SIGKILL` (exit code 137) before the app even initializes. LaunchServices then reports the generic "can't be opened" error.

**Diagnosis:**
```bash
# Direct exec shows exit 137 (SIGKILL)
./Seelie.app/Contents/MacOS/Seelie
echo $?   # → 137

# lldb reveals the real error
lldb ./Seelie.app/Contents/MacOS/Seelie
(lldb) run
error: Malformed Mach-o file
```

**Fix:** Clean rebuild from scratch:
```bash
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build . --parallel 2
python3 scripts/build_release.py --skip-build   # repackage from clean build
```

Key: do **not** trust incremental builds after seeing this error. The corrupted artifact may survive `make clean` if it only removes object files — wipe the entire build directory.

#### Root cause B: QML frameworks bundled by macdeployqt (legacy, pre-2025)

`macdeployqt` used to bundle QML frameworks (QtQml, QtQuick, etc.) and `platforminputcontexts/libqtvirtualkeyboardplugin.dylib` even for pure Widgets apps. Some Homebrew Qt builds had malformed headers in these frameworks. LaunchServices validates all embedded binaries, and one malformed framework caused blanket rejection.

**Fix** (if this specific cause applies — check with `otool -h` on frameworks):
```bash
rm -rf Seelie.app/Contents/PlugIns/platforminputcontexts
rm -rf Seelie.app/Contents/Frameworks/QtQml*.framework
rm -rf Seelie.app/Contents/Frameworks/QtQuick.framework
```

#### Distinguishing the causes

| Check | Stale build (Cause A) | QML frameworks (Cause B) |
|-------|----------------------|--------------------------|
| `lldb` shows | `Malformed Mach-o file` on **main binary** | `Malformed Mach-o file` on **QtQml.framework** |
| `otool -h` on main binary | fails / shows corruption | clean |
| Clean rebuild fixes it? | **yes** | no (need to strip QML) |
| Seelie uses QML? | no (never) | irrelevant to root cause |

#### Post-fix: codesign and quarantine

After either fix, re-sign bottom-up and clear quarantine:
```bash
find Seelie.app/Contents/Frameworks -type f \( -name "*.dylib" -o -path "*/Versions/A/*" \) \
  -exec codesign --force --sign - {} \;
find Seelie.app/Contents/PlugIns -name "*.dylib" \
  -exec codesign --force --sign - {} \;
codesign --force --sign - Seelie.app/Contents/MacOS/Seelie
codesign --deep --force --sign - Seelie.app
xattr -cr Seelie.app
```

Key points:
- `--deep` alone is not reliable — sign leaf binaries explicitly first.
- `com.apple.provenance` xattr is re-applied by macOS automatically; `com.apple.quarantine` is the one to clear.
- If distributing without Developer ID, users will always need the Privacy & Security → "Open Anyway" step.

**Reference:** This pitfall was rewritten after a real incident where a clean rebuild (not QML stripping) fixed the issue. The corrupted binary was produced by normal incremental development builds, not by any packaging script bug.

### 18. macOS: TTS silent in production build — multimedia plugin pinned to Homebrew

Symptoms: TTS works perfectly on the dev machine (`./build/Seelie.app/Contents/MacOS/Seelie` or `open build/Seelie.app`), but the **installed** `Seelie-<v>.dmg` is silent. No errors in the UI; the engine fetches MP3 bytes from the provider, then nothing plays. On a fresh Mac without Homebrew Qt at `/opt/homebrew/opt/qtbase/...`, `seelie_debug.log` shows:
```
[WARN] QAudioDecoder error: Cannot find media backend
```
or `QAudioDecoder` silently emits zero buffers.

**Why this isn't caught on the dev machine.** The same machine that built the DMG also has Homebrew Qt at the exact absolute path the plugin was pinned to. dyld happily resolves the absolute deps from the Homebrew tree, the plugin loads, MP3 decoding works. The bug only surfaces on a clean machine — or after `brew uninstall qt`.

**Root cause.** Same class as pitfall #15: `macdeployqt` on Qt 6.11 leaves absolute `/opt/homebrew/opt/qtbase/lib/Qt*.framework/...` and `/opt/homebrew/opt/glib/...` references in plugin dylibs. The fix in #15 originally redeployed only `PlugIns/imageformats/`. `multimedia/libdarwinmediaplugin.dylib`, `networkinformation/libqglib.dylib`, and `iconengines/libqsvgicon.dylib` were silently broken too — but unlike imageformats, the only one Seelie *needs* at runtime is multimedia (for `QAudioDecoder` + `QAudioSink` in `TTSEngine`). Qt has no built-in fallback for media decoding, so TTS just stops working with no obvious error.

**Diagnostic** — one command tells you whether any bundled plugin still references Homebrew absolute paths:
```bash
find Seelie.app/Contents/PlugIns -name "*.dylib" -exec sh -c '
  bad=$(otool -L "$1" 2>/dev/null | grep -E "/opt/(homebrew|local)")
  [ -n "$bad" ] && { echo "=== $1 ==="; echo "$bad"; }
' _ {} \;
```
Expected output: nothing. Any line printed is a plugin that will fail on a clean machine.

**Fix** — already merged into `scripts/build_release.py` as the list `plugin_dirs_to_repair` (`imageformats`, `multimedia`, `networkinformation`, `iconengines`). The general rule, captured here so the next plugin regression is a 1-line change instead of a multi-hour debug session:

> **When you add a new Qt module / plugin to the build** (e.g. add `Qt::Multimedia` → ship `multimedia/`, add `Qt::TextToSpeech` → ship `texttospeech/`, add a new image format → no action needed, already covered), append the plugin subdir name to `plugin_dirs_to_repair` in `scripts/build_release.py`. Do this in the same commit as the CMake `find_package(... COMPONENTS X)` change.

The Qt module → plugin subdir mapping you'll most likely need:

| Qt component | Plugin subdir under `PlugIns/` |
|---|---|
| `Qt::Multimedia` | `multimedia/` |
| `Qt::Network` (TLS) | `tls/` (already correct via `@rpath` — no repair needed) |
| `Qt::TextToSpeech` | `texttospeech/` |
| `Qt::Sql` | `sqldrivers/` |
| `Qt::PrintSupport` | `printsupport/` |
| `Qt::WebEngineCore` | `webengine/` |
| Anything using SVG icons | `iconengines/` |
| Anything using `QNetworkInformation` | `networkinformation/` |

**Smoke test the DMG on a Homebrew-free path.** The cheapest way to catch this without a clean VM is:
```bash
# Move Homebrew Qt out of the way, mount the DMG, run the installed app.
sudo mv /opt/homebrew/opt/qtbase /opt/homebrew/opt/qtbase.bak
sudo mv /opt/homebrew/opt/qt    /opt/homebrew/opt/qt.bak
hdiutil attach build/Seelie-<v>.dmg
open "/Volumes/Seelie/Seelie.app"
# Test TTS in the AI tab. If silent → a plugin is still pinned to Homebrew.
hdiutil detach "/Volumes/Seelie"
sudo mv /opt/homebrew/opt/qtbase.bak /opt/homebrew/opt/qtbase
sudo mv /opt/homebrew/opt/qt.bak    /opt/homebrew/opt/qt
```
Slightly destructive — only do this on a dev machine and only if you can't easily test on a clean Mac.

**Reference:** Discovered 2026-05-16 while debugging "TTS isn't worked in production build". The `_redeploy_plugin` infrastructure already existed (pitfall #15) and was working perfectly — it just wasn't pointed at the dir that mattered. Pitfall #15 even predicted this regression: *"a feature that touches an additional plugin (image format, icon engine, style, multimedia codec) fails silently because the post-macdeployqt redeploy pass didn't cover that file type."*

**Follow-up correction (same day).** Adding `multimedia/` to `plugin_dirs_to_repair` didn't actually fix TTS — the symptom shifted but the user still got no voice. The real error in `seelie_debug.log` was:
```
[WARN] No functional TLS backend was found
[WARN] QSslSocket::connectToHostEncrypted: TLS initialization failed
[WARN] synthesis error kind=0 http=0 msg="TLS initialization failed"
```
Network synthesis was failing before audio playback ever started. `Contents/PlugIns/tls/libqopensslbackend.dylib` was pinned to `/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib` and `/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib`. The bundled `libssl.3.dylib`/`libcrypto.3.dylib` in `Contents/Frameworks/` were healthy, just unreachable. `securetransport` and `certonly` backends also failed, so Qt declared "no TLS backend" and refused to do HTTPS. Adding `tls` to `plugin_dirs_to_repair` finally fixed it.

**The bigger lesson.** Pitfall #15 told you to look at *Qt frameworks* deps in plugins. That misled future-me into a too-narrow `otool -L | grep Qt` filter. **Plugins also depend on non-Qt Homebrew libraries** — openssl@3 in this case, glib in `networkinformation`. The diagnostic in this pitfall (`find ... -exec otool -L ... | grep "/opt/"`) catches both. Always use the broad `/opt/` filter, never just `/opt/.*Qt`.

### 19. macOS: TTS silent because build picked Homebrew Qt instead of the official installer

Symptoms identical to pitfall #18 — `seelie_debug.log` shows:
```
[DEBUG] decoder start, audio bytes= 57130 mime hint= "audio/mpeg"
[WARN] QAudioDecoder error QAudioDecoder::FormatError "Could not load media source's tracks"
```
The bytes arrive, `QAudioDecoder` refuses to decode them. Fixing the multimedia *plugin paths* (pitfall #18) doesn't help — the plugin loads, it just can't decode raw MP3 from a `QBuffer`.

**Root cause: the build picked the wrong Qt.** On macOS, `QAudioDecoder` can decode MP3 only when the **FFmpeg multimedia backend** (`libffmpegmediaplugin.dylib`) is loaded. The AVFoundation backend (`libdarwinmediaplugin.dylib`) requires real container metadata and cannot decode the raw MP3 byte stream the TTS providers return.

- **Homebrew's `qtmultimedia` bottle ships only `libdarwinmediaplugin.dylib`** — no FFmpeg backend, no MP3 over `QAudioDecoder`. This is silent: the framework loads fine, the plugin loads fine, decoding just fails per-call.
- **The official Qt Online Installer (`/opt/Qt/6.11.1/macos/plugins/multimedia/`) ships both** `libdarwinmediaplugin.dylib` *and* `libffmpegmediaplugin.dylib` plus the bundled `libavformat.*`, `libavcodec.*`, `libswresample.*`, `libswscale.*`, `libavutil.*` dylibs. With FFmpeg present, MP3 decode works.

**The trap.** `scripts/build_release.py`'s auto-discovery used to query `qmake` on `$PATH` *before* scanning filesystem candidates. Homebrew puts its Qt's `qmake` on PATH by default, so Homebrew Qt always won — even when the user had the official installer at `/opt/Qt` and was using Qt Creator + the Maintenance Tool for everything else. The first hint that anything was wrong was "TTS used to work, now it doesn't," which is misleading because the code didn't change — the build environment did, on the first build after Homebrew Qt landed on PATH.

**Fix already applied** in `scripts/build_release.py`'s `find_qt_prefix()`:

1. **Filesystem scan runs before PATH scan.** macOS candidates are `/opt/Qt`, `/Applications/Qt`, `~/Qt` — the official-installer locations. Whichever exists wins, regardless of what's on PATH.
2. **Interactive fallback if everything fails.** Rather than silently fall through to a broken state, prompt the user for the Qt kit path. Useful when the user installed Qt somewhere unusual.

The macOS `plugin_dirs_to_repair` list already covers `multimedia/`, so the FFmpeg plugin's absolute deps to `/opt/Qt/.../lib/...` are rewritten to `@rpath` automatically. No extra script work needed for `ffmpeg`-backed builds.

**Verify after a build** — these two are the smoking-gun checks:
```bash
# 1. Which Qt did CMake actually link against?
grep Qt6Multimedia_DIR build/CMakeCache.txt
# Expected: /opt/Qt/6.11.1/macos/lib/cmake/Qt6Multimedia
# Bad:      /opt/homebrew/opt/qt/lib/cmake/Qt6Multimedia

# 2. Is the FFmpeg backend in the bundle?
ls build/Seelie.app/Contents/PlugIns/multimedia/
# Expected: libdarwinmediaplugin.dylib  libffmpegmediaplugin.dylib
# Bad:      libdarwinmediaplugin.dylib  (only)

# 3. Are the ffmpeg dylibs bundled?
ls build/Seelie.app/Contents/Frameworks/ | grep -E "libav|libsw"
# Expected: libavcodec, libavformat, libavutil, libswresample, libswscale (all present)
```

If you ever see Homebrew Qt in the cache after a `build_release.py` run, it means either (a) you have a stale `CMakeCache.txt` from a previous Homebrew build (wipe `build/` and rerun — the old generator/Qt mix produces "generator does not match" errors at the rlottie FetchContent step), or (b) your `/opt/Qt` install is missing or broken (verify `/opt/Qt/<version>/macos/bin/qmake -query QT_VERSION`).

**Why the rebuild has to be from a clean `build/` dir.** Switching Qt prefixes mid-stream is a recipe for stale-cache pain. The old `CMakeCache.txt` records the old Qt6_DIR; the new run overrides it via `-DCMAKE_PREFIX_PATH=...` but the FetchContent subdirs (`build/_deps/rlottie-subbuild/`, `qhotkey-subbuild/`) were generated with the old generator and now fight the new one. Symptom: `Error: generator : Ninja Does not match the generator used previously: Unix Makefiles`. Fix: `rm -rf build && mkdir build` before rerunning.

**Reference:** Discovered 2026-05-16 in the same debugging session as #15/#18. After fixing the plugin paths in #18, TTS was still mute. The decoder error was identical to a "plugin not loaded" error, but the plugin *was* loaded — just the wrong one. `find / -name "libffmpegmediaplugin*"` revealed `/opt/Qt/6.11.1/macos/plugins/multimedia/libffmpegmediaplugin.dylib` existed but wasn't being used. The build script was happily finding Homebrew Qt via `qmake` on PATH before ever checking `/opt/Qt`.

**Follow-up trap (same incident).** Picking the right Qt at the *CMake* layer is necessary but not sufficient. macdeployqt itself shells out to `qmake -query QT_INSTALL_PLUGINS` to find plugins, and **honors `$PATH` order** even when invoked via absolute path. So:

```
[OK] macdeployqt: /opt/Qt/6.11.1/macos/bin/macdeployqt   ← correct
qmake on PATH: /opt/homebrew/bin/qmake                    ← Homebrew wins
```

→ macdeployqt copies Homebrew's 6.11.0 arm64-only plugins into a bundle whose Qt frameworks come from the official installer (6.11.1 universal). The plugin loader then rejects them at runtime on minor-version mismatch:
```
[WARN] WEBP image format: NOT supported. Codex pets will fail to load.
[DEBUG] Failed to find metadata in lib .../libqwebp.dylib
```
WEBP is the loud failure because Codex pets need it; SVG/JPEG silently fall back to Qt's built-in raster decoders. Symptom looks like pitfall #15 (corrupted plugins) but the magic bytes are fine — the file is just from the wrong Qt minor version.

**Fix already applied** to `scripts/build_release.py`:

```python
deploy_env = os.environ.copy()
deploy_env["PATH"] = str(Path(qt_prefix) / "bin") + os.pathsep + deploy_env["PATH"]
run([str(macdeployqt), str(app_bundle)], env=deploy_env)
```

Plus `plug_roots` (used by the post-macdeployqt redeploy pass) now lists `<qt_prefix>/plugins` (official-installer layout) before `<qt_prefix>/share/qt/plugins` (Homebrew/Linux layout). The previously-hardcoded `/opt/homebrew/Cellar/...` fallback was removed — it was the *source* of the wrong-version plugins, not a useful fallback.

The Homebrew-only support-dylib repair (recopy `Frameworks/lib*.dylib` to fix stripped `LC_ID_DYLIB`, hunt down `libsharpyuv` next to libwebp) is now gated on `qt_prefix` actually pointing at a Homebrew install. Building against the official Qt installer skips that block entirely — those dylibs aren't even bundled.

**Verify the bundled plugin matches the bundled Qt minor version:**
```bash
# The plugin's LC_LOAD_DYLIB compatibility version + the plugin's metadata
# must match the bundled QtCore / QtGui current version.
otool -l build/Seelie.app/Contents/PlugIns/imageformats/libqwebp.dylib | grep -A 4 QtGui
file build/Seelie.app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui
# Both should report the same Qt version (e.g. "current version 6.11.1").
# A mismatch (e.g. plugin 6.11.0 vs framework 6.11.1) means macdeployqt's
# qmake shell-out picked the wrong Qt — see PATH override above.
```

### 20. macOS / Linux: build hangs the machine — terminal "freezes"

Symptom: `python scripts/build_release.py` is invoked, the terminal stops responding, cursor moves at 1 fps, Activity Monitor / `top` shows load average over 100. Hitting Ctrl-C eventually works after 30+ seconds.

**Not a build error.** The system is thrashing. The default `cmake --build --parallel` spawns one job per CPU core (8 on M-series Macs), and each clang process compiling a Qt-heavy translation unit peaks at 0.5–2 GB resident — so 8 concurrent compiles can demand 8–16 GB of RAM on top of whatever else is running (browsers, Lark, multiple `claude` sessions, etc.). Once macOS exhausts physical RAM and starts compressing/swapping, every keystroke waits behind page faults and the UI looks frozen.

**Diagnostic** (from another terminal or via SSH):
```
top -l 1 -n 0 | head -6
# Load Avg over CPU count + Pages free near zero + heavy swapins/swapouts → thrashing
```

**Fix during the build:** kill the build, then re-run with parallelism capped:
```bash
pkill -INT -f "cmake --build"; pkill clang
CMAKE_BUILD_PARALLEL_LEVEL=1 python scripts/build_release.py
```

`CMAKE_BUILD_PARALLEL_LEVEL=1` forces serial compilation. Build takes 25–35 min instead of 5–10, but the machine stays usable. `=2` is a sweet spot on machines with ≥ 16 GB RAM and few other apps open. Anything ≥ `=4` will thrash on a 16 GB Mac with a normal app load.

**Why a "lower" `-j` can still hang the machine:** the signal isn't whether parallelism is "low" — it's whether `top` shows `Pages free` collapsing into the thousands and `Pages compressed` rising. If that happens, drop further. Don't trust core count.

**Reference:** Encountered 2026-05-16 while doing back-to-back rebuilds. The default `--parallel` had been fine on previous builds because fewer apps were open. Default-document `CMAKE_BUILD_PARALLEL_LEVEL=1` for any "how to rebuild" instructions targeting unknown hardware; recover speed only when you've measured headroom.

## Cross-platform packaging summary

| Platform | Built artefact | Bundles | Auto-discovered tooling |
|----------|----------------|---------|-------------------------|
| Windows | `SeelieInstaller-<v>.exe` | Seelie.exe, all .spks, Live2DCubismCore.dll, Qt DLLs (via windeployqt), 32 locale .qm files | `binarycreator`, `windeployqt`, MinGW from Qt Tools |
| macOS | `Seelie-<v>.dmg` | Seelie.app (with packs in `Contents/Resources/packs/`, Cubism Core lib statically linked, Qt frameworks via macdeployqt, ad-hoc codesigned) | `macdeployqt`, `codesign`, `xattr`, `hdiutil` |
| Linux | `Seelie-<v>-<arch>.AppImage` | usr/bin/Seelie, usr/lib/, usr/share/applications/seelie.desktop, usr/share/icons/.../seelie.png | `appimagetool` *or* `linuxdeploy` (whichever is on PATH) |

The macOS path uses static Cubism linkage (`libLive2DCubismCore.a`); Win/Linux use the shared Cubism runtime. CMakeLists.txt:147–177 has the platform branching.

## Output verification

After a successful run, sanity checks:

```bash
# Windows
ls -la build/SeelieInstaller-*.exe
ls build/Live2DCubismCore.dll              # must exist for direct-run from build/
ls installer/packages/im.cheng.seelie.desktop/data/packs/ | wc -l   # should be 116

# macOS
ls -la build/Seelie-*.dmg
codesign -dv build/Seelie.app                 # ad-hoc signed
ls build/Seelie.app/Contents/Resources/packs/ | wc -l

# Linux
ls -la build/Seelie-*.AppImage
file build/Seelie-*.AppImage                  # ELF executable, AppImage
```

If the Windows installer comes out under ~250 MB, packs were not staged — either `installer_stage.stamp` is stale (see pitfall #4) or `assets/packs/` is empty (you forgot to import or only have the 21 first-party packs).

## Adjacent skills

- `import-live2d-models` — bake more `.spk` files from the Eikanya archive into `assets/packs/`.
- `seelie-server` — the Aliyun-hosted UDP update server that the built binary phones home to.

If you find yourself wanting to ship a build flavour without Cubism (e.g. an embedded sprite-only build), don't — change the asset story first by enabling sprite packs from `assets/packs-disabled/`. Live2D is currently a hard requirement of the build (commit `0520516`).
