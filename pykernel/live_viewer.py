"""
live_viewer.py — Continuous xyzA writer + live viewer launcher.

This module drives the watch loop from the Python side:

    1. Writes successive .xyzA frames to a file (from simulation, benchmark,
       or synthetic trajectory) at a configurable frame rate.
    2. Optionally launches live-xyza-viewer as a subprocess so the user
       can watch the 3D window update in real time.
    3. Pipes every frame's metadata (step, energy, n_atoms, timing) through
       the pykernel Pipe infrastructure for live analysis:
           frame_pipe  → CSVSink, JSONSink
           energy_pipe → PolyFitter (live scaling curve)
           timing_pipe → EigenCounter

Architecture:
    SimulationDriver (or SyntheticDriver)
        ↓  xyzA frames
    XYZAFrameWriter  ─── writes ──→  trajectory.xyzA
        ↓  metadata dict            (live-xyza-viewer watches this file)
    Pipe[dict] "frame_pipe"
        ├── Transform → Pipe[float] "energy_pipe"   → PolyFitter
        ├── Transform → Pipe[float] "timing_pipe"   → EigenCounter
        ├── CSVSink "out/live/frames.csv"
        └── JSONSink "out/live/frames.jsonl"

Usage:
    # Headless (write frames only):
    runner = LiveViewer()
    runner.run(steps=500, fps=10)

    # With 3D window:
    runner = LiveViewer(launch_viewer=True)
    runner.run(steps=500, fps=10)

    # Feed from external generator:
    runner = LiveViewer()
    for frame in my_simulation():
        runner.push_frame(frame)

Anti-black-box: every frame's energy, forces, and timing are logged.
"""

import os
import math
import time
import subprocess
import threading
import shutil
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Dict, Iterator, Callable

import numpy as np

from pykernel.pipe import Pipe, Transform, FanOut, CSVSink, JSONSink, Accumulator
from pykernel.poly_fitter import PolyFitter
from pykernel.eigen_counter import EigenCounter
from pykernel.gpu_bridge import GPUBridge


# ============================================================================
# Frame data
# ============================================================================

@dataclass
class XYZAFrame:
    """One frame of atomistic trajectory data."""
    step:       int
    n_atoms:    int
    elements:   List[str]                    # e.g. ["C", "H", "O", ...]
    positions:  np.ndarray                   # (N, 3) Angstrom
    charges:    Optional[np.ndarray] = None  # (N,) e
    velocities: Optional[np.ndarray] = None  # (N, 3) Å/fs
    forces:     Optional[np.ndarray] = None  # (N, 3) eV/Å
    energies:   Optional[np.ndarray] = None  # (N,) eV per atom
    total_energy: float = 0.0               # eV
    timestamp:  float   = field(default_factory=time.time)

    @property
    def max_force(self) -> float:
        if self.forces is None:
            return 0.0
        return float(np.max(np.linalg.norm(self.forces, axis=1)))

    @property
    def avg_charge(self) -> float:
        if self.charges is None:
            return 0.0
        return float(np.mean(self.charges))


# ============================================================================
# xyzA writer
# ============================================================================

class XYZAFrameWriter:
    """
    Writes XYZAFrame objects to a .xyzA file (append or overwrite mode).

    The viewer's file watcher picks up mtime changes and reloads.
    We overwrite the same file each frame so the viewer always sees
    the latest snapshot (not a growing trajectory unless keep_history=True).
    """

    def __init__(self, path: str, keep_history: bool = False,
                 precision: int = 6):
        self._path        = Path(path)
        self._keep_history = keep_history
        self._precision   = precision
        self._path.parent.mkdir(parents=True, exist_ok=True)

    def write(self, frame: XYZAFrame):
        """Write frame to disk (overwrites by default)."""
        mode = "a" if self._keep_history else "w"
        with open(self._path, mode) as f:
            self._write_frame(f, frame)

    def _write_frame(self, f, frame: XYZAFrame):
        n = frame.n_atoms
        f.write(f"{n}\n")

        # Comment line with metadata
        comment = (f"step={frame.step} energy={frame.total_energy:.6e} "
                   f"n_atoms={n} ts={frame.timestamp:.3f}")
        f.write(comment + "\n")

        prec = self._precision
        for i in range(n):
            px, py, pz = frame.positions[i]
            parts = [frame.elements[i],
                     f"{px:.{prec}f}", f"{py:.{prec}f}", f"{pz:.{prec}f}"]

            if frame.charges is not None:
                parts.append(f"{frame.charges[i]:.{prec}f}")
            else:
                parts.append("0.000000")

            if frame.velocities is not None:
                vx, vy, vz = frame.velocities[i]
                parts += [f"{vx:.{prec}f}", f"{vy:.{prec}f}", f"{vz:.{prec}f}"]
            else:
                parts += ["0.000000", "0.000000", "0.000000"]

            if frame.forces is not None:
                fx, fy, fz = frame.forces[i]
                parts += [f"{fx:.{prec}f}", f"{fy:.{prec}f}", f"{fz:.{prec}f}"]
            else:
                parts += ["0.000000", "0.000000", "0.000000"]

            if frame.energies is not None:
                parts.append(f"{frame.energies[i]:.{prec}f}")
            else:
                parts.append("0.000000")

            f.write(" ".join(parts) + "\n")


