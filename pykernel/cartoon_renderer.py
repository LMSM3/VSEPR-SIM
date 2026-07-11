"""
cartoon_renderer.py — Persistent Cartoon-3D Molecular Renderer
================================================================
GPU-accelerated VisPy renderer with cartoon shading for VSEPR-SIM.

Architecture:
  Renderer (this) ─── ZMQ SUB ──← tcp://127.0.0.1:5555
                                      └── Producer (zmq_producer.py) PUB

The renderer creates a single persistent OpenGL window and updates
geometry in-place when new frames arrive over ZeroMQ.  The window
never relaunches — it stays alive even when the producer pauses.

Visual Style — Cartoon-3D Hybrid:
  ┌──────────────────────────────────────────────────┐
  │  Beads (atoms):                                  │
  │    • 3D markers with solid CPK face color        │
  │    • Thick black edge outline (ink look)         │
  │    • Sizes proportional to vdW radius            │
  │                                                  │
  │  Bonds:                                          │
  │    • Double-layer line segments:                  │
  │      - Bottom: thick black (outline)              │
  │      - Top: thinner species-colored (fill)        │
  │    • Constant screen-space width (cartoon)        │
  │                                                  │
  │  Palette:                                         │
  │    • Saturated, limited CPK-derived colors        │
  │    • Classification coloring by Z range/group     │
  │    • Actinides: deep teal / purple                │
  │    • Transition metals: warm metallics            │
  └──────────────────────────────────────────────────┘

Usage:
  # Start renderer (waits for data on ZMQ):
  python -m pykernel.cartoon_renderer

  # Start with options:
  python -m pykernel.cartoon_renderer --endpoint tcp://127.0.0.1:5555
  python -m pykernel.cartoon_renderer --bg white
  python -m pykernel.cartoon_renderer --bead-scale 0.4

  # Typical two-terminal workflow:
  # Terminal 1:  python -m pykernel.cartoon_renderer
  # Terminal 2:  python -m pykernel.zmq_producer --demo

Dependencies:
  pip install vispy pyzmq msgpack numpy

Performance targets:
  100+ FPS rendering, 10 Hz data update (ZMQ poll)
"""

import sys
import json
import time
import argparse
import threading
from typing import Optional, Dict, List, Any, Tuple

import numpy as np

try:
    from vispy import scene, app
    from vispy.color import Color
except ImportError:
    print("ERROR: VisPy required.  pip install vispy")
    sys.exit(1)

try:
    import zmq
except ImportError:
    print("ERROR: pyzmq required.  pip install pyzmq")
    sys.exit(1)

try:
    import msgpack
    HAS_MSGPACK = True
except ImportError:
    HAS_MSGPACK = False


# ============================================================================
# Default CPK Palette (cartoon-saturated visual variant)
#
# These are NOT scientific reference data — they are a deliberate visual style
# choice for the cartoon/ZMQ pipeline.  The authoritative Jmol CPK palette
# lives in the kernel (src/vis/renderer_base.cpp) and is accessible via
# pykernel.element_data.cpk_color().
# ============================================================================

