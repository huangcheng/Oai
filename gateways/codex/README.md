# @clippy/codex

Codex hooks and JSONL parser for the Clippy desktop pet.

## Install

```bash
npm install -g @eastlake/clippy-gateway
npm install -g @clippy/codex
npx @eastlake/clippy-codex
```

The last command copies the hooks to `~/.codex/hooks.json`.

## Non-interactive mode

Pipe `codex exec --json` through the parser:

```bash
codex exec --json "your prompt" | npx @clippy/codex parser.mjs
```

Or use the CLI directly:

```bash
codex exec --json "your prompt" | clippy-codex-parser
```
