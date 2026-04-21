#!/usr/bin/env python3
"""
Fix Lottie effect JSON files for rlottie compatibility.

rlottie requires 'i' (in-tangent) field on keyframes to keep them.
Keyframes without 'i' are discarded, which can leave empty frame arrays
and cause segfaults during rendering.

This script adds standard linear interpolation 'i' and 'o' fields to
any keyframe that has an 's' (start value) field.
"""

import json
import sys
from pathlib import Path

LINEAR_INTERPOLATOR = {"x": 0.167, "y": 0.167}


def fix_keyframes(obj):
    """Recursively fix keyframes in a Lottie JSON object."""
    if isinstance(obj, dict):
        # Check if this is an animated property with keyframes
        if obj.get("a") == 1 and "k" in obj:
            k = obj["k"]
            if isinstance(k, list):
                for frame in k:
                    if isinstance(frame, dict) and "s" in frame and "i" not in frame:
                        frame["i"] = dict(LINEAR_INTERPOLATOR)
                        if "o" not in frame:
                            frame["o"] = dict(LINEAR_INTERPOLATOR)
        # Recurse
        for key, val in obj.items():
            fix_keyframes(val)
    elif isinstance(obj, list):
        for item in obj:
            fix_keyframes(item)


def main():
    effects_dir = Path(__file__).parent.parent / "assets" / "lottie" / "effects"
    if not effects_dir.exists():
        print(f"Effects directory not found: {effects_dir}")
        sys.exit(1)

    for json_file in sorted(effects_dir.glob("*.json")):
        print(f"Processing {json_file.name}...")
        with open(json_file, "r") as f:
            data = json.load(f)

        fix_keyframes(data)

        with open(json_file, "w") as f:
            json.dump(data, f, indent=2)
            f.write("\n")

    print("Done!")


if __name__ == "__main__":
    main()