CPK_COLORS: Dict[str, Tuple[float, float, float, float]] = {
    "H":  (0.95, 0.95, 0.95, 1.0),
    "C":  (0.35, 0.35, 0.35, 1.0),
    "N":  (0.20, 0.30, 0.90, 1.0),
    "O":  (0.90, 0.15, 0.15, 1.0),
    "F":  (0.20, 0.85, 0.35, 1.0),
    "P":  (0.95, 0.55, 0.10, 1.0),
    "S":  (0.95, 0.90, 0.15, 1.0),
    "Cl": (0.15, 0.80, 0.25, 1.0),
    "Br": (0.60, 0.20, 0.15, 1.0),
    "I":  (0.55, 0.10, 0.55, 1.0),
    "Li": (0.75, 0.15, 0.75, 1.0),
    "Na": (0.65, 0.40, 0.90, 1.0),
    "K":  (0.55, 0.35, 0.85, 1.0),
    "Ca": (0.25, 0.75, 0.25, 1.0),
    "Mg": (0.20, 0.65, 0.20, 1.0),
    "Fe": (0.70, 0.45, 0.15, 1.0),
    "Co": (0.55, 0.35, 0.65, 1.0),
    "Ni": (0.40, 0.60, 0.35, 1.0),
    "Cu": (0.75, 0.50, 0.20, 1.0),
    "Zn": (0.55, 0.55, 0.70, 1.0),
    "Ti": (0.60, 0.65, 0.70, 1.0),
    "Cr": (0.50, 0.55, 0.65, 1.0),
    "Mn": (0.60, 0.40, 0.60, 1.0),
    "Ag": (0.80, 0.80, 0.85, 1.0),
    "Au": (0.90, 0.80, 0.20, 1.0),
    "Pt": (0.75, 0.75, 0.80, 1.0),
    "Si": (0.65, 0.55, 0.40, 1.0),
    "Ge": (0.55, 0.60, 0.55, 1.0),
    "Ga": (0.60, 0.55, 0.65, 1.0),
    "As": (0.50, 0.65, 0.50, 1.0),
    "He": (0.80, 0.95, 1.00, 1.0),
    "Ne": (0.90, 0.50, 0.40, 1.0),
    "Ar": (0.50, 0.70, 0.85, 1.0),
    "Kr": (0.45, 0.75, 0.60, 1.0),
    "Xe": (0.40, 0.55, 0.80, 1.0),
    "U":  (0.15, 0.50, 0.45, 1.0),
    "Pu": (0.20, 0.45, 0.50, 1.0),
    "Th": (0.30, 0.55, 0.50, 1.0),
    "Am": (0.40, 0.30, 0.55, 1.0),
    "B":  (0.85, 0.70, 0.55, 1.0),
    "Al": (0.70, 0.70, 0.75, 1.0),
    "?":  (0.90, 0.20, 0.90, 1.0),
}

# Size scale for cartoon beads (bead_display_radius = vdw_radius * BEAD_SCALE)
DEFAULT_BEAD_SCALE = 0.35


# ============================================================================
# ZeroMQ Receiver Thread
# ============================================================================

class ZMQReceiver:
    """Background thread that receives ZMQ frames and stores the latest."""

    def __init__(self, endpoint: str = "tcp://127.0.0.1:5555"):
        self.endpoint = endpoint
        self.latest_frame: Optional[Dict[str, Any]] = None
        self.frame_count = 0
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None

    def start(self):
        self._running = True
        self._thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=2.0)

    def get_latest(self) -> Optional[Dict[str, Any]]:
        """Return latest frame (or None) and clear it."""
        with self._lock:
            frame = self.latest_frame
            self.latest_frame = None
            return frame

    def _recv_loop(self):
        ctx = zmq.Context()
        sock = ctx.socket(zmq.SUB)
        sock.connect(self.endpoint)
        sock.subscribe(b"FRAME")
        sock.setsockopt(zmq.RCVTIMEO, 100)  # 100ms timeout

        print(f"[Renderer] Subscribed to {self.endpoint}")

        while self._running:
            try:
                msg = sock.recv()
            except zmq.Again:
                continue

            # Strip "FRAME" prefix (5 bytes)
            payload = msg[5:]

            try:
                if HAS_MSGPACK:
                    try:
                        frame = msgpack.unpackb(payload, raw=False)
                    except (msgpack.UnpackException, ValueError):
                        frame = json.loads(payload.decode('utf-8'))
                else:
                    frame = json.loads(payload.decode('utf-8'))

                with self._lock:
                    self.latest_frame = frame
                    self.frame_count += 1
            except Exception as e:
                print(f"[Renderer] Frame decode error: {e}")

        sock.close()
        ctx.term()


# ============================================================================
# Cartoon Molecular Renderer
# ============================================================================

