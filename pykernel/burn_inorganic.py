"""
burn_inorganic — Combustion & Flash-Point Engine for Inorganic Materials
========================================================================

Report #071: Burning of Inorganic Materials and Flash Pointing Metals

VSEPR-SIM 4.0.4 — trades time for accuracy.

Features:
  1. Metal combustion / flash-point database (from metallic_cp)
  2. Thermal diffusion matrix builder  (N×N, 4→64)
  3. Jacobi iterative solver with per-iteration RMS tracking
  4. QR eigen decomposition (pure-Python Householder + shifted QR)
  5. Equation logger — captures every equation line for live pass-in
  6. Resolution cascade: 4×4 → 8×8 → 16×16 → 32×32 → 64×64

All pure Python — no NumPy.
"""

from __future__ import annotations

import math
import time
import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple

# ═══════════════════════════════════════════════════════════════════════
# Constants
# ═══════════════════════════════════════════════════════════════════════

R_GAS      = 8.314462618          # J/(mol·K)
SIGMA_SB   = 5.670374419e-8      # W/(m²·K⁴)  Stefan–Boltzmann
MISSING    = -9999.0

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "out" / "reports" / "report_071_burn"
OUT_DIR.mkdir(parents=True, exist_ok=True)
LOG_FILE = OUT_DIR / "burn_log.jsonl"
EQ_LOG   = OUT_DIR / "equations.log"


# ═══════════════════════════════════════════════════════════════════════
# Metal combustion database
# ═══════════════════════════════════════════════════════════════════════

@dataclass(frozen=True)
class BurnMetal:
    """Combustion-relevant metal record."""
    symbol: str
    name: str
    Z: int
    molar_mass: float       # g/mol
    density: float          # g/cm³
    melting_point: float    # K
    flash_point: float      # K  (ignition / flash in air)
    oxide_formula: str
    delta_H_comb: float     # kJ/mol  (enthalpy of combustion, exo = positive)
    thermal_cond: float     # W/(m·K)
    cp_298: float           # J/(mol·K) at 298 K
    diffusivity: float      # m²/s  thermal diffusivity (α = k/(ρ·cp))

# Sourced from CRC Handbook + Brandes & Brook (Smithells Metals Ref Book)
BURN_DB: list[BurnMetal] = [
    BurnMetal("Al",  "Aluminium",   13, 26.982, 2.70, 933,   913,  "Al2O3",  1676.0, 237.0, 24.20, 9.7e-5),
    BurnMetal("Mg",  "Magnesium",   12, 24.305, 1.74, 923,   746,  "MgO",     601.6, 156.0, 24.87, 8.7e-5),
    BurnMetal("Fe",  "Iron",        26, 55.845, 7.87, 1811, 1588,  "Fe2O3",   824.2,  80.4, 25.10, 2.3e-5),
    BurnMetal("Ti",  "Titanium",    22, 47.867, 4.51, 1941, 1473,  "TiO2",    944.0,  21.9, 25.06, 9.2e-6),
    BurnMetal("Zr",  "Zirconium",   40, 91.224, 6.52, 2128, 1733,  "ZrO2",   1100.6,  22.7, 25.36, 1.2e-5),
    BurnMetal("W",   "Tungsten",    74,183.840,19.25, 3695, 3273,  "WO3",     842.9, 173.0, 24.27, 6.8e-5),
    BurnMetal("Na",  "Sodium",      11, 22.990, 0.97, 371,   388,  "Na2O",    414.2, 142.0, 28.23, 7.1e-5),
    BurnMetal("K",   "Potassium",   19, 39.098, 0.86, 336,   344,  "K2O",     363.2, 102.5, 29.60, 1.6e-4),
    BurnMetal("Ca",  "Calcium",     20, 40.078, 1.55, 1115,  1063, "CaO",     635.1, 201.0, 25.93, 2.3e-4),
    BurnMetal("Li",  "Lithium",      3,  6.941, 0.53, 454,   453,  "Li2O",    598.7,  84.8, 24.86, 4.6e-5),
    BurnMetal("Cr",  "Chromium",    24, 51.996, 7.15, 2180, 2073,  "Cr2O3",  1139.7,  93.9, 23.35, 2.9e-5),
    BurnMetal("Mn",  "Manganese",   25, 54.938, 7.44, 1519, 1373,  "MnO2",    520.0,   7.8, 26.32, 8.0e-6),
    BurnMetal("Zn",  "Zinc",        30, 65.380, 7.13, 693,   733,  "ZnO",     350.5, 116.0, 25.39, 4.2e-5),
    BurnMetal("Sn",  "Tin",         50,118.710, 7.29, 505,   503,  "SnO2",    580.7,  66.8, 27.11, 4.0e-5),
    BurnMetal("Cu",  "Copper",      29, 63.546, 8.96, 1358, 1573,  "CuO",     157.3, 401.0, 24.44, 1.2e-4),
]


