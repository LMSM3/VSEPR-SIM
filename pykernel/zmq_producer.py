"""
zmq_producer.py — ZeroMQ Molecular Frame Publisher
====================================================
Producer process for the live cartoon renderer.

Reads molecular data from:
  - .xyz / .xyzA files (file-watch mode)
  - VSEPR-SIM simulation output (pipe mode)
  - Programmatic API (push mode)

Publishes structured frame packets over ZeroMQ PUB socket.

Architecture:
  Producer (this) ─── ZMQ PUB ──→ tcp://127.0.0.1:5555
                                      └── Renderer (cartoon_renderer.py) SUB

Packet format (MessagePack or JSON):
  {
      "frame_id": int,
      "time": float,
      "positions": [[x,y,z], ...],
      "symbols": ["C", "H", ...],
      "radii": [1.7, 1.2, ...],
      "colors": [[r,g,b,a], ...],
      "bonds": [[i,j], ...],
      "labels": ["C1", "H2", ...],
      "box": [Lx, Ly, Lz],
      "energy": float,
      "title": str,
  }

Usage:
  # File-watch mode (watches for changes, publishes on update):
  python -m pykernel.zmq_producer --watch path/to/mol.xyz

  # Single-shot mode (publish one frame and exit):
  python -m pykernel.zmq_producer --file path/to/mol.xyz

  # Demo mode (synthetic rotating molecule):
  python -m pykernel.zmq_producer --demo

  # Programmatic:
  from pykernel.zmq_producer import MolecularPublisher
  pub = MolecularPublisher()
  pub.publish_frame(positions, symbols, bonds=bonds)

Dependencies:
  pip install pyzmq msgpack numpy
"""

import os
import sys
import time
import json
import struct
import hashlib
import argparse
from pathlib import Path
from typing import List, Optional, Tuple, Dict, Any

import numpy as np

from pykernel.element_data import (
    covalent_radius as _kernel_cov_radius,
    vdw_radius as _kernel_vdw_radius,
    cpk_color as _kernel_cpk_color,
)

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
# CPK Color Palette (cartoon-saturated visual variant)
#
# These are NOT scientific reference data — they are a deliberate visual style
# choice for the cartoon/ZMQ pipeline.  The authoritative Jmol CPK palette
# lives in the kernel (src/vis/renderer_base.cpp) and is accessible via
# pykernel.element_data.cpk_color().
# ============================================================================

