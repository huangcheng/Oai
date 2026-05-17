---
name: seelie-codex-pet
description: Build a Codex animated pet for the Seelie persona (or any other character) and prepare it for distribution on codex-pets.net. Wraps OpenAI's `hatch-pet` skill, runs it via the local `codex` CLI, and enforces the rule that only `pet.json` + `spritesheet.webp` ship in the distributable bundle — QA artifacts stay local under `codex-pet-runs/` (gitignored) while the durable upload bundle lives at `codex-pet/<id>/` (tracked). Use when the user says "make a Codex pet", "hatch a pet for Seelie", "build the codex pet", "upload to codex-pets.net", "regenerate the seelie codex pet", or anything else about distributing a Codex-compatible pet asset.
---

# Seelie Codex Pet

## What this is

A **Codex pet** is an animated pet asset for OpenAI's **Codex CLI app** (separate from the Seelie desktop pet itself). It's a single sprite atlas of 9 named animation rows (`idle`, `running-right`, `running-left`, `waving`, `jumping`, `failed`, `waiting`, `running`, `review`) plus a manifest. Codex CLI reads pets from `~/.codex/pets/<id>/` at runtime and lets the user pick one from the app's pet selector.

This is **not** the same as Seelie's internal pack format (`assets/packs/<id>/`) — Seelie's packs use a different manifest schema and are bundled inside the Seelie .app. The two pets can share an aesthetic (the in-Seelie spriteSheet pack at `assets/packs/seelie/` was built from the same atlas this skill produces), but they ship through different distribution channels:

| Distribution | Format | Lives at | Bundled with |
|---|---|---|---|
| Seelie internal pack | `manifest.json` (Seelie schema) + `spritesheet.webp` + `animations.json` | `assets/packs/<id>/` | Seelie installer / DMG |
| Codex pet | `pet.json` (Codex schema, 4 fields) + `spritesheet.webp` | `~/.codex/pets/<id>/` on the end user's machine | Uploaded to https://codex-pets.net |

## When to use

Triggers:
- "make a Codex pet", "build a Codex pet", "hatch a pet for Seelie"
- "regenerate the Seelie codex pet"
- "upload to codex-pets.net", "prep for codex-pets.net upload"
- "run hatch-pet on this character"

## Prerequisites

| Tool | Why | Install |
|------|-----|---------|
| `codex` CLI | Runs the hatch-pet skill end-to-end | `brew install codex-cli` or download from openai.com |
| `~/.codex/skills/hatch-pet/` | The skill that scaffolds the run, generates rows, assembles the atlas, validates, and packages | Installed with Codex CLI |
| `~/.codex/skills/.system/imagegen/` | Image generation backend the hatch-pet skill delegates to | Installed with Codex CLI |
| OpenAI API key | Codex CLI auth — `codex auth status` should report `api-key` source | `codex login` |
| Reference image (recommended) | A canonical front-facing portrait of the character. Hatch-pet uses it to lock identity across all 9 rows. Without one, the base job runs prompt-only and identity drifts more. | For Seelie: use `artworks/mascot/seelie-avatar_001.jpg` (the MiniMax-generated mascot, tracked in the repo) |

Verify Codex install + auth before starting:

```bash
codex --version                           # should report codex-cli 0.130.0+
codex auth status                         # should show method=api-key
ls ~/.codex/skills/hatch-pet/SKILL.md     # the skill we wrap
ls ~/.codex/skills/.system/imagegen       # the image backend
```

## Workflow

The whole run is **one `codex exec` call** with a structured prompt. Codex (acting as the agent) reads its own `hatch-pet` SKILL.md, spawns lightweight workers for the 10 image jobs (base + 9 rows), assembles the atlas with the skill's Python scripts, runs visual QA, and packages to `~/.codex/pets/<id>/`.

### 1. Stage the workspace