# ============================================================================
# Synthetic trajectory driver
# ============================================================================

class SyntheticDriver:
    """
    Generates a synthetic trajectory for testing and demonstration.

    Simulates an N-atom cluster with simple Lennard-Jones dynamics:
    random positions that slowly relax, energy decreasing over steps.

    No actual MD integration — purely for driving the viewer pipeline.
    """

    ELEMENTS = ["C", "H", "N", "O", "C", "C", "H", "H"]  # weighted toward C/H

    def __init__(self, n_atoms: int = 32, seed: int = 42):
        self._n      = n_atoms
        self._rng    = np.random.RandomState(seed)
        self._step   = 0

        # Initial positions: random sphere
        r = 3.0 * (n_atoms ** (1/3))
        self._pos    = self._rng.uniform(-r, r, (n_atoms, 3)).astype(np.float64)
        self._vel    = self._rng.normal(0, 0.1, (n_atoms, 3)).astype(np.float64)
        self._elems  = [self.ELEMENTS[i % len(self.ELEMENTS)]
                        for i in range(n_atoms)]
        self._charges = self._rng.normal(0, 0.05, n_atoms)

    def step(self) -> XYZAFrame:
        """Advance one step and return the new frame."""
        n = self._n
        dt = 0.02  # femtoseconds (synthetic)

        # Simple pair force: LJ-like repulsion + spring to origin
        forces = np.zeros((n, 3))
        energy = 0.0

        for i in range(n):
            for j in range(i + 1, n):
                r_vec = self._pos[i] - self._pos[j]
                r     = np.linalg.norm(r_vec) + 1e-6
                # Soft repulsion (prevents collapse)
                sig = 1.5
                sr6 = (sig / r) ** 6
                e_ij = 4 * 0.02 * (sr6 * sr6 - sr6)
                f_mag = 4 * 0.02 * (12 * sr6 ** 2 - 6 * sr6) / r
                f_vec = f_mag * r_vec / r
                forces[i] += f_vec
                forces[j] -= f_vec
                energy += e_ij

        # Weak spring to centre (keeps cluster together)
        for i in range(n):
            forces[i] -= 0.005 * self._pos[i]
            energy += 0.005 * 0.5 * np.dot(self._pos[i], self._pos[i])

        # Verlet-ish velocity update with damping
        self._vel  = 0.95 * self._vel + dt * forces
        self._pos += dt * self._vel

        # Per-atom energies (rough)
        atom_energies = 0.01 * np.linalg.norm(forces, axis=1)

        self._step += 1
        return XYZAFrame(
            step=self._step,
            n_atoms=n,
            elements=list(self._elems),
            positions=self._pos.copy(),
            charges=self._charges + self._rng.normal(0, 0.001, n),
            velocities=self._vel.copy(),
            forces=forces.copy(),
            energies=atom_energies,
            total_energy=energy,
            timestamp=time.time(),
        )

    def stream(self, n_steps: int) -> Iterator[XYZAFrame]:
        for _ in range(n_steps):
            yield self.step()


# ============================================================================
# Pipe-wired analysis
# ============================================================================