CPK_COLORS: Dict[str, Tuple[float, float, float, float]] = {
    # Main-group organic
    "H":  (0.95, 0.95, 0.95, 1.0),   # near-white
    "C":  (0.35, 0.35, 0.35, 1.0),   # dark gray
    "N":  (0.20, 0.30, 0.90, 1.0),   # blue
    "O":  (0.90, 0.15, 0.15, 1.0),   # red
    "F":  (0.20, 0.85, 0.35, 1.0),   # green
    "P":  (0.95, 0.55, 0.10, 1.0),   # orange
    "S":  (0.95, 0.90, 0.15, 1.0),   # yellow
    "Cl": (0.15, 0.80, 0.25, 1.0),   # green
    "Br": (0.60, 0.20, 0.15, 1.0),   # brown
    "I":  (0.55, 0.10, 0.55, 1.0),   # purple
    # Alkali / alkaline
    "Li": (0.75, 0.15, 0.75, 1.0),   # violet
    "Na": (0.65, 0.40, 0.90, 1.0),   # purple
    "K":  (0.55, 0.35, 0.85, 1.0),   # purple
    "Ca": (0.25, 0.75, 0.25, 1.0),   # green
    "Mg": (0.20, 0.65, 0.20, 1.0),   # dark green
    # Transition metals
    "Fe": (0.70, 0.45, 0.15, 1.0),   # rust
    "Co": (0.55, 0.35, 0.65, 1.0),   # mauve
    "Ni": (0.40, 0.60, 0.35, 1.0),   # olive
    "Cu": (0.75, 0.50, 0.20, 1.0),   # copper
    "Zn": (0.55, 0.55, 0.70, 1.0),   # slate
    "Ti": (0.60, 0.65, 0.70, 1.0),   # steel
    "Cr": (0.50, 0.55, 0.65, 1.0),   # chrome
    "Mn": (0.60, 0.40, 0.60, 1.0),   # purple-gray
    "Ag": (0.80, 0.80, 0.85, 1.0),   # silver
    "Au": (0.90, 0.80, 0.20, 1.0),   # gold
    "Pt": (0.75, 0.75, 0.80, 1.0),   # platinum
    # Semiconductors
    "Si": (0.65, 0.55, 0.40, 1.0),   # tan
    "Ge": (0.55, 0.60, 0.55, 1.0),   # gray-green
    "Ga": (0.60, 0.55, 0.65, 1.0),   # lavender
    "As": (0.50, 0.65, 0.50, 1.0),   # sage
    # Noble gases
    "He": (0.80, 0.95, 1.00, 1.0),   # ice blue
    "Ne": (0.90, 0.50, 0.40, 1.0),   # neon red
    "Ar": (0.50, 0.70, 0.85, 1.0),   # sky blue
    "Kr": (0.45, 0.75, 0.60, 1.0),   # teal
    "Xe": (0.40, 0.55, 0.80, 1.0),   # blue-gray
    # Actinides
    "U":  (0.15, 0.50, 0.45, 1.0),   # deep teal
    "Pu": (0.20, 0.45, 0.50, 1.0),   # teal
    "Th": (0.30, 0.55, 0.50, 1.0),   # green-gray
    "Am": (0.40, 0.30, 0.55, 1.0),   # purple
    # Boron group
    "B":  (0.85, 0.70, 0.55, 1.0),   # tan
    "Al": (0.70, 0.70, 0.75, 1.0),   # aluminum
    # Default
    "?":  (0.90, 0.20, 0.90, 1.0),   # magenta (unknown)
}

# Van der Waals radii — read from C++ kernel (src/pot/vdw_radii.hpp)
# Retained as a module-level name for backward compatibility with __init__.py
# re-exports.  Delegates to pykernel.element_data at lookup time.
def _vdw_lookup(s: str) -> float:
    return _kernel_vdw_radius(s)

# Backward-compat alias (used by __init__.py re-export ZMQ_VDW_RADII)
VDW_RADII = type("_VDWProxy", (), {
    "get": staticmethod(lambda s, default=2.0: _kernel_vdw_radius(s)),
    "__getitem__": staticmethod(lambda s: _kernel_vdw_radius(s)),
    "__repr__": staticmethod(lambda: "<VDW radii proxy → pykernel.element_data>"),
})()


# ============================================================================
# XYZ File Parser
# ============================================================================

def parse_xyz_file(path: str) -> Optional[Dict[str, Any]]:
    """Parse standard .xyz or .xyzA file into frame dict."""
    try:
        with open(path, 'r') as f:
            lines = f.readlines()
    except (IOError, OSError):
        return None

    if len(lines) < 3:
        return None

    try:
        n_atoms = int(lines[0].strip())
    except ValueError:
        return None

    comment = lines[1].strip()

    symbols = []
    positions = []
    for i in range(2, min(2 + n_atoms, len(lines))):
        parts = lines[i].split()
        if len(parts) < 4:
            continue
        symbols.append(parts[0])
        positions.append([float(parts[1]), float(parts[2]), float(parts[3])])

    if len(positions) != n_atoms:
        return None

    positions = np.array(positions, dtype=np.float64)

    # Infer bonds from distances (simple covalent radius check)
    bonds = infer_bonds(positions, symbols)

    # Build radii and colors
    radii = [_kernel_vdw_radius(s) for s in symbols]
    colors = [list(CPK_COLORS.get(s, CPK_COLORS["?"])) for s in symbols]

    # Build labels
    element_counter: Dict[str, int] = {}
    labels = []
    for s in symbols:
        element_counter[s] = element_counter.get(s, 0) + 1
        labels.append(f"{s}{element_counter[s]}")

    return {
        "positions": positions.tolist(),
        "symbols": symbols,
        "radii": radii,
        "colors": colors,
        "bonds": bonds,
        "labels": labels,
        "box": [0.0, 0.0, 0.0],
        "energy": 0.0,
        "title": comment,
    }


