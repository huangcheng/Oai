## Context

The Seelie desktop pet uses TTS (text-to-speech) to vocalize tips and notifications via 4 provider backends: StepFun, MiniMax, Azure Speech, and OpenAI. The `TTSEngine` sits on its own QThread and calls `ITtsProvider::synthesize()` for every `speak()` invocation. There is no caching — identical text spoken twice results in two billable API calls.

The tip content is quasi-static: bubble text comes from a fixed set of `TipDatabase` entries and user notification strings generated from a known template set. This means a significant fraction of `speak()` calls are repeated text.

The relevant TTS config that affects voice output (and thus cache validity) per provider:
- `tts/activeProvider` — switching providers produces different audio even for same text
- `tts/providers/<id>/voice` — voice ID within a provider
- `tts/providers/<id>/model` — model ID within a provider (StepFun and OpenAI have model selection)

The following config fields are **not** cache-relevant (changing them does not require cache invalidation): `token`, `baseUrl` (StepFun), `region` (Azure).

## Goals / Non-Goals

**Goals:**
- Eliminate redundant TTS API calls for repeated text
- Reduce latency on cache hits (instant audio vs. network round-trip)
- Persist cache across app restarts (disk-based, not in-memory)
- Automatically invalidate cache when any cache-relevant config changes
- Be fully transparent to the rest of the TTS engine — cache is a layer inside `TTSEngine`

**Non-Goals:**
- Cache invalidation based on TTL or size limits (bounded only by user manually clearing cache directory)
- User-facing cache management UI
- Caching for streaming / partial audio responses
- Supporting cache across different text that semantically means the same

## Decisions

### 1. Cache Key Strategy

**Decision:** Cache key = SHA-256 of `activeProvider | voice_id | model_id | text`, stored as `SHA256.hexstring.mp3`.

**Rationale:** The fingerprint must uniquely identify the audio that would be returned by the provider. Using only text as key would cause incorrect behavior when a user switches voice or provider — the old cached audio would be served for the new config. Concatenating all three config values ensures any relevant config change produces a different key and thus a cache miss.

**Alternatives considered:**
- Store cache under a versioned namespace per provider+voice+model triple — same idea, but SHA-256 of the concatenated string is simpler and collision-resistant.

### 2. Cache Storage Location

**Decision:** `~/.cache/Seelie/tts_voice_cache/` (Qt `QStandardPaths::CacheLocation`).

**Rationale:** Follows the XDG cache convention. `QStandardPaths` resolves this correctly on macOS (`~/Library/Caches/Seelie/`), Windows, and Linux. The cache survives app restarts and does not pollute the config directory.

### 3. Cache Invalidation Mechanism

**Decision:** `ConfigManager` emits a new signal `ttsCacheInvalidated()` whenever `activeProvider`, `voice`, or `model` changes for any provider. `TTSEngine` connects to this signal and wipes the cache directory on emission.

**Rationale:** The config layer already has signals for individual field changes (`ttsProviderFieldChanged`). Rather than a new signal per field, a single `ttsCacheInvalidated` signal is emitted after any of the three cache-relevant fields change. `TTSEngine` is already connected to `ConfigManager` for provider hot-swap, so adding one more signal is minimal.

**Alternative considered:** Store a cache manifest JSON file alongside each MP3 recording the config fingerprint at write time, and validate on read. Adds complexity and is unnecessary — invalidation on config change is sufficient because:
- Cache reads only happen when the current config matches what was used to write the cached file
- If config changes, we simply wipe the entire cache; the next speak repopulates it

### 4. Cache Lookup Order

**Decision:**
1. Compute cache key from current config fingerprint + text
2. If `cache_dir/{key}.mp3` exists → return audio bytes immediately (cache hit)
3. If not found → call provider `synthesize()` as today, then write result to `{key}.mp3`

**Rationale:** This keeps the cache as a transparent middleware layer inside `TTSEngine::doSynthesize()`. No changes to provider adapters or the audio playback pipeline.

### 5. Thread Safety

**Decision:** All cache file operations (read, write, exists check, wipe) happen on the `TTSEngine` thread (the same thread that calls the provider). No additional mutex needed because Qt's signal-slot queue serializes calls from the main thread.

**Rationale:** `TTSEngine` owns its own event loop on a dedicated thread. Only one `speak()` call processes at a time (the debounce flag prevents concurrent `doSynthesize`). Disk I/O is fast enough for the expected cache hit rate that a background worker thread adds unnecessary complexity.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Cache grows unbounded over time | Users can manually delete `~/.cache/Seelie/tts_voice_cache/`. Future enhancement: add a cache size limit with LRU eviction. |
| Cache serves stale audio after provider API update | Cache is invalidated on any relevant config change. A provider-side API change without config change would serve stale audio — unlikely in practice and recoverable by manual cache clear. |
| SHA-256 collision (theoretical) | SHA-256 output space is 2^256; practical collisions for short text inputs are astronomically unlikely. |
| Concurrent cache writes from simultaneous `speak()` calls | Debounce in `TTSEngine` prevents concurrent `doSynthesize()`. Only one synthesis + write at a time. |

## Open Questions

1. Should cache size be bounded? If yes, implement LRU eviction on write — deferred to a future change.
2. Should the cache be encrypted? The cached MP3 files contain the TTS audio output but do not contain credentials — encryption adds complexity for no real security benefit.
