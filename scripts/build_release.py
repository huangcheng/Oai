#!/usr/bin/env python3
"""
Cross-platform release builder for Oai.

Auto-detects Qt, build tools, and platform-specific packagers:
  - Windows: Qt Installer Framework (binarycreator) -> .exe installer
  - macOS:   macdeployqt + hdiutil -> .dmg
  - Linux:   cmake install + appimagetool -> .AppImage

Usage:
    python scripts/build_release.py
    python scripts/build_release.py --build-dir build-rel --config Release
"""

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent


def run(cmd, **kwargs):
    """Run a command, printing it first."""
    cmd_str = " ".join(str(c) for c in cmd)
    print(f"  -> {cmd_str}")
    return subprocess.run(cmd, check=True, **kwargs)


def run_output(cmd, **kwargs):
    """Run a command and return stdout."""
    result = subprocess.run(cmd, capture_output=True, text=True, check=False, **kwargs)
    return result.stdout.strip() if result.returncode == 0 else ""


def check_program(name, hint=""):
    """Check if a program exists in PATH."""
    path = shutil.which(name)
    if path:
        print(f"  [OK] {name}: {path}")
        return Path(path)
    msg = f"  [MISSING] {name} not found in PATH"
    if hint:
        msg += f" ({hint})"
    print(msg)
    return None


def get_project_version():
    """Parse version from top-level CMakeLists.txt."""
    cmake = PROJECT_ROOT / "CMakeLists.txt"
    if not cmake.exists():
        return "1.0.0"
    text = cmake.read_text(encoding="utf-8")
    m = re.search(r"project\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", text)
    return m.group(1) if m else "1.0.0"


def get_cached_qt_prefix(build_dir):
    """If a CMakeCache exists, return the Qt prefix that was last configured."""
    cache = build_dir / "CMakeCache.txt"
    if not cache.exists():
        return None
    text = cache.read_text(errors="ignore")
    m = re.search(r"^Qt6_DIR:PATH=(.+)/lib/cmake/Qt6$", text, re.MULTILINE)
    if m:
        p = Path(m.group(1))
        if p.exists():
            return p
    return None


def find_qt_prefix(cached_prefix=None):
    """
    Detect Qt 6 installation prefix.
    Returns Path(prefix) or None.
    """
    # 0. Prefer the Qt that was already used for an existing build
    if cached_prefix:
        print(f"  [OK] Qt prefix from existing build cache: {cached_prefix}")
        return cached_prefix

    # 1. Environment variables
    for env in ("QT_DIR", "QT6_DIR", "Qt6_DIR", "QT_ROOT", "QT_INSTALL_PREFIX"):
        val = os.environ.get(env)
        if val:
            p = Path(val)
            if p.exists():
                print(f"  [OK] Qt prefix from {env}: {p}")
                return p

    # 2. Query tools in PATH
    for tool in ("qtpaths6", "qtpaths", "qmake6", "qmake"):
        exe = shutil.which(tool)
        if not exe:
            continue
        try:
            if tool.startswith("qtpaths"):
                out = run_output([exe, "--query", "QT_INSTALL_PREFIX"])
            else:
                # Verify it's Qt 6
                ver = run_output([exe, "-query", "QT_VERSION"])
                if ver and not ver.startswith("6."):
                    continue
                out = run_output([exe, "-query", "QT_INSTALL_PREFIX"])
            if out:
                p = Path(out)
                if p.exists():
                    print(f"  [OK] Qt prefix from {tool}: {p}")
                    return p
        except Exception:
            continue

    # 3. Common filesystem locations (last resort)
    home = Path.home()
    candidates = []
    if sys.platform == "win32":
        candidates = [Path(r"C:\Qt")]
    elif sys.platform == "darwin":
        candidates = [Path("/Applications/Qt"), home / "Qt"]
    else:
        candidates = [Path("/opt/Qt"), home / "Qt"]

    for base in candidates:
        if not base.exists():
            continue
        # Look for 6.x directories, newest first
        versions = sorted(
            (d for d in base.iterdir() if d.is_dir() and d.name.startswith("6.")),
            key=lambda d: d.name,
            reverse=True,
        )
        # Prefer host kits over cross-compile kits when scanning a 6.x dir.
        # On Windows a single Qt install often ships android_*, wasm_*, and
        # mingw_64/msvc2022_64 side by side; alphabetic order would pick the
        # Android kit first.
        if sys.platform == "win32":
            host_prefixes = ("mingw", "llvm-mingw", "msvc")
        elif sys.platform == "darwin":
            host_prefixes = ("macos", "clang_64")
        else:
            host_prefixes = ("gcc_64", "linux")

        def _kit_rank(d):
            name = d.name.lower()
            for i, pref in enumerate(host_prefixes):
                if name.startswith(pref):
                    return (0, i, name)
            return (1, 0, name)  # cross / unknown kits sort last

        for v in versions:
            kits = sorted((d for d in v.iterdir() if d.is_dir()), key=_kit_rank)
            for sub in kits:
                bin_dir = sub / "bin"
                if sys.platform == "win32":
                    names = ("qtpaths6.exe", "qtpaths.exe", "qmake6.exe", "qmake.exe")
                else:
                    names = ("qtpaths6", "qtpaths", "qmake6", "qmake")
                if any((bin_dir / name).exists() for name in names):
                    print(f"  [OK] Qt prefix from filesystem search: {sub}")
                    return sub
    return None


