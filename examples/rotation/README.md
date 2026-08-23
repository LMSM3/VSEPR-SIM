# Rotation Examples

Three self-contained rotation demonstration scenes for the XSIM viewer.

---

## Files

| File | Kind | Description |
|------|------|-------------|
| `cube.x` | `xsim_scene` | BCC iron cube — Y-axis spin, 40 deg/s |
| `sphere.x` | `xsim_scene` | Noble-gas bead sphere shell — X-axis roll, 25 deg/s |
| `sphere_shaded.x` | `xsim_export_bundle` | Export package for the shaded + sampled sphere |
| `sphere_shaded.vsim` | `.vsim` simulation | Full VSIM pipeline companion for `sphere_shaded.x` |
| `run_rotation.sh` | bash wrapper | Launches GL window + Python live viewer together |
| `live_viewer.py` | Python viewer | 800×800 live window with `!new` / `!live` commands |

---

## Quick start

### One-command launch (wrapper — recommended)

```sh
# sphere_shaded  (default)
bash examples/rotation/run_rotation.sh

# cube
bash examples/rotation/run_rotation.sh cube

# sphere
bash examples/rotation/run_rotation.sh sphere

# wipe previous outputs first (!new flag)
bash examples/rotation/run_rotation.sh sphere_shaded !new
```

`run_rotation.sh` starts two processes in parallel:
1. `vsepr` — the GL simulation / viewer window
2. `live_viewer.py` — an 800×800 Python window that watches the output dir

Ctrl+C shuts both down cleanly.

---

### Python live viewer commands

Type in the command bar at the bottom of the Python window:

| Command | Action |
|---------|--------|
| `!live` | Force an immediate repaint |
| `!new`  | Wipe outputs and restart `run_rotation.sh <scene> !new` |

Auto-refresh triggers every 800 ms whenever any file in the watch dir changes.
If `watchdog` is installed (`pip install watchdog`) it uses filesystem events
instead of polling.

---

### Cube (static BCC iron block, spinning)

```sh
xsim scene examples/rotation/cube.x
```

Produces `out/cube_rotation/trajectory.xyz`.

---

### Sphere (noble-gas shell, rolling)

```sh
xsim scene examples/rotation/sphere.x
```

Produces `out/sphere_rotation/trajectory.xyz`.

---

### Sphere — shaded and sampled (two-step)

**Step 1 — run the VSIM simulation:**

```sh
xsim run examples/rotation/sphere_shaded.vsim
```

Opens a live `gl_overlay_cycle` viewer while the sphere spins.
Cycles density / coordination / energy panels automatically every 300 ms.
Writes RDF, packing heatmap, and energy trace SVGs to `out/sphere_shaded_sim/figures/`.

**Step 2 — export the bundle:**

```sh
xsim xport examples/rotation/sphere_shaded.x
```

Produces a fully verified output bundle:

```
out/sphere_shaded/
├── trajectory.xyz
├── analysis.json
├── metrics.tsv
├── report.md
├── verify_report.md
├── verify.tsv
└── manifest.json
```

**Step 3 — verify:**

```sh
xsim verify out/sphere_shaded
```

---

## Visual settings summary

| Scene | output_type | gl_spin | axis | deg/s |
|-------|-------------|---------|------|-------|
| cube | `gl_live_60fps` | true | Y | 40 |
| sphere | `gl_live_60fps` | true | X | 25 |
| sphere_shaded | `gl_overlay_cycle` | true | Y | 35 |

All three scenes set `max_steps = 1` (cube, sphere) or run a short NVT pass
(sphere_shaded). Spin is a **viewer-side transform only** — atom positions in
the output `.xyz` are not rotated.

---

## .vsim → .x tie-in

`sphere_shaded.vsim` writes its completed run output to `out/sphere_shaded_sim/`
and also populates `runs/sphere_shaded/` — the path that `sphere_shaded.x` reads
as its `run_dir`. The export package then repackages those artifacts into the
final verified bundle at `out/sphere_shaded/`.

This is the canonical XSIM two-step workflow:

```
.vsim  →  [xsim run]  →  runs/<id>/
							  ↓
.x     →  [xsim xport] →  out/<bundle>/
```