Always work under `codex-pet-runs/<id>/` inside the project — this keeps Codex's per-run artifacts (decoded row strips, frame extracts, intermediate atlas PNGs, prompts, etc.) out of the repo. The directory is gitignored.

```bash
mkdir -p codex-pet-runs/<id>
# `<id>` = the pet id, e.g. "seelie". Same id used in pet.json and the
# final ~/.codex/pets/<id>/ destination.
```

### 2. Run hatch-pet via codex exec

```bash
codex exec --skip-git-repo-check 'Use the hatch-pet skill to create a Codex pet.

## Inputs
- pet name: <Display Name>
- pet id (folder): <id>
- description: <one paragraph persona — body/hair/eyes/outfit/accessory/personality>
- style preset: auto
- style notes: <e.g. "clean cel-shaded anime, virtual-idol look, palette consistent across rows">
- reference image (canonical base): <ABSOLUTE PATH to portrait>
- run output dir: <ABSOLUTE PATH to codex-pet-runs/<id>>
- final pet package destination: ~/.codex/pets/<id>/ (skill default)

## What to do
Run the hatch-pet workflow end-to-end per ~/.codex/skills/hatch-pet/SKILL.md:
1. prepare_pet_run.py (--pet-name, --description, --reference, --output-dir, --pet-notes, --style-preset auto, --force)
2. base → idle → running-right (identity + gait check)
3. running-left (mirror only if visually safe)
4. waving, jumping, failed, waiting, running, review
5. extract_strip_frames → inspect_frames → compose_atlas → validate_atlas → make_contact_sheet → render_animation_previews
6. final visual QA worker (single repair pass per row if it fails)
7. package to ~/.codex/pets/<id>/{pet.json, spritesheet.webp}
8. write qa/run-summary.json
9. clean intermediates per the skill keep-list

## Constraints
- Subagent use is approved.
- Approve creating files in the run dir, ~/.codex/pets/<id>/, and ~/.codex/generated_images.
- Do not modify files outside those locations.
- Keep identity consistent across all 9 rows.

Report absolute paths to pet.json, spritesheet.webp, contact-sheet.png, validation.json, and run-summary.json on success.

Proceed.'
```

**Expected duration**: 5–25 minutes depending on how many rows fail visual QA on the first pass. Each `$imagegen` call takes 20–90s; up to two run concurrently.

### 3. If the run exits before packaging

Codex CLI sometimes exits after a worker spins down, leaving the manifest in a mixed state (some jobs `complete`, others still `pending`). **Resume with a focused continuation prompt** rather than starting over — every completed row in `decoded/` is reusable.

Diagnostic:
```bash
jq '[.jobs[] | {id, status}]' codex-pet-runs/<id>/imagegen-jobs.json
ls codex-pet-runs/<id>/decoded/        # rows already produced
ls codex-pet-runs/<id>/outputs/        # rows generated but not yet promoted
```

A row in `outputs/` but not `decoded/` was generated by a worker that returned `selected_source=...` but the parent never copied it into the manifest. **Promote it** by `cp outputs/<row>-selected-output.png decoded/<row>.png` and marking the job `complete` with `jq` — don't regenerate.

Continuation prompt template:

```bash
codex exec --skip-git-repo-check 'Resume the in-progress hatch-pet run at <ABSOLUTE run dir>.

State:
- Completed rows in decoded/: <list>
- Pending rows: <list>
- Rows generated to outputs/ but not promoted: <list — instruct to cp + mark complete without regenerating>

Continue from where the previous run stopped: promote already-generated rows, generate any missing rows, run the assembly pipeline, run final visual QA, package to ~/.codex/pets/<id>/, write run-summary, clean intermediates. Do NOT regenerate any complete row.

Proceed.'
```

### 4. Verify

