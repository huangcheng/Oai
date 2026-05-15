---
name: seelie-gateway-setup
description: Install the @eastlake/seelie-gateway CLI and wire AI coding tools (Codex, Claude Code, Kimi-CLI, OpenCode, and similar) to forward lifecycle events to the Seelie desktop pet over UDP. Use when the user wants to "set up the gateway", "hook up Codex/Claude/Kimi/OpenCode to Seelie", or add a new tool integration.
license: MIT
metadata:
  author: seelie
  version: "1.0"
---

The gateway is a tiny Node.js CLI (`seelie-gateway`) that turns AI tool hook events into UDP datagrams the Seelie pet listens for on `127.0.0.1:52847`. Each tool's hook system invokes the CLI; the CLI normalizes to the 17 canonical Seelie event names.

## When to use

Triggers:
- "install/setup the gateway", "hook up <tool> to Seelie", "make the pet react to <tool>"
- "add gateway support for a new tool"
- debugging why the pet doesn't react to a specific tool's events
- the user mentions hooks for Codex, Claude Code, Kimi-CLI, OpenCode, Cursor, Aider, etc.

## Install the gateway

From the repo (development):
```sh
cd gateways/seelie-gateway && npm link
```

From npm (end users):
```sh
npm i -g @eastlake/seelie-gateway
```

Verify:
```sh
seelie-gateway --ping     # → "Seelie is alive" if the pet is running
seelie-gateway --health   # alias of --ping with different wording
```

## Canonical event vocabulary

The gateway accepts exactly these 17 names (passed via `--event`):

```
session.start  session.end  session.idle  session.error
prompt.submitted
tool.before  tool.after  tool.failed
permission.requested  permission.denied  permission.response
subagent.started  subagent.stopped
notification.sent
file.edited  file.watched  todo.updated
```

Each tool's native event names must be mapped onto these.

## CRITICAL: PATH in hook contexts

Most tools spawn hooks with a **minimal environment** — no fnm/nvm/asdf shims, no user shell rc, often no `$HOME/.local/bin`. A bare `seelie-gateway ...` command works in your terminal but silently fails from a hook.

**Solution: a wrapper script with absolute paths.** Save as `~/.local/bin/seelie-gateway-hook` (chmod +x):

```sh
#!/bin/sh
set -eu
# fnm users: locate the active node + the globally-linked CLI
fnm_root="$HOME/Library/Application Support/fnm/node-versions"
selected_base=""
for base in "$fnm_root"/*/installation; do
  [ -d "$base" ] || continue
  selected_base="$base"
done
[ -z "$selected_base" ] && { echo "no fnm installation found" >&2; exit 127; }
node_bin="$selected_base/bin/node"
gateway_cli="$selected_base/lib/node_modules/@eastlake/seelie-gateway/cli.mjs"
[ -x "$node_bin" ] || { echo "node not found" >&2; exit 127; }
[ -f "$gateway_cli" ] || { echo "gateway cli not found" >&2; exit 127; }
exec "$node_bin" "$gateway_cli" "$@"
```

For nvm users, replace the `fnm_root` block with `node_bin="$HOME/.nvm/versions/node/<version>/bin/node"`. For system Node, use `which node` output as `node_bin` and `npm root -g` as the prefix for `gateway_cli`.

Reference all hooks below at `~/.local/bin/seelie-gateway-hook` rather than `seelie-gateway`.

## Codex CLI

1. Enable hooks in `~/.codex/config.toml`:
   ```toml
   [features]
   codex_hooks = true
   ```

2. Write `~/.codex/hooks.json` (note: `"type": "command"` is **lowercase** — capital `C` is silently ignored):
   ```json
   {
     "hooks": {
       "SessionStart":      [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source codex --event session.start","timeout":5}]}],
       "UserPromptSubmit":  [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source codex --event prompt.submitted","timeout":5}]}],
       "PreToolUse":        [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source codex --event tool.before","timeout":5}]}],
       "PostToolUse":       [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source codex --event tool.after","timeout":5}]}],
       "PermissionRequest": [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source codex --event permission.requested","timeout":5}]}],
       "Stop":              [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source codex --event session.end","timeout":5}]}]
     }
   }
   ```

   Skip `"async": true` — UDP sends are fire-and-forget already, sync keeps ordering simple.

3. Session ID auto-detected by the gateway from `~/.codex/session_index.jsonl` — no `--session` flag needed.

## Claude Code

Edit `~/.claude/settings.json` (merge with existing settings):

```json
{
  "hooks": {
    "SessionStart":     [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source claude-code --event session.start"}]}],
    "UserPromptSubmit": [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source claude-code --event prompt.submitted"}]}],
    "PreToolUse":       [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source claude-code --event tool.before"}]}],
    "PostToolUse":      [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source claude-code --event tool.after"}]}],
    "Notification":     [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source claude-code --event notification.sent"}]}],
    "Stop":             [{"hooks":[{"type":"command","command":"/Users/you/.local/bin/seelie-gateway-hook --source claude-code --event session.end"}]}]
  }
}
```

