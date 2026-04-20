## ADDED Requirements

### Requirement: IPC server with QLocalServer
The system SHALL start a QLocalServer listening on `~/.opencode-clippy/clippy.sock` (macOS/Linux) or `\\.\pipe\opencode-clippy` (Windows). The server SHALL accept connections from external tools (OpenCode plugin, CLI, etc.).

#### Scenario: IPC server starts successfully
- **WHEN** the application launches and no other instance is listening on the socket
- **THEN** the QLocalServer starts listening and logs the socket path

#### Scenario: Socket already in use
- **WHEN** the application launches and another process is already listening on the socket
- **THEN** the application detects the conflict, notifies the user, and exits (single-instance enforcement)

### Requirement: Newline-delimited JSON protocol
The system SHALL parse incoming data as newline-delimited JSON messages with three supported types:

1. **Event**: `{"type":"event","event":"<event-name>"}`
2. **Tip**: `{"type":"tip","title":"...","body":"...","animation":"<name>"}`
3. **Heartbeat**: `{"type":"ping"}` → responds with `{"type":"pong"}`

#### Scenario: Receive event message
- **WHEN** the server receives `{"type":"event","event":"message.updated"}\n`
- **THEN** the system parses the JSON and routes the event to the animation engine and speech bubble

#### Scenario: Receive tip message
- **WHEN** the server receives `{"type":"tip","title":"It looks like you're editing configuration!","body":"...","animation":"GetAttention"}\n`
- **THEN** the system displays the tip in a speech bubble and plays the specified animation

#### Scenario: Heartbeat response
- **WHEN** the server receives `{"type":"ping"}\n`
- **THEN** the server responds with `{"type":"pong"}\n`

### Requirement: Connection lifecycle management
The system SHALL accept multiple simultaneous connections. When a client disconnects, the system SHALL clean up the socket. When a new client connects, the system SHALL log the connection.

#### Scenario: Client connects and disconnects
- **WHEN** a client connects, sends an event, then disconnects
- **THEN** the event is processed, and the disconnected socket is cleaned up without errors

### Requirement: Resilient parsing
The system SHALL gracefully handle malformed JSON, incomplete messages, and unknown message types by logging a warning and continuing operation.

#### Scenario: Malformed JSON
- **WHEN** the server receives `{invalid json}\n`
- **THEN** the system logs a warning and continues accepting new messages

#### Scenario: Unknown message type
- **WHEN** the server receives `{"type":"unknown_type"}\n`
- **THEN** the system logs a warning and ignores the message

### Requirement: Unified event naming scheme
All gateway adapters SHALL map source-specific events to the canonical unified event names defined in the table below. The Clippy IPC server SHALL only handle unified event names; source-specific names are the adapter's responsibility.

| Unified Event | OpenCode | Claude Code | Codex |
|---|---|---|---|
| `session.start` | `session.created` | `SessionStart` | `SessionStart` / `thread.started` |
| `session.end` | `session.deleted` | `SessionEnd` | — |
| `session.idle` | `session.idle` | `Stop` | `turn.completed` |
| `session.error` | `session.error` | `StopFailure` | `turn.failed` |
| `prompt.submitted` | — | `UserPromptSubmit` | `UserPromptSubmit` |
| `tool.before` | `tool.execute.before` | `PreToolUse` | `PreToolUse` |
| `tool.after` | `tool.execute.after` | `PostToolUse` | `PostToolUse` |
| `tool.failed` | — | `PostToolUseFailure` | — |
| `permission.requested` | `permission.asked` | `PermissionRequest` | `PermissionRequest` |
| `permission.denied` | — | `PermissionDenied` | — |
| `permission.response` | `permission.replied` | — | — |
| `subagent.started` | — | `SubagentStart` | — |
| `subagent.stopped` | — | `SubagentStop` | — |
| `notification.sent` | `tui.toast.show` | `Notification` | — |
| `file.edited` | `file.edited` | `FileChanged` | item subtype `FileChange` |
| `file.watched` | `file.watcher.updated` | `FileChanged` | — |
| `todo.updated` | `todo.updated` | `TaskCreated` / `TaskCompleted` | item subtype `TodoList` |

#### Scenario: Unknown unified event is rejected
- **WHEN** a gateway adapter sends an event with a name NOT in the unified table
- **THEN** the system logs a warning and ignores the message (same as unknown message type)

#### Scenario: Source maps events correctly
- **WHEN** the OpenCode adapter receives `session.created` from the OpenCode event bus
- **THEN** it sends `{"type":"event","event":"session.start","source":"opencode"}` to the IPC socket

### Requirement: Gateway adapter architecture
Each supported tool (OpenCode, Claude Code, Codex) SHALL have a dedicated gateway adapter that subscribes to its native event system, maps events to unified names, and forwards them over the IPC socket. Adapters SHALL be stateless and decoupled from the Clippy app.

#### Scenario: OpenCode gateway via plugin
- **WHEN** the OpenCode plugin at `packages/plugin/src/clippy.ts` receives a bus event (e.g. `tool.execute.after`)
- **THEN** it maps the event to the unified name (`tool.after`), attaches `source: "opencode"`, and sends the message to `~/.opencode-clippy/clippy.sock`

#### Scenario: Claude Code gateway via command hooks
- **WHEN** Claude Code triggers a hook event (e.g. `PreToolUse` with tool `Edit`)
- **THEN** the command hook configured in `~/.claude/settings.json` invokes `clippy-gateway --source claude-code --event tool.before` with `"async": true` so it does not block Claude Code

#### Scenario: Claude Code hook filtering via matcher
- **WHEN** a hook is configured with `"matcher": "Edit|Write"` under `PreToolUse`
- **THEN** only `Edit` and `Write` tool uses trigger the hook; other tools are ignored

#### Scenario: Codex gateway via hooks (interactive mode)
- **WHEN** Codex is running interactively with `codex_hooks = true` in config.toml
- **THEN** hooks in `~/.codex/hooks.json` fire `clippy-gateway --source codex --event <unified-name>` for `PreToolUse` and `PostToolUse` on the Bash tool only

#### Scenario: Codex gateway via JSONL stream (non-interactive mode)
- **WHEN** `codex exec --json` emits a JSONL line containing `"type":"item","item":{"subtype":"CommandExecution"}}`
- **THEN** the JSONL parser maps it to `tool.after`, attaches `source: "codex"`, and forwards to the IPC socket

### Requirement: Event message enrichment
All event messages sent by any gateway adapter SHALL include a `source` field identifying the originating tool. The Clippy app SHALL process events based on the unified event name and MAY use the `source` and additional metadata for display hints only.

#### Scenario: Event message includes source
- **WHEN** any gateway adapter sends an event
- **THEN** the JSON message includes `"source"` set to one of `"opencode"`, `"claude-code"`, or `"codex"`

#### Scenario: Additional metadata is preserved
- **WHEN** a gateway adapter includes extra fields such as `tool_name`, `file_path`, or `duration_ms`
- **THEN** the Clippy app accepts the message without error and ignores unrecognized fields

#### Scenario: Message without source field is rejected
- **WHEN** a message arrives with `{"type":"event","event":"tool.after"}` but no `source` field
- **THEN** the system logs a warning and ignores the message
