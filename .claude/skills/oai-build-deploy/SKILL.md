---
name: oai-build-deploy
description: Bring up the Oai project on a fresh machine — install Qt + Cubism SDK Core, populate submodules, build with scripts/build_release.py, and produce the platform installer (.exe / .dmg / .AppImage). Use when the user is setting up on a new machine, getting a build error before reaching `Configuring done`, debugging a packaging failure, or asking how to ship a release. Triggers on phrases like "build the project", "make an installer", "deploy", "first build", "Live2DCubismCore not found", "GLEW not found", "package_installer", "windeployqt", "macdeployqt", "appimagetool", "fresh clone", "new machine setup".
license: MIT
metadata:
  author: oai
  version: "1.0"
---

Oai is a native Qt6/C++ desktop pet that renders Live2D Cubism characters and reacts to AI-coding-tool events over UDP. Getting from `git clone` to a shippable installer involves five moving parts that bite first-time setups in predictable ways. This skill captures the entire pipeline plus the workarounds for the gotchas we've actually hit.

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
F:/Oai/  (or wherever the repo is)
├── src/                        # C++ sources (Qt 6, OpenGL via GLEW)
├── assets/
│   ├── packs/                  # Live2D character packs (.opk source dirs)
│   └── packs-disabled/         # sprite packs not currently shipped
├── thirdparty/
│   ├── CubismNativeFramework/  # SUBMODULE — Live2D wrapper (open source)
│   ├── CubismNativeSamples/    # SUBMODULE — build glue + GLEW/GLFW staging
│   │   └── Core/               # Cubism Core SDK (PROPRIETARY — manual download)
│   └── upstream-live2d/        # SUBMODULE — Eikanya asset archive (~16 GB, opt-in)
├── installer/                  # Qt Installer Framework config
│   ├── config.xml.in           # rendered to config.xml at configure time
│   └── packages/im.cheng.oai.desktop/
├── scripts/
│   ├── build_release.py        # one-shot build + package driver
│   └── import_live2d.py        # bake .opks from upstream archive
└── CMakeLists.txt              # entry point for both build flavours
```

Three things ship in the installer: the `Oai.exe` binary, ~116 Live2D `.opk` packs, and `Live2DCubismCore.dll` (or platform equivalent) next to the executable.

## Prerequisites

| Tool | Min version | Notes |
|------|-------------|-------|
| Qt 6 | 6.5+ | 6.11 known-working. Install via the official online installer; pick the kit that matches your toolchain (mingw_64 on Windows, clang_64 on macOS, gcc_64 on Linux). |
| CMake | 3.19 | Bundled in Qt's Tools/CMake_64; the build script auto-discovers it. |
| Ninja | any | Bundled in Qt's Tools/Ninja; preferred over Make. |
| Compiler | matching Qt kit | Windows: `Qt/Tools/mingw1310_64`. macOS: Xcode CLT. Linux: distro gcc/clang. |
| Python | 3.11+ | Drives `build_release.py` and `import_live2d.py`. |
| QtIFW (Win/macOS) | 4.9+ | Qt Maintenance Tool → Tools → Qt Installer Framework. Ships `binarycreator`. |
| appimagetool *or* linuxdeploy | any | Linux only. |

The build script auto-detects all of these — you do not have to put any of them on `PATH` manually. The only thing it cannot auto-install is the Cubism SDK Core.

## Step 1 — Clone and submodules

```bash
git clone git@github.com:huangcheng/Oai.git
cd Oai
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

