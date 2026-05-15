# TTS Provider Abstraction — Design

**Date:** 2026-05-14
**Status:** Approved for planning
**Author:** HUANG Cheng (with Claude)

## Background

The current `TTSEngine` (`src/TTSEngine.h`/`.cpp`) is hard-wired to StepFun's realtime WebSocket protocol: `tts.create` → `tts.text.delta` → `tts.text.flush` → `tts.text.done`, base64 PCM chunks fed to `QAudioSink`, heartbeat ping every 15s, exponential-backoff retry. `ConfigManager` exposes `ttsBaseUrl`, `ttsToken`, `ttsModel`, `ttsLanguage`, `ttsVoice` as a flat StepFun-shaped config.

`openspec/changes/add-tts-ai-tab/design.md` anticipated multi-provider support via "internal provider adapters" but never implemented them. We now want first-class support for StepFun, MiniMax, Azure Speech, and OpenAI TTS, with room for more.

The WSS streaming path has been observed to produce noisy, disfluent playback in production. We are also moving the entire engine to HTTP with complete-audio responses as part of this refactor.

## Goals

- Replace WebSocket streaming with HTTP complete-audio synthesis across all providers.
- Support a curated set of providers (StepFun, MiniMax, Azure, OpenAI) chosen via a settings dropdown, with a clear extension path for future additions.
- Hot-swap the active provider when settings change — no app restart.
- Keep per-provider expressive features (emotion, style, instruction) accessible via a small cross-provider `SpeakOptions` mapping, not by exposing every provider's full parameter set in the UI.
- Preserve each provider's saved credentials when the user switches between providers.

## Non-Goals

- Streaming / progressive playback. Complete-audio only. (Re-evaluate later if a specific provider mandates it.)
- Offline / on-device TTS.
- Multiple concurrent TTS streams. New `speak()` cancels the in-flight one.
- Per-tip queueing with priority/pause semantics.
- Voice cloning APIs (`voices/preview`, `voices` upload). Out of scope for this change.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Caller (TipBubbleWidget, MainWindow, TipsEngine)           │
│       speak(text, SpeakOptions{emotion, rate, …})           │
└──────────────────────────┬──────────────────────────────────┘
                           │  Qt signal (queued, cross-thread)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  TTSEngine  (lives on dedicated QThread, owns audio)        │
│  • Reads ConfigManager → picks adapter via TtsProviderId    │
│  • Watches config signals → hot-swaps adapter               │
│  • Owns QAudioSink, QAudioDecoder, retry/backoff            │
│  • Cancels in-flight request when a new speak() arrives     │
│  • Owns the shared QNetworkAccessManager                    │
└──────────────────────────┬──────────────────────────────────┘
                           │  ITtsProvider::synthesize(req, onOk, onErr)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  ITtsProvider  (pure-virtual, stateless)                    │
│    StepFunHttpProvider                                      │
│    MiniMaxHttpProvider                                      │
│    AzureSpeechProvider                                      │
│    OpenAiTtsProvider                                        │
│  Each: builds request, POSTs via engine-provided QNAM,      │
│  returns {bytes, mimeType, sampleRate?}                     │
└─────────────────────────────────────────────────────────────┘
```

Three layers, sharp boundaries:

- **Engine** owns everything time- and audio-related (threading, sink, decoding, retry, request cancellation, signals to UI). Knows nothing about provider HTTP shapes.
- **Provider adapters** are pure request-builders + response-parsers. No timers, no audio, no Qt threading concerns. Adapters do not own a `QNetworkAccessManager` — the engine owns one and passes a pointer in the factory call. This centralizes connection pooling and proxy settings.
- **`ITtsProvider`** is the contract — small enough to fit on one screen.

A `TtsProviderRegistry` (a static map `TtsProviderId → ProviderDescriptor`) lets the settings panel populate its dropdown and lets the engine instantiate the right adapter without a switch statement.

## Configuration

### Storage Schema

Storage is **nested per-provider** in QSettings. Each provider's credentials persist independently of which provider is currently active, so switching providers and switching back does not require re-entering tokens.

```ini
tts/enabled            = true
tts/activeProvider     = "stepfun"          # one of: stepfun | minimax | azure | openai

tts/providers/stepfun/token   = "..."
tts/providers/stepfun/baseUrl = "https://api.stepfun.com"
tts/providers/stepfun/model   = "stepaudio-2.5-tts"
tts/providers/stepfun/voice   = "cixingnansheng"

tts/providers/minimax/token   = "..."
tts/providers/minimax/groupId = "..."
tts/providers/minimax/model   = "speech-02-hd"
tts/providers/minimax/voice   = "female-shaonv"

tts/providers/azure/key       = "..."
tts/providers/azure/region    = "eastus"
tts/providers/azure/voice     = "zh-CN-XiaoxiaoNeural"

