import { sendToClippy } from '@eastlake/clippy-gateway/lib/ipc.mjs';

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

export const clippy = async () => {
  return {
    event: async ({ event }) => {
      const unified = EVENT_MAP[event.type];
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

      sendToClippy(message).catch(() => {});
    },
  };
};