# ═══════════════════════════════════════════════════════════════════════
# Equation logger
# ═══════════════════════════════════════════════════════════════════════

class EquationLog:
    """Captures every equation line for display and live streaming."""

    def __init__(self):
        self.lines: list[str] = []
        self._fh = open(EQ_LOG, "w", encoding="utf-8")

    def log(self, eq: str):
        self.lines.append(eq)
        self._fh.write(eq + "\n")
        self._fh.flush()

    def close(self):
        self._fh.close()

    def as_text(self) -> str:
        return "\n".join(self.lines)


# ═══════════════════════════════════════════════════════════════════════
# Pure-Python matrix utilities
# ═══════════════════════════════════════════════════════════════════════

Matrix = list[list[float]]
Vector = list[float]


def zeros(n: int, m: int) -> Matrix:
    return [[0.0] * m for _ in range(n)]


def eye(n: int) -> Matrix:
    I = zeros(n, n)
    for i in range(n):
        I[i][i] = 1.0
    return I


def mat_copy(A: Matrix) -> Matrix:
    return [row[:] for row in A]


def mat_mul(A: Matrix, B: Matrix) -> Matrix:
    n = len(A)
    m = len(B[0])
    k = len(B)
    C = zeros(n, m)
    for i in range(n):
        for j in range(m):
            s = 0.0
            for p in range(k):
                s += A[i][p] * B[p][j]
            C[i][j] = s
    return C


def mat_transpose(A: Matrix) -> Matrix:
    n = len(A)
    m = len(A[0])
    T = zeros(m, n)
    for i in range(n):
        for j in range(m):
            T[j][i] = A[i][j]
    return T


def vec_norm(v: Vector) -> float:
    return math.sqrt(sum(x * x for x in v))


def mat_vec(A: Matrix, x: Vector) -> Vector:
    return [sum(A[i][j] * x[j] for j in range(len(x))) for i in range(len(A))]


# ═══════════════════════════════════════════════════════════════════════
# Thermal diffusion matrix builder
# ═══════════════════════════════════════════════════════════════════════

def build_thermal_matrix(N: int, metal: BurnMetal, L: float = 0.01,
                         eqlog: Optional[EquationLog] = None) -> Tuple[Matrix, float]:
    """
    Build the N×N finite-difference thermal diffusion matrix for a 1-D rod.

    Returns (A, dx) where A·T_new = T_old  encodes:
        ∂T/∂t = α · ∂²T/∂x²

    Discretised:  T_i^{n+1} = T_i^n + (α·dt/dx²)(T_{i-1} - 2T_i + T_{i+1})

    The matrix form:  A = I + r·D   where D is the tri-diagonal Laplacian
    and r = α·dt/dx².
    """
    alpha = metal.diffusivity       # m²/s
    dx = L / (N + 1)
    # Stability: dt ≤ dx²/(2α)  — use 40% for safety
    dt = 0.4 * dx * dx / (2.0 * alpha)
    r = alpha * dt / (dx * dx)

    A = eye(N)
    for i in range(N):
        A[i][i] += -2.0 * r
        if i > 0:
            A[i][i - 1] += r
        if i < N - 1:
            A[i][i + 1] += r

    if eqlog:
        eqlog.log(f"# Thermal matrix for {metal.symbol}, N={N}")
        eqlog.log(f"# α = {alpha:.3e} m²/s,  dx = {dx:.6e} m,  dt = {dt:.6e} s")
        eqlog.log(f"# r = α·dt/dx² = {r:.6f}")
        eqlog.log(f"# A = I + r·D,  D = tridiag(1, -2, 1)")
        eqlog.log(f"# ∂T/∂t = α · ∂²T/∂x²")
        eqlog.log(f"# T_i^{{n+1}} = T_i^n + r·(T_{{i-1}} - 2·T_i + T_{{i+1}})")
        eqlog.log(f"# Stability criterion: r = {r:.6f} ≤ 0.5  ✓" if r <= 0.5 else
                   f"# WARNING: r = {r:.6f} > 0.5 — unstable!")
        eqlog.log("")

    return A, dx


