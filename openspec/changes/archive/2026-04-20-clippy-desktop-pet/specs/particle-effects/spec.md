## ADDED Requirements

### Requirement: Lottie effect types and triggers
The system SHALL support 6 visual effect types, each as a Lottie JSON file loaded via rlottie:
- **sparkles.json**: Triggered on tool success and Congratulate animations
- **confetti.json**: Triggered on Congratulate animation
- **alert-pulse.json**: Triggered on Alert animation and permission.requested events
- **thinking-dots.json**: Triggered on Thinking and Processing animations
- **wave-lines.json**: Triggered on Greeting and GoodBye animations
- **speech-pop.json**: Triggered on speech bubble enter/exit
- Effects SHALL be loaded from `assets/lottie/effects/` directory at startup using `rlottie::Animation::loadFromFile()`
- Effects SHALL be loaded from `assets/lottie/effects/` directory at startup using `rlottie::Animation::loadFromFile()`

#### Scenario: Sparkles on tool success
- **WHEN** a tool success event is received
- **THEN** the sparkles Lottie effect plays once and auto-cleanup

#### Scenario: Confetti on congratulate
- **WHEN** the Congratulate animation plays
- **THEN** confetti Lottie effect plays once and auto-cleanup

#### Scenario: Alert pulse on permission request
- **WHEN** a permission.requested event is received
- **THEN** alert-pulse Lottie effect plays once and auto-cleanup

### Requirement: rlottie rendering of effects
Visual effects SHALL be rendered via the same rlottie pipeline as character animations: `rlottie::Animation::render(frame, surface)` → QImage(Format_ARGB32_Premultiplied) → QPainter::drawImage()
Effects SHALL composite on top of the character animation in the same paint pass
Each effect instance SHALL have its own frame counter and render buffer

#### Scenario: Effect rendering active
- **WHEN** a visual effect is triggered
- **THEN** the effect renders via rlottie in the same paint pass as the character animation with no interference to the sprite rendering layer

#### Scenario: Effect positioned relative to pet widget
- **WHEN** a visual effect is rendered
- **THEN** it is positioned relative to the pet widget geometry and composited on top

### Requirement: Effect lifecycle management
Each effect SHALL have a defined playback mode: once (play to last frame then cleanup), loop (repeat until explicitly stopped)
Default behavior: most effects play once; thinking-dots loops until the thinking state ends
When an effect completes (once mode), it SHALL be removed from the active effects list
Multiple effects MAY play simultaneously without interference

#### Scenario: Simultaneous effects
- **WHEN** sparkles and confetti are triggered at the same time
- **THEN** both Lottie effects render simultaneously without interference

#### Scenario: Effect auto-cleanup after playback
- **WHEN** a sparkle effect completes its once-mode playback
- **THEN** the effect is removed from the active effects list and resources are freed

#### Scenario: Loop stop (thinking-dots stops when thinking ends)
- **WHEN** the thinking state ends
- **THEN** thinking-dots effect is explicitly stopped and removed from the active effects list

### Requirement: Effect positioning
Effects SHALL be positionable relative to the pet widget's geometry
Default position: centered on the pet widget
Per-effect offset MAY be configured (e.g., confetti bursts from top, sparkles around edges)

#### Scenario: Confetti positioned above pet
- **WHEN** confetti effect is triggered
- **THEN** confetti is positioned above the pet widget with appropriate offset

#### Scenario: Sparkles centered on pet
- **WHEN** sparkles effect is triggered
- **THEN** sparkles are centered on the pet widget
