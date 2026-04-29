"""
eigen_counter.py — Eigenvalue accumulator for each simulation run.

Tracks eigenvalues from inertia tensors, covariance matrices, and
Hessian approximations across simulation steps. Provides:
  - Per-run eigenvalue spectra
  - Spectral gap tracking (stability indicator)
  - Condition number evolution
  - Cumulative eigenvalue statistics

Each run appends an EigenSnapshot. Over many runs the eigenvalue
distribution reveals structural stability, phase transitions, and
degenerate modes.

Anti-black-box: raw eigenvalues stored, all derived metrics recomputable.
"""

import numpy as np
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Tuple
import json
import csv
import os
from datetime import datetime


@dataclass
class EigenSnapshot:
    """Eigenvalue snapshot from one computation."""
    run_id: str
    source: str                       # "inertia", "covariance", "hessian", "property"
    eigenvalues: np.ndarray           # sorted descending
    timestamp: str = ""
    n_atoms: int = 0
    label: str = ""

    # Derived (computed on demand)
    @property
    def condition_number(self) -> float:
        ev = np.abs(self.eigenvalues)
        if len(ev) == 0 or ev[-1] < 1e-30:
            return float("inf")
        return float(ev[0] / ev[-1])

    @property
    def spectral_gap(self) -> float:
        """Gap between largest and second-largest eigenvalue."""
        if len(self.eigenvalues) < 2:
            return 0.0
        ev = np.sort(np.abs(self.eigenvalues))[::-1]
        return float(ev[0] - ev[1])

    @property
    def trace(self) -> float:
        return float(np.sum(self.eigenvalues))

    @property
    def rank(self) -> int:
        """Numerical rank (eigenvalues > epsilon)."""
        return int(np.sum(np.abs(self.eigenvalues) > 1e-12))

    def to_dict(self) -> dict:
        return {
            "run_id": self.run_id,
            "source": self.source,
            "eigenvalues": self.eigenvalues.tolist(),
            "timestamp": self.timestamp,
            "n_atoms": self.n_atoms,
            "label": self.label,
            "condition_number": self.condition_number,
            "spectral_gap": self.spectral_gap,
            "trace": self.trace,
            "rank": self.rank,
        }


