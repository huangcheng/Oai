/**
 * ipc.mjs — IPC transport for Oai desktop pet.
 *
 * Uses UDP localhost (127.0.0.1:52847) for zero-overhead fire-and-forget.
 */

import { createSocket } from 'node:dgram';

// ── Default endpoint ────────────────────────────────────────────────────────

const DEFAULT_ENDPOINT = '127.0.0.1:52847';

export function getEndpoint(override) {
  return override || DEFAULT_ENDPOINT;
}

function parseEndpoint(endpoint) {
  const idx = endpoint.lastIndexOf(':');
  if (idx > 0) {
    const host = endpoint.slice(0, idx);
    const port = parseInt(endpoint.slice(idx + 1), 10);
    if (!isNaN(port)) {
      return { host, port };
    }
  }
  return { host: '127.0.0.1', port: 52847 };
}

// ── Send a message ─────────────────────────────────────────────────────────

export function sendToOai(message, opts = {}) {
  const endpoint = getEndpoint(opts.endpoint);
  const { host, port } = parseEndpoint(endpoint);

  return new Promise((resolve, reject) => {
    const socket = createSocket('udp4');
    const payload = Buffer.from(JSON.stringify(message) + '\n');

    socket.send(payload, port, host, (err) => {
      socket.close();
      if (err) {
        reject(new Error(`IPC error (${endpoint}): ${err.message}`));
      } else {
        resolve();
      }
    });
  });
}

// ── Ping / health check ────────────────────────────────────────────────────

export function pingOai(opts = {}) {
  const endpoint = getEndpoint(opts.endpoint);
  const { host, port } = parseEndpoint(endpoint);
  const timeout = opts.timeout ?? 2000;

  return new Promise((resolve) => {
    const socket = createSocket('udp4');
    let resolved = false;

    socket.on('message', (data) => {
      try {
        const msg = JSON.parse(data.toString().trim());
        if (msg.type === 'pong' && !resolved) {
          resolved = true;
          socket.close();
          resolve(true);
        }
      } catch {
        // ignore malformed
      }
    });

    socket.on('error', () => {
      if (!resolved) {
        resolved = true;
        socket.close();
        resolve(false);
      }
    });

    socket.send('{"type":"ping"}\n', port, host, (err) => {
      if (err && !resolved) {
        resolved = true;
        socket.close();
        resolve(false);
      }
    });

    setTimeout(() => {
      if (!resolved) {
        resolved = true;
        socket.close();
        resolve(false);
      }
    }, timeout);
  });
}

export { DEFAULT_ENDPOINT };
