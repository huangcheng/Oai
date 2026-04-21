#!/usr/bin/env node

/**
 * codex-jsonl-parser — Reads `codex exec --json` JSONL output and maps
 * events to unified names, forwarding them to Clippy's IPC endpoint.
 *
 * Usage:
 *   codex exec --json "prompt" | node parser.mjs [--endpoint <path>]
 *
 * Platform transport:
 *   Linux / macOS → Unix domain socket  (~/.clippy/clippy.sock)
 *   Windows       → Named pipe          (\\.\pipe\clippy)
 */

import { createInterface } from 'node:readline';
import { sendToClippy, getEndpoint } from '@eastlake/clippy-gateway/lib/ipc.mjs';

// Codex JSONL event → unified event mapping (D10 table)
const EVENT_MAP = {
  'thread.started': 'session.start',
  'turn.completed': 'session.idle',
  'turn.failed': 'session.error',
  'FileChange': 'file.edited',
  'TodoList': 'todo.updated',
};

// Item subtype mapping
const ITEM_SUBTYPE_MAP = {
  'FileChange': 'file.edited',
  'TodoList': 'todo.updated',
};

// --- Optional --endpoint override from CLI args -----------------------------
const endpointArg = process.argv.includes('--endpoint')
  ? process.argv[process.argv.indexOf('--endpoint') + 1]
  : undefined;

function send(message) {
  sendToClippy(message, { endpoint: endpointArg }).catch(() => {});
}

function processLine(line) {
  try {
    const event = JSON.parse(line);

    // Handle top-level thread/turn events
    if (event.type && EVENT_MAP[event.type]) {
      send({
        type: 'event',
        source: 'codex',
        event: EVENT_MAP[event.type],
      });
      return;
    }

    // Handle item events with subtypes
    if (event.type === 'item' && event.subtype) {
      const unified = ITEM_SUBTYPE_MAP[event.subtype];
      if (unified) {
        const message = {
          type: 'event',
          source: 'codex',
          event: unified,
        };

        if (event.filePath) {
          message.filePath = event.filePath;
        }

        send(message);
      }
    }

    // Handle PreToolUse/PostToolUse from interactive mode hooks
    if (event.hook === 'PreToolUse') {
      send({
        type: 'event',
        source: 'codex',
        event: 'tool.before',
        toolName: event.toolName,
      });
    } else if (event.hook === 'PostToolUse') {
      send({
        type: 'event',
        source: 'codex',
        event: 'tool.after',
        toolName: event.toolName,
      });
    }
  } catch (err) {
    // Ignore malformed JSON lines
    console.error(`Malformed JSONL line: ${line.substring(0, 100)}`);
  }
}

// Read from stdin (piped from codex exec --json)
const rl = createInterface({ input: process.stdin });
rl.on('line', processLine);

console.error(`codex-jsonl-parser: sending to ${getEndpoint(endpointArg)}`);
