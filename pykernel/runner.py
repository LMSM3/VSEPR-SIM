"""
runner.py — Continuous execution loop for VSEPR-SIM via PyKernel.

Runs simulations in a loop, collecting polynomial fits (11-15 layers)
and eigenvalue snapshots on every run. Outputs are written to disk
after each run so data is useful even if the process is interrupted.

This is the "walk-away" component: start it, leave, come back to
accumulated data.

Usage:
    python -m pykernel.runner --spec "NaCl@crystal" --runs 0
    (runs=0 means infinite)

Or from Python:
    runner = ContinuousRunner()
    runner.run_forever(specs=["NaCl@crystal", "Fe@crystal"])
"""

import os
import sys
import json
import time
import signal
import hashlib
import argparse
from pathlib import Path
from datetime import datetime
from typing import List, Optional, Dict

import numpy as np

from pykernel.poly_fitter import PolyFitter, PolyFitSweep
from pykernel.eigen_counter import EigenCounter
from pykernel.gpu_bridge import GPUBridge


class ContinuousRunner:
    """
    Walk-away continuous simulation + analysis runner.

    Each iteration:
      1. Run a VSEPR simulation via CLI
      2. Parse output data (energy trajectory, coordinates)
      3. Fit 11-15 layer polynomials to the trajectory
      4. Compute eigenvalues (inertia tensor, covariance)
      5. Write incremental results to disk
      6. Log progress

    All data is append-only: killing the process loses nothing.
    """

    def __init__(self, output_dir: str = "out/walkaway",
                 build_dir: Optional[str] = None):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self.bridge = GPUBridge(build_dir=build_dir)
        self.fitter = PolyFitter(min_degree=11, max_degree=15)
        self.eigen_counter = EigenCounter()

        self._run_count = 0
        self._start_time = None
        self._stop_requested = False

        # Register signal handler for graceful shutdown
        signal.signal(signal.SIGINT, self._handle_signal)
        signal.signal(signal.SIGTERM, self._handle_signal)

    def _handle_signal(self, signum, frame):
        print(f"\n[PyKernel] Signal {signum} received — finishing current run and saving...")
        self._stop_requested = True

    def _generate_run_id(self) -> str:
        self._run_count += 1
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        return f"run_{self._run_count:06d}_{ts}"

    def _parse_trajectory(self, stdout: str) -> Optional[Dict]:
        """Parse energy/force trajectory from CLI output."""
        steps = []
        energies = []
        forces = []

        for line in stdout.split("\n"):
            line_lower = line.lower().strip()

            # Look for step/energy/force patterns
            if "step" in line_lower and ("energy" in line_lower or "e=" in line_lower):
                parts = line.split()
                step_val = None
                energy_val = None
                force_val = None

                for i, p in enumerate(parts):
                    try:
                        if p.lower().startswith("step") and i + 1 < len(parts):
                            step_val = int(parts[i + 1].strip(":,"))
                        elif p.lower() in ("e=", "energy=", "energy:") or p.lower().startswith("e="):
                            val_str = p.split("=")[-1] if "=" in p else parts[i + 1]
                            energy_val = float(val_str.strip(","))
                        elif "=" in p and "e" in p.lower():
                            try:
                                energy_val = float(p.split("=")[-1].strip(","))
                            except ValueError:
                                pass
                        elif p.lower().startswith("f=") or p.lower().startswith("fmax="):
                            force_val = float(p.split("=")[-1].strip(","))
                    except (ValueError, IndexError):
                        pass

                if energy_val is not None:
                    steps.append(step_val if step_val is not None else len(steps))
                    energies.append(energy_val)
                    if force_val is not None:
                        forces.append(force_val)

        if not energies:
            return None

        return {
            "steps": np.array(steps, dtype=np.float64),
            "energies": np.array(energies, dtype=np.float64),
            "forces": np.array(forces, dtype=np.float64) if forces else None,
        }

    def _parse_coordinates(self, stdout: str) -> Optional[np.ndarray]:
        """Parse final coordinates from output (XYZ-like format)."""
        coords = []
        in_xyz = False
        for line in stdout.split("\n"):
            stripped = line.strip()
            if not stripped:
                continue

            parts = stripped.split()
            if len(parts) == 4:
                try:
                    # element x y z
                    _ = parts[0]
                    x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                    coords.append([x, y, z])
                    in_xyz = True
                except ValueError:
                    if in_xyz:
                        break

        if not coords:
            return None
        return np.array(coords, dtype=np.float64)

    def run_single(self, spec: str, action: str = "relax",
                   extra_args: Optional[List[str]] = None) -> Dict:
        """Run one simulation + analysis cycle."""
        run_id = self._generate_run_id()
        result = {"run_id": run_id, "spec": spec, "action": action}

        print(f"[{run_id}] Running: vsepr {spec} {action}")

        # Run simulation
        sim_result = self.bridge.run_simulation(spec, action, extra_args)
        result["sim_success"] = sim_result["success"]
        result["returncode"] = sim_result.get("returncode", -1)

        if not sim_result["success"]:
            result["error"] = sim_result.get("stderr", "unknown")[:500]
            self._save_run_result(result)
            return result

        stdout = sim_result.get("stdout", "")

        # Parse trajectory
        traj = self._parse_trajectory(stdout)
        if traj and len(traj["energies"]) >= 12:
            # Polynomial fits (11-15 layers)
            sweep = self.fitter.fit_sweep(
                traj["steps"], traj["energies"],
                run_id=run_id, target_name=f"{spec}_energy",
            )
            best = sweep.select_best()
            if best:
                result["poly_fit"] = {
                    "best_degree": best.degree,
                    "r_squared": best.r_squared,
                    "condition_number": best.condition_number,
                    "residual_norm": best.residual_norm,
                }
                print(f"  Poly fit: degree={best.degree} R²={best.r_squared:.6f} "
                      f"cond={best.condition_number:.2e}")

            # Force trajectory poly fit if available
            if traj["forces"] is not None and len(traj["forces"]) >= 12:
                force_sweep = self.fitter.fit_sweep(
                    traj["steps"][:len(traj["forces"])], traj["forces"],
                    run_id=run_id, target_name=f"{spec}_force",
                )
                fbest = force_sweep.select_best()
                if fbest:
                    result["force_poly_fit"] = {
                        "best_degree": fbest.degree,
                        "r_squared": fbest.r_squared,
                    }
        else:
            result["poly_fit"] = None
            if traj:
                result["n_energy_points"] = len(traj["energies"])

        # Eigenvalue analysis on coordinates
        coords = self._parse_coordinates(stdout)
        if coords is not None and len(coords) >= 3:
            # Covariance matrix eigenvalues
            centered = coords - coords.mean(axis=0)
            cov = centered.T @ centered / max(len(coords) - 1, 1)
            snap = self.eigen_counter.record_from_matrix(
                cov, run_id=run_id, source="covariance",
                n_atoms=len(coords), label=spec,
            )
            result["eigen"] = {
                "source": "covariance",
                "eigenvalues": snap.eigenvalues.tolist(),
                "condition_number": snap.condition_number,
                "spectral_gap": snap.spectral_gap,
                "rank": snap.rank,
            }
            print(f"  Eigen: cond={snap.condition_number:.2e} "
                  f"gap={snap.spectral_gap:.4f} rank={snap.rank}")

            # Inertia tensor eigenvalues (uniform masses)
            masses = np.ones(len(coords))
            inertia_snap = self.eigen_counter.record_inertia(
                coords, masses, run_id=run_id, label=f"{spec}_inertia",
            )
            result["inertia_eigen"] = {
                "eigenvalues": inertia_snap.eigenvalues.tolist(),
                "condition_number": inertia_snap.condition_number,
                "spectral_gap": inertia_snap.spectral_gap,
            }
        else:
            result["eigen"] = None

        self._save_run_result(result)
        return result

    def _save_run_result(self, result: Dict):
        """Append run result to the log file."""
        log_path = self.output_dir / "run_log.jsonl"
        with open(log_path, "a") as f:
            f.write(json.dumps(result, default=str) + "\n")

    def save_accumulated(self):
        """Save all accumulated data to disk."""
        self.fitter.export_history(str(self.output_dir / "poly_fits.json"))
        self.fitter.export_csv(str(self.output_dir / "poly_fits.csv"))
        self.eigen_counter.export_summary(str(self.output_dir / "eigen_summary.json"))
        self.eigen_counter.export_csv(str(self.output_dir / "eigen_snapshots.csv"))

        # Summary report
        summary = {
            "total_runs": self._run_count,
            "start_time": self._start_time,
            "end_time": datetime.now().isoformat(),
            "bridge": self.bridge.info(),
            "eigen_summary": self.eigen_counter.summary(),
            "poly_fits_count": sum(len(s.fits) for s in self.fitter.history),
        }
        with open(self.output_dir / "session_summary.json", "w") as f:
            json.dump(summary, f, indent=2, default=str)

        print(f"\n[PyKernel] Accumulated data saved to {self.output_dir}")

    def run_loop(self, specs: List[str], max_runs: int = 0,
                 action: str = "relax", delay: float = 1.0,
                 extra_args: Optional[List[str]] = None):
        """
        Run continuous simulation loop.

        Args:
            specs: List of molecular specs to cycle through
            max_runs: Maximum runs (0 = infinite)
            action: CLI action (relax, emit, test)
            delay: Seconds between runs
            extra_args: Additional CLI arguments
        """
        self._start_time = datetime.now().isoformat()
        spec_idx = 0

        print(f"[PyKernel] Continuous runner started")
        print(f"  Backend: {self.bridge.backend}")
        print(f"  Specs: {specs}")
        print(f"  Max runs: {'infinite' if max_runs == 0 else max_runs}")
        print(f"  Output: {self.output_dir}")
        print(f"  Poly degrees: {self.fitter.min_degree}-{self.fitter.max_degree}")
        print()

        try:
            while not self._stop_requested:
                if max_runs > 0 and self._run_count >= max_runs:
                    break

                spec = specs[spec_idx % len(specs)]
                spec_idx += 1

                self.run_single(spec, action, extra_args)

                # Save after every run (walk-away safe)
                self.save_accumulated()

                if delay > 0 and not self._stop_requested:
                    time.sleep(delay)
        finally:
            self.save_accumulated()
            print(f"\n[PyKernel] Session complete: {self._run_count} runs")


def main():
    parser = argparse.ArgumentParser(
        description="PyKernel Continuous Runner — walk-away VSEPR-SIM automation"
    )
    parser.add_argument("--spec", nargs="+", default=["H2O"],
                        help="Molecular specifications to simulate")
    parser.add_argument("--runs", type=int, default=0,
                        help="Max runs (0=infinite)")
    parser.add_argument("--action", default="relax",
                        help="CLI action: relax, emit, test")
    parser.add_argument("--delay", type=float, default=1.0,
                        help="Seconds between runs")
    parser.add_argument("--output", default="out/walkaway",
                        help="Output directory")
    parser.add_argument("--build-dir", default=None,
                        help="Build directory (auto-detected if omitted)")

    args = parser.parse_args()

    runner = ContinuousRunner(
        output_dir=args.output,
        build_dir=args.build_dir,
    )
    runner.run_loop(
        specs=args.spec,
        max_runs=args.runs,
        action=args.action,
        delay=args.delay,
    )


if __name__ == "__main__":
    main()
