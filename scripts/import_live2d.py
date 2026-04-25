#!/usr/bin/env python3
"""Import Live2D character packs from github.com/Eikanya/Live2d-model.

Each pack lands in assets/packs/<local_id>/ as a directory tree with a
manifest.json that follows schemas/character-pack-v1.schema.json. The script
also patches model3.json on the way in: upstream Azur Lane models put every
motion into a single anonymous group "" instead of named groups, so we
classify motions by filename and emit Idle / Tap groups that match the
manifest's eventMap defaults.

Usage:
    python scripts/import_live2d.py                      # imports all picks
    python scripts/import_live2d.py z23 abercrombie       # imports a subset
    python scripts/import_live2d.py --list                # shows known picks
"""

import argparse
import json
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

REPO = "Eikanya/Live2d-model"
RAW_BASE = f"https://raw.githubusercontent.com/{REPO}/master"

# Filename prefixes that mark a motion as ambient/idle. Everything else is Tap.
IDLE_PREFIXES = ("idle", "home", "main_", "stand", "wait")


def gh_list(upstream_path: str, *, retries: int = 4):
    """List directory entries via the gh contents API. Retries on transient
    network failures because raw.githubusercontent.com / api.github.com from
    behind some Chinese ISPs flakes routinely."""
    import time
    last_exc = None
    for attempt in range(retries):
        try:
            out = subprocess.check_output(
                ["gh", "api", f"repos/{REPO}/contents/{upstream_path}"],
                text=True, encoding="utf-8",
            )
            return [(it["name"], it["type"]) for it in json.loads(out)]
        except subprocess.CalledProcessError as exc:
            last_exc = exc
            time.sleep(1.5 * (attempt + 1))
    raise last_exc


def walk_files(upstream_path: str):
    """Recursively yield file paths (full upstream paths) under upstream_path."""
    for name, ftype in gh_list(upstream_path):
        sub = f"{upstream_path}/{name}"
        if ftype == "file":
            yield sub
        elif ftype == "dir":
            yield from walk_files(sub)


def download_file(upstream_path: str, dst: Path, *, retries: int = 4) -> None:
    import time
    url = f"{RAW_BASE}/{urllib.parse.quote(upstream_path, safe='/')}"
    dst.parent.mkdir(parents=True, exist_ok=True)
    last_exc = None
    for attempt in range(retries):
        try:
            urllib.request.urlretrieve(url, dst)
            return
        except Exception as exc:                 # noqa: BLE001
            last_exc = exc
            time.sleep(1.5 * (attempt + 1))
    raise last_exc


