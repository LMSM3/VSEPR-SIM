"""
demo_calibration_3d.py — 3D Visual Demo of the Calibration Shell

Launches a VisPy 3D scene showing:
  - Pipe segments colored by temperature (blue→red gradient)
  - Sphere markers at loop boundaries (primary/secondary/tertiary)
  - Degradation severity bars rising from each segment
  - Regime transition markers
  - Auto-spin camera for inspection
  - Live annotation overlay with session stats

Runs CalibrationShell.quick_run() on the helium HTGR preset,
then builds the scene from the recorded history.

Usage:
    python scripts/demos/demo_calibration_3d.py
"""

import sys, pathlib, importlib.util, math
import numpy as np

# ── Direct-load pykernel modules (bypass __init__ VisPy gate) ─────────
_root = pathlib.Path(__file__).resolve().parent.parent.parent

def _load(name, filename):
    p = _root / 'pykernel' / filename
    spec = importlib.util.spec_from_file_location(name, str(p))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

gas  = _load('pykernel.gas', 'gas.py')
form = _load('pykernel.formation', 'formation.py')
cal  = _load('pykernel.calibration_shell', 'calibration_shell.py')

# ── Run calibration ───────────────────────────────────────────────────
print("[demo] Running helium HTGR calibration (1200C peak, 1000 steps)...")
cfg = cal.CalibrationShell.preset_helium_htgr()
cfg.max_steps = 500
cfg.n_thermal_cycles = 50
session = cal.CalibrationShell.quick_run(cfg)
print(session.summary())

history = session.history
n_pts = len(history)

# ── Build geometry arrays ─────────────────────────────────────────────
# Pipe as a helix (3 loops = 3 heating circuits)
helix_turns = 3.0
helix_radius = 4.0
helix_pitch = 6.0

positions = np.zeros((n_pts, 3), dtype=np.float32)
for i, rec in enumerate(history):
    frac = i / max(n_pts - 1, 1)
    theta = frac * helix_turns * 2 * math.pi
    positions[i, 0] = helix_radius * math.cos(theta)
    positions[i, 1] = helix_radius * math.sin(theta)
    positions[i, 2] = frac * helix_pitch * helix_turns

# Temperature colormap (blue=600C → red=1200C)
T_min = min(r['T_C'] for r in history)
T_max = max(r['T_C'] for r in history)
T_range = max(T_max - T_min, 1.0)

colors = np.zeros((n_pts, 4), dtype=np.float32)
for i, rec in enumerate(history):
    t_norm = (rec['T_C'] - T_min) / T_range
    colors[i] = [t_norm, 0.15, 1.0 - t_norm, 0.9]

# Severity bars (vertical lines from pipe)
severity = np.array([r['severity'] for r in history], dtype=np.float32)
sev_scale = 3.0  # max bar height

# Regime transition markers
transitions = []
for i, rec in enumerate(history):
    if 'regime_transition' in rec:
        transitions.append((i, rec['regime_transition'], rec['T_C']))

# Loop boundary markers (where loop classification changes)
loop_boundaries = []
prev_loop = history[0]['loop']
for i, rec in enumerate(history):
    if rec['loop'] != prev_loop:
        loop_boundaries.append((i, rec['loop'], rec['T_C']))
        prev_loop = rec['loop']

# ── VisPy Scene ───────────────────────────────────────────────────────
from vispy import app, scene
from vispy.scene import visuals

canvas = scene.SceneCanvas(
    title=f'VSEPR-SIM — Calibration 3D: {cfg.run_name}',
    keys='interactive',
    size=(1400, 900),
    bgcolor='#0a0a1a',
    show=False,
)

view = canvas.central_widget.add_view()
view.camera = scene.TurntableCamera(
    distance=35,
    elevation=25,
    azimuth=45,
    fov=50,
    center=(0, 0, helix_pitch * helix_turns / 2),
)

# 1. Pipe segments as thick line
pipe_line = visuals.Line(
    pos=positions,
    color=colors,
    width=4,
    connect='strip',
    parent=view.scene,
)

# 2. Temperature spheres at each sample point (sized by pressure drop)
dP_arr = np.array([abs(r['dP_Pa']) for r in history], dtype=np.float32)
dP_norm = dP_arr / max(dP_arr.max(), 1.0)
marker_sizes = 3.0 + 8.0 * dP_norm

pipe_markers = visuals.Markers(parent=view.scene)
pipe_markers.set_data(
    positions,
    face_color=colors,
    edge_color='white',
    edge_width=0.5,
    size=marker_sizes,
)

