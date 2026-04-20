# Sprite Animation Engine Specification

## Overview

The sprite animation engine drives character animations for the desktop pet using the **rlottie** C++ library. Instead of sprite sheet rendering, this engine loads Lottie JSON files, plays named animations via rlottie's API, and manages animation priority, idle pooling, and rest pose — all rendered through Qt's QPainter pipeline.

---

## Requirement: Lottie Animation Loading and Caching

The system SHALL load all Lottie JSON files from the `assets/lottie/character/` directory at startup using `rlottie::Animation::loadFromFile()` or `rlottie::Animation::loadFromData()`.

- The system SHALL use `rlottie::Animation::loadFromFile(const std::string& path)` to load each `.json` Lottie file from the configured directory.
- Each loaded animation SHALL be stored as a `std::shared_ptr<rlottie::Animation>` in a `std::unordered_map<std::string, std::shared_ptr<rlottie::Animation>>` keyed by animation name (derived from the filename without extension).
- The system SHALL query each animation's metadata at load time: `size()`, `totalFrame()`, `frameRate()`, and `duration()` — storing these in a parallel struct for quick access during playback.
- A reusable render buffer (`std::vector<uint32_t>`) SHALL be allocated once at startup, sized to `(width * height)` pixels based on the largest animation's dimensions, and reused for all frame renders to avoid per-frame allocations.
- The system SHALL log the animation name, dimensions, total frames, frame rate, and duration upon successful load at `INFO` level.

### Scenario: Successful Load

- **WHEN** the application starts and `assets/lottie/character/` contains `Greeting.json`, `Thinking.json`, `Writing.json`, `Alert.json`, `Congratulate.json`, `Idle1.json`, `Idle2.json`, and `RestPose.json`
- **THEN** the system loads each file via `rlottie::Animation::loadFromFile()`, stores a `shared_ptr` for each in the animation map, queries metadata (size, totalFrame, frameRate, duration), and logs confirmation at INFO level.

### Scenario: Missing Lottie File

- **WHEN** a Lottie file referenced in the directory scan does not exist
- **THEN** the system logs an error message: `"Failed to load animation 'Missing': file not found"` and continues loading remaining animations. The missing entry SHALL NOT appear in the animation map.

### Scenario: Invalid Lottie File

- **WHEN** a `.json` file is present but is malformed or not a valid Lottie file
- **THEN** `loadFromFile()` throws or returns `nullptr`. The system SHALL catch any exception, log an error including the animation name, and continue startup without that animation.

---

## Requirement: Named Animation Playback

The system SHALL play any named animation by string identifier (e.g., `"Greeting"`, `"Thinking"`, `"Writing"`, `"Alert"`, `"Congratulate"`) by looking up the entry in the animation cache map.

- The system SHALL render each frame via rlottie's `animation->render(frameNumber, surface)` method, where `surface` is constructed from the reusable buffer as `rlottie::Surface(buffer_data, width, height, stride_in_pixels * 4)`.
- The resulting buffer SHALL be wrapped as a `QImage` using `QImage((const uchar*)buffer.data(), width, height, width * 4, QImage::Format_ARGB32_Premultiplied)` and drawn onto the target pixmap via `QPainter::drawImage()`.
- Frame advancement SHALL be driven by a `QTimer` firing at approximately 16ms intervals (~60fps), incrementing the frame counter by `ceil(frameRate() / 60.0)` each tick to achieve correct playback speed.
- The system SHALL support three playback modes per animation definition:
  - **Once**: play from frame 0 to `totalFrame() - 1`, then stop and emit an `animationFinished` signal.
  - **Loop**: play from 0 to `totalFrame() - 1`, then wrap to 0 and continue indefinitely.
  - **Ping-pong**: play `0 → totalFrame()-1 → 0 → ...` alternating direction on each end.

### Scenario: Play Named Animation

- **WHEN** a request to play `"Greeting"` arrives
- **THEN** the system looks up `"Greeting"` in the animation map, starts playback from frame 0, renders each frame via `rlottie::Surface` → `QImage` → `QPainter::drawImage()`, and advances frames until the animation ends or is interrupted.

### Scenario: Unknown Animation Name

- **WHEN** a requested animation name is not found in the map
- **THEN** the system logs a warning: `"Animation 'Unknown' not found, falling back to 'Idle1'"` and attempts to play `"Idle1"` instead. If `"Idle1"` is also not found, the system logs an error and displays the rest pose.

---

## Requirement: Priority-Based Animation Queue