class EigenCounter:
    """
    Accumulator for eigenvalue spectra across simulation runs.

    Collects EigenSnapshots from inertia tensors (3x3), covariance
    matrices (NxN), or Hessian approximations. Provides summary
    statistics for monitoring structural stability over time.

    Usage:
        counter = EigenCounter()

        # After each simulation run:
        matrix = compute_inertia_tensor(atoms)
        eigenvalues = np.linalg.eigvalsh(matrix)
        counter.record(eigenvalues, run_id="run_042", source="inertia")

        # After many runs:
        counter.export_summary("out/eigen_summary.json")
    """

    def __init__(self):
        self.snapshots: List[EigenSnapshot] = []
        self._run_counter = 0

    def record(self, eigenvalues: np.ndarray, run_id: str = "",
               source: str = "unknown", n_atoms: int = 0,
               label: str = "") -> EigenSnapshot:
        """Record an eigenvalue snapshot."""
        ev = np.sort(np.asarray(eigenvalues, dtype=np.float64).ravel())[::-1]

        if not run_id:
            self._run_counter += 1
            run_id = f"auto_{self._run_counter:06d}"

        snap = EigenSnapshot(
            run_id=run_id,
            source=source,
            eigenvalues=ev,
            timestamp=datetime.now().isoformat(),
            n_atoms=n_atoms,
            label=label,
        )
        self.snapshots.append(snap)
        return snap

    def record_from_matrix(self, matrix: np.ndarray, **kwargs) -> EigenSnapshot:
        """Compute eigenvalues of a symmetric matrix and record them."""
        M = np.asarray(matrix, dtype=np.float64)
        if M.ndim != 2 or M.shape[0] != M.shape[1]:
            raise ValueError(f"Expected square matrix, got shape {M.shape}")
        # Symmetrize
        M = 0.5 * (M + M.T)
        eigenvalues = np.linalg.eigvalsh(M)
        return self.record(eigenvalues, **kwargs)

    def record_inertia(self, positions: np.ndarray, masses: np.ndarray,
                       run_id: str = "", label: str = "") -> EigenSnapshot:
        """Compute and record eigenvalues of the inertia tensor."""
        pos = np.asarray(positions, dtype=np.float64)
        m = np.asarray(masses, dtype=np.float64).ravel()
        n = len(m)

        if pos.shape != (n, 3):
            raise ValueError(f"Positions shape {pos.shape} vs {n} masses")

        # Center of mass
        total_mass = np.sum(m)
        if total_mass < 1e-30:
            raise ValueError("Total mass is zero")
        com = np.sum(pos * m[:, np.newaxis], axis=0) / total_mass

        # Relative positions
        r = pos - com

        # Inertia tensor: I_ij = sum_k m_k (|r_k|^2 delta_ij - r_k,i * r_k,j)
        I = np.zeros((3, 3))
        for k in range(n):
            r2 = np.dot(r[k], r[k])
            for i in range(3):
                for j in range(3):
                    I[i, j] += m[k] * (r2 * (1 if i == j else 0) - r[k, i] * r[k, j])

        return self.record_from_matrix(
            I, run_id=run_id, source="inertia", n_atoms=n, label=label
        )

    def summary(self) -> Dict:
        """Compute summary statistics across all snapshots."""
        if not self.snapshots:
            return {"n_snapshots": 0}

        all_conds = [s.condition_number for s in self.snapshots
                     if np.isfinite(s.condition_number)]
        all_gaps = [s.spectral_gap for s in self.snapshots]
        all_traces = [s.trace for s in self.snapshots]
        all_ranks = [s.rank for s in self.snapshots]

        sources = {}
        for s in self.snapshots:
            sources[s.source] = sources.get(s.source, 0) + 1

        return {
            "n_snapshots": len(self.snapshots),
            "sources": sources,
            "condition_number": {
                "mean": float(np.mean(all_conds)) if all_conds else 0.0,
                "std": float(np.std(all_conds)) if all_conds else 0.0,
                "min": float(np.min(all_conds)) if all_conds else 0.0,
                "max": float(np.max(all_conds)) if all_conds else 0.0,
            },
            "spectral_gap": {
                "mean": float(np.mean(all_gaps)),
                "std": float(np.std(all_gaps)),
            },
            "trace": {
                "mean": float(np.mean(all_traces)),
                "std": float(np.std(all_traces)),
            },
            "rank": {
                "mean": float(np.mean(all_ranks)),
                "min": int(np.min(all_ranks)),
                "max": int(np.max(all_ranks)),
            },
        }

    def export_summary(self, path: str):
        """Export summary and all snapshots to JSON."""
        data = {
            "summary": self.summary(),
            "snapshots": [s.to_dict() for s in self.snapshots],
        }
        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else ".", exist_ok=True)
        with open(path, "w") as f:
            json.dump(data, f, indent=2)

    def export_csv(self, path: str):
        """Export one row per snapshot to CSV."""
        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else ".", exist_ok=True)
        with open(path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "run_id", "source", "n_atoms", "rank", "trace",
                "condition_number", "spectral_gap", "n_eigenvalues",
                "largest_ev", "smallest_ev", "label", "timestamp",
            ])
            for s in self.snapshots:
                ev = s.eigenvalues
                writer.writerow([
                    s.run_id, s.source, s.n_atoms, s.rank,
                    f"{s.trace:.6e}", f"{s.condition_number:.6e}",
                    f"{s.spectral_gap:.6e}", len(ev),
                    f"{ev[0]:.6e}" if len(ev) > 0 else "0",
                    f"{ev[-1]:.6e}" if len(ev) > 0 else "0",
                    s.label, s.timestamp,
                ])
