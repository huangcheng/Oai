#!/usr/bin/env python3
"""
Import a ClippyJS agent into an Oai sprite pack.

Downloads map.png and agent.js from clippyjs/clippy.js GitHub repo,
converts the agent.js animation data to Oai manifest.json format,
and writes the pack to assets/packs/<name>/.

Usage:
    python3 scripts/import_clippyjs_agent.py Merlin
    python3 scripts/import_clippyjs_agent.py --all
"""

import json
import os
import re
import sys
import urllib.request

REPO = "clippyjs/clippy.js"
BRANCH = "master"
BASE_URL = f"https://raw.githubusercontent.com/{REPO}/{BRANCH}/agents"
PACKS_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "assets", "packs")

CHARACTERS = ["Bonzi", "F1", "Genie", "Genius", "Links", "Merlin", "Peedy", "Rocky", "Rover"]

CHAR_DESCRIPTIONS = {
    "Bonzi":  "The friendly purple gorilla from BonziBuddy",
    "F1":     "The futuristic robot assistant",
    "Genie":  "The magical genie from a lamp",
    "Genius": "The Einstein-style scientist assistant",
    "Links":  "The playful cat from Office",
    "Merlin": "The classic wizard from Office 97",
    "Peedy":  "The colorful parrot assistant",
    "Rocky":  "The energetic dog with attitude",
    "Rover":  "The search dog from Windows XP",
}

# Canonical event → animation name candidates (first match wins)
EVENT_MAP_CANDIDATES = {
    "session.start":        ["Greeting", "Greet", "Wave", "Show"],
    "session.end":          ["RestPose", "Idle1_1", "Idle", "GoodBye", "Goodbye"],
    "session.idle":         ["RestPose", "Idle1_1", "Idle"],
    "session.error":        ["Alert", "Surprised", "Sad", "Decline"],
    "prompt.submitted":     ["Thinking", "Think"],
    "tool.before":          ["Explain", "Processing", "Process", "Searching", "Search"],
    "tool.after":           ["Congratulate", "Pleased", "Acknowledge"],
    "tool.failed":          ["Alert", "Sad", "Surprised", "Decline"],
    "permission.requested": ["GetAttention", "Suggest", "Announce"],
    "permission.denied":    ["Alert", "Decline", "Sad", "Surprised"],
    "permission.response":  ["RestPose", "Idle1_1", "Idle"],
    "subagent.started":     ["Explain", "Processing", "Process", "Searching"],
    "subagent.stopped":     ["RestPose", "Idle1_1", "Idle"],
    "notification.sent":    ["GetAttention", "Announce", "Suggest", "Wave"],
    "file.edited":          ["Writing", "Write", "SendMail", "Explain"],
    "file.watched":         ["RestPose", "Idle1_1", "Idle"],
    "todo.updated":         ["Congratulate", "Pleased", "Acknowledge", "CharacterSucceeds"],
}


def download(url):
    """Download a URL and return bytes."""
    req = urllib.request.Request(url, headers={"User-Agent": "oai-importer/1.0"})
    with urllib.request.urlopen(req) as resp:
        return resp.read()


def parse_agent_js(data):
    """Parse clippy.js agent.js callback format into a dict."""
    text = data.decode("utf-8")
    # Strip: clippy.ready('Name', { ... });
    match = re.search(r"clippy\.ready\s*\(\s*['\"](\w+)['\"]\s*,\s*(\{.*\})\s*\)\s*;?\s*$", text, re.DOTALL)
    if not match:
        raise ValueError("Could not parse agent.js format")
    return json.loads(match.group(2))


def convert_animation(anim_data, frame_w, frame_h):
    """Convert a clippy.js animation to Oai frame definitions."""
    frames = []
    for frame in anim_data.get("frames", []):
        duration = frame.get("duration", 0)
        if duration <= 0:
            continue  # Skip sentinel frames

        images = frame.get("images", [])
        if not images or not images[0]:
            continue

        # Use first overlay layer only
        x, y = images[0][0], images[0][1]
        frames.append({
            "x": x,
            "y": y,
            "w": frame_w,
            "h": frame_h,
            "duration": duration,
        })

    return frames