1. Auto-detects Qt prefix, cmake, ninja, the compiler matching the Qt kit, and QtIFW's `binarycreator`.
2. Configures the build into `build/` (or `--build-dir` arg).
3. Runs `cmake --build` with `--parallel`.
4. Hands off to the platform packager:
   - **Windows** → `cmake --target package_installer` (which stages files via QtIFW's data dir, runs `windeployqt`, then `binarycreator --offline-only`). Output: `build/OaiInstaller-<version>.exe` (~2 GB with all 116 packs).
   - **macOS** → `macdeployqt` on `Oai.app`, ad-hoc `codesign`, then `hdiutil create ... -format UDZO`. Output: `build/Oai-<version>.dmg`.
   - **Linux** → `cmake --install` to `build/AppDir/usr/`, write a `.desktop` file, copy the icon to `hicolor/256x256/apps/`, then `appimagetool` (or `linuxdeploy --output appimage`). Output: `build/Oai-<version>-<arch>.AppImage`.

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

Cause: `Q_OBJECT` macros inside `#ifdef OAI_LIVE2D_SUPPORT` weren't seen by `moc` on the first pass, and ninja didn't re-run moc when the macro flipped because the header mtime didn't change.

Fix:
```bash
rm -rf build/Oai_autogen build/CMakeFiles/Oai.dir
touch src/Live2DAnimationEngine.h src/mainwindow.h src/EventRouter.h
python scripts/build_release.py --skip-package
```

### 4. Stale installer staging — "ships old packs even after re-import"

The `installer_stage` CMake target depends on `Oai` and `${ALL_PACK_OPKS}`. If you somehow regenerate packs without touching either (rare — manual file edits inside the staging dir, partial cleanups), the installer can ship a stale data dir.

Fix:
```bash
rm -rf installer/packages/im.cheng.oai.desktop/data build/installer_stage.stamp
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
- `dwmapi` is in `target_link_libraries` for `Oai` and any test binary that pulls in `TipBubbleWidget.cpp`.

### 7. The path you have entered is not valid \Oai

The QtIFW wizard's TargetDir line was ending up as a literal `/Oai`, resolving to root-of-current-drive. Cause: `configure_file`'s @-substitution was eating `@ApplicationsDir@` (which is meant to be substituted by IFW at install time, not by CMake at configure time).

Fix already committed (`ecc27a5`): route through a CMake variable `IFW_TARGET_DIR` whose *value* is `@ApplicationsDir@/Oai`. `configure_file` writes the `@-token` verbatim; IFW expands it later. If this regresses, check `installer/config.xml.in` says `<TargetDir>@IFW_TARGET_DIR@</TargetDir>` and `CMakeLists.txt` does `set(IFW_TARGET_DIR "@ApplicationsDir@/Oai")` before the `configure_file` call.

### 8. CMake picks the wrong Qt kit

On Windows it's common to have `C:\Qt\6.11.0\` containing `android_armv7/`, `mingw_64/`, `msvc2022_64/`, `wasm_singlethread/`, etc. side by side. The build script's filesystem fallback now sorts host kits first (`mingw`, `llvm-mingw`, `msvc` on Win), but `qtpaths` on `PATH` may still resolve to whichever Qt happens to be there.

Override:
```bash
python scripts/build_release.py --qt-dir C:/Qt/6.11.0/mingw_64
# or
export QT_DIR=C:/Qt/6.11.0/mingw_64
```

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

### 10. Installed Oai.exe crashes with "Live2DCubismCore.dll was not found"

```
Oai.exe - System Error
The code execution cannot proceed because Live2DCubismCore.dll was not found.
Reinstalling the program may fix this problem.
```

Cause: `windeployqt` only handles **Qt's own DLLs and plugins**. For any third-party runtime DLL (Live2D Cubism Core, OpenCV, FFmpeg, etc.), it logs `Adding local dependency …` on noticing the file in the build dir but does **not** copy it into the install staging tree. Without an explicit `install(FILES …)` rule, `cmake --install` then leaves the DLL behind.

Fix already committed (`7ebadcb`) for Cubism Core specifically. The pattern for *any* third-party runtime DLL on Windows MinGW:

```cmake
# 1. Make the DLL part of the install set so package_installer bundles it.
install(FILES ${PATH_TO_DLL}/Foo.dll
        DESTINATION ${CMAKE_INSTALL_BINDIR})

# 2. Stage it next to Oai.exe in the build dir so direct-run from
#    Qt Creator / `./Oai.exe` works without a manual cp.
add_custom_command(TARGET Oai POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PATH_TO_DLL}/Foo.dll
            $<TARGET_FILE_DIR:Oai>
    VERBATIM)