# ═══════════════════════════════════════════════════════════════════════
# Jacobi iterative solver with RMS tracking
# ═══════════════════════════════════════════════════════════════════════

def jacobi_solve(A: Matrix, b: Vector, tol: float = 1e-10,
                 max_iter: int = 5000,
                 eqlog: Optional[EquationLog] = None) -> Tuple[Vector, list[float], int]:
    """
    Solve Ax = b via Jacobi iteration.
    Returns (x, rms_history, iterations).

    x_i^{k+1} = (1/a_ii) · (b_i - Σ_{j≠i} a_ij · x_j^k)
    """
    N = len(b)
    x = [0.0] * N
    rms_history: list[float] = []

    if eqlog:
        eqlog.log(f"# Jacobi solver: N={N}, tol={tol:.1e}, max_iter={max_iter}")
        eqlog.log(f"# x_i^{{k+1}} = (1/a_ii)·(b_i - Σ_{{j≠i}} a_ij·x_j^k)")
        eqlog.log("")

    for k in range(max_iter):
        x_new = [0.0] * N
        for i in range(N):
            s = b[i]
            for j in range(N):
                if j != i:
                    s -= A[i][j] * x[j]
            x_new[i] = s / A[i][i] if A[i][i] != 0.0 else 0.0

        # RMS of residual
        residual = [b[i] - sum(A[i][j] * x_new[j] for j in range(N)) for i in range(N)]
        rms = math.sqrt(sum(r * r for r in residual) / N)
        rms_history.append(rms)

        if rms < tol:
            if eqlog:
                eqlog.log(f"# Converged at iteration {k+1}, RMS = {rms:.3e}")
            return x_new, rms_history, k + 1

        x = x_new

    if eqlog:
        eqlog.log(f"# Max iterations reached ({max_iter}), final RMS = {rms_history[-1]:.3e}")
    return x, rms_history, max_iter


# ═══════════════════════════════════════════════════════════════════════
# Thermal iterations (time-stepping)
# ═══════════════════════════════════════════════════════════════════════

def thermal_iterate(metal: BurnMetal, N: int, n_steps: int = 200,
                    T_left: float = 300.0, T_right: float = 300.0,
                    T_flash: Optional[float] = None,
                    eqlog: Optional[EquationLog] = None) -> dict:
    """
    Run explicit thermal iteration on a 1-D rod for `metal`.

    Initial condition: T = T_left at x=0, T_flash (or metal.flash_point)
    as a hot source at center, T_right at x=L.

    Returns dict with temperature profile, RMS history, timing, etc.
    """
    if T_flash is None:
        T_flash = metal.flash_point

    A, dx = build_thermal_matrix(N, metal, eqlog=eqlog)

    # Initial temperature: ambient with flash-point spike at center
    T = [T_left + (T_right - T_left) * i / (N - 1) if N > 1 else T_left for i in range(N)]
    mid = N // 2
    T[mid] = T_flash

    if eqlog:
        eqlog.log(f"# Thermal iteration: {metal.symbol}, N={N}, steps={n_steps}")
        eqlog.log(f"# T_left={T_left} K, T_right={T_right} K, T_flash={T_flash} K")
        eqlog.log(f"# Initial T[{mid}] = {T_flash} K (flash-point source)")
        eqlog.log("")

    rms_history = []
    t0 = time.perf_counter()

    for step in range(n_steps):
        T_new = mat_vec(A, T)
        # Enforce boundary conditions
        T_new[0] = T_left
        T_new[-1] = T_right

        # RMS change
        rms = math.sqrt(sum((T_new[i] - T[i]) ** 2 for i in range(N)) / N)
        rms_history.append(rms)
        T = T_new

    elapsed = time.perf_counter() - t0

    if eqlog:
        eqlog.log(f"# Completed {n_steps} steps in {elapsed:.4f} s")
        eqlog.log(f"# Final RMS change: {rms_history[-1]:.6e}")
        eqlog.log(f"# T_max = {max(T):.2f} K, T_min = {min(T):.2f} K")
        eqlog.log("")

    return {
        "metal": metal.symbol,
        "N": N,
        "steps": n_steps,
        "elapsed_s": round(elapsed, 6),
        "rms_final": rms_history[-1] if rms_history else 0.0,
        "rms_history": rms_history,
        "T_profile": T,
        "T_max": max(T),
        "T_min": min(T),
    }