The system SHALL implement a two-tier priority queue to manage animation requests:

- **High Priority**: When a high-priority animation request arrives (e.g., `"Alert"` triggered by a system notification), the system SHALL immediately interrupt the currently playing animation, stop the `QTimer`, and start the new animation from frame 0.
- **Normal Priority**: When a normal-priority animation request arrives (e.g., a user action that is not urgent), the system SHALL enqueue the request if an animation is currently playing. The queued animation SHALL start only after the current animation completes (plays to its final frame and stops).
- The queue SHALL be processed FIFO for normal-priority entries.

### Scenario: High Priority Interrupt

- **WHEN** `Idle1` is playing on loop and a system event triggers `"Alert"` at high priority
- **THEN** the `Idle1` loop is immediately stopped, `Alert` starts from frame 0, plays to completion, then stops. The normal-priority queue is unaffected.

### Scenario: Normal Priority Queue

- **WHEN** `Greeting` plays (Once mode) and a user action requests `Writing` at normal priority
- **THEN** `Writing` is enqueued. After `Greeting` finishes and emits `animationFinished`, the queue is checked. `Writing` is dequeued and begins playback.

---

## Requirement: Idle Animation Pool

The system SHALL maintain a pool of idle animation names with weighted random selection.

- The idle pool SHALL be a list of `{ name, weight }` entries, e.g., `[{ "Idle1", 3 }, { "Idle2", 2 }]` where higher weight means higher probability of selection.
- After **3 seconds** of no animation activity (no animation playing and no queued requests), the system SHALL randomly select an idle animation from the pool using weighted random selection and begin playback in **Loop** mode.
- Upon completion of an idle animation (if still idle — i.e., no new animation requests arrived during playback), the system SHALL wait the same 3-second idle timeout, then select and play another idle animation from the pool.

### Scenario: Idle After Inactivity

- **WHEN** no animation has played and no queue entries exist for 3 seconds
- **THEN** `Idle2` is randomly selected (weighted) and plays in Loop mode. After 5 more seconds of inactivity, the loop ends, the 3-second idle timer restarts, and `Idle1` is selected and plays.

### Scenario: Idle Interrupted by Event

- **WHEN** `Idle1` is playing in Loop mode and a high-priority `"Alert"` event arrives
- **THEN** the idle playback is immediately interrupted, `Alert` plays to completion. After `Alert` finishes, the idle pool logic is NOT triggered because the system responded to activity and is not in an idle state.

---

## Requirement: Rest Pose

The system SHALL define a "RestPose" as a static Lottie animation (single frame or the first frame of a designated rest animation) displayed when no animation is active and the idle pool has no animation queued.

- On startup, before any animation is loaded or played, the system SHALL display the rest pose by rendering frame 0 of the `"RestPose"` animation.
- After every animation completes and before the idle timer triggers the next animation, the system SHALL return to displaying the rest pose.
- The rest pose is rendered using the same `rlottie::Surface` → `QImage` → `QPainter::drawImage()` pipeline as any other animation, using frame index 0.

### Scenario: Rest Pose on Startup

- **WHEN** the desktop pet window opens
- **THEN** no animations have been triggered yet. Frame 0 of `RestPose` is rendered and displayed as the baseline character state.

---

## Requirement: Per-Animation Speed Multiplier

The system SHALL support a speed multiplier per animation definition (default `1.0x`) that scales the frame advancement rate.

- The speed multiplier SHALL affect the frame counter increment per `QTimer` tick: instead of incrementing by `ceil(frameRate() / 60.0)`, the increment is `ceil(frameRate() / 60.0 * multiplier)`.
- A multiplier of `2.0x` SHALL advance frames twice as fast as the nominal rate; a multiplier of `0.5x` SHALL advance frames half as fast.
- Speed multipliers SHALL be configured per animation name, e.g., in a configuration map: `{"Congratulate": 1.5, "Thinking": 0.7}`.

### Scenario: Fast Playback

- **WHEN** `Congratulate` has a `1.5x` multiplier configured and `frameRate()` returns 30
- **THEN** the frame increment per 60Hz timer tick is `ceil(30/60 * 1.5) = ceil(0.75) = 1`, but the effective playback position advances 1.5x faster relative to wall clock time, making the animation complete in less real time.

### Scenario: Slow Playback

- **WHEN** `Thinking` has a `0.5x` multiplier configured
- **THEN** frame advancement per tick is halved, making the character's thought process appear slower and more deliberate than the nominal rate.
