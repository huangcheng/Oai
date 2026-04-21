/**
 * ipc.mjs — Platform-aware IPC transport for Qlippy desktop pet.
 *
 *   Linux / macOS → Unix domain socket
 *   Windows       → Named pipe
 *
 * Usage:
 *   import { getEndpoint, sendToQlippy, pingQlippy } from './ipc.mjs';
 *
 *   const endpoint = getEndpoint();                      // auto-detect
 *   const endpoint = getEndpoint('/custom/sock/path');   // override
 *   await sendToQlippy({ type: 'event', source: 'opencode', event: 'session.start' });
 *   const alive = await pingQlippy();                    // health check
 */

import { createConnection } from 'node:net';
import { homedir } from 'node:os';
import { platform } from 'node:process';

// ── Default endpoints per platform ──────────────────────────────────────────

const UNIX_SOCKET = () => `${homedir()}/.qlippy/qlippy.sock`;
const NAMED_PIPE  = '\\\\.\\pipe\\im.cheng.qlippy';

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
 * Send a JSON message to the Qlippy desktop pet via IPC.
 *
 * @param {object} message  Must have at least `type` field.
 * @param {object} [opts]
 * @param {string} [opts.endpoint]  Custom endpoint (auto-detected if omitted)
 * @param {number} [opts.timeout]   Connection timeout in ms (default 3000)
 * @param {number} [opts.retries]   Number of retries on failure (default 0)
 * @returns {Promise<void>}
 */
export function sendToQlippy(message, opts = {}) {
  const endpoint = getEndpoint(opts.endpoint);
  const timeout  = opts.timeout ?? 3000;
  const retries  = opts.retries ?? 0;

  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(message) + '\n';
    let attempts = 0;

    function trySend() {
      attempts++;
      const client = createConnection(endpoint, () => {
        client.write(payload, () => {
          client.end();
          resolve();
        });
      });

      client.on('error', (err) => {
        if (attempts <= retries) {
          setTimeout(trySend, 500 * attempts); // exponential backoff
          return;
        }
        if (err.code === 'ENOENT') {
          reject(new Error(`Qlippy IPC endpoint not found: ${endpoint}\nIs the Qlippy desktop pet running?`));
        } else {
          reject(new Error(`IPC error (${endpoint}): ${err.message}`));
        }
      });

      const timer = setTimeout(() => {
        client.destroy();
        if (attempts <= retries) {
          setTimeout(trySend, 500 * attempts);
        } else {
          reject(new Error(`Timeout connecting to Qlippy IPC endpoint: ${endpoint}`));
        }
      }, timeout);

      client.on('close', () => clearTimeout(timer));
    }

    trySend();
  });
}

// ── Heartbeat / health check ───────────────────────────────────────────────

/**
 * Send a ping to Qlippy and wait for a pong response.
 *
 * @param {object} [opts]
 * @param {string} [opts.endpoint]  Custom endpoint (auto-detected if omitted)
 * @param {number} [opts.timeout]   Response timeout in ms (default 2000)
 * @returns {Promise<boolean>}  true if Qlippy responded, false otherwise
 */
export function pingQlippy(opts = {}) {
  const endpoint = getEndpoint(opts.endpoint);
  const timeout  = opts.timeout ?? 2000;

  return new Promise((resolve) => {
    const client = createConnection(endpoint, () => {
      client.write('{"type":"ping"}\n');
    });

    let resolved = false;

    client.on('data', (data) => {
      const lines = data.toString().trim().split('\n');
      for (const line of lines) {
        try {
          const msg = JSON.parse(line);
          if (msg.type === 'pong' && !resolved) {
            resolved = true;
            client.end();
            resolve(true);
          }
        } catch {
          // ignore malformed lines
        }
      }
    });

    client.on('error', () => {
      if (!resolved) {
        resolved = true;
        resolve(false);
      }
    });

    client.on('close', () => {
      if (!resolved) {
        resolved = true;
        resolve(false);
      }
    });

    setTimeout(() => {
      if (!resolved) {
        resolved = true;
        client.destroy();
        resolve(false);
      }
    }, timeout);
  });
}

// ── Helpers ────────────────────────────────────────────────────────────────

export { NAMED_PIPE, UNIX_SOCKET };