class LiveAnalysis:
    """
    Wires frame metadata through Pipe infrastructure for live analysis.

    Pipes:
        frame_pipe   [dict]   — full frame metadata
        energy_pipe  [float]  — total energy per step
        timing_pipe  [float]  — ms per frame
        force_pipe   [float]  — max force per step
    """

    def __init__(self, output_dir: str = "out/live"):
        outdir = Path(output_dir)
        outdir.mkdir(parents=True, exist_ok=True)

        self.frame_pipe  = Pipe[dict]("live_frames")
        self.energy_pipe = Pipe[float]("live_energy")
        self.timing_pipe = Pipe[float]("live_timing")
        self.force_pipe  = Pipe[float]("live_force")

        # Transforms: extract scalars from frame dict
        self._e_xform = Transform(
            self.frame_pipe, self.energy_pipe,
            lambda d: float(d.get("total_energy", 0)),
            name="frame→energy",
        )
        self._t_xform = Transform(
            self.frame_pipe, self.timing_pipe,
            lambda d: float(d.get("frame_ms", 0)),
            name="frame→timing",
        )
        self._f_xform = Transform(
            self.frame_pipe, self.force_pipe,
            lambda d: float(d.get("max_force", 0)),
            name="frame→force",
        )

        # Sinks
        self._csv = CSVSink(
            self.frame_pipe,
            str(outdir / "frames.csv"),
            ["step", "n_atoms", "total_energy", "max_force", "avg_charge", "frame_ms"],
        )
        self._json = JSONSink(
            self.frame_pipe,
            str(outdir / "frames.jsonl"),
        )

        # Analysis engines
        self.fitter  = PolyFitter()
        self.eigen   = EigenCounter()

        # Accumulator: run poly fit every 16 frames
        self._acc = Accumulator(
            self.energy_pipe,
            batch_size=16,
            on_flush=self._fit_energy_batch,
        )
        self._step_buf: List[float] = []

    def _fit_energy_batch(self, energies: List[float]):
        """Fit polynomial to energy buffer."""
        n = len(energies)
        if n < 4:
            return
        x = np.arange(n, dtype=float)
        y = np.array(energies, dtype=float)
        try:
            result = self.fitter.fit_single(x, y, degree=min(4, n-1),
                                            run_id="live_energy",
                                            target_name="energy_vs_step")
            # Eigenvalue analysis on energy covariance
            if n >= 4:
                data = np.column_stack([x / max(x.max(), 1), y / max(abs(y.max()), 1e-12)])
                cov  = np.cov(data.T)
                evs  = np.linalg.eigvalsh(cov)[::-1]
                self.eigen.record(evs, run_id=f"live_e_{n}",
                                  source="energy_covariance", n_atoms=n,
                                  label="live_energy")
        except Exception:
            pass

    def push(self, frame: XYZAFrame, frame_ms: float):
        """Push one frame's data into the pipe."""
        self.frame_pipe.push({
            "step":         frame.step,
            "n_atoms":      frame.n_atoms,
            "total_energy": frame.total_energy,
            "max_force":    frame.max_force,
            "avg_charge":   frame.avg_charge,
            "frame_ms":     frame_ms,
        }, source="live_viewer")

    @property
    def stats(self) -> Dict:
        return {
            "frame":  self.frame_pipe.stats(),
            "energy": self.energy_pipe.stats(),
            "timing": self.timing_pipe.stats(),
            "force":  self.force_pipe.stats(),
        }


# ============================================================================
# Live Viewer
# ============================================================================

