# Batch Display Layer — API Reference

**Module:** `src/vis/`  
**Branch:** `v5.0.0-beta.7-step-attempt`  
**Status:** Production-ready (12/12 stress tests pass)

---

## Overview

The batch display layer exposes a passive, non-blocking window representation
for continuous-run workflows (database generation, report pipelines, extended
batch relaxation). The producer thread never waits on the display.

```
Producer thread
  │  push_frame()       ← lock-free atomic store
  │  push_progress()    ← mutex-guarded status update (infrequent)
  ▼
BatchWindowBridge       ← drop-oldest frame slot
  │  display_tick()     ← throttled to display_fps
  │  latest_frame()     ← shared_ptr read, never blocks
  ▼
Window::run_batch()     ← GL render loop (VSEPR_HAS_GL builds)
  or
ConsoleProgressBar      ← inline fallback, zero GL dependency
```

---

## `struct BatchStatus`
**Header:** `vis/batch_window_bridge.hpp`

| Field | Type | Description |
|---|---|---|
| `run_label` | `std::string` | Material tag, report ID, or any producer identifier |
| `frame_index` | `int` | Current frame / supercell index (0-based) |
| `total_frames` | `int` | Total expected frames; 0 = unknown |
| `artifacts_done` | `int` | Files written so far |
| `artifacts_total` | `int` | Total expected artifacts |
| `elapsed_s` | `double` | Wall-clock seconds since `start()` |
| `current_op` | `std::string` | Short label for the current step, e.g. `"writing .xyza"` |

**Method:** `std::string to_overlay_text() const`  
Formats all fields into a single-line status string suitable for window overlays or console output.

---

## `class BatchWindowBridge`
**Header:** `vis/batch_window_bridge.hpp`

Thread model: **one producer, one consumer**.  
Uses `std::atomic<std::shared_ptr<XYZFrame>>` (C++20) for the frame slot — guaranteed lock-free on x86-64 and ARM64.

### Producer API

```cpp
void start();
```
Resets the frame counter and elapsed clock. Call once at the beginning of each batch cycle. Safe to call again after `finish()` for multi-cycle runs.

```cpp
void finish();
```
Signals that the batch is complete. `is_done()` returns `true` after this call.

```cpp
void push_frame(const io::XYZFrame& frame, const std::string& label = "");
```
Stores `frame` in the atomic slot. If the display hasn't consumed the previous frame, it is silently replaced (drop-oldest policy). Never blocks. If `label` is non-empty it is written into `status_.run_label`.

```cpp
void push_status(const BatchStatus& s);
```
Replaces the entire status record atomically under a mutex. Use when you want to set all fields at once.

```cpp
void push_progress(int frame_index, int total_frames,
				   int artifacts_done, int artifacts_total,
				   const std::string& current_op = "");
```
Lightweight update — only touches the progress counters and `current_op`. Updates `elapsed_s` automatically. Preferred over `push_status()` in tight loops.

### Consumer API

```cpp
std::shared_ptr<io::XYZFrame> latest_frame() const;
```
Returns the most recently pushed frame, or `nullptr` if none pushed yet. Never blocks.

```cpp
std::string status_text() const;
```
Returns a formatted overlay string from the current `BatchStatus`.

```cpp
BatchStatus status_snapshot() const;
```
Returns a full copy of the current `BatchStatus` under the mutex. Safe to call from any thread.

```cpp
bool display_tick(float target_fps = 5.0f);
```
Returns `true` if enough wall-clock time has elapsed since the last tick for the given frame rate. The render loop calls this to throttle without sleeping.

```cpp
bool is_done() const;
double elapsed_seconds() const;
std::uint64_t frames_pushed_count() const;
```

---

## `struct DisplayConfig`
**Header:** `vis/continuous_run_display.hpp`