tts/providers/openai/token    = "..."
tts/providers/openai/baseUrl  = "https://api.openai.com/v1"
tts/providers/openai/model    = "gpt-4o-mini-tts"
tts/providers/openai/voice    = "nova"
```

### Migration

Current production config is StepFun-only with flat keys (`tts/baseUrl`, `tts/token`, `tts/model`, `tts/voice`). On first launch after the upgrade, `ConfigManager` runs a one-shot migration:

1. If `tts/activeProvider` is unset and any of `tts/baseUrl`/`tts/token`/`tts/voice` exist → copy them to `tts/providers/stepfun/*`, set `tts/activeProvider = "stepfun"`, delete the flat keys.
2. Otherwise no-op.

### Provider Descriptor

Each provider registers a static descriptor:

```cpp
struct VoicePreset {
    QString id;             // "cixingnansheng", "zh-CN-XiaoxiaoNeural"
    QString displayName;    // "Cixingnansheng (male)"
    QString language;       // "zh-CN"
};

struct ProviderDescriptor {
    TtsProviderId id;                       // enum: StepFun, MiniMax, Azure, OpenAI
    QString displayName;                    // "StepFun"
    QString configNamespace;                // "stepfun" → tts/providers/stepfun/*
    QStringList requiredFields;             // {"token", "voice"}
    QStringList optionalFields;             // {"baseUrl", "model"}
    QList<VoicePreset> voiceCatalog;
    QList<Emotion> supportedEmotions;       // for SpeakOptions mapping
    std::function<std::unique_ptr<ITtsProvider>(
        const ProviderConfig&,
        QNetworkAccessManager*)> factory;
};
```

The settings panel iterates this descriptor when building its page, so the StepFun page asks for `{token, voice, baseUrl, model}` and the Azure page asks for `{key, region, voice}` — same code path, different descriptor.

### UI Layout

The AI tab top-row is a `QComboBox` of providers. Below it sits a `QStackedWidget` whose pages are generated from each provider's descriptor.

Voice selection is a curated dropdown built from `ProviderDescriptor::voiceCatalog`, with a final `Custom…` item that reveals a `QLineEdit` for user-supplied voice IDs. This handles both "user just wants it to work" and "user has a cloned voice ID."

When the user changes the dropdown:

1. Save current page's edits to `tts/providers/<old>/*`.
2. Set `tts/activeProvider = <new>`.
3. Load `<new>`'s page from its subtree.
4. `ConfigManager` emits `activeProviderChanged` → `TTSEngine` hot-swaps adapter.

## The `ITtsProvider` Contract

```cpp
enum class Emotion {
    Neutral, Happy, Sad, Angry, Calm, Whisper
};

struct SpeakOptions {
    std::optional<Emotion> emotion;     // null = adapter omits the field
    std::optional<float>   rate;        // 0.5–2.0; null = provider default
    QString                languageHint;
};

struct SynthesisRequest {
    QString       text;
    SpeakOptions  options;
};

struct SynthesisResult {
    QByteArray  audio;       // complete blob
    QString     mimeType;    // "audio/mpeg", "audio/wav", "audio/x-pcm"
    int         sampleRate;  // 0 = inferred from container
};

class ITtsProvider {
public:
    virtual ~ITtsProvider() = default;

    virtual RequestHandle synthesize(
        const SynthesisRequest& req,
        std::function<void(SynthesisResult)> onSuccess,
        std::function<void(TtsError)> onError) = 0;

    virtual void cancel(RequestHandle) = 0;
};
```

Each adapter is approximately 150 LOC: build the JSON/SSML body, set headers, POST, parse the response (hex-decode for MiniMax, passthrough for the others), hand bytes to the engine.

Engine-side decoding lives in **one place**: `QAudioDecoder` handles MP3/WAV/OGG; PCM goes straight to `QAudioSink`. Adapter-side audio knowledge is reduced to a single MIME string.

### Emotion Mapping

Each adapter translates `SpeakOptions::emotion` into its provider's expressive parameter:

| Emotion | StepFun (`instruction`) | MiniMax (`emotion`) | Azure (SSML `style`)   | OpenAI       |
|---------|-------------------------|---------------------|-------------------------|--------------|
| Neutral | omitted                 | `"calm"`            | omitted                 | omitted      |
| Happy   | `"语气愉快"`             | `"happy"`           | `"cheerful"`            | omitted      |
| Sad     | `"语气低沉，悲伤"`        | `"sad"`             | `"sad"`                 | omitted      |
| Angry   | `"语气严厉"`             | `"angry"`           | `"angry"`               | omitted      |
| Calm    | `"语气平静"`             | `"calm"`            | `"calm"`                | omitted      |
| Whisper | `"低声耳语"`             | `"whisper"`         | `"whispering"`          | omitted      |

OpenAI's `tts-1` family does not expose a style parameter, so emotion is silently dropped for that adapter.

## Error Handling, Retry, Cancellation

### Cancellation on Supersession

A new `speak()` while a request is in flight causes the engine to call `cancel()` on the old handle and drop its result if it lands late. This prevents stale audio after a tip's content changes.

Cancellation is silent: after `cancel()` returns, neither `onSuccess` nor `onError` should fire for that handle. Adapters implement this by clearing their callback pointers before aborting the underlying `QNetworkReply` — the reply's `finished` signal then has nothing to invoke. The engine treats cancelled handles as terminal and does not emit `speakingFinished()` for them; that signal is reserved for completed playback.

### Retry Policy

The engine retries only on transient transport errors:

- `QNetworkReply::NetworkError` (connection failures)
- HTTP 5xx
- HTTP 408 (Request Timeout)
- HTTP 429 with `Retry-After` header (honor the value)

Maximum 2 retries, exponential backoff: 250ms, 1s. No retry on other 4xx — those are config errors and should surface immediately, not be hidden behind a delay.

### Auth Surfacing

HTTP 401/403 maps to `TtsError::AuthFailed`. The engine emits a one-shot `authFailed(provider)` signal so the AI tab can show a red "check token" hint next to the active provider's credentials field. No retry, no spam.

### Audio Sink Errors

Playback failures (device unavailable, format unsupported) are logged and `speakingFinished()` is still emitted. The UI must not hang waiting for a finish signal that never arrives.

## Testing Strategy

### Adapter Unit Tests (`tests/test_tts_providers.cpp`)

Each adapter is tested in isolation:

- Inject a fake `QNetworkAccessManager` (subclass that records requests and replays canned responses) or run against a local `QHttpServer`.
- Assert outgoing request body, headers, URL match the provider's spec for known input parameters.
- Assert correct parsing: hex-encoded MiniMax bodies decode to bytes; raw audio passes through for the others.
- Assert `SpeakOptions` mapping: each emotion produces the expected provider-specific field.
- No live network calls.

### Engine Integration Tests (`tests/test_tts_engine.cpp`)

Engine behavior is tested against a `FakeProvider` that returns canned 100ms WAV blobs:

- Cancel-on-supersession: speak A, immediately speak B, assert only B plays.
- Retry-on-5xx: provider returns 503, then 200. Assert one retry, success.
- No-retry-on-4xx: provider returns 401. Assert `authFailed` signal, no retry.
- Hot-swap: change `activeProvider` in config. Assert old provider's `cancel()` is called, next `speak()` routes to new provider.
- Audio-sink-error: stub a sink that fails on `start()`. Assert `speakingFinished` still fires.

### Manual Live Tests

A manual test (`tests/manual/test_tts_live.cpp`) gated behind an env var with real credentials covers each provider against the real API. Run before releases. Not in CI.

## Implementation Notes

- The dedicated `QThread` model is preserved — the engine still lives on its own thread, callers reach it via queued connections.
- The shared `QNetworkAccessManager` lives on the engine's thread. Adapters use it directly; engine does not proxy.
- `RequestHandle` is a strong type wrapping `QNetworkReply*` (or an opaque token if an adapter ever needs a non-QNetworkReply transport). `cancel()` aborts the reply; the reply's `finished` slot must check `isRunning()` to avoid firing callbacks after cancellation.
- Existing `SEELIE_TTS_ENABLED` CMake flag stays. QtWebSockets dependency can be removed since we no longer need it; that's a build-config cleanup item in the implementation plan.

## Files Affected

**Modified:**

- `src/TTSEngine.h`/`.cpp` — full rewrite: HTTP coordinator, no WebSocket
- `src/ConfigManager.h`/`.cpp` — add provider-namespaced fields, migration, `activeProviderChanged` signal
- `src/SettingsPanelWidget.h`/`.cpp` — replace flat TTS form with provider dropdown + `QStackedWidget`
- `CMakeLists.txt` — drop `Qt6::WebSockets`, add new source files
- `Seelie_zh_CN.ts` — translations for new strings

**New:**

- `src/tts/ITtsProvider.h` — contract, types
- `src/tts/TtsProviderRegistry.h`/`.cpp` — descriptor table
- `src/tts/StepFunHttpProvider.h`/`.cpp`
- `src/tts/MiniMaxHttpProvider.h`/`.cpp`
- `src/tts/AzureSpeechProvider.h`/`.cpp`
- `src/tts/OpenAiTtsProvider.h`/`.cpp`
- `tests/test_tts_engine.cpp`
- `tests/test_tts_providers.cpp`
- `tests/manual/test_tts_live.cpp`

## Open Items for the Implementation Plan

- Exact `QAudioDecoder` wiring for memory-buffered decode (it's natively file-oriented; needs a `QBuffer` adapter).
- Whether to keep the existing `/tmp/seelie_tts.log` file logging (StepFun-era debug aid) or migrate to `qDebug` with a `tts.engine` logging category.
- Whether `tts/providers/<id>/*` should use `QSettings::beginGroup` block writes for atomicity when the user changes provider mid-edit.
