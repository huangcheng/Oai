# Codex Pet State Machine — Design

**Date:** 2026-05-09
**Status:** Approved (pending user review of written spec)
**Supersedes:** behavior in `EventRouter.cpp` (per-event direct `playAnimation`) and `EmotionEngine.cpp` (mood → animation fan-out)

## Context

The Codex pet format (loaded via `CharacterPack::loadFromCodexPet`) ships a fixed 8×9 atlas with nine named animation rows: `idle`, `running-right`, `running-left`, `waving`, `jumping`, `failed`, `waiting`, `running`, `review`. Each row has authoring rules from the upstream `hatch-pet` SKILL.md (e.g. `running` means "active working/in-progress loop," not literal locomotion; `waving` shows the wave through paw pose only, no motion arcs).

Today, Oai dispatches gateway events to animations through two parallel paths:

1. `EventRouter::routeEvent` — maps each canonical event directly to an animation chain via `m_eventMap` (`EventRouter.cpp:148-164`) and fires `playAnimation()` on whichever engine has data loaded (Live2D > Lottie > Sprite).
2. `EmotionEngine::processEvent` — accumulates valence/arousal scores per event, classifies into a mood (`happy`, `frustrated`, `bored`, `focused`, `stressed`, `neutral`), and emits `moodChanged(animationName)` which `main.cpp:384-397` also pipes into `playAnimation()`.

Three problems with this:

- **No state.** Nothing tracks "the pet is currently in a working session." A burst of `tool.before` followed by an unrelated `file.watched` plays whatever animation `file.watched` maps to, breaking the working loop.
- **Codex rows go unused.** `running-left`, `running-right`, `waiting`, `review` are authored in the spritesheet but unreachable from gateway events. The current `nameMap` in `CharacterPack.cpp:251-291` covers some canonical names but `EventRouter`'s defaults don't reference them.
- **Coverage gap on web-sourced packs.** Most Live2D models from the public web ship with `Idle`, `TouchHead`, `TouchBody`, `Tap`, sometimes `Login` — and nothing else. There's no fallback strategy when a pack lacks an animation for an incoming event.

This design replaces the two competing dispatch paths with a single state machine and an abstract-state vocabulary that all pack types map onto.

## Goals / Non-goals

**Goals:**
- A single `PetStateMachine` owns "what is the pet doing right now."
- Eight abstract states cover Codex pets 1:1 and degrade gracefully on poor-coverage Live2D / Lottie / sprite packs.
- Sustained states (Working, Thinking, Reviewing) hold for the duration of activity, not single events.
- `running-left` / `running-right` finally get used, via a Walking overlay driven by position deltas.
- `EventRouter` keeps tip-text routing and event validation; it stops firing animations.
- `EmotionEngine` is removed.

**Non-goals:**
- Authoring tool for new Codex pets (out of scope — upstream `hatch-pet` skill handles that).
- Changing the Codex `pet.json` schema or the 8×9 atlas geometry.
- Auto-walk / wander behavior (Walking overlay is built but only triggered by future features).
- Per-mood animation flavor variants (the `mood=happy → idle-happy` idea is dropped with EmotionEngine).
- Customizing state-to-animation mappings per user (mappings are pack-author and engine-default only).

## Abstract state vocabulary

Eight states. Names match Codex rows where the meaning aligns; abstract names where Codex's vocabulary is too narrow.

| State | Meaning | Codex row | Kind |
|---|---|---|---|
| `Idle` | Default quiet motion, no work in progress | `idle` | base (default) |
| `Greeting` | Brief hello on session start | `waving` | one-shot |
| `Thinking` | Prompt submitted, model generating, tools haven't started | `waiting` | sustained |
| `Working` | Tool running, subagent active, file edits flowing | `running` | sustained |
| `Reviewing` | Model is asking the user to look (permission, notification) | `review` | sustained |
| `Failed` | Error or permission denial reaction | `failed` | one-shot |
| `Celebrating` | Todo completed, prompt finished cleanly | `jumping` | one-shot |
| `Walking` | Pet relocating across screen | `running-left` / `running-right` | overlay |