class LiveViewer:
    """
    Continuous xyzA writer with optional 3D viewer subprocess.

    Usage:
        viewer = LiveViewer(n_atoms=64, launch_viewer=True)
        viewer.run(steps=1000, fps=10)

    Or headless (no window):
        viewer = LiveViewer(n_atoms=64)
        viewer.run(steps=1000, fps=10)
        viewer.report()
    """

    def __init__(self,
                 n_atoms:       int  = 32,
                 output_dir:    str  = "out/live",
                 xyza_path:     Optional[str] = None,
                 launch_viewer: bool = False,
                 keep_history:  bool = False,
                 seed:          int  = 42):

        self._root       = Path(__file__).parent.parent.absolute()
        self._output_dir = Path(output_dir)
        self._output_dir.mkdir(parents=True, exist_ok=True)

        self._xyza_path = Path(xyza_path) if xyza_path \
            else self._output_dir / "live_trajectory.xyzA"

        self._writer   = XYZAFrameWriter(str(self._xyza_path),
                                         keep_history=keep_history)
        self._driver   = SyntheticDriver(n_atoms=n_atoms, seed=seed)
        self._analysis = LiveAnalysis(output_dir=str(self._output_dir))
        self._bridge   = GPUBridge()

        self._launch_viewer = launch_viewer
        self._viewer_proc: Optional[subprocess.Popen] = None
        self._running = False

        # Stats
        self._frame_count = 0
        self._start_time  = 0.0

    def _find_viewer(self) -> Optional[Path]:
        """Find live-xyza-viewer binary."""
        build = self._bridge._build_dir
        for ext in [".exe", ""]:
            name = f"live-xyza-viewer{ext}"
            for root, dirs, files in os.walk(build):
                if name in files:
                    return Path(root) / name
        return None

    def _launch_viewer_window(self, poll_ms: int = 100):
        """Launch the C++ viewer as a subprocess."""
        exe = self._find_viewer()
        if not exe:
            print("[LiveViewer] live-xyza-viewer not found in build tree. "
                  "Run: cmake --build build --target live-xyza-viewer")
            return

        cmd = [str(exe), str(self._xyza_path),
               "--poll", str(poll_ms), "--pipe"]
        print(f"[LiveViewer] Launching viewer: {' '.join(cmd)}")
        self._viewer_proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        # Background thread reads viewer's pipe CSV output
        threading.Thread(target=self._consume_viewer_pipe,
                         daemon=True).start()

    def _consume_viewer_pipe(self):
        """Read CSV rows from the viewer's stdout pipe."""
        if not self._viewer_proc or not self._viewer_proc.stdout:
            return
        header_skipped = False
        for line in self._viewer_proc.stdout:
            line = line.strip()
            if not header_skipped:
                header_skipped = True
                continue
            parts = line.split(",")
            if len(parts) >= 5:
                try:
                    step     = int(parts[0])
                    n_atoms  = int(parts[1])
                    energy   = float(parts[2])
                    max_force = float(parts[3])
                    # Push into analysis pipe from viewer's perspective
                except (ValueError, IndexError):
                    pass

    def run(self, steps: int = 500, fps: float = 10.0,
            driver: Optional[Callable[[], Iterator[XYZAFrame]]] = None):
        """
        Run the live viewer loop.

        Args:
            steps:  Number of steps to simulate.
            fps:    Target frame rate (frames per second written to disk).
            driver: Optional custom frame generator. If None, uses SyntheticDriver.
        """
        frame_interval = 1.0 / max(fps, 0.1)
        self._start_time = time.time()
        self._running = True

        # Write initial placeholder so viewer can open the file
        init_frame = self._driver.step()
        self._writer.write(init_frame)

        # Launch viewer window if requested
        if self._launch_viewer:
            self._launch_viewer_window(poll_ms=max(50, int(frame_interval * 1000 * 0.8)))

        print(f"[LiveViewer] Starting: {steps} steps @ {fps:.1f} fps")
        print(f"[LiveViewer] xyzA file: {self._xyza_path}")
        print(f"[LiveViewer] Output dir: {self._output_dir}")
        if not self._launch_viewer:
            print(f"[LiveViewer] Launch viewer manually:")
            print(f"    live-xyza-viewer {self._xyza_path} --poll 100")
        print()

        frame_gen = driver() if driver else self._driver.stream(steps)

        for frame in frame_gen:
            t0 = time.time()

            # Write xyzA (watcher picks this up)
            self._writer.write(frame)

            # Push through analysis pipes
            frame_ms = (time.time() - t0) * 1000
            self._analysis.push(frame, frame_ms)

            self._frame_count += 1

            # Progress print every 50 steps
            if self._frame_count % 50 == 0:
                elapsed = time.time() - self._start_time
                actual_fps = self._frame_count / max(elapsed, 1e-9)
                print(f"  step={frame.step:6d}  "
                      f"E={frame.total_energy:+.4e} eV  "
                      f"Fmax={frame.max_force:.3f} eV/Å  "
                      f"fps={actual_fps:.1f}")

            # Rate limiting
            elapsed_loop = time.time() - t0
            sleep_t = frame_interval - elapsed_loop
            if sleep_t > 0:
                time.sleep(sleep_t)

        self._running = False
        self._stop()

    def push_frame(self, frame: XYZAFrame):
        """
        Push a single externally-generated frame into the pipeline.
        Useful when driving from a real MD integrator.
        """
        t0 = time.time()
        self._writer.write(frame)
        frame_ms = (time.time() - t0) * 1000
        self._analysis.push(frame, frame_ms)
        self._frame_count += 1

    def _stop(self):
        """Gracefully stop the viewer subprocess."""
        if self._viewer_proc and self._viewer_proc.poll() is None:
            # Don't kill the viewer — let the user close it manually
            print("[LiveViewer] Simulation complete. Close the viewer window to exit.")

    def report(self):
        """Print a summary of the live session."""
        elapsed = time.time() - self._start_time
        actual_fps = self._frame_count / max(elapsed, 1e-9)

        print(f"\n{'='*60}")
        print(f"  Live Viewer Session Report")
        print(f"{'='*60}")
        print(f"  Frames written : {self._frame_count}")
        print(f"  Elapsed        : {elapsed:.1f} s")
        print(f"  Actual fps     : {actual_fps:.2f}")
        print(f"  xyzA file      : {self._xyza_path}")
        print(f"  CSV frames     : {self._output_dir / 'frames.csv'}")
        print(f"  JSONL frames   : {self._output_dir / 'frames.jsonl'}")

        pipe_s = self._analysis.stats
        print(f"\n  Pipe stats:")
        for name, s in pipe_s.items():
            print(f"    {name:10s}: {s['total_pushed']} records")

        print(f"{'='*60}")

    @property
    def output_dir(self) -> Path:
        return self._output_dir

    @property
    def xyza_path(self) -> Path:
        return self._xyza_path

    @property
    def analysis(self) -> LiveAnalysis:
        return self._analysis