def build_event_map(anim_names):
    """Build eventMap by matching canonical events to available animations."""
    name_set = set(anim_names)
    event_map = {}
    for event, candidates in EVENT_MAP_CANDIDATES.items():
        for candidate in candidates:
            if candidate in name_set:
                event_map[event] = candidate
                break
    return event_map


def build_idle_pool(anim_names):
    """Build idle pool from available animations."""
    pool = []
    for name in sorted(anim_names):
        if name.startswith("Idle"):
            pool.append({"name": name, "weight": 3})
        elif name.startswith("Look") and not name.endswith("Return") and not name.endswith("Blink"):
            pool.append({"name": name, "weight": 2})
    return pool


def import_character(name):
    """Download and convert a single character."""
    print(f"Importing {name}...")

    # Download assets
    agent_js = download(f"{BASE_URL}/{name}/agent.js")
    map_png = download(f"{BASE_URL}/{name}/map.png")

    # Parse agent data
    agent = parse_agent_js(agent_js)
    frame_w, frame_h = agent["framesize"]
    animations_raw = agent.get("animations", {})

    print(f"  Frame size: {frame_w}x{frame_h}")
    print(f"  Animations: {len(animations_raw)}")

    # Convert animations
    animations = {}
    for anim_name, anim_data in animations_raw.items():
        frames = convert_animation(anim_data, frame_w, frame_h)
        if not frames:
            continue
        animations[anim_name] = {
            "type": "sprite",
            "frames": frames,
        }

    print(f"  Converted: {len(animations)} animations")

    # Build event map and idle pool
    anim_names = list(animations.keys())
    event_map = build_event_map(anim_names)
    idle_pool = build_idle_pool(anim_names)

    # Build manifest
    manifest = {
        "formatVersion": "1.0.0",
        "id": f"com.oaipet.{name.lower()}",
        "name": name,
        "author": "Microsoft / ClippyJS (MIT)",
        "version": "1.0.0",
        "description": CHAR_DESCRIPTIONS.get(name, f"The {name} Office Assistant"),
        "preview": "preview.png",
        "tags": ["retro", "microsoft", "office-assistant", name.lower()],
        "license": "MIT",
        "minAppVersion": "1.0.0",
        "character": {
            "type": "spriteSheet",
            "spriteSheet": "sprites/map.png",
            "frameWidth": frame_w,
            "frameHeight": frame_h,
            "animations": animations,
            "idlePool": idle_pool,
            "eventMap": event_map,
        },
        "sounds": {
            "directory": "sounds"
        }
    }

    # Write pack directory
    pack_dir = os.path.join(PACKS_DIR, name.lower())
    sprites_dir = os.path.join(pack_dir, "sprites")
    os.makedirs(sprites_dir, exist_ok=True)

    # Write map.png
    map_path = os.path.join(sprites_dir, "map.png")
    with open(map_path, "wb") as f:
        f.write(map_png)
    print(f"  Wrote {map_path}")

    # Write manifest.json
    manifest_path = os.path.join(pack_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"  Wrote {manifest_path}")

    print(f"  Event map: {len(event_map)} events mapped")
    print(f"  Idle pool: {len(idle_pool)} animations")
    print(f"  Done: {name}")
    return pack_dir


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <CharacterName|--all>")
        print(f"Available: {', '.join(CHARACTERS)}")
        sys.exit(1)

    if sys.argv[1] == "--all":
        names = CHARACTERS
    else:
        names = sys.argv[1:]

    for name in names:
        if name not in CHARACTERS:
            print(f"Unknown character: {name}")
            print(f"Available: {', '.join(CHARACTERS)}")
            sys.exit(1)

    for name in names:
        try:
            import_character(name)
        except Exception as e:
            print(f"  ERROR importing {name}: {e}")
            sys.exit(1)

    print(f"\nImported {len(names)} character(s) successfully.")


if __name__ == "__main__":
    main()