Session ID auto-detected from `~/.claude/sessions/<ppid>.json`.

## Kimi-CLI

Hooks live in `~/.kimi/config.toml` as a TOML array of `[[hooks]]` tables. Schema (from `kimi_cli/hooks/config.py`): `event` (required), `command` (required, gets event JSON on stdin), `matcher` (regex, default `""` = match all), `timeout` (1–600 s, default 30).

Available events: `PreToolUse`, `PostToolUse`, `PostToolUseFailure`, `UserPromptSubmit`, `Stop`, `StopFailure`, `SessionStart`, `SessionEnd`, `SubagentStart`, `SubagentStop`, `PreCompact`, `PostCompact`, `Notification`.

```toml
[[hooks]]
event = "SessionStart"
command = "/Users/you/.local/bin/seelie-gateway-hook --source kimi-cli --event session.start"
timeout = 5

[[hooks]]
event = "UserPromptSubmit"
command = "/Users/you/.local/bin/seelie-gateway-hook --source kimi-cli --event prompt.submitted"
timeout = 5

[[hooks]]
event = "PreToolUse"
command = "/Users/you/.local/bin/seelie-gateway-hook --source kimi-cli --event tool.before"
timeout = 5

[[hooks]]
event = "PostToolUse"
command = "/Users/you/.local/bin/seelie-gateway-hook --source kimi-cli --event tool.after"
timeout = 5

[[hooks]]
event = "PostToolUseFailure"
command = "/Users/you/.local/bin/seelie-gateway-hook --source kimi-cli --event tool.failed"
timeout = 5

[[hooks]]
event = "Notification"
command = "/Users/you/.local/bin/seelie-gateway-hook --source kimi-cli --event notification.sent"
timeout = 5

[[hooks]]
event = "SessionEnd"
command = "/Users/you/.local/bin/seelie-gateway-hook --source kimi-cli --event session.end"
timeout = 5
```

## OpenCode

OpenCode uses JS plugins, not shell hooks — invoke the gateway as a child process. Save as `~/.config/opencode/plugins/seelie.mjs`:

```js
import { spawn } from 'node:child_process';

const HOOK = '/Users/you/.local/bin/seelie-gateway-hook';

// OpenCode bus event → Seelie canonical event
const EVENT_MAP = {
  'session.created':       'session.start',
  'session.idle':          'session.idle',
  'session.error':         'session.error',
  'message.user.created':  'prompt.submitted',
  'tool.execute.before':   'tool.before',
  'tool.execute.after':    'tool.after',
  'permission.requested':  'permission.requested',
  'permission.replied':    'permission.response',
  'file.edited':           'file.edited',
  'file.watcher.updated':  'file.watched',
};

export const SeeliePlugin = async () => ({
  event: async ({ event }) => {
    const mapped = EVENT_MAP[event.type];
    if (!mapped) return;
    spawn(HOOK, ['--source', 'opencode', '--event', mapped], {
      stdio: 'ignore',
      detached: true,
    }).unref();
  },
});
```

## Generic pattern: any other tool

If a tool can run a shell command on lifecycle events, the recipe is:

1. Identify the tool's hook/plugin/event mechanism and pick a `--source <name>` slug.
2. Map each native event to the closest canonical Seelie event from the 17-name list.
3. Invoke the wrapper: `/Users/you/.local/bin/seelie-gateway-hook --source <name> --event <canonical>`.
4. Optional flags: `--tool-name <Read|Write|Bash...>`, `--file-path <path>`, `--session <id>`, `--title "..."` `--content "..."` `--animation wave` for inline tips.

If the tool can't run shell commands but can run JS/Python plugins, spawn the wrapper as a detached child process (see OpenCode example).

## Verifying end-to-end

With Seelie running:
```sh
seelie-gateway --ping
seelie-gateway --source claude-code --event session.start  # pet should react
~/.local/bin/seelie-gateway-hook --source codex --event tool.before --tool-name Read   # test the wrapper
```

Watch the pet — every event maps to an animation/effect via `EventRouter` in `src/`. If no reaction, the event name probably isn't one of the 17 canonical names.

## Pitfalls

1. **Capital `"Command"` in Codex hooks.json** — silently ignored. Use lowercase `"command"`.
2. **Bare `seelie-gateway` in hooks** — works in terminals (fnm shim on PATH), fails from hooks. Always use the wrapper.
3. **Long `timeout`** — Codex/Kimi block on hooks up to the timeout. Keep ≤ 5 s; UDP send takes < 10 ms.
4. **Forgetting to enable `codex_hooks = true`** — Codex silently ignores `hooks.json` without the feature flag.
5. **`npm link` then switching Node versions with fnm** — the global symlink lives under one Node version's `lib/node_modules`. Either `npm link` again under the new version or hard-code that version's path in the wrapper.
6. **OpenCode plugin not detached** — without `detached: true` + `.unref()`, plugin keeps a handle on the child and can hang shutdown.