# 3. Severity bars — vertical lines from pipe upward
sev_positions = []
sev_colors = []
step_interval = max(n_pts // 80, 1)  # show ~80 bars
for i in range(0, n_pts, step_interval):
    base = positions[i].copy()
    top = base.copy()
    top[2] += severity[i] * sev_scale
    sev_positions.append(base)
    sev_positions.append(top)
    # Green=safe -> Yellow=warning -> Red=critical
    s = severity[i]
    if s < 0.3:
        c = [0.1, 0.9, 0.2, 0.8]
    elif s < 0.7:
        c = [1.0, 0.8, 0.1, 0.8]
    else:
        c = [1.0, 0.15, 0.1, 0.9]
    sev_colors.append(c)
    sev_colors.append(c)

if sev_positions:
    sev_line = visuals.Line(
        pos=np.array(sev_positions, dtype=np.float32),
        color=np.array(sev_colors, dtype=np.float32),
        width=3,
        connect='segments',
        parent=view.scene,
    )

# 4. Regime transition markers — large spheres
if transitions:
    tr_pos = np.array([positions[i] for i, _, _ in transitions], dtype=np.float32)
    tr_colors_map = {
        'MOLTEN_SALT': [0.2, 0.6, 1.0, 1.0],
        'HELIUM_GAS':  [1.0, 0.9, 0.2, 1.0],
        'RADIATION':   [1.0, 0.2, 0.1, 1.0],
    }
    tr_colors = np.array([tr_colors_map.get(name, [1,1,1,1]) for _, name, _ in transitions], dtype=np.float32)

    tr_markers = visuals.Markers(parent=view.scene)
    tr_markers.set_data(
        tr_pos,
        face_color=tr_colors,
        edge_color='white',
        edge_width=2,
        size=18,
        symbol='diamond',
    )

    # Labels for transitions
    for idx, (i, name, T_C) in enumerate(transitions):
        pos_label = positions[i].copy()
        pos_label[2] += 2.5
        label = visuals.Text(
            f'{name}\n{T_C:.0f}C',
            pos=pos_label,
            color='white',
            font_size=8,
            anchor_x='center',
            anchor_y='bottom',
            parent=view.scene,
        )

# 5. Loop boundary spheres
if loop_boundaries:
    lb_pos = np.array([positions[i] for i, _, _ in loop_boundaries], dtype=np.float32)
    lb_colors_map = {
        'PRIMARY':   [0.3, 0.5, 1.0, 0.9],
        'SECONDARY': [0.5, 0.8, 0.3, 0.9],
        'TERTIARY':  [1.0, 0.6, 0.1, 0.9],
        'BEYOND':    [1.0, 0.1, 0.1, 0.9],
    }
    lb_colors = np.array([lb_colors_map.get(name, [1,1,1,1]) for _, name, _ in loop_boundaries], dtype=np.float32)
    lb_markers = visuals.Markers(parent=view.scene)
    lb_markers.set_data(
        lb_pos,
        face_color=lb_colors,
        edge_color='yellow',
        edge_width=1.5,
        size=14,
        symbol='square',
    )

# 6. Axes + grid
axis = visuals.XYZAxis(parent=view.scene)

# 7. Title overlay
title_text = visuals.Text(
    f'VSEPR-SIM Calibration Demo\n'
    f'{cfg.run_name}  |  {cfg.gas_species} @ {cfg.T_peak_C:.0f}C peak\n'
    f'Steps: {session.step_count}  |  Severity: {session.degradation.severity_score():.4f}\n'
    f'Regime: {session.regime_watch.current_regime.name}  |  '
    f'Transitions: {len(session.regime_watch.history)}',
    pos=(10, 10),
    color='#ccddff',
    font_size=10,
    anchor_x='left',
    anchor_y='top',
    parent=canvas.scene,
)

# 8. Legend
legend_items = [
    ('Blue = Cold (600C)', '#3366ff'),
    ('Red = Hot (1200C+)', '#ff3333'),
    ('Green bar = Low severity', '#22cc44'),
    ('Red bar = High severity', '#ff2222'),
    ('Diamond = Regime transition', '#ffdd33'),
    ('Square = Loop boundary', '#ffaa22'),
]
for idx, (label, color) in enumerate(legend_items):
    visuals.Text(
        label,
        pos=(10, 80 + idx * 18),
        color=color,
        font_size=8,
        anchor_x='left',
        anchor_y='top',
        parent=canvas.scene,
    )

# ── Auto-spin ─────────────────────────────────────────────────────────
spin_timer = app.Timer(interval=0.03, connect=lambda ev: _spin(ev), start=True)

def _spin(ev):
    view.camera.azimuth += 0.3

# ── Launch ────────────────────────────────────────────────────────────
print("\n[demo] Launching 3D viewer...")
print("  Controls: drag=rotate, scroll=zoom, right-drag=pan")
print("  Press ESC to close\n")
canvas.show()
app.run()