def infer_bonds(positions: np.ndarray, symbols: List[str],
                tolerance: float = 1.3) -> List[List[int]]:
    """Infer bonds from covalent radii with tolerance factor.

    Covalent radii are read from the C++ kernel (src/pot/covalent_radii.hpp)
    via pykernel.element_data.
    """
    n = len(symbols)
    bonds = []
    for i in range(n):
        ri = _kernel_cov_radius(symbols[i])
        for j in range(i + 1, n):
            rj = _kernel_cov_radius(symbols[j])
            dist = np.linalg.norm(positions[i] - positions[j])
            if dist < (ri + rj) * tolerance and dist > 0.4:
                bonds.append([i, j])
    return bonds


# ============================================================================
# ZeroMQ Publisher
# ============================================================================

class MolecularPublisher:
    """Publishes molecular frame data over ZeroMQ PUB socket."""

    def __init__(self, endpoint: str = "tcp://127.0.0.1:5555",
                 use_msgpack: bool = True):
        self.endpoint = endpoint
        self.use_msgpack = use_msgpack and HAS_MSGPACK
        self.frame_id = 0

        self.ctx = zmq.Context()
        self.socket = self.ctx.socket(zmq.PUB)
        self.socket.bind(endpoint)

        # Allow subscribers time to connect
        time.sleep(0.3)
        print(f"[Producer] Publishing on {endpoint}"
              f" (format: {'msgpack' if self.use_msgpack else 'json'})")

    def publish_frame(self, positions: np.ndarray, symbols: List[str],
                      bonds: Optional[List[List[int]]] = None,
                      radii: Optional[List[float]] = None,
                      colors: Optional[List[List[float]]] = None,
                      labels: Optional[List[str]] = None,
                      box: Optional[List[float]] = None,
                      energy: float = 0.0,
                      title: str = ""):
        """Publish a single frame."""
        self.frame_id += 1

        if radii is None:
            radii = [_kernel_vdw_radius(s) for s in symbols]
        if colors is None:
            colors = [list(CPK_COLORS.get(s, CPK_COLORS["?"])) for s in symbols]
        if bonds is None:
            bonds = infer_bonds(np.array(positions), symbols)
        if labels is None:
            labels = symbols
        if box is None:
            box = [0.0, 0.0, 0.0]

        packet = {
            "frame_id": self.frame_id,
            "time": time.time(),
            "positions": np.asarray(positions, dtype=np.float64).tolist(),
            "symbols": symbols,
            "radii": radii,
            "colors": colors,
            "bonds": bonds,
            "labels": labels,
            "box": box,
            "energy": energy,
            "title": title,
        }

        if self.use_msgpack:
            data = msgpack.packb(packet, use_bin_type=True)
            self.socket.send(b"FRAME" + data)
        else:
            data = json.dumps(packet).encode('utf-8')
            self.socket.send(b"FRAME" + data)

    def publish_xyz_file(self, path: str):
        """Parse and publish a .xyz file."""
        frame = parse_xyz_file(path)
        if frame is None:
            print(f"[Producer] Failed to parse: {path}")
            return False
        self.publish_frame(
            positions=frame["positions"],
            symbols=frame["symbols"],
            bonds=frame["bonds"],
            radii=frame["radii"],
            colors=frame["colors"],
            labels=frame["labels"],
            box=frame["box"],
            energy=frame["energy"],
            title=frame["title"],
        )
        return True

    def close(self):
        self.socket.close()
        self.ctx.term()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


# ============================================================================
# File Watcher
# ============================================================================

