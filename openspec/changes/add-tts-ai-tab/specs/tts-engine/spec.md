## ADDED Requirements

### Requirement: TTSEngine supports WebSocket TTS streaming
The system SHALL provide a TTSEngine class that connects to TTS providers via WebSocket Secure (WSS) and streams synthesized speech.

#### Scenario: Connection establishment
- **WHEN** TTSEngine::connect() is called with valid configuration
- **THEN** a WSS connection is established to the configured base URL
- **AND** the connection emits a connected signal

#### Scenario: Speech synthesis
- **WHEN** TTSEngine::speak("Hello world") is called
- **THEN** the engine sends the text to the WSS endpoint
- **AND** audio data is received via WebSocket binary frames
- **AND** the audio is played through the system audio output

#### Scenario: Provider selection
- **WHEN** the user configures StepFun as the provider
- **THEN** TTSEngine uses StepFun Audio API WebSocket protocol
- **AND** when the user configures MiniMax, TTSEngine uses MiniMax speech-t2a-websocket protocol

### Requirement: TTSEngine handles connection errors gracefully
The TTSEngine SHALL handle connection failures, authentication errors, and network interruptions without crashing the application.

#### Scenario: Connection failure
- **WHEN** the WSS connection fails to establish
- **THEN** TTSEngine emits an error signal with a descriptive message
- **AND** the application continues running normally

#### Scenario: Authentication failure
- **WHEN** the API token is invalid
- **THEN** TTSEngine emits an authError signal
- **AND** disconnects from the endpoint

### Requirement: TTS respects enable toggle
The system SHALL only trigger TTS when `ttsEnabled` is true in ConfigManager.

#### Scenario: TTS disabled
- **WHEN** `ttsEnabled` is false
- **THEN** calls to TTSEngine::speak() are ignored
- **AND** no network connection is attempted

#### Scenario: TTS enabled
- **WHEN** `ttsEnabled` is true
- **THEN** TTSEngine::speak() sends text to the provider
- **AND** audio is streamed and played
