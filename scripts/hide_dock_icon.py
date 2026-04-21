#!/usr/bin/env python3
"""Add LSUIElement to Info.plist to hide the app from the macOS Dock."""
import os
import plistlib
import sys

plist_path = sys.argv[1]

# Make writable if needed
mode = os.stat(plist_path).st_mode
os.chmod(plist_path, mode | 0o200)

with open(plist_path, "rb") as f:
    plist = plistlib.load(f)

plist["LSUIElement"] = True

with open(plist_path, "wb") as f:
    plistlib.dump(plist, f)

print("LSUIElement added to Info.plist — Dock icon hidden.")
