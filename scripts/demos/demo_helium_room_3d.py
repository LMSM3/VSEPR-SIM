#!/usr/bin/env python
"""
demo_helium_room_3d.py -- 3D Room Heat Dispersion Visualization

Renders helium thermal diffusion through a room with obstacles as
a 3D "dust cloud" colour gradient using VisPy scatter markers.

Runs multiple presets: ambient, cold, hot, lab, reactor.
Each preset produces a VisPy window showing:
  - Obstacle voxels (grey opaque cubes)
  - Heat field as semi-transparent coloured point cloud
  - Colour scale: blue (cold) -> white -> red (hot)

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import sys
import importlib.util
import pathlib
import math
import numpy as np

# -- direct module loading (bypass pykernel.__init__ VisPy gate) -----------
def _load(name, filename):
    base = pathlib.Path(__file__).resolve().parent.parent.parent / "pykernel"
    path = base / filename
    spec = importlib.util.spec_from_file_location(name, str(path))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

room_sim = _load("room_sim", "room_sim.py")

# VisPy imports
from vispy import app, scene
from vispy.color import Colormap


def temperature_to_rgba(T_array, T_min, T_max):
    """Map temperature values to RGBA colours: blue -> white -> red."""
    if T_max - T_min < 0.01:
        norm = np.zeros_like(T_array)
    else:
        norm = (T_array - T_min) / (T_max - T_min)
    norm = np.clip(norm, 0, 1)
    rgba = np.zeros((len(norm), 4), dtype=np.float32)
    # Blue (cold) to white (mid) to red (hot)
    # r: 0 -> 1 -> 1
    # g: 0 -> 1 -> 0
    # b: 1 -> 1 -> 0
    mid = 0.5
    cold = norm <= mid
    hot = norm > mid
    t_cold = norm[cold] / mid
    t_hot = (norm[hot] - mid) / mid
    # Cold side: blue -> white
    rgba[cold, 0] = t_cold
    rgba[cold, 1] = t_cold
    rgba[cold, 2] = 1.0
    # Hot side: white -> red
    rgba[hot, 0] = 1.0
    rgba[hot, 1] = 1.0 - t_hot
    rgba[hot, 2] = 1.0 - t_hot
    # Alpha: stronger near extremes, faint in the middle
    rgba[:, 3] = 0.05 + 0.6 * np.abs(norm - 0.5) * 2
    return rgba


def render_room(sim, title="Helium Room Heat Dispersion"):
    """Create a VisPy 3D scene for a completed room simulation."""
    geom = sim.geom
    solver = sim.solver
    T = solver.T

    canvas = scene.SceneCanvas(title=title, keys="interactive",
                               size=(1200, 900), bgcolor="black", show=False)
    view = canvas.central_widget.add_view()
    view.camera = scene.cameras.TurntableCamera(
        fov=60, distance=max(geom.Lx, geom.Ly, geom.Lz) * 2.5,
        elevation=30, azimuth=45)

    # -- obstacle voxels (grey cubes) ------------------------------------
    obs_idx = np.argwhere(geom.mask)
    if len(obs_idx) > 0:
        obs_pos = obs_idx.astype(np.float32)
        obs_pos[:, 0] *= geom.dx
        obs_pos[:, 1] *= geom.dy
        obs_pos[:, 2] *= geom.dz
        obs_colors = np.full((len(obs_pos), 4), [0.4, 0.4, 0.4, 0.85],
                             dtype=np.float32)
        obs_scatter = scene.visuals.Markers()
        obs_scatter.set_data(obs_pos, face_color=obs_colors,
                             edge_width=0, size=8, symbol="square")
        view.add(obs_scatter)

    # -- heat field (dust cloud) -----------------------------------------
    open_idx = np.argwhere(~geom.mask)
    if len(open_idx) > 0:
        # Subsample if too many points for performance
        max_pts = 80000
        if len(open_idx) > max_pts:
            step = len(open_idx) // max_pts
            open_idx = open_idx[::step]

        heat_pos = open_idx.astype(np.float32)
        heat_pos[:, 0] *= geom.dx
        heat_pos[:, 1] *= geom.dy
        heat_pos[:, 2] *= geom.dz

        T_vals = np.array([T[i, j, k] for i, j, k in open_idx],
                          dtype=np.float32)

        T_min_val = solver.T_min
        T_max_val = solver.T_max
        heat_rgba = temperature_to_rgba(T_vals, T_min_val, T_max_val)

        heat_scatter = scene.visuals.Markers()
        heat_scatter.set_data(heat_pos, face_color=heat_rgba,
                              edge_width=0, size=5, symbol="disc")
        view.add(heat_scatter)

    # -- room wireframe (bounding box) -----------------------------------
    corners = np.array([
        [0, 0, 0], [geom.Lx, 0, 0], [geom.Lx, geom.Ly, 0], [0, geom.Ly, 0],
        [0, 0, geom.Lz], [geom.Lx, 0, geom.Lz],
        [geom.Lx, geom.Ly, geom.Lz], [0, geom.Ly, geom.Lz],
    ], dtype=np.float32)
    edges = np.array([
        corners[0], corners[1], corners[1], corners[2],
        corners[2], corners[3], corners[3], corners[0],
        corners[4], corners[5], corners[5], corners[6],
        corners[6], corners[7], corners[7], corners[4],
        corners[0], corners[4], corners[1], corners[5],
        corners[2], corners[6], corners[3], corners[7],
    ], dtype=np.float32)
    line = scene.visuals.Line(pos=edges, color=(0.3, 0.8, 0.3, 0.6),
                              width=2, connect="segments")
    view.add(line)

    # -- axis + labels ---------------------------------------------------
    axis = scene.visuals.XYZAxis(parent=view.scene)

    # Info text
    info = "T: min={:.0f}K  mean={:.0f}K  max={:.0f}K | steps={} t={:.1f}s".format(
        solver.T_min, solver.T_mean, solver.T_max,
        solver.step_count, solver.t_elapsed)
    text = scene.visuals.Text(info, pos=(10, 20), color="white",
                              font_size=12, anchor_x="left",
                              parent=canvas.scene)

    canvas.show()
    return canvas


def run_preset(name, config, n_steps=200):
    """Run a simulation preset and render it."""
    print("\n" + "=" * 70)
    print("  PRESET: {}".format(name))
    print("=" * 70)
    sim = room_sim.RoomSimulation(config)
    sim.run(n_steps=n_steps, record_interval=50)
    canvas = render_room(sim, title="He Room: {} (T_amb={:.0f}K)".format(
        name, config.T_ambient))
    return sim, canvas


def main():
    presets = [
        ("Ambient 300K (random)",  room_sim.RoomSimulation.preset_ambient()),
        ("Cold 200K (random)",     room_sim.RoomSimulation.preset_cold()),
        ("Hot 400K (random)",      room_sim.RoomSimulation.preset_hot()),
        ("Lab Layout 295K",        room_sim.RoomSimulation.preset_lab()),
        ("Reactor Hall 600K/50atm", room_sim.RoomSimulation.preset_reactor()),
        ("Large Volume 300K",      room_sim.RoomSimulation.preset_large_volume()),
    ]

    canvases = []
    for name, cfg in presets:
        sim, canvas = run_preset(name, cfg, n_steps=cfg.n_steps)
        canvases.append(canvas)

    print("\n[demo] All presets rendered. Close windows to exit.")
    app.run()


if __name__ == "__main__":
    main()
