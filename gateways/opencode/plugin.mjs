// Clippy plugin for OpenCode — maps OpenCode bus events to unified event names
// and sends them to the Clippy desktop pet via IPC.
//
// Platform transport:
//   Linux / macOS → Unix domain socket  (~/.clippy/clippy.sock)
//   Windows       → Named pipe          (\\.\pipe\clippy)

import { sendToClippy } from '@eastlake/clippy-gateway/lib/ipc.mjs';

// Unified event mapping from OpenCode bus events (D10 table)
const EVENT_MAP = {
  'session.created': 'session.start',
  'session.deleted': 'session.end',
  'session.idle': 'session.idle',
  'session.error': 'session.error',
  'tool.execute.before': 'tool.before',
  'tool.execute.after': 'tool.after',
  'permission.asked': 'permission.requested',
  'permission.replied': 'permission.response',
  'tui.toast.show': 'notification.sent',
  'file.edited': 'file.edited',
  'file.watcher.updated': 'file.watched',
  'todo.updated': 'todo.updated',
};

export default function clippyPlugin() {
  return {
    name: 'clippy',

    onEvent(event) {
      const unified = EVENT_MAP[event.name];
      if (!unified) return;

      const message = {
        type: 'event',
        source: 'opencode',
        event: unified,
      };

      if (event.data?.toolName) {
        message.toolName = event.data.toolName;
      }
      if (event.data?.filePath) {
        message.filePath = event.data.filePath;
      }

      // Fire-and-forget — don't block the OpenCode event bus
      sendToClippy(message).catch(() => {
        // Silently fail — Clippy might not be running
      });
    },
  };
}