```bash
ls ~/.codex/pets/<id>/                                 # pet.json + spritesheet.webp
jq . ~/.codex/pets/<id>/pet.json                       # 4 fields: id, displayName, description, spritesheetPath
file ~/.codex/pets/<id>/spritesheet.webp               # WebP, 1536x1872
jq '.ok, .visual_qa, .repaired_rows' \
   codex-pet-runs/<id>/qa/run-summary.json             # ok=true, visual_qa=pass
open codex-pet-runs/<id>/qa/contact-sheet.png          # visual sanity check
open codex-pet-runs/<id>/qa/previews/                  # 9 GIFs, one per row
```

## **CRITICAL: what goes in the codex-pets.net upload**

When you ship the pet to codex-pets.net, the bundle **must contain only the two files Codex CLI needs at runtime**:

```
seelie/
├── pet.json           ← 4-field manifest (id, displayName, description, spritesheetPath)
└── spritesheet.webp   ← 1536×1872 WebP atlas
```

That is **exactly** what lives in `~/.codex/pets/<id>/` after a successful run. **Nothing else.**

Do NOT include in the upload:
- The hatch-pet run dir (`codex-pet-runs/<id>/`) — contains prompts, decoded row strips, frame extracts, intermediate PNGs, the manifest schema, layout-guide pixels, validation JSON, contact sheets, preview GIFs. All of this is local-only diagnostic content.
- `pet_request.json` — input config for the run, not for end users
- `qa/` artifacts — useful for debugging but irrelevant once the pet works
- Source reference images (e.g. `artworks/mascot/seelie-avatar_001.jpg`) — separate IP question; don't bundle without thinking
- The `.codex-plugin` or `agents/openai.yaml` files from hatch-pet — those belong to the skill, not the pet

Standard stash + zip command (from project root, after a successful run):

```bash
ID=<id>     # e.g. seelie

# 1. Stash the upload bundle in the tracked, durable location.
#    codex-pet/<id>/ is committed to the repo; codex-pet-runs/ is gitignored
#    and will be wiped by `git clean -fd`. The two files here ARE the
#    bundle codex-pets.net expects — see the table below for why this
#    dir exists.
mkdir -p codex-pet/$ID
cp ~/.codex/pets/$ID/pet.json         codex-pet/$ID/pet.json
cp ~/.codex/pets/$ID/spritesheet.webp codex-pet/$ID/spritesheet.webp

# 2. Optionally produce a ready-to-upload zip. -j flattens the zip so the
#    two files sit at the archive root — pet registries usually expect
#    that layout. Drop -j if codex-pets.net wants a folder inside the
#    zip instead.
cd codex-pet/$ID
zip -j ../$ID-codex-pet.zip pet.json spritesheet.webp
cd -
```

Confirm the zip contains exactly two entries:
```bash
unzip -l codex-pet/$ID-codex-pet.zip
# expected: 2 files (pet.json + spritesheet.webp), <2 MB total
```

## Tracked deliverable vs throwaway run dir

Two top-level dirs hold codex-pet content. They serve different purposes:

| Path | Tracked? | Contents | Lifetime |
|---|---|---|---|
| `codex-pet/<id>/` | **Yes** (committed) | `pet.json` + `spritesheet.webp` only — the exact two files codex-pets.net expects. README in `codex-pet/README.md` documents the relationship to the in-app `assets/packs/<id>/` pack. | Permanent. Survives `git clean`. |
| `codex-pet-runs/<id>/` | **No** (`.gitignore:108`) | Hatch-pet's per-run intermediates: prompts, decoded row strips, frame extracts, `outputs/`, `decoded/`, `qa/`, `imagegen-jobs.json`, layout-guide pixels. | Throwaway. Useful only for debugging or resuming a partial run. |

When the user asks "where is my codex pet?" the answer is **`codex-pet/<id>/`** — that's the durable deliverable. The run dir is incidental. Always stash to `codex-pet/<id>/` before the user closes the shell; a `git clean -fd` between sessions would otherwise erase the upload bundle.

## Run-dir layout (`codex-pet-runs/<id>/`)