def patch_model3(model3_path: Path) -> None:
    """Normalize a model3.json's motion groups into Idle / Tap so the runtime
    eventMap (which references those names) actually finds something to play.

    Three upstream layouts need flattening:

    1) **Single anonymous group `""`** (z23, abercrombie). Cubism allows
       motions with no group name; we split that bag into Idle / Tap based on
       filename.

    2) **Many per-file groups** (adalbert, albion). Each motion sits in its
       own group named after the file (`complete`, `effect`, `home`, `idle`,
       `idle1`...). The model still loads — Cubism's group lookup is
       case-sensitive though, so events that ask for `Idle`/`Tap` either fall
       back to the alphabetically first group or play nothing. We collapse
       those groups into Idle / Tap by the same filename heuristic so the
       eventMap behaves predictably.

    3) **No `Motions` key at all** (helena). The model3.json declares no
       motions even though `motions/` on disk has files. We scan the dir
       and build Idle / Tap from the filenames.

    A `Motions` block that already has named, multi-entry groups (e.g.
    Cubism Free Sample's `Tap` + `Idle`) is left alone."""
    data = json.loads(model3_path.read_text(encoding="utf-8"))
    refs = data.setdefault("FileReferences", {})
    motions = refs.get("Motions", {})

    keys = list(motions.keys())
    is_single_anon = (keys == [""])
    is_per_file = (
        len(motions) > 4
        and "Idle" not in motions and "Tap" not in motions
        and all(len(v) <= 2 for v in motions.values())
    )
    # Case 3: no Motions block but motion files exist on disk
    is_missing_block = (not motions)
    motions_dir = model3_path.parent / "motions"
    disk_motions = sorted(motions_dir.glob("*.motion3.json")) if motions_dir.is_dir() else []
    if is_missing_block and not disk_motions:
        return  # genuinely no motions; nothing to do
    if not (is_single_anon or is_per_file or is_missing_block):
        return  # already has named multi-entry groups; leave alone

    if is_missing_block:
        flat = [
            {"File": f"motions/{m.name}", "FadeInTime": 0.5, "FadeOutTime": 0.5}
            for m in disk_motions
        ]
    else:
        flat = []
        for entries in motions.values():
            flat.extend(entries)

    seen = set()
    idle, tap = [], []
    for entry in flat:
        f = entry.get("File", "")
        if not f or f in seen:
            continue
        seen.add(f)
        stem = Path(f).stem.lower()
        bucket = idle if any(stem.startswith(p) for p in IDLE_PREFIXES) else tap
        bucket.append({"File": f, "FadeInTime": 0.5, "FadeOutTime": 0.5})

    new_groups = {}
    if idle:
        new_groups["Idle"] = idle
    if tap:
        new_groups["Tap"] = tap
    if not new_groups:
        return

    refs["Motions"] = new_groups
    model3_path.write_text(
        json.dumps(data, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def sanitize_motion(motion_path: Path) -> None:
    """Normalize a motion3.json so Cubism's LoadMotion can ingest it without
    heap corruption.

    Two distinct upstream defects need patching:

    1) **UserData mismatch**: upstream files declare `UserDataCount >= 1` but
       `TotalUserDataSize == 0`. Cubism allocates a zero-byte user-data
       buffer based on TotalUserDataSize, then iterates UserDataCount entries
       into it — heap overrun on load. UserData entries hold empty `Value`
       strings anyway (no real payload), so we strip the block and zero the
       counters.

    2) **Wrong TotalSegmentCount / TotalPointCount**: Azur Lane's exporter
       writes incorrect counts in Meta (e.g. z23.main_3 declares 326 points
       but the actual curves contain 372). Cubism uses these counts to
       size buffers up-front, then writes the real number of points →
       heap corruption on the first motion of the first crashing pack.
       We recount from the curves data and overwrite the Meta values."""
    data = json.loads(motion_path.read_text(encoding="utf-8"))
    meta = data.get("Meta", {})
    changed = False

    # (1) UserData
    if data.get("UserData") or meta.get("UserDataCount", 0) != 0:
        if meta.get("TotalUserDataSize", 0) == 0:
            data.pop("UserData", None)
            meta["UserDataCount"] = 0
            meta["TotalUserDataSize"] = 0
            changed = True

    # (2) Recount segments and points from the curves themselves.
    # Cubism segment encoding (after the initial [t0, v0] point):
    #   type 1 (Bezier): 7 floats including type, 3 endpoint points
    #   types 0/2/3 (Linear/Stepped/InverseStepped): 3 floats, 1 endpoint
    total_segs = 0
    total_pts = 0
    for curve in data.get("Curves", []):
        segs = curve.get("Segments", [])
        if len(segs) < 2:
            continue
        i = 2
        total_pts += 1   # initial point
        while i < len(segs):
            t = int(segs[i])
            if t == 1:   # Bezier
                i += 7
                total_pts += 3
            else:        # Linear / Stepped / InverseStepped
                i += 3
                total_pts += 1
            total_segs += 1
    if meta.get("TotalSegmentCount") != total_segs:
        meta["TotalSegmentCount"] = total_segs
        changed = True
    if meta.get("TotalPointCount") != total_pts:
        meta["TotalPointCount"] = total_pts
        changed = True

    if changed:
        motion_path.write_text(
            json.dumps(data, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )


def write_manifest(pack_dir: Path, local_id: str, name_en: str, name_zh: str,
                   model3_rel: str, category: str = "azur_lane",
                   author: str = f"Imported from github.com/{REPO}") -> None:
    manifest = {
        "formatVersion": "1.0.0",
        "id": f"im.cheng.oai.{local_id}",
        "name": name_en,
        "nameLocalized": {"zh_CN": name_zh} if name_zh and name_zh != name_en else {},
        "author": author,
        "version": "1.0.0",
        "description": (
            f"{name_en} — Live2D character imported from upstream collection. "
            "Asset rights belong to the original game studio."
        ),
        "preview": "preview.png",
        "tags": ["live2d", local_id],
        "license": "Asset rights belong to original studio; personal use only",
        "category": category,
        "minAppVersion": "2.0.0",
        "character": {
            "type": "live2d",
            "model": model3_rel,
            "frameWidth": 300,
            "frameHeight": 300,
        },
        "idlePool": [{"name": "Idle", "weight": 5}],
        "eventMap": {
            "session.start":        "Tap",
            "session.end":          "Idle",
            "session.idle":         "Idle",
            "session.error":        "Tap",
            "prompt.submitted":     "Idle",
            "tool.before":          "Idle",
            "tool.after":           "Tap",
            "tool.failed":          "Tap",
            "permission.requested": "Tap",
            "permission.denied":    "Tap",
            "permission.response":  "Idle",
            "subagent.started":     "Idle",
            "subagent.stopped":     "Idle",
            "notification.sent":    "Tap",
            "file.edited":          "Idle",
            "file.watched":         "Idle",
            "todo.updated":         "Tap",
        },
    }
    if not manifest["nameLocalized"]:
        manifest.pop("nameLocalized")
    (pack_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def import_model(upstream_path: str, local_id: str, name_en: str,
                 name_zh: str, out_dir: Path) -> None:
    pack_dir = out_dir / local_id
    if pack_dir.exists():
        # Treat anything <100 KB as a half-finished previous run and retry.
        total_bytes = sum(f.stat().st_size for f in pack_dir.rglob("*") if f.is_file())
        if total_bytes < 100_000:
            print(f"[retry] {local_id} looks partial ({total_bytes} bytes) — wiping and re-fetching")
            import shutil
            shutil.rmtree(pack_dir)
        else:
            print(f"[skip] {local_id} already exists at {pack_dir}")
            return

    print(f"[import] {local_id}  <-  {upstream_path}")
    files = list(walk_files(upstream_path))
    print(f"  enumerating {len(files)} files...")

    for src in files:
        rel = src[len(upstream_path) + 1:]   # strip prefix and slash
        dst = pack_dir / rel
        download_file(src, dst)

    model3 = next(pack_dir.rglob("*.model3.json"), None)
    if not model3:
        print(f"  [warn] no .model3.json in {local_id}; manifest may not load")
        model3_rel = ""
    else:
        patch_model3(model3)
        model3_rel = model3.relative_to(pack_dir).as_posix()

    # Sanitize every motion3.json — see sanitize_motion() docstring.
    for motion in pack_dir.rglob("*.motion3.json"):
        sanitize_motion(motion)

    # Copy the first texture as preview.png (the runtime expects an image).
    first_texture = next(pack_dir.rglob("textures/*.png"), None)
    if first_texture is None:
        # Fallback: any PNG in any subdir.
        first_texture = next(pack_dir.rglob("*.png"), None)
    if first_texture and first_texture.name != "preview.png":
        (pack_dir / "preview.png").write_bytes(first_texture.read_bytes())

    write_manifest(pack_dir, local_id, name_en, name_zh, model3_rel)
    print(f"[done] {local_id}: {len(files)} files, model={model3_rel}")


PICKS = [
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/z23",
        'z23',                 'Z23',                   'Z23'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/abeikelongbi_3",
        'abercrombie',         'Abercrombie',           '阿贝克隆比'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/adaerbote_3",
        'adalbert',            'Adalbert',              '阿达尔贝特'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/aerbien_3",
        'albion',              'Albion',                '阿尔比恩'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/lafei",
        'laffey',              'Laffey',                '拉菲'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/beierfasite_2",
        'belfast',             'Belfast',               '贝尔法斯特'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/qiye_7",
        'enterprise',          'Enterprise',            '企业'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/chicheng_5",
        'akagi',               'Akagi',                 '赤城'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/ougen_5",
        'prinz_eugen',         'Prinz Eugen',           '欧根亲王'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/tianlangxing_3",
        'sirius',              'Sirius',                '天狼星'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/bisimai_2",
        'bismarck',            'Bismarck',              '俾斯麦'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/gaoxiong_7",
        'takao',               'Takao',                 '高雄'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/huangjiafangzhou_3",
        'ark_royal',           'Ark Royal',             '皇家方舟'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/wuzang_3",
        'musashi',             'Musashi',               '武藏'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/aierdeliqi_5",
        'aldridge',            'Aldridge',              '奥尔德里奇'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/aijier_4",
        'ajax',                'Ajax',                  '埃阿斯'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/ailunsamuna_2",
        'allen_m_sumner',      'Allen M. Sumner',       '艾伦·萨姆纳'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/baerdimo_6",
        'baltimore',           'Baltimore',             '巴尔的摩'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/baifeng_2",
        'shoukaku',            'Shoukaku',              '翔鹤'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/dafeng_7",
        'taihou',              'Taihou',                '大凤'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/edu_4",
        'eldridge',            'Eldridge',              '爱尔德里奇'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/feiteliedadi_4",
        'frederick_the_great', 'Frederick the Great',   '腓特烈大帝'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/geliqiya_2",
        'graf_zeppelin',       'Graf Zeppelin',         '齐柏林伯爵'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/hailunna_4",
        'helena',              'Helena',                '海伦娜'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/heitaizi_2",
        'black_prince',        'Black Prince',          '黑太子'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/hemin_3",
        'formidable',          'Formidable',            '可畏'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/jialisuoniye_4",
        'california',          'California',            '加利福尼亚'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/kalvbudisi_2",
        'cleveland',           'Cleveland',             '克利夫兰'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/kebensi_2",
        'jervis',              'Jervis',                '杰维斯'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/meikelunbao_2",
        'mecklenburg',         'Mecklenburg',           '梅克伦堡'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/naximofu_2",
        'nakhimov',            'Nakhimov',              '纳希莫夫'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/nengdai_2",
        'noshiro',             'Noshiro',               '能代'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/pinghai_6",
        'ping_hai',            'Ping Hai',              '平海'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/rangbaer_5",
        'jean_bart',           'Jean Bart',             '让·巴尔'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/shengluyisi_5",
        'saint_louis',         'Saint Louis',           '圣路易斯'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/taiyuan_2",
        'taiyuan',             'Taiyuan',               '太原'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/telafaerjia_2",
        'trafalgar',           'Trafalgar',             '特拉法尔加'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/tiancheng_3",
        'tencheng',            'Tencheng',              '天城'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/weineituo_2",
        'veneto',              'Veneto',                '维内托'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/weixi_2",
        'vincennes',           'Vincennes',             '文森斯'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/xianghe_2",
        'zuikaku',             'Zuikaku',               '瑞鹤'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/yibei_3",
        'ibuki',               'Ibuki',                 '伊吹'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/aersasi_3",
        'aersasi',             'Aersasi',               'aersasi'),
    ("碧蓝航线 Azue Lane/Azue Lane(JP)/aidang_2",
        'aidang',              'Aidang',                'aidang'),
]


# ----------------------------------------------------------------------------
# Bulk import from a local clone (no network)
# ----------------------------------------------------------------------------
# When the upstream repo is cloned to disk, we can sidestep the flaky
# raw.githubusercontent.com path entirely and just shutil.copytree each
# pack into assets/packs/<id>/. Much faster, and supports categories
# beyond Azur Lane (BanG Dream, Konosuba, Girls Frontline, ...).
import re

# Whitelist: raw upstream folder name -> (category_id, local_id_prefix).
# Categories not listed here (NSFW, oversized, Cubism-2-only) are skipped.
LOCAL_CATEGORIES = {
    '碧蓝航线 Azue Lane':                 ('azur_lane',       'al_'),
    '少女前线 girls Frontline':            ('girls_frontline', 'gf_'),
    '少女次元':                            ('idol_dimension',  'sd_'),
    '为美好的世界献上祝福！Fantastic Days': ('konosuba',        'ks_'),
    'Live2D':                              ('live2d_samples',  'l2d_'),
}

# Already-imported local_ids from PICKS (without prefix). Bulk import skips
# these so we don't overwrite the curated names.
_PICKED_IDS = {t[1] for t in PICKS}


def _slugify(name: str) -> str:
    """ASCII slug for use as a local_id segment. Lowercases, replaces
    non-alphanumerics with underscores, collapses runs."""
    slug = re.sub(r'[^A-Za-z0-9]+', '_', name).strip('_').lower()
    return slug or 'pack'


def _dedup_pick_dirs(pack_dirs, cap):
    """For each character base name (dir name with trailing _N stripped),
    keep the highest-numbered non-`_hx` variant. Then take the first `cap`
    bases alphabetically — keeps each submenu navigable."""
    bases = {}
    ver_re = re.compile(r'_(\d+)(?:_hx)?$')
    for d in pack_dirs:
        name = d.name
        if name.endswith('_hx'):
            continue
        m = ver_re.search(name)
        base = name[:m.start()] if m else name
        version = int(m.group(1)) if m else 0
        if base not in bases or bases[base][0] < version:
            bases[base] = (version, d)
    chosen = sorted(bases.items())[:cap]
    return [d for _, (_, d) in chosen]


def import_local_pack(pack_src: Path, local_id: str, name_en: str, name_zh: str,
                      category: str, author: str, out_dir: Path) -> bool:
    """Copy a pack from a local upstream dir to assets/packs/<local_id>/,
    apply the same patch + sanitize pipeline as the network importer."""
    pack_dir = out_dir / local_id
    if pack_dir.exists():
        return False  # silent skip — caller can detect via return value

    import shutil
    shutil.copytree(pack_src, pack_dir)

    model3 = next(pack_dir.rglob("*.model3.json"), None)
    if not model3:
        print(f"  [warn] no .model3.json in {local_id}; removing")
        shutil.rmtree(pack_dir)
        return False
    patch_model3(model3)
    model3_rel = model3.relative_to(pack_dir).as_posix()

    for motion in pack_dir.rglob("*.motion3.json"):
        sanitize_motion(motion)

    first_texture = next(pack_dir.rglob("textures/*.png"), None)
    if first_texture is None:
        first_texture = next(pack_dir.rglob("*.png"), None)
    if first_texture and first_texture.name != "preview.png":
        (pack_dir / "preview.png").write_bytes(first_texture.read_bytes())

    write_manifest(pack_dir, local_id, name_en, name_zh, model3_rel,
                   category=category, author=author)
    return True


def bulk_import_local(clone_root: Path, out_dir: Path, cap_per_cat: int,
                      max_pack_mb: int = 60) -> tuple[int, int]:
    """Walk every whitelisted category in `clone_root`, pick up to
    `cap_per_cat` packs per category (deduped, dropping `_hx` skin variants
    and over-sized packs), and copy them into out_dir.

    Returns (imported, skipped)."""
    imported = 0
    skipped = 0
    for raw_cat, (cat_id, prefix) in LOCAL_CATEGORIES.items():
        cat_root = clone_root / raw_cat
        if not cat_root.is_dir():
            print(f"[skip] {raw_cat} not found in clone")
            continue

        # Pack dir = parent of any .moc3 found under the category root.
        pack_dirs = sorted(set(m.parent for m in cat_root.rglob('*.moc3')))
        # Filter oversized packs (textures + motions can balloon).
        pack_dirs = [
            d for d in pack_dirs
            if sum(f.stat().st_size for f in d.rglob('*') if f.is_file())
               < max_pack_mb * 1024 * 1024
        ]
        picks = _dedup_pick_dirs(pack_dirs, cap_per_cat)
        print(f"\n[{cat_id}] {len(picks)} picks (cap {cap_per_cat}, max_pack {max_pack_mb}MB)")

        for src in picks:
            base = re.sub(r'_(\d+)(?:_hx)?$', '', src.name)
            local_id = prefix + _slugify(base)
            if base in _PICKED_IDS or local_id in _PICKED_IDS:
                skipped += 1
                continue  # honour curated names from PICKS

            display = base.replace('_', ' ').title()
            ok = import_local_pack(
                src, local_id, name_en=display, name_zh=display,
                category=cat_id,
                author=f"Imported from github.com/{REPO}",
                out_dir=out_dir,
            )
            if ok:
                imported += 1
                print(f"  [+] {local_id:32s} ({src.name})")
            else:
                skipped += 1
    return imported, skipped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out-dir", default="assets/packs",
                        help="Where to write packs (default: assets/packs)")
    parser.add_argument("--list", action="store_true",
                        help="Print known picks and exit")
    parser.add_argument("--local",
                        help="Bulk-import from a local clone of the upstream "
                             "repo (e.g. F:/Live2d-model). Skips network and "
                             "pulls from every whitelisted category.")
    parser.add_argument("--cap", type=int, default=50,
                        help="Max packs per category in --local mode (default: 50)")
    parser.add_argument("ids", nargs="*",
                        help="Subset of local_ids to import (default: all)")
    args = parser.parse_args()

    if args.list:
        for upstream, lid, en, zh in PICKS:
            print(f"  {lid:<14}  {en:<14}  {zh:<10}  ({upstream})")
        return 0

    out_dir = Path(args.out_dir).resolve()

    if args.local:
        clone_root = Path(args.local).resolve()
        if not clone_root.is_dir():
            print(f"[error] --local path is not a directory: {clone_root}", file=sys.stderr)
            return 2
        imported, skipped = bulk_import_local(clone_root, out_dir, args.cap)
        print(f"\n[done] bulk import: {imported} imported, {skipped} skipped")
        return 0

    picks = PICKS if not args.ids else [p for p in PICKS if p[1] in args.ids]
    if not picks:
        print(f"No matching picks for: {args.ids}", file=sys.stderr)
        return 2

    failed = 0
    for upstream, lid, en, zh in picks:
        try:
            import_model(upstream, lid, en, zh, out_dir)
        except Exception as exc:                 # noqa: BLE001
            print(f"[error] {lid}: {exc}", file=sys.stderr)
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
