## Why

TTS voice generation currently makes a fresh HTTP request for every `speak()` call, even when the same text has been spoken before. Since the Oai desktop pet uses a fixed set of tip messages and notification texts that repeat frequently, this results in redundant API calls that waste tokens and increase latency. A cache layer using the input text as key will eliminate duplicate requests.

## What Changes

- Add a persistent file-based cache for TTS audio results
- Cache key = hash of (text + provider + voice + model) ensuring provider/voice switches correctly invalidate cache
- Cache auto-invalidates when any TTS config that affects voice generation changes
- Cache hit returns audio immediately without network request
- First-ever speak of a text phrase triggers cache population
- No user-visible UI changes; cache operates transparently

## Capabilities

### New Capabilities

- `tts-voice-cache`: Persistent disk cache for TTS audio, keyed by text + provider config fingerprint, auto-invalidated on relevant config changes

### Modified Capabilities

- (none — no existing spec behavior changes)

## Impact

- **New files**: `src/tts/TtsVoiceCache.{h,cpp}` — cache storage and lookup logic
- **Modified files**: `src/TTSEngine.{h,cpp}` — integrate cache into synthesize pipeline; `src/ConfigManager.{h,cpp}` — emit signal on relevant config changes for cache invalidation
- **Config signal**: `ConfigManager` gains a new signal `ttsCacheInvalidated` triggered when voice/model/activeProvider changes
- **Storage**: cache stored in `~/.cache/Oai/tts_voice_cache/` as `{hash}.mp3` files
- **No external dependencies** — uses Qt's file I/O only