| Field | Type | Default | Description |
|---|---|---|---|
| `run_label` | `std::string` | `"Batch Run"` | Title shown in window and console bar |
| `display_fps` | `float` | `5.0f` | Window refresh rate (not simulation rate) |
| `win_width` | `int` | `960` | GL window width in pixels |
| `win_height` | `int` | `720` | GL window height in pixels |
| `auto_close_on_done` | `bool` | `true` | Close window when `bridge.finish()` is called |
| `console_fallback` | `bool` | `true` | Print progress bar if no GL window |
| `viz` | `VizConfig` | — | `BATCH_PASSIVE` preset applied automatically |

---

## `class ContinuousRunDisplay`
**Header:** `vis/continuous_run_display.hpp`

Top-level controller. Owns the `BatchWindowBridge` and optionally a `Window`.

```cpp
void configure(const DisplayConfig& cfg);
void configure(const std::string& label, float display_fps = 5.0f);
```
Set run label and fps. Call before `open()`.

```cpp
bool open(int width = 960, int height = 720);
```
Attempt to open a GL window. Returns `true` on success. Returns `false` on headless/no-GL systems; `run_event_loop()` automatically uses the console bar in that case.  
**Must be called from the thread that will call `run_event_loop()`** (GLFW context affinity).  
When `VSEPR_HAS_GL` is not defined, this function is an inline no-op that always returns `false`.

```cpp
BatchWindowBridge& bridge();
```
Returns the bridge. Hand this reference to the producer thread.

```cpp
void run_event_loop();
```
Blocks the calling thread until:
- the batch is done and `auto_close_on_done` is set, **or**
- the OS window is closed by the user, **or**
- `close()` is called from another thread.

If no GL window was opened, runs the console progress bar instead.

```cpp
void close();
```
Thread-safe shutdown signal. Safe to call from any thread.

---

## `class ConsoleProgressBar`
**Header:** `vis/continuous_run_display.hpp`  
Fully inline. Zero GL/GLFW dependency.

```cpp
void print(const BatchStatus& s) const;  // \r overwrite line
void done(const BatchStatus& s) const;   // final newline + summary
```

---

## `Window::run_batch()`
**Header:** `vis/window.hpp` | **Implemented:** `vis/window.cpp`  
Only available in GL builds.

```cpp
void run_batch(vis::BatchWindowBridge& bridge, float display_fps = 5.0f);
```
Passive render loop:
- Applies `VizMode::BATCH_PASSIVE` preset (no ImGui, no outlines, no shadows)
- Pulls `latest_frame()` from the bridge at `display_fps`
- Sets `FrameSnapshot::status_message` from `bridge.status_text()` for the renderer overlay
- Ticks `AutoPilot` (passive spin opt-in via `auto_pilot_.set_spin_enabled()`)
- Sleeps the remainder of each tick budget to cap CPU use
- Returns when `bridge.is_done()` or the OS window is closed

---

## `xyz_frame_to_snapshot()`
**Header:** `vis/xyz_to_snapshot.hpp`

```cpp
FrameSnapshot vsepr::vis::xyz_frame_to_snapshot(const io::XYZFrame& frame);
```
Single conversion point from `XYZFrame` (IO layer) to `FrameSnapshot` (renderer contract).

| XYZFrame field | → | FrameSnapshot field |
|---|---|---|
| `atoms[i].{x,y,z}` | → | `positions[i]` (float Vec3) |
| `atoms[i].Z` | → | `atomic_numbers[i]` |
| `energy` | → | `energy` (if present) |
| `comment` | → | `status_message` |

Bonds are not inferred. If bond rendering is needed, populate `FrameSnapshot::bonds` after conversion.

---

## `VizMode::BATCH_PASSIVE`
**Header:** `vis/viz_config.hpp`

Applied automatically by `Window::run_batch()` and `ContinuousRunDisplay::configure()`.

Disables: outlines, shadows, antialiasing, motion blur, interpolation, interactive overlays.  
Enables: box rendering, energy visibility, status overlay.

Additional fields: `batch_display_fps`, `batch_run_label`, `batch_show_status`.
