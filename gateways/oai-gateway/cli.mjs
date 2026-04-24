#!/usr/bin/env node

/**
 * oai-gateway — CLI tool for sending events to Oai desktop pet.
 *
 * Usage:
 *   oai-gateway --source <tool> --event <unified-name> [--tool-name <name>] [--file-path <path>] [--endpoint <path>]
 *   oai-gateway --ping [--endpoint <path>]
 *
 * Platform transport:
 *   Linux / macOS → Unix domain socket  (~/.oai/oai.sock)
 *   Windows       → Named pipe          (\\.\pipe\oai)
 *   Override      → --endpoint <path>
 */

import { platform } from 'node:process';
import { readFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { join } from 'node:path';
import { getEndpoint, sendToOai, pingOai } from './lib/ipc.mjs';

// --- Argument parsing -------------------------------------------------------

function parseArgs(argv) {
  const args = {};
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg.startsWith('--')) {
      const key = arg.slice(2);
      const next = argv[i + 1];
      if (next && !next.startsWith('--')) {
        args[key] = next;
        i++;
      } else {
        args[key] = true;
      }
    }
  }
  return args;
}

const args = parseArgs(process.argv);

// --- Ping mode -------------------------------------------------------------

if (args.ping) {
  const alive = await pingOai({ endpoint: args.endpoint });
  if (alive) {
    console.log('Oai is alive');
    process.exit(0);
  } else {
    console.log('Oai is not responding');
    process.exit(1);
  }
}

// --- Health check mode -------------------------------------------------------

if (args.health) {
  const alive = await pingOai({ endpoint: args.endpoint });
  if (alive) {
    console.log('Oai IPC server is healthy');
    process.exit(0);
  } else {
    console.log('Oai IPC server is not responding');
    process.exit(1);
  }
}

// --- Event mode ------------------------------------------------------------

if (!args.source || !args.event) {
  console.error('Usage: oai-gateway --source <tool> --event <name> [--tool-name <name>] [--file-path <path>] [--endpoint <path>]');
  console.error('       oai-gateway --ping [--endpoint <path>]');
  console.error('');
  console.error('Sources: opencode, claude-code, codex');
  console.error('Events: session.start, session.end, session.idle, session.error,');
  console.error('        prompt.submitted, tool.before, tool.after, tool.failed,');
  console.error('        permission.requested, permission.denied, permission.response,');
  console.error('        subagent.started, subagent.stopped, notification.sent,');
  console.error('        file.edited, file.watched, todo.updated');
  console.error('');
  console.error(`Platform: ${platform}  →  endpoint: ${getEndpoint()}`);
  process.exit(1);
}

// --- Auto-detect session name ------------------------------------------------

function detectSessionName(source) {
  try {
    if (source === 'claude-code') {
      // Claude Code stores session info in ~/.claude/sessions/<ppid>.json
      const ppid = process.ppid;
      const sessionFile = join(homedir(), '.claude', 'sessions', `${ppid}.json`);
      const data = JSON.parse(readFileSync(sessionFile, 'utf8'));
      return data.name || '';
    }
    if (source === 'codex') {
      // Codex stores session meta in ~/.codex/session_index.jsonl
      // Read last line for current session ID
      const indexFile = join(homedir(), '.codex', 'session_index.jsonl');
      const lines = readFileSync(indexFile, 'utf8').trim().split('\n');
      const last = JSON.parse(lines[lines.length - 1]);
      return last.id ? last.id.slice(0, 8) : '';
    }
    // OpenCode: no known session file — use --session flag
  } catch {
    // Silently fail — session name is optional
  }
  return '';
}

// --- Build message ----------------------------------------------------------

const message = {
  type: 'event',
  source: args.source,
  event: args.event,
};

// Session: explicit --session flag, or auto-detect from tool
message.session = args.session || detectSessionName(args.source) || '';
if (args['tool-name']) {
  message.toolName = args['tool-name'];
}
if (args['file-path']) {
  message.filePath = args['file-path'];
}

// --- Connect and send -------------------------------------------------------

try {
  await sendToOai(message, { endpoint: args.endpoint, retries: 2 });
} catch (err) {
  console.error(err.message);
  process.exit(1);
}