class CartoonMoleculeRenderer:
    """
    Persistent VisPy window with cartoon-3D molecular visualization.

    Visual layers (back to front):
      1. Bond outlines  (thick black lines)
      2. Bond fills      (thinner colored lines)
      3. Bead markers    (3D markers with black edge)
      4. HUD text        (frame info overlay)
    """

    def __init__(self, endpoint: str = "tcp://127.0.0.1:5555",
                 bead_scale: float = DEFAULT_BEAD_SCALE,
                 bond_width_outline: float = 4.0,
                 bond_width_fill: float = 2.0,
                 bg_color: str = "#1a1a2e",
                 title: str = "VSEPR-SIM Cartoon Renderer"):

        self.bead_scale = bead_scale
        self.bond_width_outline = bond_width_outline
        self.bond_width_fill = bond_width_fill

        # ── Canvas & View ──
        self.canvas = scene.SceneCanvas(
            keys='interactive',
            show=False,
            title=title,
            size=(1200, 800),
            bgcolor=bg_color,
        )
        self.view = self.canvas.central_widget.add_view()
        self.view.camera = scene.TurntableCamera(
            distance=15.0,
            elevation=20.0,
            azimuth=45.0,
            fov=45.0,
        )

        # ── Visual Layers ──

        # Layer 1: Bond outlines (thick black)
        self.bond_outlines = scene.visuals.Line(
            connect='segments',
            color='black',
            width=bond_width_outline,
            parent=self.view.scene,
        )

        # Layer 2: Bond fills (thinner, colored)
        self.bond_fills = scene.visuals.Line(
            connect='segments',
            color='gray',
            width=bond_width_fill,
            parent=self.view.scene,
        )

        # Layer 3: Bead markers (atoms)
        self.beads = scene.visuals.Markers(parent=self.view.scene)

        # Layer 4: HUD text
        self.hud = scene.visuals.Text(
            text="Waiting for data...",
            color='white',
            font_size=10,
            anchor_x='left',
            anchor_y='top',
            pos=(10, 18),
            parent=self.canvas.scene,  # screen-space, not 3D
        )

        # ── State ──
        self.current_symbols: List[str] = []
        self.current_frame_id = 0
        self.fps_counter = _FPSCounter()

        # ── ZMQ Receiver ──
        self.receiver = ZMQReceiver(endpoint=endpoint)

        # ── Timer for polling ZMQ ──
        self.poll_timer = app.Timer(
            interval=0.05,  # 20 Hz poll → more than enough for 10 Hz data
            connect=self._on_poll,
            start=False,
        )

    def run(self):
        """Start the renderer (blocking)."""
        self.receiver.start()
        self.poll_timer.start()
        self.canvas.show()
        print("[Renderer] Window open. Waiting for frames...")
        print("[Renderer] Controls: drag=orbit, scroll=zoom, R=reset, ESC=quit")
        app.run()

    def _on_poll(self, event):
        """Called by timer — check for new ZMQ frame and update visuals."""
        frame = self.receiver.get_latest()
        if frame is not None:
            self._update_visuals(frame)

        # Update FPS in HUD
        self.fps_counter.tick()

    def _update_visuals(self, frame: Dict[str, Any]):
        """Update all visual layers from a frame packet."""
        positions = np.array(frame.get("positions", []), dtype=np.float32)
        symbols = frame.get("symbols", [])
        bonds = frame.get("bonds", [])
        colors_raw = frame.get("colors", [])
        radii_raw = frame.get("radii", [])
        frame_id = frame.get("frame_id", 0)
        energy = frame.get("energy", 0.0)
        title = frame.get("title", "")

        n = len(positions)
        if n == 0:
            return

        self.current_symbols = symbols
        self.current_frame_id = frame_id

        # ── Colors ──
        if colors_raw and len(colors_raw) == n:
            face_colors = np.array(colors_raw, dtype=np.float32)
        else:
            face_colors = np.array(
                [CPK_COLORS.get(s, CPK_COLORS["?"]) for s in symbols],
                dtype=np.float32
            )
        # Ensure RGBA
        if face_colors.shape[1] == 3:
            alpha = np.ones((n, 1), dtype=np.float32)
            face_colors = np.hstack([face_colors, alpha])

        # ── Sizes (screen-space pixels, scaled from vdW radii) ──
        if radii_raw and len(radii_raw) == n:
            radii = np.array(radii_raw, dtype=np.float32)
        else:
            radii = np.array(
                [1.5] * n, dtype=np.float32
            )
        sizes = radii * self.bead_scale * 40.0  # scale to pixel units

        # ── Update Beads ──
        self.beads.set_data(
            pos=positions,
            face_color=face_colors,
            edge_color='black',
            edge_width=1.5,
            size=sizes,
        )

        # ── Update Bonds ──
        if bonds:
            bond_arr = np.array(bonds, dtype=np.int32)
            # Build segment endpoints: (2*M, 3)
            seg_pos = np.zeros((2 * len(bond_arr), 3), dtype=np.float32)
            seg_colors_fill = np.zeros((2 * len(bond_arr), 4), dtype=np.float32)

            for k, (i, j) in enumerate(bond_arr):
                if i < n and j < n:
                    seg_pos[2 * k] = positions[i]
                    seg_pos[2 * k + 1] = positions[j]
                    # Color: average of the two atoms
                    seg_colors_fill[2 * k] = face_colors[i]
                    seg_colors_fill[2 * k + 1] = face_colors[j]

            self.bond_outlines.set_data(pos=seg_pos, connect='segments')
            self.bond_fills.set_data(
                pos=seg_pos,
                color=seg_colors_fill,
                connect='segments',
            )
        else:
            # No bonds — clear
            empty = np.zeros((0, 3), dtype=np.float32)
            self.bond_outlines.set_data(pos=empty)
            self.bond_fills.set_data(pos=empty)

        # ── Auto-center camera on first frame ──
        if frame_id <= 1 and n > 0:
            center = positions.mean(axis=0)
            extent = np.linalg.norm(positions.max(axis=0) - positions.min(axis=0))
            self.view.camera.center = tuple(center)
            self.view.camera.distance = max(extent * 2.0, 8.0)

        # ── Update HUD ──
        fps_str = f"{self.fps_counter.fps:.0f}" if self.fps_counter.fps > 0 else "..."
        hud_text = (
            f"Frame: {frame_id}  |  Atoms: {n}  |  "
            f"Bonds: {len(bonds)}  |  E: {energy:.2f} kcal/mol\n"
            f"{title}  |  Render FPS: {fps_str}  |  "
            f"ZMQ frames: {self.receiver.frame_count}"
        )
        self.hud.text = hud_text

        self.canvas.update()


