# tts-voice-cache

## ADDED Requirements

### Requirement: Cache key is derived from text and relevant config

The cache SHALL compute a SHA-256 hash from the concatenation of the active provider ID, selected voice ID, model ID, and input text. The resulting hex string SHALL form the filename of the cached audio file with `.mp3` extension.

#### Scenario: Same text, same config produces same key

- **WHEN** `speak("Hello")` is called with active provider `stepfun`, voice `cixingnansheng`, model `stepaudio-2.5-tts`
- **THEN** cache key is SHA-256(`stepfun|cixingnansheng|stepaudio-2.5-tts|Hello`)
- **AND** cache lookup checks for file `{key}.mp3`

#### Scenario: Different text produces different key

- **WHEN** `speak("Hello")` and `speak("World")` are called with identical config
- **THEN** cache key for first call differs from cache key for second call
- **AND** first call is a cache miss, second call is a cache miss

#### Scenario: Voice change produces different key

- **WHEN** `speak("Hello")` is called with voice `cixingnansheng`, then voice is changed to `linjiajiejie`
- **AND** `speak("Hello")` is called again
- **THEN** the second call produces a different cache key
- **AND** second call is a cache miss

#### Scenario: Provider switch produces different key

- **WHEN** `speak("Hello")` is called with provider `stepfun`, then provider is switched to `minimax`
- **AND** `speak("Hello")` is called again
- **THEN** the second call produces a different cache key
- **AND** second call is a cache miss

### Requirement: Cache lookup returns cached audio on hit

When a `speak()` call is invoked and a cached audio file matching the computed cache key exists on disk, the system SHALL return the cached audio bytes immediately without making any network request to the TTS provider.

#### Scenario: Cache hit returns audio without network request

- **WHEN** `speak("Hello")` is called and `{key}.mp3` exists in the cache directory
- **THEN** the cached MP3 bytes are returned directly
- **AND** no HTTP request is made to any TTS provider
- **AND** audio playback proceeds with the cached content

#### Scenario: Cache hit on repeated tip text

- **WHEN** the same tip text is triggered twice in succession (debounce allows only one in-flight request)
- **AND** the first call populated the cache
- **WHEN** the same text is spoken after audio playback completes
- **THEN** second call returns cached audio immediately

### Requirement: Cache miss triggers synthesis and stores result

When a `speak()` call is invoked and no cached audio file exists for the computed key, the system SHALL call the TTS provider as normal, and after receiving the audio, SHALL write the audio bytes to a file named `{cache_key}.mp3` in the cache directory.

#### Scenario: Cache miss synthesizes and caches

- **WHEN** `speak("Hello")` is called and no cache file exists
- **THEN** TTS provider is called via `ITtsProvider::synthesize()`
- **AND** on successful audio reception, the audio bytes are written to `{key}.mp3`
- **AND** audio playback proceeds normally

#### Scenario: Cache miss due to new text

- **WHEN** a brand new tip message is triggered
- **THEN** cache miss occurs
- **AND** audio is synthesized and cached for future use

### Requirement: Cache is invalidated when cache-relevant config changes

The system SHALL listen for the `ttsCacheInvalidated` signal from `ConfigManager`. On emission of this signal, the system SHALL delete all files in the cache directory.

#### Scenario: Voice change invalidates cache

- **WHEN** user changes voice from `cixingnansheng` to `linjiajiejie` in settings
- **THEN** `ConfigManager` emits `ttsCacheInvalidated`
- **AND** all files in `~/.cache/Seelie/tts_voice_cache/` are deleted
- **AND** next `speak()` call repopulates cache with new voice

#### Scenario: Provider switch invalidates cache

- **WHEN** user switches TTS provider from `stepfun` to `minimax`
- **THEN** `ConfigManager` emits `ttsCacheInvalidated`
- **AND** all files in `~/.cache/Seelie/tts_voice_cache/` are deleted

#### Scenario: Model change invalidates cache

- **WHEN** user changes model from `stepaudio-2.5-tts` to `stepaudio-3-tts`
- **THEN** `ConfigManager` emits `ttsCacheInvalidated`
- **AND** all files in `~/.cache/Seelie/tts_voice_cache/` are deleted

### Requirement: Cache directory is created on first access

The system SHALL create the cache directory (`~/.cache/Seelie/tts_voice_cache/`) using `QStandardPaths::CacheLocation` if it does not already exist, before performing any cache read or write operations.

#### Scenario: First speak call creates cache directory

- **WHEN** `speak("Hello")` is called on first app launch
- **AND** the cache directory does not exist
- **THEN** the directory is created via `QDir::mkpath()`
- **AND** cache lookup/write proceeds normally

### Requirement: Cache operates transparently to audio pipeline

The cache layer SHALL be integrated inside `TTSEngine::doSynthesize()` such that the audio playback pipeline (QAudioDecoder → QAudioSink), error handling, retry logic, and cancellation behavior remain unchanged.

#### Scenario: Cache hit still plays through normal audio pipeline

- **WHEN** cache hit occurs and cached MP3 is available
- **THEN** audio bytes are passed to `QAudioDecoder` exactly as synthesized bytes would be
- **AND** all existing playback, error, and cancellation behaviors apply unchanged

#### Scenario: Cache miss follows normal synthesis path

- **WHEN** cache miss occurs
- **THEN** existing `TTSEngine::doSynthesize()` flow executes unchanged
- **AND** only the cache write step is added after successful `onSynthesisSuccess`

### Requirement: Cache file uses SHA-256 hex string as filename

The cache file SHALL be named using the 64-character hexadecimal SHA-256 digest of the cache key input, with a `.mp3` extension. No other metadata files SHALL be created alongside the audio file.

#### Scenario: Cache filename format

- **WHEN** cache key is computed as SHA-256 of `stepfun|cixingnansheng|stepaudio-2.5-tts|Hello`
- **THEN** resulting filename is `a7b3c2d1e4f5...64charhex..0.mp3` (64 hex chars)
- **AND** file contains raw MP3 audio bytes

### Requirement: Synthesis failure does not create cache entry

When a TTS provider returns an error for a cache-miss request, the system SHALL NOT write any file to the cache directory.

#### Scenario: Auth failure does not cache

- **WHEN** `speak("Hello")` is called with invalid API token
- **AND** provider returns authentication error
- **THEN** no file is written to the cache directory
- **AND** error is reported to user via normal error handling path
