/**
 * ipc.mjs — IPC transport for Orai desktop pet.
 *
 * Uses TCP localhost (127.0.0.1:52847) for cross-platform compatibility.
 *
 * Usage:
 *   import { getEndpoint, sendToOrai } from './ipc.mjs';
 *
 *   const endpoint = getEndpoint();                        // auto-detect
 *   const endpoint = getEndpoint('127.0.0.1:99999');       // override
 *   await sendToOrai({ type: 'event', source: 'opencode', event: 'session.start' });
 */

import { createConnection } from 'node:net';

// ── Default endpoint ────────────────────────────────────────────────────────

const DEFAULT_ENDPOINT = '127.0.0.1:52847';

/**
 * Return the default IPC endpoint.
 * Pass an explicit "host:port" string to override.
 */
export function getEndpoint(override) {
  return override || DEFAULT_ENDPOINT;
}

// ── Send a message ─────────────────────────────────────────────────────────

/**
 * Send a JSON message to the Orai desktop pet via IPC.
 *
 * @param {object} message  Must have at least `type` field.
 * @param {object} [opts]
 * @param {string} [opts.endpoint]  Custom endpoint "host:port" (auto-detected if omitted)
 * @param {number} [opts.timeout]   Connection timeout in ms (default 3000)
 * @returns {Promise<void>}
 */
export function sendToOrai(message, opts = {}) {
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
      if (err.code === 'ECONNREFUSED') {
        reject(new Error(`Orai IPC endpoint not reachable: ${endpoint}\nIs the Orai desktop pet running?`));
      } else {
        reject(new Error(`IPC error (${endpoint}): ${err.message}`));
      }
    });

    const timer = setTimeout(() => {
      client.destroy();
      reject(new Error(`Timeout connecting to Orai IPC endpoint: ${endpoint}`));
    }, timeout);

    client.on('close', () => clearTimeout(timer));
  });
}

// ── Helpers ────────────────────────────────────────────────────────────────

export { DEFAULT_ENDPOINT };