def find_binarycreator(qt_prefix=None):
    """Find Qt Installer Framework's binarycreator executable."""
    name = "binarycreator.exe" if sys.platform == "win32" else "binarycreator"

    # PATH
    path = shutil.which(name)
    if path:
        return Path(path)

    # QTIFW_DIR env
    qtifw = os.environ.get("QTIFW_DIR")
    if qtifw:
        p = Path(qtifw) / "bin" / name
        if p.exists():
            return p

    # Relative to Qt prefix: <qt>/../../Tools/QtInstallerFramework/*/bin
    if qt_prefix:
        qt_root = qt_prefix
        # If prefix is like /opt/Qt/6.8.0/gcc_64, go up two levels
        for _ in range(2):
            qt_root = qt_root.parent
        tools = qt_root / "Tools" / "QtInstallerFramework"
        if tools.exists():
            for ver in sorted(tools.iterdir(), key=lambda d: d.name, reverse=True):
                p = ver / "bin" / name
                if p.exists():
                    return p

    # Hard-coded common paths (only used as fallback when auto-detection fails)
    common = []
    if sys.platform == "win32":
        common = [Path(r"C:\Qt\Tools\QtInstallerFramework")]
    elif sys.platform == "darwin":
        common = [Path("/Applications/Qt/Tools/QtInstallerFramework"), Path.home() / "Qt/Tools/QtInstallerFramework"]
    else:
        common = [Path("/opt/Qt/Tools/QtInstallerFramework"), Path.home() / "Qt/Tools/QtInstallerFramework"]

    for base in common:
        if base.exists():
            for ver in sorted(base.iterdir(), key=lambda d: d.name, reverse=True):
                p = ver / "bin" / name
                if p.exists():
                    return p
    return None


def find_cmake(qt_prefix=None):
    """Find cmake, preferring Qt-bundled copies."""
    # PATH
    path = shutil.which("cmake")
    if path:
        return Path(path)
    # Relative to Qt prefix Tools directory
    if qt_prefix:
        qt_root = qt_prefix
        for _ in range(2):
            qt_root = qt_root.parent
        tools = qt_root / "Tools"
        if tools.exists():
            # CMake_64/bin/cmake.exe or CMake/bin/cmake
            for cand in (tools / "CMake_64" / "bin" / "cmake", tools / "CMake" / "bin" / "cmake"):
                exe = cand.with_suffix(".exe") if sys.platform == "win32" else cand
                if exe.exists():
                    return exe
            # Generic search under Tools/*/bin/cmake
            for sub in tools.iterdir():
                if sub.is_dir():
                    exe = sub / "bin" / ("cmake.exe" if sys.platform == "win32" else "cmake")
                    if exe.exists():
                        return exe
    return None


