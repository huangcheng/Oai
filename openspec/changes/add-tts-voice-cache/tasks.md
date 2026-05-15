## 1. ConfigManager: Add cache invalidation signal

- [x] 1.1 Add `void ttsCacheInvalidated()` signal to `ConfigManager.h`
- [x] 1.2 Emit `ttsCacheInvalidated()` in `ConfigManager::setTtsProviderField()` when field name is `voice` or `model`
- [x] 1.3 Emit `ttsCacheInvalidated()` in `ConfigManager::setTtsActiveProvider()` when provider changes

## 2. TtsVoiceCache: New class

- [x] 2.1 Create `src/tts/TtsVoiceCache.h` with class declaration:
  - `cacheKey(providerId, voiceId, modelId, options, text)` → `QString` (SHA-256 hex)
  - `cacheDir()` → `QString` (pure const accessor; creation is internal)
  - `hasCachedAudio(key)` → `bool`
  - `getCachedAudio(key)` → `QByteArray`
  - `getCachedMimeType(key)` → `QString`
  - `writeCachedAudio(key, audioData, mimeType)` → `bool`
  - `wipeAll()` → `void`
- [x] 2.2 Create `src/tts/TtsVoiceCache.cpp` with implementation using QCryptographicHash for SHA-256
- [x] 2.3 Persist mime type as a `.mime` sidecar so non-MP3 providers replay correctly
- [x] 2.4 Bound cache at 100 MB with LRU eviction by mtime; expose `setMaxBytes()`
- [x] 2.5 Register new class with CMakeLists.txt alongside other tts sources

## 3. TTSEngine: Integrate cache into synthesis pipeline

- [x] 3.1 Add `#include "tts/TtsVoiceCache.h"` and `std::unique_ptr<TtsVoiceCache> m_voiceCache` member to `TTSEngine.h`
- [x] 3.2 Construct `m_voiceCache` in `TTSEngine::initOnThread()` after provider is set
- [x] 3.3 Connect `m_config->ttsCacheInvalidated` to `m_voiceCache->wipeAll` (queued)
- [x] 3.4 In `TTSEngine::doSynthesize()`:
  - Snapshot `(activeProvider, voice, model, opts, text)` into `m_pendingCacheKey`
  - Check `m_voiceCache->hasCachedAudio(key)` before calling provider
  - If cache hit: skip provider call, replay cached audio with cached mime type
  - If cache miss: after `onSynthesisSuccess`, write `(audio, mimeType)` under the snapshotted key
- [x] 3.5 Do NOT cache on synthesis failure (auth error, network error, etc.)
- [x] 3.6 Add `clearVoiceCache()` slot for the settings UI

## 4. SettingsPanelWidget: Clear-cache button

- [x] 4.1 Add **Clear voice cache** button to the TTS tab
- [x] 4.2 Emit `clearVoiceCacheRequested()` on click
- [x] 4.3 Wire the signal to `TTSEngine::clearVoiceCache` in MainWindow
- [x] 4.4 Translate the button label in `retranslateUi()`

## 5. Build and compile

- [x] 5.1 Add `TtsVoiceCache.{h,cpp}` to `CMakeLists.txt`
- [x] 5.2 Add `TtsVoiceCache.{h,cpp}` to `tests/CMakeLists.txt`
- [x] 5.3 Build project: `cd build && cmake .. && cmake --build .`
- [x] 5.4 Verify no compile errors

## 6. Tests

- [x] 6.1 Add `test_tts_voice_cache.cpp` to `tests/`:
  - `testCacheKeyDeterministic`: same inputs → same SHA-256 key
  - `testCacheKeyDifferentPerProvider`: different provider → different key
  - `testCacheKeyDifferentPerVoice`: different voice → different key
  - `testCacheKeyDifferentPerModel`: different model → different key
  - `testCacheKeyDifferentPerOptions`: different SpeakOptions → different key
  - `testCacheKeyNormalizesWhitespace`: `"Hello"` and `" Hello "` collapse to one entry
  - `testCacheHit`: write then read returns same bytes
  - `testCacheMiss`: non-existent key returns false
  - `testWipeAll`: `wipeAll()` deletes all files
  - `testEmptyAudioNotCached`: refuses to cache empty audio (no sidecar leaked)
  - `testMimeTypeRoundTrip`: stored mime survives a write/read round trip
  - `testLruEvictionOnOverflow`: oldest entry evicted when cap exceeded
- [x] 6.2 Run tests: `cd build && ctest -R tts_voice_cache -V`
