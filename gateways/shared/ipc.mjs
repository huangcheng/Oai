/**
 * ipc.mjs — Platform-aware IPC transport for Clippy desktop pet.
 *
 *   Linux / macOS → Unix domain socket
 *   Windows       → Named pipe
 *
 * Usage:
 *   import { getEndpoint, sendToClippy } from './ipc.mjs';
 *
 *   const endpoint = getEndpoint();                      // auto-detect
 *   const endpoint = getEndpoint('/custom/sock/path');   // override
 *   await sendToClippy({ type: 'event', source: 'opencode', event: 'session.start' });
 */

import { createConnection } from 'node:net';
import { homedir } from 'node:os';
import { platform } from 'node:process';

// ── Default endpoints per platform ──────────────────────────────────────────

const UNIX_SOCKET = () => `${homedir()}/.opencode-clippy/clippy.sock`;
const NAMED_PIPE  = '\\\\.\\pipe\\im.cheng.clippy';

/**
 * Return the default IPC endpoint for the current platform.
 * Pass an explicit path to override.
 */
export function getEndpoint(override) {
  if (override) return override;
  return platform === 'win32' ? NAMED_PIPE : UNIX_SOCKET();
}

// ── Send a message ─────────────────────────────────────────────────────────

/**
 * Send a JSON message to the Clippy desktop pet via IPC.
 *
 * @param {object} message  Must have at least `type` field.
 * @param {object} [opts]
 * @param {string} [opts.endpoint]  Custom endpoint (auto-detected if omitted)
 * @param {number} [opts.timeout]   Connection timeout in ms (default 3000)
 * @returns {Promise<void>}
 */
export function sendToClippy(message, opts = {}) {
  const endpoint = getEndpoint(opts.endpoint);
  const timeout  = opts.timeout ?? 3000;

  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(message) + '\n';

    const client = createConnection(endpoint, () => {
      client.write(payload, () => {
        client.end();
        resolve();
      });
    });

    client.on('error', (err) => {
      if (err.code === 'ENOENT') {
        reject(new Error(`Clippy IPC endpoint not found: ${endpoint}\nIs the Clippy desktop pet running?`));
      } else {
        reject(new Error(`IPC error (${endpoint}): ${err.message}`));
      }
    });

    const timer = setTimeout(() => {
      client.destroy();
      reject(new Error(`Timeout connecting to Clippy IPC endpoint: ${endpoint}`));
    }, timeout);

    client.on('close', () => clearTimeout(timer));
  });
}

// ── Helpers ────────────────────────────────────────────────────────────────

export { NAMED_PIPE, UNIX_SOCKET };