# ============================================================================
# FPS Counter
# ============================================================================

class _FPSCounter:
    """Simple rolling-window FPS counter."""

    def __init__(self, window: int = 30):
        self.window = window
        self._times: list = []
        self.fps: float = 0.0

    def tick(self):
        now = time.time()
        self._times.append(now)
        if len(self._times) > self.window:
            self._times = self._times[-self.window:]
        if len(self._times) >= 2:
            dt = self._times[-1] - self._times[0]
            if dt > 0:
                self.fps = (len(self._times) - 1) / dt


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="VSEPR-SIM Cartoon-3D Molecular Renderer (VisPy + ZeroMQ)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Visual style: Cartoon-3D (solid beads + thick inked bonds + limited palette)

Two-terminal workflow:
  Terminal 1:  python -m pykernel.cartoon_renderer
  Terminal 2:  python -m pykernel.zmq_producer --demo

Controls:
  Left drag    Orbit camera
  Scroll       Zoom
  Middle drag  Pan
  R            Reset view
  ESC          Quit
        """)

    parser.add_argument("--endpoint", default="tcp://127.0.0.1:5555",
                        help="ZeroMQ SUB endpoint (default: tcp://127.0.0.1:5555)")
    parser.add_argument("--bead-scale", type=float, default=DEFAULT_BEAD_SCALE,
                        help=f"Bead size multiplier (default: {DEFAULT_BEAD_SCALE})")
    parser.add_argument("--bond-outline", type=float, default=4.0,
                        help="Bond outline width in pixels (default: 4.0)")
    parser.add_argument("--bond-fill", type=float, default=2.0,
                        help="Bond fill width in pixels (default: 2.0)")
    parser.add_argument("--bg", default="#1a1a2e",
                        help="Background color (default: #1a1a2e dark blue)")
    parser.add_argument("--title", default="VSEPR-SIM Cartoon Renderer",
                        help="Window title")

    args = parser.parse_args()

    renderer = CartoonMoleculeRenderer(
        endpoint=args.endpoint,
        bead_scale=args.bead_scale,
        bond_width_outline=args.bond_outline,
        bond_width_fill=args.bond_fill,
        bg_color=args.bg,
        title=args.title,
    )
    renderer.run()


if __name__ == "__main__":
    main()
