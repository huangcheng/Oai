#!/usr/bin/env node

/**
 * qlippy-gateway — CLI tool for sending events to Qlippy desktop pet.
 *
 * Usage:
 *   clippy-gateway --source <tool> --event <unified-name> [--tool-name <name>] [--file-path <path>] [--endpoint <path>]
 *   clippy-gateway --ping [--endpoint <path>]
 *
 * Platform transport:
 *   Linux / macOS → Unix domain socket  (~/.qlippy/qlippy.sock)
 *   Windows       → Named pipe          (\\.\pipe\qlippy)
 *   Override      → --endpoint <path>
 */

import { platform } from 'node:process';
import { getEndpoint, sendToQlippy, pingQlippy } from './lib/ipc.mjs';

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
  const alive = await pingQlippy({ endpoint: args.endpoint });
  if (alive) {
    console.log('Qlippy is alive');
    process.exit(0);
  } else {
    console.error('Qlippy is not responding');
    process.exit(1);
  }
}

// --- Event mode ------------------------------------------------------------

if (!args.source || !args.event) {
  console.error('Usage: qlippy-gateway --source <tool> --event <name> [--tool-name <name>] [--file-path <path>] [--endpoint <path>]');
  console.error('       qlippy-gateway --ping [--endpoint <path>]');
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

// --- Build message ----------------------------------------------------------

const message = {
  type: 'event',
  source: args.source,
  event: args.event,
};

if (args['tool-name']) {
  message.toolName = args['tool-name'];
}
if (args['file-path']) {
  message.filePath = args['file-path'];
}

// --- Connect and send -------------------------------------------------------

try {
  await sendToQlippy(message, { endpoint: args.endpoint, retries: 2 });
} catch (err) {
  console.error(err.message);
  process.exit(1);
}