def file_content_hash(path: str) -> Optional[str]:
    """Return MD5 of file content, or None if unreadable."""
    try:
        with open(path, 'rb') as f:
            return hashlib.md5(f.read()).hexdigest()
    except (IOError, OSError):
        return None


def watch_and_publish(pub: MolecularPublisher, path: str,
                      poll_interval: float = 0.1):
    """Watch a .xyz file for changes and publish on update."""
    print(f"[Producer] Watching: {path}  (poll={poll_interval}s)")
    last_hash = None

    while True:
        current_hash = file_content_hash(path)
        if current_hash is not None and current_hash != last_hash:
            last_hash = current_hash
            if pub.publish_xyz_file(path):
                print(f"[Producer] Frame {pub.frame_id} published"
                      f" ({os.path.basename(path)})")
        time.sleep(poll_interval)


# ============================================================================
# Demo Mode — Synthetic Rotating Molecule
# ============================================================================

def run_demo(pub: MolecularPublisher, fps: float = 10.0):
    """Publish a rotating methane molecule at given FPS."""
    print(f"[Producer] Demo mode: CH4 rotation at {fps} FPS")

    # Methane geometry (tetrahedral)
    symbols = ["C", "H", "H", "H", "H"]
    base_pos = np.array([
        [0.000, 0.000, 0.000],  # C
        [0.629, 0.629, 0.629],  # H
        [-0.629, -0.629, 0.629],
        [-0.629, 0.629, -0.629],
        [0.629, -0.629, -0.629],
    ])
    bonds = [[0, 1], [0, 2], [0, 3], [0, 4]]

    frame = 0
    dt = 1.0 / fps

    while True:
        frame += 1
        theta = frame * 0.05  # radians per frame

        # Rotate around Y axis
        cos_t, sin_t = np.cos(theta), np.sin(theta)
        R = np.array([
            [cos_t, 0, sin_t],
            [0, 1, 0],
            [-sin_t, 0, cos_t],
        ])
        pos = base_pos @ R.T

        pub.publish_frame(
            positions=pos,
            symbols=symbols,
            bonds=bonds,
            energy=-40.0 + 0.1 * np.sin(theta),
            title=f"CH4 demo frame {frame}",
        )
        time.sleep(dt)


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="ZeroMQ Molecular Frame Publisher for VSEPR-SIM",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python -m pykernel.zmq_producer --demo
  python -m pykernel.zmq_producer --file mol.xyz
  python -m pykernel.zmq_producer --watch trajectory.xyz --poll 0.05
  python -m pykernel.zmq_producer --watch mol.xyz --endpoint tcp://127.0.0.1:6000
        """)

    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--demo", action="store_true",
                      help="Publish synthetic rotating CH4 at 10 FPS")
    mode.add_argument("--file", type=str,
                      help="Publish single .xyz file and exit")
    mode.add_argument("--watch", type=str,
                      help="Watch .xyz file for changes and stream updates")

    parser.add_argument("--endpoint", default="tcp://127.0.0.1:5555",
                        help="ZeroMQ PUB endpoint (default: tcp://127.0.0.1:5555)")
    parser.add_argument("--poll", type=float, default=0.1,
                        help="File poll interval in seconds (default: 0.1)")
    parser.add_argument("--fps", type=float, default=10.0,
                        help="Demo mode FPS (default: 10)")
    parser.add_argument("--json", action="store_true",
                        help="Use JSON instead of MessagePack")

    args = parser.parse_args()

    use_msgpack = not args.json

    with MolecularPublisher(endpoint=args.endpoint,
                            use_msgpack=use_msgpack) as pub:
        if args.demo:
            run_demo(pub, fps=args.fps)
        elif args.file:
            if pub.publish_xyz_file(args.file):
                print(f"[Producer] Published {args.file}")
                time.sleep(1.0)  # Keep alive for subscriber
            else:
                print(f"[Producer] Failed: {args.file}")
                sys.exit(1)
        elif args.watch:
            watch_and_publish(pub, args.watch, poll_interval=args.poll)


if __name__ == "__main__":
    main()