A "base state" is what the pet returns to when nothing else is happening — only Idle is base. Sustained states replace the base state for as long as their exit condition isn't met. One-shot states play once and then return to whichever sustained state was active (or Idle). Walking is an overlay: it replaces the rendered animation without changing the underlying logical state.

## Event → state transitions

### One-shot transitions

| Event | State | Notes |
|---|---|---|
| `session.start` | Greeting | only fires when current state is Idle (won't interrupt mid-task work on session restart) |
| `session.error` | Failed | |
| `tool.failed` | Failed | |
| `permission.denied` | Failed | |
| `notification.sent` | Reviewing (one-shot, ~2s) | brief glance variant; uses Reviewing's chain but auto-exits |
| `todo.updated` | Celebrating | only when payload indicates a todo transitioned to "completed" — see Open Questions |

### Sustained transitions

| Event | State | Exit rule |
|---|---|---|
| `prompt.submitted` | Thinking | exits on first `tool.before` (Working takes over) or on 15s timeout |
| `tool.before` | Working | exits when no `tool.before` / `subagent.started` / `file.edited` for **1500 ms** (grace window) |
| `subagent.started` | Working | extends Working grace |
| `file.edited` | Working | extends Working grace |
| `permission.requested` | Reviewing | exits on `permission.response` or 60s timeout |

### Passive transitions

`tool.after`, `subagent.stopped`, `permission.response`, `file.watched` — extend or reset relevant grace timers but don't push a new state. `session.end` and `session.idle` — explicitly drop to Idle, cancelling any sustained state.

### Priority and preemption

When a transition fires while another state is active:

```
Critical (Failed, Celebrating)  > Sustained (Working, Thinking, Reviewing) > Greeting > Idle
```

- A Critical one-shot interrupts any sustained state, plays its animation, then returns to the sustained state it interrupted (saved in `m_savedSustained`).
- A new sustained state replaces the current sustained state immediately. No queueing.
- Greeting only fires when current state is Idle. If a session restarts mid-work, no greeting.
- Idle is never explicitly transitioned *to* by an event other than `session.end` / `session.idle` / sustained-exit; it's the floor.

## Walking overlay

Structurally different from the seven event-driven states.

- **Trigger:** the pet's window position changes by more than 32 px in a single frame *and* the move is not a user drag. Sources today: nothing wired (auto-snap and gaming-mode return are future features). Triggering is built; no producers exist yet.
- **Direction:** `running-right` chain when Δx > 0, `running-left` chain when Δx < 0. Pure-Δy motion uses `running-right` (right-facing default).
- **Stacking:** Walking replaces the rendered animation while motion is happening. The underlying logical state (Working / Thinking / Idle / etc.) is preserved. When motion stops, the renderer immediately resumes whatever the logical state's chain says.
- **User drag is excluded.** The cursor is already moving the pet visibly; an extra animation reads as busy. (Reversible decision; controlled by an `excludeUserDrag` constructor flag.)
- **Coverage degradation:** if the pack has no `walk` / `running-left` / `running-right` mapping, Walking is a no-op — the pet slides with its current animation playing. Acceptable; this is what most Live2D and Clippy packs will look like.

## Per-state fallback chains

Each state has an animation chain — the renderer tries each name in order, plays the first one the active engine has loaded. Chains are built at pack-load time from three sources:

1. Pack manifest's `stateMap` field (new, optional) — explicit pack-author override.
2. Pack manifest's existing `eventMap` and `nameMap` — translated automatically into the abstract vocabulary using a fixed event-to-state lookup.
3. Engine-default chain — the fallback baked into `PetStateMachine`.

Engine defaults:

```
Idle        → ["idle", "Idle"]
Greeting    → ["waving", "greet", "Login", "Tap"]
Thinking    → ["waiting", "think", "Thinking", "TouchHead", "Tap"]
Working     → ["running", "work", "Processing", "Writing", "TouchBody", "Tap"]
Reviewing   → ["review", "attention", "GetAttention", "TouchHead", "Tap"]
Failed      → ["failed", "alert", "Alert", "TouchHead", "Tap"]
Celebrating → ["jumping", "celebrate", "Congratulate", "TouchBody", "Tap"]
Walking     → ["running-{dir}", "walk", "Walk"]
```

Every chain emitted by the FSM has the pack's idle entry appended as a final fallback. Engines try the chain in order and play the first one they have; if a pack only has `Idle`, every chain resolves to `Idle` and the pet visibly does nothing on state change rather than going blank. The append happens in `PetStateMachine::resolveChain` at emit time, so engines stay unaware of state semantics.

For Codex pets: the existing `m_nameMap` in `CharacterPack.cpp:251-291` already covers all the canonical names; pack loading translates that into a `stateMap` automatically. No Codex pet changes needed.

## Architecture

### New class: `PetStateMachine`

Lives in `src/PetStateMachine.{h,cpp}`. Owns the FSM state, grace timers, and chain resolution.

```cpp
class PetStateMachine : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Greeting, Thinking, Working, Reviewing, Failed, Celebrating };
    enum class WalkDir { None, Left, Right };

public slots:
    void onCanonicalEvent(const QString &eventName, const QJsonObject &payload);
    void onSyntheticEvent(const QString &eventName);  // user.click, user.doubleclick, etc.
    void onPositionChanged(const QPoint &oldPos, const QPoint &newPos, bool isUserDrag);
    void onActivePackChanged(const CharacterPack *pack);

signals:
    void animationRequested(const QStringList &chain, int priority);

private:
    State m_baseState = State::Idle;          // sustained state, default Idle
    State m_overlayState = State::Idle;       // active one-shot (Idle = none)
    State m_savedSustained = State::Idle;     // restored after one-shot exits
    bool m_walking = false;
    WalkDir m_walkDir = WalkDir::None;

    QTimer m_workingGrace;       // 1500 ms
    QTimer m_thinkingTimeout;    // 15000 ms
    QTimer m_reviewingTimeout;   // 60000 ms
    QTimer m_oneShotTimer;       // duration of current one-shot animation

    QMap<State, QStringList> m_chains;  // resolved per-pack at load time
};
```

State derives from `(m_baseState, m_overlayState, m_walking)`:
- if `m_walking` → render Walking chain for `m_walkDir`
- else if `m_overlayState != Idle` → render `m_overlayState`'s chain
- else → render `m_baseState`'s chain

### Wiring in `main.cpp`

```
EventRouter      --eventProcessed--> PetStateMachine::onCanonicalEvent
MainWindow       --positionChanged--> PetStateMachine::onPositionChanged
CharacterPackMgr --activePackChanged--> PetStateMachine::onActivePackChanged
PetStateMachine  --animationRequested--> [Live2D|Lottie|Sprite] engine fan-out
```

The fan-out lambda lives in `main.cpp` (the same shape as today's `moodChanged` lambda at `main.cpp:384-397`): pick whichever engine has animations loaded, call `playAnimationChain` (Live2D) or `playAnimation(chain.first(), priority)` (Lottie/Sprite).

### Changes to existing components

**`EventRouter`**
- Keep: `validateEvent`, `s_validEvents`, `s_validSources`, tip-text dispatch via `TipsCatalog`, `eventProcessed` signal.
- Delete: `s_activeEvents` (priority is now FSM-driven), the per-event animation chains in `m_eventMap` (lines 148-164), and the engine fan-out block in `routeEvent` (lines 77-100). The animation chains move into `PetStateMachine` keyed by abstract state.
- Delete: `loadFromCharacterPack` — the FSM consumes pack data via `onActivePackChanged` instead. `EventRouter` no longer needs to know about packs.
- Delete: `triggerEvent` — synthetic local events (`user.click`, `user.doubleclick`) wire directly to `PetStateMachine::onSyntheticEvent` from `MainWindow`'s mouse handlers, since they don't need IPC validation. The FSM treats `user.click` as a Greeting trigger when state is Idle, otherwise no-op.

**`EmotionEngine`**
- Delete `src/EmotionEngine.{h,cpp}` and its wiring in `main.cpp:381-397`. The streak-amplification behavior is dropped. If we miss it later, add an `m_intensity` float to `PetStateMachine` driven by repeated same-event arrivals — single class, single source of truth. Reversible.

**`CharacterPack`**
- Add optional `stateMap` parsing (new top-level manifest field, parallel to `eventMap`): `{"Working": ["work", "running"], "Failed": ["alert"]}`. Codex pets don't need this — `loadFromCodexPet` synthesizes the stateMap from its built-in row names directly.

**`MainWindow`**
- Add `positionChanged` signal emission whenever the window moves (after drag drop, after gaming-mode show/hide). Already partially exists at `mainwindow.h:47`; just needs to fire on more code paths and pass `isUserDrag` flag.

### Module boundaries

| Module | Responsibility | Doesn't know about |
|---|---|---|
| `EventRouter` | Validate IPC events, route tip text, emit `eventProcessed` | animations, packs, FSM state |
| `PetStateMachine` | Own FSM state, resolve state→chain, emit `animationRequested` | engines, IPC, tip text |
| `CharacterPack` | Parse manifest, expose `stateMap`/`nameMap` | FSM, engines |
| Engines (Live2D/Lottie/Sprite) | Play named chains, render | FSM, packs (consume via `loadFromCharacterPack`) |
| `main.cpp` | Wire signals end-to-end, engine fan-out lambda | FSM internals |

### Tests

`tests/test_pet_state_machine.cpp` (new), Qt Test framework, no UDP:
- One-shot transitions return to saved sustained state.
- Working grace window holds across a `tool.after` → `tool.before` gap of 1000 ms; releases after 1500 ms of silence.
- Thinking → Working takeover on first `tool.before`.
- Greeting suppressed when state is not Idle.
- Walking overlay preserves underlying base state.
- Pack switch rebuilds chains from new pack's `stateMap` / `nameMap` / engine defaults.
- Critical preemption: `session.error` during Working → Failed → back to Working.
- Reviewing exits on `permission.response`, on 60s timeout, and on Failed preemption.

## Risks / trade-offs

| Risk | Mitigation |
|---|---|
| 1500 ms grace window feels wrong in practice for some users | Make it a `ConfigManager` setting after we ship and observe |
| Web-sourced Live2D packs sit in `Tap` for every state (boring) | Acceptable degradation; coverage indicator in pack picker is post-MVP |
| Removing EmotionEngine loses streak amplification | Reversible; re-add as `m_intensity` field on FSM if missed |
| `todo.updated` payload doesn't tell completion from update | See Open Questions; conservative default is "celebrate on every update," disable via setting |
| FSM state held in memory only — restart loses state | Acceptable; all sustained states have natural exit timeouts and would re-enter on first event anyway |
| Critical preemption (Failed during Working) saves only one sustained state, not a stack | If two Criticals overlap, the second's saved sustained is the first Critical, which collapses to Idle on exit. Acceptable; this case is rare and the recovery is to wait for the next sustained event |

## Migration plan

No user-visible config or asset migration. Existing `.opk` packs work unchanged — their `eventMap` and `nameMap` translate automatically. Codex pets work unchanged. EmotionEngine deletion is invisible (it never had a public surface). Tip-text behavior is unchanged because it stays in `EventRouter`.

## Open questions

1. **`todo.updated` payload shape**: does the gateway send enough context to distinguish "completed" from "added"? If not, do we (a) celebrate on every update, (b) celebrate never, or (c) extend the gateway payload? Current proposal: (a) with a `ConfigManager` setting `celebrateEveryTodoUpdate` defaulting to false (safer) — pet only celebrates on explicit completion when payload allows; otherwise no celebration.
2. **Walking trigger threshold**: 32 px is a guess. May need tuning when the first auto-walk producer (gaming-mode return) ships. Tuneable via constructor arg.
3. **One-shot duration for `notification.sent`**: 2 s is arbitrary. Should match the Reviewing chain's animation duration when it's a sprite-pack `review` row (~1080 ms total per Codex spec). Leave at 2 s for now, revisit if it looks wrong.