# ═══════════════════════════════════════════════════════════════════════
# QR decomposition (Householder)
# ═══════════════════════════════════════════════════════════════════════

def qr_decompose(A: Matrix, eqlog: Optional[EquationLog] = None) -> Tuple[Matrix, Matrix]:
    """
    Householder QR decomposition: A = Q·R

    For column k, construct Householder reflector:
        v = x - ||x||·e₁
        H_k = I - 2·(v·vᵀ)/(vᵀ·v)
    """
    n = len(A)
    Q = eye(n)
    R = mat_copy(A)

    if eqlog:
        eqlog.log(f"# QR decomposition (Householder), N={n}")
        eqlog.log(f"# A = Q·R,  H_k = I - 2·(v·vᵀ)/(vᵀ·v)")
        eqlog.log("")

    for k in range(n - 1):
        # Extract column k below diagonal
        x = [R[i][k] for i in range(k, n)]
        norm_x = vec_norm(x)
        if norm_x < 1e-15:
            continue

        sign = 1.0 if x[0] >= 0 else -1.0
        x[0] += sign * norm_x
        norm_v = vec_norm(x)
        if norm_v < 1e-15:
            continue
        v = [xi / norm_v for xi in x]

        # Apply H to R (columns k..n-1)
        for j in range(k, n):
            dot = sum(v[i - k] * R[i][j] for i in range(k, n))
            for i in range(k, n):
                R[i][j] -= 2.0 * v[i - k] * dot

        # Accumulate Q
        for j in range(n):
            dot = sum(v[i - k] * Q[i][j] for i in range(k, n))
            for i in range(k, n):
                Q[i][j] -= 2.0 * v[i - k] * dot

    Q = mat_transpose(Q)  # We accumulated Q^T, so transpose
    return Q, R


# ═══════════════════════════════════════════════════════════════════════
# QR eigenvalue iteration
# ═══════════════════════════════════════════════════════════════════════

def qr_eigenvalues(A: Matrix, max_iter: int = 300, tol: float = 1e-10,
                   eqlog: Optional[EquationLog] = None) -> Tuple[list[float], int]:
    """
    Compute eigenvalues via QR iteration with Wilkinson shift.

    Repeat:  A_k = Q_k · R_k  →  A_{k+1} = R_k · Q_k

    Eigenvalues converge on the diagonal.
    """
    n = len(A)
    Ak = mat_copy(A)

    if eqlog:
        eqlog.log(f"# QR eigenvalue iteration, N={n}, max_iter={max_iter}")
        eqlog.log(f"# A_k = Q_k·R_k → A_{{k+1}} = R_k·Q_k  (with shift)")
        eqlog.log("")

    for it in range(max_iter):
        # Wilkinson shift: use bottom-right 2×2 eigenvalue closer to a_{nn}
        if n >= 2:
            a = Ak[n - 2][n - 2]
            b = Ak[n - 2][n - 1]
            c = Ak[n - 1][n - 2]
            d = Ak[n - 1][n - 1]
            tr = a + d
            det = a * d - b * c
            disc = tr * tr - 4.0 * det
            if disc >= 0:
                lam1 = (tr + math.sqrt(disc)) / 2.0
                lam2 = (tr - math.sqrt(disc)) / 2.0
                shift = lam1 if abs(lam1 - d) < abs(lam2 - d) else lam2
            else:
                shift = d
        else:
            shift = Ak[0][0]

        # Shift
        for i in range(n):
            Ak[i][i] -= shift

        Q, R_mat = qr_decompose(Ak)
        Ak = mat_mul(R_mat, Q)

        # Un-shift
        for i in range(n):
            Ak[i][i] += shift

        # Check convergence: sub-diagonal elements
        off_diag = 0.0
        for i in range(1, n):
            off_diag += Ak[i][i - 1] ** 2
        off_diag = math.sqrt(off_diag)

        if off_diag < tol:
            eigenvalues = [Ak[i][i] for i in range(n)]
            if eqlog:
                eqlog.log(f"# Eigen converged at iteration {it+1}, off-diag norm = {off_diag:.3e}")
                eqlog.log(f"# Eigenvalues: {[round(e, 6) for e in eigenvalues]}")
                eqlog.log("")
            return eigenvalues, it + 1

    eigenvalues = [Ak[i][i] for i in range(n)]
    if eqlog:
        eqlog.log(f"# Eigen: max iter ({max_iter}), off-diag = {off_diag:.3e}")
        eqlog.log(f"# Eigenvalues: {[round(e, 6) for e in eigenvalues]}")
        eqlog.log("")
    return eigenvalues, max_iter


