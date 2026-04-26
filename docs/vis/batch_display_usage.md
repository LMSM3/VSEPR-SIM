# Batch Display Layer — Usage & Integration Guide

**VSEPR-SIM** | `v5.0.0-beta.7-step-attempt`

---

## Purpose

The batch display layer keeps a live simulation window (or console progress
bar) running as a **passive observer** while a continuous generation, database,
or report pipeline runs on worker threads. The producer is never blocked by the
display.

This is not an interactive visualization. There are no controls, no
parameter panels, no hot-reload. It is a status window for long-running
deterministic workflows.

---

## Minimal integration (5 lines)

```cpp
#include "vis/continuous_run_display.hpp"

ContinuousRunDisplay display;
display.configure("MyBatch", 5.0f);   // label, fps
display.open(960, 720);               // attempt GL; silent fallback if unavailable

std::thread worker([&]{
	display.bridge().start();

	for (int i = 0; i < total; ++i) {
		XYZFrame frame = build_frame(presets[i]);
		display.bridge().push_frame(frame, presets[i].tag);
		display.bridge().push_progress(i, total, artifacts_done, total_art, "step");
		// ... write artifacts ...
	}

	display.bridge().finish();
});

display.run_event_loop();   // blocks main thread; returns when done
worker.join();
```

The same code works with or without a GL window. `open()` returning `false`
is not an error — it silently activates the console bar.

---

## Thread model

```
Main thread                         Worker thread(s)
──────────────────────────────────  ──────────────────────────────────
display.open()                      bridge.start()
display.run_event_loop() ───────┐   bridge.push_frame(...)         │
  loop:                          │   bridge.push_progress(...)      │
	bridge.display_tick()        │   bridge.push_frame(...)         │
	latest_frame() → render      │   ...                            │
	status_text()  → overlay     │   bridge.finish() ───────────────┘
	sleep(tick budget)           │
  bridge.is_done() → return ─────┘
worker.join()
```

**Rules:**
- `run_event_loop()` must be called from the thread that called `open()` (GLFW context affinity).
- Only one producer thread writes to the bridge at a time; multi-producer is not supported.
- `bridge()` returns a reference — do not store it past the lifetime of the `ContinuousRunDisplay`.

---

## Build variants

### No GL (standalone / CI / SSH)

```
g++ -std=c++20 -O2 -I. -I../../ your_runner.cpp -o runner
```

`open()` is an inline no-op. `run_event_loop()` runs the console bar.
No GLFW, no OpenGL, no ImGui headers needed.

### With GL (`VSEPR_HAS_GL`)

```
g++ -std=c++20 -O2 -DVSEPR_HAS_GL -I. -I../../ \
	your_runner.cpp \
	../../src/vis/continuous_run_display.cpp \
	../../src/vis/window.cpp \
	-lglfw -lGL -o runner
```

`open()` constructs a real GLFW window. `run_event_loop()` drives
`Window::run_batch()` with the `BATCH_PASSIVE` preset.

---

## Console progress bar output

The bar overwrites the current terminal line with `\r`:

```
[########################################] 100% NiTi_B2 | writing .xyza  0m3s
[BATCH DONE] NiTi_B2  frames=54  artifacts=270  elapsed=3s
```

Fields: `[progress_bar] pct% run_label | current_op  Xm Ys`

The bar is driven by `BatchStatus` fields. Update them with `push_progress()` after each significant step.

---

## Progress update cadence

`push_frame()` and `push_progress()` are cheap — call them as often as useful.
The display throttles itself via `display_tick(fps)` and only renders at the
configured rate (default 5 fps). The producer is never slowed down.

Recommended call pattern:

```cpp
// Before a major step:
bridge.push_progress(i, total, art_done, art_total, "building supercell");

// After building the frame:
bridge.push_frame(frame, tag);

// After writing each artifact:
bridge.push_progress(i, total, ++art_done, art_total, "writing .xyza");
```

---

## Multi-cycle runs (restart without recreating)

`BatchWindowBridge` supports multiple start/finish cycles on the same object.
`start()` resets the frame counter and elapsed clock.