After a successful end-to-end run with cleanup applied, the kept artifacts are:

```
codex-pet-runs/<id>/
├── pet_request.json                 — input config (id, name, description, refs, chroma key)
├── final/
│   ├── spritesheet.webp             — the atlas (same file shipped in ~/.codex/pets/)
│   └── validation.json              — atlas geometry + transparency invariant check
└── qa/
    ├── contact-sheet.png            — 9-row × 8-col visual grid for human review
    ├── previews/                    — one looping GIF per animation row
    ├── review.json                  — frame-by-frame inspection metadata
    └── run-summary.json             — ok/visual_qa/repaired_rows + absolute paths
```

These stay on disk for debugging future regenerations. They are gitignored via `/codex-pet-runs/` in `.gitignore`.

The hatch-pet skill itself defines what gets cleaned (see its "Default Workflow" step 6). Don't add custom cleanup steps — the skill knows which intermediates are safe to delete (prompts, layout guides, decoded row strips, frame extracts, the PNG version of the atlas, and the imagegen-jobs manifest) and which to keep.

## Pitfalls

### 1. Never write into `~/.codex/pets/` without explicit user consent

`~/.codex/` is OpenAI Codex CLI's private config directory, not a generic install location. Even on the developer's own machine, writing a pet there is a side-effect users notice (Codex's pet picker changes). Three rules:

- **Don't auto-install during the Seelie installer's post-install scripts.** If the user installs Seelie, that should not silently spawn a pet in someone else's app's config dir.
- **Don't extract distribution zips directly into `~/.codex/pets/`** in any "how to install" recipe. The user can read a one-line instruction and unzip it themselves.
- **The `codex exec` hatch-pet run is the only acceptable way to write `~/.codex/pets/<id>/`** in automation, because the user explicitly invoked the build by running the command.

If the user wants Seelie's Codex pet to also be available alongside the Seelie .app for convenience, the correct UX is an opt-in action inside Seelie itself (e.g. a Settings → "Install Seelie as Codex pet" button) that prompts and copies on click. The pet assets would ship inside `Seelie.app/Contents/Resources/codex-pet/` and the button would `cp` them to `~/.codex/pets/seelie/` when activated. Not yet implemented — leave as future work unless explicitly asked.

### 2. `~/.codex/generated_images/` bloats fast

Every `$imagegen` call writes a PNG (and often a JPEG sidecar) into `~/.codex/generated_images/<uuid>/`. The hatch-pet skill auto-removes the **selected** output after copying it to `decoded/`, but rejected variants, retry attempts from failed visual QA, and runs that exited mid-way all leave orphans. A single run can deposit 200–800 MB.

After every run, clean orphans:

```bash
du -sh ~/.codex/generated_images
# If > 1 GB and you don't need them for debugging:
rm -rf ~/.codex/generated_images/*
```

Don't `rm -rf ~/.codex/generated_images` while a run is active — workers may still be reading from it.

### 3. Codex CLI exits before final packaging

Symptom: `codex exec` completes (exit 0) but `~/.codex/pets/<id>/` is empty and several jobs in `imagegen-jobs.json` are still `pending`. Sometimes the "selected-output" file exists in `outputs/` from the last worker that finished, but the parent agent never promoted it into `decoded/` and never marked the manifest.

This is normal — Codex's session timeouts, worker rate-limiting, and the skill's "max two parallel workers" rule all interact in ways that sometimes drop the parent thread before it finishes the last cleanup. Always check job status before assuming success, and use the resume prompt in Workflow §3 to continue. The QA worker is also a separate stage — if it fails a row, Codex needs another pass.

### 4. Hatch-pet rejects detached visual effects

The hatch-pet skill is strict about transparency invariants: motion lines, dust, glow, shadow, sparkles, and floating particles all get rejected by visual QA because they break the bundled chroma-key cleanup. If your reference image has *any* glow or particle effects (common for fae/spirit-themed characters — both Seelie's MiniMax base and her wisp companion have these), expect the first attempt at `running` or `jumping` to fail QA. The skill's allowed single repair pass will retry with stricter prompting and usually succeed.

Seelie's run hit exactly this on `running` — first attempt had glow/spark smear effects, repair pass produced clean output. See `codex-pet-runs/seelie/qa/review.json` for the diagnostic.

### 5. macOS: stale `/Volumes/<Name>/` mounts confuse verification

If you `hdiutil attach` a DMG, then later rebuild and reattach the new DMG with the same volume name, macOS appends `" 1"`, `" 2"`, etc. to the path. The old mount is still active. `ls /Volumes/<Name>/` reads from the **original** mount, not the freshest one — so you can convince yourself the new DMG is missing assets when it's not.

Before verifying any rebuilt artifact:

```bash
mount | grep -i <volume-name>
for v in /Volumes/<Name>*/; do hdiutil detach "$v" -force -quiet; done
hdiutil attach <fresh DMG path> -nobrowse        # produces a new clean mount
```

### 6. Identity drift across rows

The single most common visual QA failure mode is one row's character looking subtly different — hair color drift, prop missing, palette shifted, proportions wrong. Three preventives:

- **Always pass a reference image to `prepare_pet_run.py --reference`.** The hatch-pet skill uses it as the canonical base for every row job. Identity locks far better than text-only prompts.
- **Generate `base` first and visually verify** before letting the row workers loose. If the base looks wrong, regenerate it before generating any row.
- **Don't let `running-left` be mirrored** unless `running-right` is genuinely symmetric (no asymmetric props, no one-side-only markings, no directional gestures). The skill exposes `derive_running_left_from_running_right.py` for the mirror path; only invoke it after explicit visual confirmation.

## Currently shipped Seelie Codex pet

The pet under `~/.codex/pets/seelie/` was built 2026-05-17 from:

- Reference: `artworks/mascot/seelie-avatar_001.jpg` (MiniMax `image-01` generation, twintail anime mascot fusion of Hatsune Miku + Bilibili 233娘 + fae spirit cues)
- Style preset: `auto`
- 10 image jobs total: 1 base + 8 first-pass rows + 1 repair on `running` (glow smear → clean)
- Final atlas: 1536×1872 WebP, 1.7 MB, no validation errors
- Persona description: *"A small fae-spirit virtual mascot girl who reacts to AI coding tool events. Pale cyan-to-lavender twintail hair, large warm cyan eyes, subtly pointed elf ears, gentle half-smile, oversized white hoodie with lavender trim, glowing cyan crystal pendant. Personality: warm, encouraging, briefly playful when you ship, quietly concerned when you go idle, gently honest when tests fail."*

Lives in the repo at:
- **`codex-pet/seelie/pet.json`** + **`codex-pet/seelie/spritesheet.webp`** — the upload-ready bundle, tracked. This is what gets sent to codex-pets.net.
- `assets/packs/seelie/spritesheet.webp` — byte-identical copy of the same atlas, but wrapped in Seelie's own pack manifest (`manifest.json`) for the in-app sprite engine. The atlas is shared by design so the desktop pet and the Codex pet are visually the same character.
- `codex-pet/README.md` documents the relationship between the two paths and the regenerate workflow.

## Adjacent skills

- **`hatch-pet`** (OpenAI, `~/.codex/skills/hatch-pet/SKILL.md`) — the skill this wraps. Read it for atlas geometry rules, allowed visual effects per state, and the deterministic Python scripts.
- **`minimax-multimodal-toolkit`** — what we used to generate `seelie-avatar_001.jpg`. Use it (or any image-gen path) to produce reference portraits before kicking off a hatch-pet run.
- **`seelie-build-deploy`** — separate concern: building the Seelie *app* and its internal sprite-sheet packs. Don't conflate the two.