```

After applying:

```bash
# Wipe stale install staging — installer_stage's DEPENDS doesn't track
# new install() rules, only Oai + ${ALL_PACK_OPKS}.
rm -rf installer/packages/im.cheng.oai.desktop/data build/installer_stage.stamp
python scripts/build_release.py
```

Verify the DLL made it:
```bash
ls installer/packages/im.cheng.oai.desktop/data/Foo.dll  # at root, NOT data/bin/
```

(The staging step flattens `bin/` into the data root — that matches the installer's flat target layout next to `Oai.exe`.)

The MSVC Windows path doesn't need this: the `_MD.lib` it links against is the **static** Cubism Core variant, not an import library. Static linkage keeps the runtime DLL out of the picture entirely. macOS / Linux also static-link Cubism (`libLive2DCubismCore.a`) and are similarly unaffected. The install-rule pattern above is specifically for **MinGW + import-library DLLs**.

### 11. Installed Oai.exe crashes with "no Qt platform plugin could be initialized"

```
Oai
This application failed to start because no Qt platform plugin could
be initialized. Reinstalling the application may fix this problem.
```

The crash means Qt6Core.dll loaded but couldn't locate `qwindows.dll` (the Win32 platform plugin), which lives at `<install>/plugins/platforms/qwindows.dll`. The crash specifically points at a wrong `qt.conf`.

Cause: `windeployqt` writes
```
[Paths]
Prefix = ..
```
into `qt.conf` because it assumes `Oai.exe` lives in `<root>/bin/` and Qt should walk up one level to find `<root>/plugins/`. But our staging step (CMakeLists.txt staging custom command) flattens `<root>/bin/` into the data root — the installed layout is `<install>/Oai.exe`, `<install>/plugins/`, `<install>/qt.conf`. With `Prefix = ..` Qt looks for plugins one level *above* the install dir (e.g. `C:\Program Files\plugins\platforms\`) and finds nothing.

Fix already committed (`238d9c7`): generate `qt.conf.flat` at configure time with `Prefix = .` and copy it over the windeployqt-written file in the staging step, after the bin/-flatten. The CMakeLists pattern:

```cmake
# Configure-time: drop a flat-layout qt.conf in the build dir.
file(WRITE ${CMAKE_BINARY_DIR}/qt.conf.flat "[Paths]\nPrefix = .\n")

# Build-time: in the installer_stage custom_command, after the bin-flatten:
COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/qt.conf.flat
        ${OAI_IFW_PKG_DATA}/qt.conf
```

Verify after a rebuild:
```bash
cat installer/packages/im.cheng.oai.desktop/data/qt.conf
# Expected:
# [Paths]
# Prefix = .
```

If it ever regresses to `Prefix = ..`, the copy step in the staging custom_command was reordered to run *before* the windeployqt-driven `cmake --install`. Move it back to fire after the bin/-flatten. The two related staging fixes (`installer_stage` depending on `${ALL_PACK_OPKS}`, `qt.conf.flat` copy after flatten) both target the same custom_command — keep them grouped.

This pitfall is closely related to #4 and #10: every "ships and crashes immediately" bug we've hit on Windows ultimately came from the bin/-flatten step interacting badly with something cmake-install or windeployqt produced. If you're debugging a *new* runtime-load crash on Windows, your first stop is `installer/packages/.../data/` — diff its contents (especially `qt.conf` and the DLL set) against the bin/-flat layout you'd expect at `<install-dir>/`.

### 12. Edited the icons but the exe still shows the old one

Two distinct caches conspire here.

**a) MinGW windres dependency tracking is incomplete.** `oai.rc` does
```
IDI_ICON1 ICON "@CMAKE_SOURCE_DIR@/assets/oai.ico"
```
but `windres` doesn't emit a `.d` deps file listing `oai.ico` as an input. Ninja therefore doesn't know to rebuild `oai.rc.obj` when only `oai.ico` changes — and the linked `Oai.exe` ships with the previous icon embedded, even though the on-disk `.ico` is fresh.

Fix already committed (`dcdf212`): make the dep explicit.

```cmake
set_source_files_properties(${CMAKE_BINARY_DIR}/oai.rc PROPERTIES
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/assets/oai.ico)
```

If this regresses, the manual recovery is one-shot:
```bash
rm build/CMakeFiles/Oai.dir/oai.rc.obj
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