# ═══════════════════════════════════════════════════════════════════════
# Resolution cascade  4→8→16→32→64
# ═══════════════════════════════════════════════════════════════════════

RESOLUTIONS = [4, 8, 16, 32, 64]


def run_cascade(metal: BurnMetal, resolutions: Optional[list[int]] = None,
                n_steps: int = 200,
                eqlog: Optional[EquationLog] = None) -> list[dict]:
    """Run the full resolution cascade for one metal."""
    if resolutions is None:
        resolutions = RESOLUTIONS

    results = []
    for N in resolutions:
        if eqlog:
            eqlog.log(f"{'='*60}")
            eqlog.log(f"# RESOLUTION CASCADE: {metal.symbol} @ {N}×{N}")
            eqlog.log(f"{'='*60}")

        # Thermal iterations
        res = thermal_iterate(metal, N, n_steps=n_steps, eqlog=eqlog)

        # Build matrix and compute eigenvalues
        A, dx = build_thermal_matrix(N, metal, eqlog=eqlog)
        eigs, eig_iters = qr_eigenvalues(A, eqlog=eqlog)
        res["eigenvalues"] = eigs
        res["eigen_iterations"] = eig_iters
        res["dx"] = dx

        # Jacobi verification solve (b = steady-state RHS)
        b = [300.0] * N
        b[N // 2] = metal.flash_point
        _, jac_rms, jac_iters = jacobi_solve(A, b, eqlog=eqlog)
        res["jacobi_iterations"] = jac_iters
        res["jacobi_rms_final"] = jac_rms[-1] if jac_rms else 0.0

        results.append(res)

    return results


# ═══════════════════════════════════════════════════════════════════════
# Full report run
# ═══════════════════════════════════════════════════════════════════════

def run_full_report(metals: Optional[list[BurnMetal]] = None) -> dict:
    """Execute the full burn analysis for Report 071."""
    if metals is None:
        metals = BURN_DB[:6]  # Al, Mg, Fe, Ti, Zr, W — most interesting

    eqlog = EquationLog()
    eqlog.log("# ════════════════════════════════════════════════════════════")
    eqlog.log("# Report #071 — Burning of Inorganic Materials")
    eqlog.log("#              & Flash Pointing Metals")
    eqlog.log("# VSEPR-SIM 4.0.4")
    eqlog.log("# ════════════════════════════════════════════════════════════")
    eqlog.log("")

    all_results = {}
    t0 = time.perf_counter()

    for metal in metals:
        eqlog.log(f"\n# {'─'*50}")
        eqlog.log(f"# Metal: {metal.name} ({metal.symbol})")
        eqlog.log(f"# Z={metal.Z}, M={metal.molar_mass} g/mol, ρ={metal.density} g/cm³")
        eqlog.log(f"# T_melt={metal.melting_point} K, T_flash={metal.flash_point} K")
        eqlog.log(f"# ΔH_comb={metal.delta_H_comb} kJ/mol")
        eqlog.log(f"# Oxide: {metal.oxide_formula}")
        eqlog.log(f"# α={metal.diffusivity:.3e} m²/s, k={metal.thermal_cond} W/(m·K)")
        eqlog.log(f"# {'─'*50}\n")

        # Combustion equation
        eqlog.log(f"# Combustion reaction:")
        if metal.symbol == "Al":
            eqlog.log(f"  4Al + 3O₂ → 2Al₂O₃  ΔH = -{metal.delta_H_comb} kJ/mol")
        elif metal.symbol == "Mg":
            eqlog.log(f"  2Mg + O₂ → 2MgO  ΔH = -{metal.delta_H_comb} kJ/mol")
        elif metal.symbol == "Fe":
            eqlog.log(f"  4Fe + 3O₂ → 2Fe₂O₃  ΔH = -{metal.delta_H_comb} kJ/mol")
        elif metal.symbol == "Ti":
            eqlog.log(f"  Ti + O₂ → TiO₂  ΔH = -{metal.delta_H_comb} kJ/mol")
        elif metal.symbol == "Zr":
            eqlog.log(f"  Zr + O₂ → ZrO₂  ΔH = -{metal.delta_H_comb} kJ/mol")
        elif metal.symbol == "W":
            eqlog.log(f"  2W + 3O₂ → 2WO₃  ΔH = -{metal.delta_H_comb} kJ/mol")
        else:
            eqlog.log(f"  {metal.symbol} + O₂ → {metal.oxide_formula}  ΔH = -{metal.delta_H_comb} kJ/mol")
        eqlog.log("")

        # Radiative loss at flash point
        q_rad = SIGMA_SB * metal.flash_point ** 4
        eqlog.log(f"# Stefan–Boltzmann radiative flux at T_flash:")
        eqlog.log(f"#   q = σ·T⁴ = {SIGMA_SB:.4e} × {metal.flash_point}⁴ = {q_rad:.2f} W/m²")
        eqlog.log(f"#   q = {q_rad/1e6:.4f} MW/m²")
        eqlog.log("")

        cascade = run_cascade(metal, eqlog=eqlog)
        all_results[metal.symbol] = {
            "metal": metal.name,
            "flash_point_K": metal.flash_point,
            "delta_H_comb_kJ": metal.delta_H_comb,
            "radiative_flux_Wm2": round(q_rad, 2),
            "cascade": []
        }
        for res in cascade:
            entry = {
                "N": res["N"],
                "elapsed_s": res["elapsed_s"],
                "rms_final": res["rms_final"],
                "T_max": round(res["T_max"], 4),
                "T_min": round(res["T_min"], 4),
                "eigen_iterations": res["eigen_iterations"],
                "eigenvalues_sample": [round(e, 6) for e in res["eigenvalues"][:5]],
                "jacobi_iterations": res["jacobi_iterations"],
                "jacobi_rms_final": res["jacobi_rms_final"],
            }
            all_results[metal.symbol]["cascade"].append(entry)

    total_time = time.perf_counter() - t0
    eqlog.log(f"\n# Total computation time: {total_time:.4f} s")
    eqlog.log(f"# Metals analysed: {len(metals)}")
    eqlog.log(f"# Resolution levels: {RESOLUTIONS}")
    eqlog.close()

    # Write JSON log
    with open(LOG_FILE, "w", encoding="utf-8") as f:
        json.dump(all_results, f, indent=2, default=str)

    return {
        "total_time_s": round(total_time, 4),
        "metals": len(metals),
        "resolutions": RESOLUTIONS,
        "results": all_results,
        "equation_log": str(EQ_LOG),
        "data_log": str(LOG_FILE),
    }


# ═══════════════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    print("Report #071 — Burning of Inorganic Materials & Flash Pointing Metals")
    print("=" * 65)
    report = run_full_report()
    print(f"\nCompleted in {report['total_time_s']} s")
    print(f"Equation log : {report['equation_log']}")
    print(f"Data log     : {report['data_log']}")
    print(f"\nRMS convergence summary:")
    for sym, data in report["results"].items():
        print(f"  {sym:3s} ({data['metal']:12s}): flash={data['flash_point_K']}K, "
              f"ΔH={data['delta_H_comb_kJ']} kJ/mol")
        for c in data["cascade"]:
            print(f"       {c['N']:3d}×{c['N']:<3d}  "
                  f"RMS={c['rms_final']:.3e}  "
                  f"eigen_its={c['eigen_iterations']:3d}  "
                  f"jacobi_its={c['jacobi_iterations']:4d}  "
                  f"time={c['elapsed_s']:.4f}s")
