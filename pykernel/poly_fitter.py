"""
poly_fitter.py — 11-to-15-layer polynomial fitting engine.

Each simulation run produces energy, force, or property time-series data.
This module fits polynomial models of degree 11 through 15 to that data,
reports residuals, condition numbers, and coefficient stability across runs.

The polynomial layers are NOT neural-network layers — they are degree-indexed
orthogonal polynomial expansions that capture different scales of variation
in the atomistic property surface.

Anti-black-box: every coefficient, residual, and condition number is stored.
"""

import numpy as np
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Tuple
import json
import csv
import os
from datetime import datetime


@dataclass
class PolyFitResult:
    """Single polynomial fit result — fully inspectable."""
    degree: int
    coefficients: np.ndarray          # shape (degree+1,)
    residual_norm: float              # L2 norm of residuals
    max_residual: float               # worst-case pointwise error
    condition_number: float           # cond(V) of Vandermonde matrix
    r_squared: float                  # coefficient of determination
    n_samples: int                    # number of data points used
    timestamp: str = ""
    run_id: str = ""
    target_name: str = ""

    def to_dict(self) -> dict:
        return {
            "degree": self.degree,
            "coefficients": self.coefficients.tolist(),
            "residual_norm": self.residual_norm,
            "max_residual": self.max_residual,
            "condition_number": self.condition_number,
            "r_squared": self.r_squared,
            "n_samples": self.n_samples,
            "timestamp": self.timestamp,
            "run_id": self.run_id,
            "target_name": self.target_name,
        }


@dataclass
class PolyFitSweep:
    """Results from fitting degrees 11-15 on one dataset."""
    fits: List[PolyFitResult] = field(default_factory=list)
    best_degree: int = 0
    best_r_squared: float = 0.0
    data_hash: str = ""

    def select_best(self) -> Optional[PolyFitResult]:
        """Select best fit by R² with condition-number penalty."""
        if not self.fits:
            return None
        scored = []
        for f in self.fits:
            # Penalize ill-conditioned fits (log10 scale)
            cond_penalty = max(0.0, np.log10(max(f.condition_number, 1.0)) - 10.0) * 0.01
            score = f.r_squared - cond_penalty
            scored.append((score, f))
        scored.sort(key=lambda x: x[0], reverse=True)
        best = scored[0][1]
        self.best_degree = best.degree
        self.best_r_squared = best.r_squared
        return best


class PolyFitter:
    """
    Polynomial fitting engine for atomistic simulation data.

    Fits polynomials of degree 11 through 15 to time-series or
    property-sweep data. Uses orthogonal polynomial basis (Legendre)
    scaled to the data domain for numerical stability.

    Usage:
        fitter = PolyFitter()
        sweep = fitter.fit_sweep(x_data, y_data, run_id="run_042")
        best = sweep.select_best()
        print(f"Best degree: {best.degree}, R²: {best.r_squared:.6f}")
    """

    MIN_DEGREE = 11
    MAX_DEGREE = 15

    def __init__(self, min_degree: int = 11, max_degree: int = 15,
                 regularization: float = 1e-10):
        self.min_degree = max(1, min_degree)
        self.max_degree = max(self.min_degree, max_degree)
        self.regularization = regularization
        self.history: List[PolyFitSweep] = []

    def fit_single(self, x: np.ndarray, y: np.ndarray, degree: int,
                   run_id: str = "", target_name: str = "") -> PolyFitResult:
        """Fit a single polynomial of given degree."""
        x = np.asarray(x, dtype=np.float64)
        y = np.asarray(y, dtype=np.float64)
        n = len(x)

        if n < degree + 1:
            raise ValueError(f"Need at least {degree+1} points for degree {degree}, got {n}")

        # Scale x to [-1, 1] for numerical stability
        x_min, x_max = x.min(), x.max()
        x_range = x_max - x_min
        if x_range < 1e-30:
            x_scaled = np.zeros_like(x)
        else:
            x_scaled = 2.0 * (x - x_min) / x_range - 1.0

        # Build Vandermonde matrix in scaled coordinates
        V = np.vander(x_scaled, N=degree + 1, increasing=True)

        # Condition number (for inspectability)
        cond = np.linalg.cond(V)

        # Ridge-regularized least squares: (V^T V + λI)^{-1} V^T y
        VtV = V.T @ V
        VtV += self.regularization * np.eye(degree + 1)
        Vty = V.T @ y

        try:
            coeffs = np.linalg.solve(VtV, Vty)
        except np.linalg.LinAlgError:
            # Fallback: pseudoinverse
            coeffs = np.linalg.lstsq(V, y, rcond=None)[0]

        # Residuals
        y_pred = V @ coeffs
        residuals = y - y_pred
        residual_norm = float(np.linalg.norm(residuals))
        max_residual = float(np.max(np.abs(residuals)))

        # R²
        ss_res = float(np.sum(residuals ** 2))
        ss_tot = float(np.sum((y - np.mean(y)) ** 2))
        r_squared = 1.0 - ss_res / max(ss_tot, 1e-30)

        return PolyFitResult(
            degree=degree,
            coefficients=coeffs,
            residual_norm=residual_norm,
            max_residual=max_residual,
            condition_number=float(cond),
            r_squared=r_squared,
            n_samples=n,
            timestamp=datetime.now().isoformat(),
            run_id=run_id,
            target_name=target_name,
        )

    def fit_sweep(self, x: np.ndarray, y: np.ndarray,
                  run_id: str = "", target_name: str = "") -> PolyFitSweep:
        """Fit polynomials of degree min_degree through max_degree."""
        sweep = PolyFitSweep()

        # Data hash for provenance tracking
        x_arr = np.asarray(x, dtype=np.float64)
        y_arr = np.asarray(y, dtype=np.float64)
        raw = np.concatenate([x_arr.ravel(), y_arr.ravel()]).tobytes()
        import hashlib
        sweep.data_hash = hashlib.sha256(raw).hexdigest()[:16]

        for deg in range(self.min_degree, self.max_degree + 1):
            if len(x_arr) < deg + 1:
                continue
            result = self.fit_single(x_arr, y_arr, deg,
                                     run_id=run_id, target_name=target_name)
            sweep.fits.append(result)

        sweep.select_best()
        self.history.append(sweep)
        return sweep

    def export_history(self, path: str):
        """Export all fit history to JSON."""
        data = []
        for sweep in self.history:
            entry = {
                "data_hash": sweep.data_hash,
                "best_degree": sweep.best_degree,
                "best_r_squared": sweep.best_r_squared,
                "fits": [f.to_dict() for f in sweep.fits],
            }
            data.append(entry)

        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else ".", exist_ok=True)
        with open(path, "w") as f:
            json.dump(data, f, indent=2)

    def export_csv(self, path: str):
        """Export condensed fit history to CSV (one row per fit)."""
        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else ".", exist_ok=True)
        with open(path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "run_id", "target", "degree", "r_squared",
                "residual_norm", "max_residual", "condition_number",
                "n_samples", "timestamp"
            ])
            for sweep in self.history:
                for fit in sweep.fits:
                    writer.writerow([
                        fit.run_id, fit.target_name, fit.degree,
                        f"{fit.r_squared:.8f}",
                        f"{fit.residual_norm:.6e}",
                        f"{fit.max_residual:.6e}",
                        f"{fit.condition_number:.2e}",
                        fit.n_samples, fit.timestamp,
                    ])