```cpp
for (int pass = 0; pass < num_passes; ++pass) {
	bridge.start();
	for (auto& item : pass_items[pass]) {
		bridge.push_frame(...);
		bridge.push_progress(...);
	}
	bridge.finish();
	// Window stays open; next start() will reset the bar
}
```

---

## Integration with `metal_gen`

`metal_gen.cpp` (`src/gen/`) is the reference integration:

```
main thread                         worker lambda
─────────────────────────           ───────────────────────────────
ContinuousRunDisplay display        bridge.start()
display.configure(label, 5.0f)      for each MaterialPreset:
display.open(960, 720)                push_progress(i, n, 0, 5, "building")
std::thread worker([&]{...})          frame = build_supercell(...)
display.run_event_loop()              push_frame(frame, tag)
worker.join()                         write artifacts (xyz/xyza/xyzc/xyzf/step)
return 0                              push_progress(i+1, n, 5, 5, "done")
									bridge.finish()
```

Run: `./metal_gen.exe output_dir`

---

## Integration with `Window::run_batch()` (GL builds)

If you already have a `Window` object, you can skip `ContinuousRunDisplay`
and call the loop directly:

```cpp
Window win(960, 720, "My Batch");
win.initialize();

BatchWindowBridge bridge;
std::thread worker([&]{
	bridge.start();
	for (...) { bridge.push_frame(f, tag); bridge.push_progress(...); }
	bridge.finish();
});

win.run_batch(bridge, 5.0f);  // blocks until done or window closed
worker.join();
```

`run_batch()` applies `VizMode::BATCH_PASSIVE` automatically. Do not call
`run()` or `run_with_ui()` alongside `run_batch()` on the same `Window`.

---

## Stress test

`src/gen/batch_display_stress.cpp` covers 12 scenarios:

| Test | Scenario |
|---|---|
| T1 | Basic push / latest_frame round-trip |
| T2 | 10 000 frames, high-frequency producer, no deadlock |
| T3 | Frame counter monotonicity under concurrent read |
| T4 | Status snapshot consistency (no torn reads) |
| T5 | Elapsed time monotonicity |
| T6 | `display_tick()` throttle correctness |
| T7 | Lifecycle state machine (start / finish / is_done) |
| T8 | Multi-cycle restart (3 × start → finish) |
| T9 | `ContinuousRunDisplay` threaded console loop end-to-end |
| T10 | Empty-label push does not overwrite existing `run_label` |
| T11 | Late consumer (producer finishes before first tick) |
| T12 | Zero-atom `XYZFrame` edge case |

Build and run:
```
cd src/gen
g++ -std=c++20 -O2 -I. -IC:\R\VSPER-SIM -IC:\R\VSPER-SIM\src batch_display_stress.cpp -o batch_display_stress.exe
.\batch_display_stress.exe
```

Expected output: `--- Results: 12/12 passed  ALL PASS ---`

---

## Known limitations / future work

| Item | Notes |
|---|---|
| GL overlay text | `status_message` is in the `FrameSnapshot` but the renderer overlay integration is renderer-dependent. Currently the field is set; visual rendering of it requires a renderer-side text pass (planned). |
| Multi-producer | Not supported. Serialize multiple producers through a single `push_frame()` call site. |
| Window spin auto-pilot | Available via `window.auto_pilot().set_spin_enabled(true)` in GL builds. Not wired by default in the batch path. |
| Display-side frame counter | `display_tick()` does not track how many frames were rendered vs. dropped. A `frames_rendered` counter can be added if display performance diagnostics are needed. |

---

## File map

```
src/vis/
  batch_window_bridge.hpp        Lock-free bridge (producer ↔ display)
  continuous_run_display.hpp     Top-level controller + console bar (header-only console path)
  continuous_run_display.cpp     GL wiring (VSEPR_HAS_GL builds only)
  xyz_to_snapshot.hpp            XYZFrame → FrameSnapshot conversion
  viz_config.hpp                 VizMode::BATCH_PASSIVE preset
  window.hpp / window.cpp        Window::run_batch() GL loop

src/gen/
  metal_gen.cpp                  Reference integration (runs standalone)
  batch_display_stress.cpp       Stress test (12 tests, exit 0 = pass)

docs/vis/
  batch_display_api.md           API reference (this layer)
  batch_display_usage.md         This file
```
