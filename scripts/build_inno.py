#!/usr/bin/env python3
"""
Build the Seelie Windows installer with Inno Setup.

Reuses the existing QtIFW staging dir (installer/packages/im.cheng.seelie.desktop/data)
as the source tree — populated by `cmake --build build --target installer_stage`.
That keeps the C++/Qt/pack-staging logic in one place. We just package what's there.

Usage:
    python scripts/build_inno.py
    python scripts/build_inno.py --build-dir build --include-nsfw
    python scripts/build_inno.py --skip-stage    # reuse existing staging

Output: build/SeelieSetup-<version>.exe
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
ISS = PROJECT_ROOT / "installer" / "inno" / "seelie.iss"
# Staging dir: where windeployqt + the cmake install rules drop a flat
# tree of Seelie.exe + Qt DLLs + assets/ + packs/. ISCC reads from here.
# Lives under build/ because it's gitignored generated output, regenerated
# every run by the inno_stage CMake target.
def stage_dir(build_dir: Path) -> Path:
    return build_dir / "staging"


def run(cmd, **kwargs):
    """Run a subprocess. Defaults to shell=False with a list[str] argv.

    Note: when invoking ISCC.exe with /D defines from msys2/git-bash we hit
    argv-translation quirks — even though list2cmdline produces an identical
    string to the working cmd.exe invocation, CreateProcess receives subtly
    different bytes and ISCC misparses. The Inno-invocation code path below
    sets shell=True explicitly to dodge that.
    """
    print("  ->", " ".join(str(c) for c in cmd) if isinstance(cmd, list) else cmd)
    kwargs.setdefault("check", True)
    return subprocess.run(cmd, **kwargs)


def find_iscc() -> Path:
    """Locate Inno Setup's compiler. winget installs to Program Files (x86)."""
    env = os.environ.get("ISCC_PATH")
    if env and Path(env).exists():
        return Path(env)
    candidates = [
        Path(r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe"),
        Path(r"C:\Program Files\Inno Setup 6\ISCC.exe"),
        # winget often installs user-scope to %LOCALAPPDATA%\Programs.
        Path.home() / "AppData" / "Local" / "Programs" / "Inno Setup 6" / "ISCC.exe",
    ]
    for c in candidates:
        if c.exists():
            return c
    on_path = shutil.which("ISCC.exe") or shutil.which("iscc")
    if on_path:
        return Path(on_path)
    sys.exit(
        "ERROR: ISCC.exe (Inno Setup compiler) not found.\n"
        "Install with: winget install JRSoftware.InnoSetup\n"
        "Or set ISCC_PATH environment variable."
    )


def get_project_version() -> str:
    cmake = PROJECT_ROOT / "CMakeLists.txt"
    text = cmake.read_text(encoding="utf-8")
    m = re.search(r"project\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", text)
    return m.group(1) if m else "0.0.0"


def find_cmake() -> Path:
    """Reuse the existing build_release.py detection to stay consistent."""
    on_path = shutil.which("cmake")
    if on_path:
        return Path(on_path)
    candidates = [
        Path(r"C:\Qt\Tools\CMake_64\bin\cmake.exe"),
        Path(r"C:\Program Files\CMake\bin\cmake.exe"),
    ]
    for c in candidates:
        if c.exists():
            return c
    sys.exit("ERROR: cmake not found in PATH or Qt Tools.")


def ensure_staged(build_dir: Path, include_nsfw: bool) -> None:
    """Run inno_stage so build/staging/ is populated with the bundled payload."""
    if not (build_dir / "CMakeCache.txt").exists():
        sys.exit(
            f"ERROR: {build_dir} hasn't been configured yet.\n"
            "Run `python scripts/build_release.py` first to do the full Qt build,\n"
            "or run cmake configure manually."
        )
    cmake = find_cmake()
    print(f"\n[Stage] Populating {stage_dir(build_dir).relative_to(PROJECT_ROOT)} ...")
    # Always pin SEELIE_INCLUDE_NSFW so a prior --include-nsfw run doesn't leak
    # into a SFW package, mirroring build_release.py's same-day fix.
    nsfw = "ON" if include_nsfw else "OFF"
    run([str(cmake), "-B", str(build_dir), "-S", str(PROJECT_ROOT),
         f"-DSEELIE_INCLUDE_NSFW={nsfw}"])
    run([str(cmake), "--build", str(build_dir),
         "--target", "inno_stage", "--config", "Release"])


def main():
    parser = argparse.ArgumentParser(description="Build Seelie Windows installer with Inno Setup.")
    parser.add_argument("--build-dir", default="build", help="CMake build dir (default: build)")
    parser.add_argument("--include-nsfw", action="store_true",
                        help="Bundle NSFW pack categories. Default: store-safe lineup only.")
    parser.add_argument("--skip-stage", action="store_true",
                        help="Skip cmake inno_stage; use existing staging dir as-is.")
    args = parser.parse_args()

    build_dir = PROJECT_ROOT / args.build_dir
    staging = stage_dir(build_dir)
    version = get_project_version()
    print(f"Seelie Inno Setup builder")
    print(f"  version:   {version}")
    print(f"  build dir: {build_dir}")
    print(f"  staging:   {staging}")
    print(f"  iss:       {ISS}")

    if not args.skip_stage:
        ensure_staged(build_dir, args.include_nsfw)
    elif not (staging / "Seelie.exe").exists():
        sys.exit(f"ERROR: --skip-stage but {staging / 'Seelie.exe'} doesn't exist.")

    iscc = find_iscc()
    print(f"\n[Compile] {iscc}")
    # shell=True with a quoted command string sidesteps a msys2/git-bash
    # argv-translation issue that causes ISCC to misparse multiple /D defines
    # when invoked through subprocess.run(list, shell=False). The same string
    # works correctly via cmd.exe directly.
    cmd = (
        f'"{iscc}" '
        f'/DSrcDir="{staging}" '
        f'/DProjectRoot="{PROJECT_ROOT}" '
        f'/DAppVersion={version} '
        f'"{ISS}"'
    )
    run(cmd, shell=True)

    out = build_dir / f"SeelieSetup-{version}.exe"
    if out.exists():
        size_mb = out.stat().st_size / 1024 / 1024
        print(f"\n[SUCCESS] {out}  ({size_mb:.1f} MB)")
    else:
        print(f"\n[WARN] expected output {out} not found — check ISCC log above.")
        sys.exit(1)


if __name__ == "__main__":
    main()