def find_ninja(qt_prefix=None):
    """Find ninja, preferring Qt-bundled copies."""
    path = shutil.which("ninja")
    if path:
        return Path(path)
    if qt_prefix:
        qt_root = qt_prefix
        for _ in range(2):
            qt_root = qt_root.parent
        tools = qt_root / "Tools"
        if tools.exists():
            exe = tools / "Ninja" / ("ninja.exe" if sys.platform == "win32" else "ninja")
            if exe.exists():
                return exe
    return None


def find_macdeployqt(qt_prefix=None):
    """Find macdeployqt."""
    if qt_prefix:
        p = qt_prefix / "bin" / "macdeployqt"
        if p.exists():
            return p
    return shutil.which("macdeployqt")


def find_appimagetool():
    """Find appimagetool."""
    return shutil.which("appimagetool")


def find_linuxdeploy():
    """Find linuxdeploy (with AppImage output support)."""
    return shutil.which("linuxdeploy")


def cmake_has_target(cmake, build_dir, target):
    """Check if a target exists in the generated build system.

    Returns False only when the build system was queried successfully and
    `target` wasn't listed. If cmake itself fails (build dir not configured,
    cmake missing, etc.) we raise so the caller doesn't silently fall through
    to a manual-staging path that masks the real problem.
    """
    result = subprocess.run(
        [str(cmake), "--build", str(build_dir), "--target", "help"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"cmake --build --target help failed (exit {result.returncode}):\n"
            f"{result.stderr.strip() or result.stdout.strip()}"
        )
    # Match target name as a whole word at the start of a line (works for
    # Ninja "target: type", Make "target:", and VS target listings)
    pattern = re.compile(rf"^{re.escape(target)}\b", re.MULTILINE)
    return bool(pattern.search(result.stdout))


# ---------------------------------------------------------------------------
# Platform packaging
# ---------------------------------------------------------------------------

def package_windows(build_dir, version, qt_prefix, cmake, config="Release"):
    print("\n[Windows] Packaging with Qt Installer Framework...")

    binarycreator = find_binarycreator(qt_prefix)
    if not binarycreator:
        print(
            "ERROR: binarycreator not found.\n"
            "Install Qt Installer Framework from the Qt Maintenance Tool\n"
            "(it's under Qt -> Tools -> Qt Installer Framework)\n"
            "or set QTIFW_DIR environment variable."
        )
        sys.exit(1)
    print(f"  [OK] binarycreator: {binarycreator}")

    # Ensure binarycreator is on PATH so cmake configure finds it
    env = os.environ.copy()
    env["PATH"] = str(binarycreator.parent) + os.pathsep + env.get("PATH", "")

    # If already configured but without binarycreator, re-configure
    cache = build_dir / "CMakeCache.txt"
    if cache.exists():
        # Quick check: was binarycreator found during last configure?
        cache_text = cache.read_text(errors="ignore")
        if "OAI_QTIFW_BINARYCREATOR-NOTFOUND" in cache_text or "OAI_QTIFW_BINARYCREATOR:FILEPATH=" not in cache_text:
            print("  Re-configuring so CMake discovers binarycreator...")
            run([str(cmake), "-B", str(build_dir), "-S", str(PROJECT_ROOT),
                 "-DCMAKE_BUILD_TYPE=Release"], env=env)
        else:
            # Just re-run configure to be safe (fast when nothing changed)
            run([str(cmake), "-B", str(build_dir), "-S", str(PROJECT_ROOT)], env=env)
    else:
        run([str(cmake), "-B", str(build_dir), "-S", str(PROJECT_ROOT),
             "-DCMAKE_BUILD_TYPE=Release"], env=env)

    if cmake_has_target(cmake, build_dir, "package_installer"):
        print("  Building package_installer target...")
        run([str(cmake), "--build", str(build_dir), "--target", "package_installer", "--config", config], env=env)
    else:
        print("  WARNING: package_installer target not found. Falling back to manual invocation...")
        # Stage manually using the existing installer target if available
        if cmake_has_target(cmake, build_dir, "installer_stage"):
            run([str(cmake), "--build", str(build_dir), "--target", "installer_stage", "--config", config], env=env)
        else:
            # Manual staging
            stage_dir = PROJECT_ROOT / "installer" / "packages" / "im.cheng.oai.desktop" / "data"
            if stage_dir.exists():
                shutil.rmtree(stage_dir)
            stage_dir.mkdir(parents=True, exist_ok=True)
            run([str(cmake), "--install", str(build_dir), "--prefix", str(stage_dir), "--config", config], env=env)
            # Flatten bin/
            bin_dir = stage_dir / "bin"
            if bin_dir.exists():
                for f in bin_dir.iterdir():
                    dest = stage_dir / f.name
                    if f.is_dir():
                        shutil.copytree(f, dest, dirs_exist_ok=True)
                    else:
                        shutil.copy2(f, dest)
                shutil.rmtree(bin_dir)
            # Clean unrelated artifacts
            for unwanted in ("include", "lib"):
                p = stage_dir / unwanted
                if p.exists():
                    shutil.rmtree(p)
            # Copy template ini
            tpl = PROJECT_ROOT / "installer" / "oai.ini.template"
            if tpl.exists():
                shutil.copy2(tpl, stage_dir / "Oai.ini")

        installer_out = build_dir / f"OaiInstaller-{version}.exe"
        config_xml = PROJECT_ROOT / "installer" / "config.xml"
        if not config_xml.exists():
            print(f"ERROR: {config_xml} not found. Run cmake configure with binarycreator in PATH.")
            sys.exit(1)
        run([
            str(binarycreator),
            "--offline-only",
            "-c", str(config_xml),
            "-p", str(PROJECT_ROOT / "installer" / "packages"),
            str(installer_out),
        ], env=env)

    # Report output
    candidates = list(build_dir.glob(f"OaiInstaller-{version}*"))
    if candidates:
        print(f"\n[SUCCESS] Installer created: {candidates[0]}")
    else:
        print(f"\n[WARNING] Could not locate installer in {build_dir}")


def package_macos(build_dir, version, qt_prefix):
    print("\n[macOS] Packaging DMG...")

    app_bundle = build_dir / "Oai.app"
    if not app_bundle.exists():
        print(f"ERROR: {app_bundle} not found. Build may have failed.")
        sys.exit(1)

    macdeployqt = find_macdeployqt(qt_prefix)
    if macdeployqt:
        print(f"  [OK] macdeployqt: {macdeployqt}")
        run([str(macdeployqt), str(app_bundle)])
    else:
        print("  [WARNING] macdeployqt not found – relying on cmake install deploy script.")

    # Ensure packs are inside the bundle (CMake target_sources usually does this,
    # but CI also copies explicitly as a safeguard)
    packs_src = build_dir / "packs"
    packs_dst = app_bundle / "Contents" / "Resources" / "packs"
    if packs_src.exists():
        packs_dst.mkdir(parents=True, exist_ok=True)
        for opk in packs_src.glob("*.opk"):
            dst = packs_dst / opk.name
            if not dst.exists():
                shutil.copy2(opk, dst)
                print(f"  Copied pack: {opk.name}")

    # Remove unused QML frameworks and plugins that macdeployqt bundles but
    # which have malformed Mach-O headers, causing LaunchServices error 153.
    for unwanted in [
        app_bundle / "Contents" / "PlugIns" / "platforminputcontexts",
        *app_bundle.glob("Contents/Frameworks/QtQml*.framework"),
        app_bundle / "Contents" / "Frameworks" / "QtQmlWorkerScript.framework",
        app_bundle / "Contents" / "Frameworks" / "QtQuick.framework",
    ]:
        if unwanted.exists():
            shutil.rmtree(unwanted)
            print(f"  Removed unused: {unwanted.name}")

    # Ad-hoc sign bottom-up (allows running on modern macOS without notarization).
    # --deep alone is unreliable; sign leaf binaries explicitly first.
    codesign = shutil.which("codesign")
    if codesign:
        import glob as _glob
        # Sign frameworks and dylibs
        for pattern in [
            str(app_bundle / "Contents" / "Frameworks" / "**" / "*.dylib"),
            str(app_bundle / "Contents" / "Frameworks" / "**" / "Versions" / "A" / "*"),
            str(app_bundle / "Contents" / "PlugIns" / "**" / "*.dylib"),
        ]:
            for f in _glob.glob(pattern, recursive=True):
                if Path(f).is_file():
                    run([codesign, "--force", "--sign", "-", f], check=False)
        # Sign main binary then bundle
        run([codesign, "--force", "--sign", "-", str(app_bundle / "Contents" / "MacOS" / "Oai")], check=False)
        run([codesign, "--deep", "--force", "--sign", "-", str(app_bundle)], check=False)

    xattr = shutil.which("xattr")
    if xattr:
        run([xattr, "-cr", str(app_bundle)], check=False)

    # Create DMG with Applications symlink for drag-to-install
    dmg_staging = build_dir / "_dmg_staging"
    if dmg_staging.exists():
        shutil.rmtree(dmg_staging)
    dmg_staging.mkdir()
    shutil.copytree(app_bundle, dmg_staging / "Oai.app", symlinks=True)
    (dmg_staging / "Applications").symlink_to("/Applications")

    dmg_path = build_dir / f"Oai-{version}.dmg"
    run([
        "hdiutil", "create",
        "-volname", "Oai",
        "-srcfolder", str(dmg_staging),
        "-ov",
        "-format", "UDZO",
        str(dmg_path),
    ])
    shutil.rmtree(dmg_staging)
    print(f"\n[SUCCESS] DMG created: {dmg_path}")


def package_linux(build_dir, version, cmake, config="Release"):
    print("\n[Linux] Packaging AppImage...")

    appimagetool = find_appimagetool()
    linuxdeploy = find_linuxdeploy()

    if not appimagetool and not linuxdeploy:
        print(
            "ERROR: Neither appimagetool nor linuxdeploy found in PATH.\n"
            "Install appimagetool from https://github.com/AppImage/AppImageKit/releases\n"
            "or linuxdeploy from https://github.com/linuxdeploy/linuxdeploy/releases"
        )
        sys.exit(1)

    appdir = build_dir / "AppDir"
    if appdir.exists():
        shutil.rmtree(appdir)
    appdir.mkdir(parents=True, exist_ok=True)

    usr = appdir / "usr"
    print("  Installing to AppDir/usr...")
    run([str(cmake), "--install", str(build_dir), "--prefix", str(usr), "--config", config])

    # Create .desktop entry
    desktop_content = (
        "[Desktop Entry]\n"
        "Name=Oai\n"
        "Comment=A native Qt6/C++ desktop pet\n"
        "Exec=Oai\n"
        "Icon=oai\n"
        "Type=Application\n"
        "Categories=Utility;Qt;\n"
        "Terminal=false\n"
    )
    desktop_file = appdir / "oai.desktop"
    desktop_file.write_text(desktop_content, encoding="utf-8")
    # Also install into FHS location
    apps_dir = usr / "share" / "applications"
    apps_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(desktop_file, apps_dir / "oai.desktop")

    # Install icon
    icon_src = PROJECT_ROOT / "assets" / "icons" / "oai.png"
    if icon_src.exists():
        # AppDir root (appimagetool looks here)
        shutil.copy2(icon_src, appdir / "oai.png")
        # FHS location
        icons_dir = usr / "share" / "icons" / "hicolor" / "256x256" / "apps"
        icons_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(icon_src, icons_dir / "oai.png")
    else:
        print("  [WARNING] Icon not found at assets/icons/oai.png")

    # AppRun symlink — cmake install always lands the binary at usr/bin/Oai
    # via install(TARGETS Oai RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}).
    exe = usr / "bin" / "Oai"
    if not exe.exists():
        print(f"ERROR: Executable not found at {exe} after cmake install.")
        sys.exit(1)
    apprun = appdir / "AppRun"
    os.symlink("usr/bin/Oai", apprun)

    arch = platform.machine().replace("_", "-")
    appimage_name = f"Oai-{version}-{arch}.AppImage"
    appimage_path = build_dir / appimage_name

    if appimagetool:
        print(f"  [OK] appimagetool: {appimagetool}")
        run([appimagetool, str(appdir), str(appimage_path)])
    elif linuxdeploy:
        print(f"  [OK] linuxdeploy: {linuxdeploy}")
        # linuxdeploy picks the output filename itself based on .desktop
        # metadata. Snapshot the existing *.AppImage set so we can identify
        # the new one without racing on mtime against stale files from
        # earlier runs.
        before = set(build_dir.glob("*.AppImage"))
        ld_cmd = [
            linuxdeploy,
            "--appdir", str(appdir),
            "--desktop-file", str(desktop_file),
            "--output", "appimage",
        ]
        if icon_src.exists():
            ld_cmd.extend(["--icon-file", str(icon_src)])
        run(ld_cmd, cwd=build_dir)
        new_files = [p for p in build_dir.glob("*.AppImage") if p not in before]
        if new_files:
            new_files[0].rename(appimage_path)
        elif appimage_path.exists():
            pass  # linuxdeploy happened to write our exact name
        else:
            print(f"\n[WARNING] linuxdeploy produced no new *.AppImage in {build_dir}")

    print(f"\n[SUCCESS] AppImage created: {appimage_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def find_compiler_bin_for_qt(qt_prefix):
    """
    On Windows with MinGW/LLVM-MinGW Qt, find the matching compiler bin
    directory under Qt Tools so we can add it to PATH.
    """
    if not qt_prefix or sys.platform != "win32":
        return None
    qt_root = qt_prefix
    for _ in range(2):
        qt_root = qt_root.parent
    tools = qt_root / "Tools"
    if not tools.exists():
        return None

    qt_name = qt_prefix.name.lower()
    if "llvm-mingw" in qt_name:
        pattern = "llvm-mingw*"
    elif "mingw" in qt_name:
        pattern = "mingw*"
    elif "msvc" in qt_name:
        # MSVC compiler should come from Developer Command Prompt
        return None
    else:
        return None

    matches = sorted(tools.glob(pattern), key=lambda d: d.name, reverse=True)
    for m in matches:
        gpp = m / "bin" / "g++.exe"
        gcc = m / "bin" / "gcc.exe"
        if gpp.exists() or gcc.exists():
            return m / "bin"
    return None


def main():
    parser = argparse.ArgumentParser(description="Build and package Oai for the current platform.")
    parser.add_argument("--build-dir", default="build", help="CMake build directory (default: build)")
    parser.add_argument("--config", default="Release", help="CMake build configuration (default: Release)")
    parser.add_argument("--qt-dir", default=None, help="Override Qt installation directory")
    parser.add_argument("--skip-build", action="store_true", help="Skip build step (use existing binaries)")
    parser.add_argument("--skip-package", action="store_true", help="Only build, skip packaging")
    args = parser.parse_args()

    build_dir = PROJECT_ROOT / args.build_dir
    version = get_project_version()
    print(f"Project version: {version}")
    print(f"Build directory: {build_dir}")
    print(f"Platform: {sys.platform}")

    # ------------------------------------------------------------------
    # Detect Qt
    # ------------------------------------------------------------------
    print("\n[Detect] Qt environment...")
    cached_qt = get_cached_qt_prefix(build_dir) if not args.qt_dir else None
    qt_prefix = find_qt_prefix(cached_qt) if not args.qt_dir else Path(args.qt_dir)
    if args.qt_dir:
        print(f"  [OK] Qt prefix from --qt-dir: {qt_prefix}")
    elif qt_prefix:
        print(f"  Qt prefix: {qt_prefix}")
    else:
        print("  [WARNING] Could not auto-detect Qt prefix. Relying on cmake to find Qt.")

    # ------------------------------------------------------------------
    # Detect essential tools
    # ------------------------------------------------------------------
    print("\n[Detect] Essential tools...")
    cmake = find_cmake(qt_prefix)
    if cmake:
        print(f"  [OK] cmake: {cmake}")
    else:
        print("  [MISSING] cmake not found in PATH or Qt Tools (install from https://cmake.org/download/)")
        sys.exit(1)

    # Prefer Ninja, fallback to Make
    ninja = find_ninja(qt_prefix)
    make = shutil.which("make")
    if ninja:
        print(f"  [OK] ninja: {ninja}")
    elif make:
        print(f"  [OK] make: {make}")
    else:
        if sys.platform != "win32":
            print("  [WARNING] Neither ninja nor make found. CMake will pick a generator automatically.")

    # ------------------------------------------------------------------
    # Configure
    # ------------------------------------------------------------------
    print("\n[Build] Configuring...")
    configure_cmd = [
        str(cmake), "-B", str(build_dir), "-S", str(PROJECT_ROOT),
        f"-DCMAKE_BUILD_TYPE={args.config}",
    ]
    if ninja:
        configure_cmd.extend(["-GNinja", f"-DCMAKE_MAKE_PROGRAM={ninja}"])
    if qt_prefix:
        configure_cmd.append(f"-DCMAKE_PREFIX_PATH={qt_prefix}")

    # On Linux set a relocatable RPATH so the AppImage finds bundled libraries
    if sys.platform.startswith("linux"):
        configure_cmd.append("-DCMAKE_INSTALL_RPATH=$ORIGIN/../lib")

    # On Windows, ensure QtIFW binarycreator directory is on PATH so CMake finds it
    env = os.environ.copy()
    if sys.platform == "win32":
        bc = find_binarycreator(qt_prefix)
        if bc:
            env["PATH"] = str(bc.parent) + os.pathsep + env.get("PATH", "")
            print(f"  Prepending QtIFW to PATH: {bc.parent}")

        # For MinGW/LLVM-MinGW Qt, the matching compiler must be on PATH
        compiler_bin = find_compiler_bin_for_qt(qt_prefix)
        if compiler_bin:
            env["PATH"] = str(compiler_bin) + os.pathsep + env.get("PATH", "")
            print(f"  Prepending compiler to PATH: {compiler_bin}")
        else:
            # Warn if we detect MinGW Qt but no compiler
            if qt_prefix and ("mingw" in qt_prefix.name.lower() or "llvm-mingw" in qt_prefix.name.lower()):
                print("  [WARNING] MinGW/LLVM-MinGW Qt detected but no matching compiler found in Qt Tools.")
                print("            Make sure you open the correct Qt shell or add the compiler to PATH.")

    run(configure_cmd, env=env)

    # ------------------------------------------------------------------
    # Build
    # ------------------------------------------------------------------
    if not args.skip_build:
        print(f"\n[Build] Building ({args.config})...")
        build_cmd = [str(cmake), "--build", str(build_dir), "--config", args.config, "--parallel"]
        run(build_cmd, env=env)
    else:
        print("\n[Build] Skipped (--skip-build)")

    if args.skip_package:
        print("\n[Package] Skipped (--skip-package)")
        return

    # ------------------------------------------------------------------
    # Package
    # ------------------------------------------------------------------
    if sys.platform == "win32":
        package_windows(build_dir, version, qt_prefix, cmake, args.config)
    elif sys.platform == "darwin":
        package_macos(build_dir, version, qt_prefix)
    elif sys.platform.startswith("linux"):
        package_linux(build_dir, version, cmake, args.config)
    else:
        print(f"ERROR: Unsupported platform: {sys.platform}")
        sys.exit(1)


if __name__ == "__main__":
    main()