Or — since the Oai installer puts the binary at a fixed path (`C:\Program Files\Oai\Oai.exe`) — uninstalling and reinstalling tends to invalidate the cache for that path naturally. The in-app tray icon and the SystemTray-rendered icon load from the qrc resource (`oai.png`), not from the exe's embedded icon, so they refresh as soon as a fresh `Oai.exe` runs — those aren't affected by IconCache, only Explorer's per-exe thumbnails are.

When debugging an "icon won't update" complaint:
1. `stat` mtimes to check `oai.rc.obj` is newer than `oai.ico` — if not, it's (a).
2. `Oai.exe` mtime is newer than `oai.rc.obj` — confirms the link picked up the new .obj.
3. If both check out, it's (b) — kick the IconCache.

## Cross-platform packaging summary

| Platform | Built artefact | Bundles | Auto-discovered tooling |
|----------|----------------|---------|-------------------------|
| Windows | `OaiInstaller-<v>.exe` | Oai.exe, all .opks, Live2DCubismCore.dll, Qt DLLs (via windeployqt), 32 locale .qm files | `binarycreator`, `windeployqt`, MinGW from Qt Tools |
| macOS | `Oai-<v>.dmg` | Oai.app (with packs in `Contents/Resources/packs/`, Cubism Core lib statically linked, Qt frameworks via macdeployqt, ad-hoc codesigned) | `macdeployqt`, `codesign`, `xattr`, `hdiutil` |
| Linux | `Oai-<v>-<arch>.AppImage` | usr/bin/Oai, usr/lib/, usr/share/applications/oai.desktop, usr/share/icons/.../oai.png | `appimagetool` *or* `linuxdeploy` (whichever is on PATH) |

The macOS path uses static Cubism linkage (`libLive2DCubismCore.a`); Win/Linux use the shared Cubism runtime. CMakeLists.txt:147–177 has the platform branching.

## Output verification

After a successful run, sanity checks:

```bash
# Windows
ls -la build/OaiInstaller-*.exe
ls build/Live2DCubismCore.dll              # must exist for direct-run from build/
ls installer/packages/im.cheng.oai.desktop/data/packs/ | wc -l   # should be 116

# macOS
ls -la build/Oai-*.dmg
codesign -dv build/Oai.app                 # ad-hoc signed
ls build/Oai.app/Contents/Resources/packs/ | wc -l

# Linux
ls -la build/Oai-*.AppImage
file build/Oai-*.AppImage                  # ELF executable, AppImage
```

If the Windows installer comes out under ~250 MB, packs were not staged — either `installer_stage.stamp` is stale (see pitfall #4) or `assets/packs/` is empty (you forgot to import or only have the 21 first-party packs).

## Adjacent skills

- `import-live2d-models` — bake more `.opk` files from the Eikanya archive into `assets/packs/`.
- `oai-server` — the Aliyun-hosted UDP update server that the built binary phones home to.

If you find yourself wanting to ship a build flavour without Cubism (e.g. an embedded sprite-only build), don't — change the asset story first by enabling sprite packs from `assets/packs-disabled/`. Live2D is currently a hard requirement of the build (commit `0520516`).
