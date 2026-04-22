# @qlippy/codex

Codex hooks and JSONL parser for the Qlippy desktop pet.

## Install

```bash
npm install -g @huangcheng/qlippy-gateway
npm install -g @qlippy/codex
npx @huangcheng/qlippy-codex
```

The last command copies the hooks to `~/.codex/hooks.json`.

## Non-interactive mode

Pipe `codex exec --json` through the parser:

```bash
codex exec --json "your prompt" | npx @qlippy/codex parser.mjs
```

Or use the CLI directly:

```bash
codex exec --json "your prompt" | qlippy-codex-parser
```
