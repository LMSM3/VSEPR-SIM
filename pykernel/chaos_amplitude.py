"""
chaos_amplitude — Chaos Factor, Amplitude Scale, and Scale Weighting Factors.

Deterministic analysis metrics for atomistic lattice frames produced by
the metal sweep and crystal TUI pipelines.  Every metric is explicit,
inspectable, and grounded in physical observables.

Definitions
-----------

**Chaos Factor (chi)**
    Dimensionless scalar in [0, 1] that quantifies how far an atomistic
    configuration deviates from its ideal crystallographic lattice.

    chi = w_disp * S_disp + w_force * S_force + w_therm * S_therm
                                          + w_aniso * S_aniso

    where each sub-score S_i in [0, 1] measures one disorder channel:

      S_disp   Positional displacement disorder
               sigma_disp / (a * D_MAX_FRAC)   clamped to [0, 1]
               sigma_disp = RMS of |r_i - r_ideal_i|

      S_force  Force-field non-uniformity
               1 - (F_min / F_max)  if F_max > 0 else 0
               Measures force anisotropy across the lattice.

      S_therm  Thermal exceedance
               (T - T_ref) / (T_melt - T_ref)   clamped to [0, 1]
               Fraction of thermal budget consumed.

      S_aniso  Lattice anisotropy
               std(a, b, c) / mean(a, b, c)     clamped to [0, 1]
               Cubic lattice -> 0; highly distorted -> 1.

**Amplitude Scale (A)**
    Per-atom force amplitude histogram binned into N_BINS logarithmic
    magnitude bands from A_MIN to A_MAX (eV/Angstrom).  Each bin stores
    the fraction of atoms whose force magnitude falls in that band.
    The full histogram is a deterministic, inspectable fingerprint of
    the force distribution for one frame.

**Scale Weighting Factors (w_i)**
    The four weights (w_disp, w_force, w_therm, w_aniso) that compose
    the chaos factor.  Default weights derived from physical reasoning:
      w_disp  = 0.35  (positional disorder dominates structural chaos)
      w_force = 0.25  (force variance reveals dynamic instability)
      w_therm = 0.25  (temperature deviation signals phase-transition risk)
      w_aniso = 0.15  (lattice distortion is a secondary structural flag)

    Weights always sum to 1.0.  Users may override for domain-specific
    screening.  All weights are logged per evaluation.

Anti-black-box: every sub-score, every bin count, every weight is
explicitly stored and inspectable.  No hidden state.

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Optional, Sequence


# =====================================================================
# Constants
# =====================================================================

# Maximum fractional displacement before S_disp saturates (fraction of a)
D_MAX_FRAC: float = 0.25

# Amplitude histogram bounds (eV/Angstrom)
A_MIN: float = 1e-4
A_MAX: float = 50.0
N_BINS: int = 20

# Default scale weighting factors
DEFAULT_WEIGHTS: dict[str, float] = {
    "w_disp":  0.35,
    "w_force": 0.25,
    "w_therm": 0.25,
    "w_aniso": 0.15,
}


# =====================================================================
# Vec3 lightweight (avoid circular import with crystal_tui)
# =====================================================================

def _norm3(x: float, y: float, z: float) -> float:
    return math.sqrt(x * x + y * y + z * z)


# =====================================================================
# Scale Weighting Factors
# =====================================================================

@dataclass(frozen=True)
class ScaleWeights:
    """Immutable set of chaos-factor channel weights.

    Weights MUST sum to 1.0 (enforced at construction).
    Each weight scales the corresponding sub-score S_i when computing
    the composite chaos factor chi.

    Attributes
    ----------
    w_disp : float
        Positional displacement disorder weight.
    w_force : float
        Force-field non-uniformity weight.
    w_therm : float
        Thermal exceedance weight.
    w_aniso : float
        Lattice anisotropy weight.
    """
    w_disp:  float = 0.35
    w_force: float = 0.25
    w_therm: float = 0.25
    w_aniso: float = 0.15

    def __post_init__(self):
        total = self.w_disp + self.w_force + self.w_therm + self.w_aniso
        if abs(total - 1.0) > 1e-9:
            raise ValueError(
                f"Scale weights must sum to 1.0, got {total:.12f}  "
                f"(w_disp={self.w_disp}, w_force={self.w_force}, "
                f"w_therm={self.w_therm}, w_aniso={self.w_aniso})"
            )

    def to_dict(self) -> dict[str, float]:
        return {
            "w_disp": self.w_disp,
            "w_force": self.w_force,
            "w_therm": self.w_therm,
            "w_aniso": self.w_aniso,
        }

    @staticmethod
    def from_dict(d: dict[str, float]) -> "ScaleWeights":
        return ScaleWeights(
            w_disp=d["w_disp"],
            w_force=d["w_force"],
            w_therm=d["w_therm"],
            w_aniso=d["w_aniso"],
        )

    @staticmethod
    def uniform() -> "ScaleWeights":
        """Equal weighting across all four channels."""
        return ScaleWeights(0.25, 0.25, 0.25, 0.25)

    @staticmethod
    def displacement_heavy() -> "ScaleWeights":
        """Emphasis on positional disorder (solid-state screening)."""
        return ScaleWeights(0.55, 0.20, 0.15, 0.10)

    @staticmethod
    def thermal_heavy() -> "ScaleWeights":
        """Emphasis on thermal deviation (phase-transition screening)."""
        return ScaleWeights(0.20, 0.20, 0.50, 0.10)

    @staticmethod
    def force_heavy() -> "ScaleWeights":
        """Emphasis on force non-uniformity (dynamic stability screening)."""
        return ScaleWeights(0.20, 0.50, 0.15, 0.15)


# =====================================================================
# Amplitude Scale
# =====================================================================

@dataclass
class AmplitudeBin:
    """One bin in the amplitude histogram."""
    lo: float       # lower bound (eV/Angstrom)
    hi: float       # upper bound (eV/Angstrom)
    count: int = 0  # number of atoms in this bin
    fraction: float = 0.0  # count / N_atoms


@dataclass
class AmplitudeScale:
    """Logarithmic force-amplitude histogram for one frame.

    Bins are spaced logarithmically from A_MIN to A_MAX.
    Each bin stores the count and fraction of atoms whose per-atom
    force magnitude falls in [lo, hi).

    Attributes
    ----------
    bins : list[AmplitudeBin]
        The histogram bins.
    n_atoms : int
        Total number of atoms evaluated.
    F_min : float
        Minimum per-atom force magnitude.
    F_max : float
        Maximum per-atom force magnitude.
    F_mean : float
        Mean per-atom force magnitude.
    F_rms : float
        Root-mean-square force magnitude.
    F_std : float
        Standard deviation of force magnitudes.
    peak_bin : int
        Index of the bin with highest count.
    spectral_entropy : float
        Shannon entropy of the bin fractions (bits).
        Low entropy = concentrated distribution; high = spread.
    """
    bins: list[AmplitudeBin] = field(default_factory=list)
    n_atoms: int = 0
    F_min: float = 0.0
    F_max: float = 0.0
    F_mean: float = 0.0
    F_rms: float = 0.0
    F_std: float = 0.0
    peak_bin: int = 0
    spectral_entropy: float = 0.0

    def to_dict(self) -> dict:
        return {
            "n_atoms": self.n_atoms,
            "F_min": self.F_min,
            "F_max": self.F_max,
            "F_mean": self.F_mean,
            "F_rms": self.F_rms,
            "F_std": self.F_std,
            "peak_bin": self.peak_bin,
            "spectral_entropy": self.spectral_entropy,
            "bins": [
                {"lo": b.lo, "hi": b.hi, "count": b.count, "fraction": b.fraction}
                for b in self.bins
            ],
        }


def build_amplitude_scale(
    force_magnitudes: Sequence[float],
    n_bins: int = N_BINS,
    a_min: float = A_MIN,
    a_max: float = A_MAX,
) -> AmplitudeScale:
    """Build a logarithmic amplitude histogram from per-atom force magnitudes.

    Parameters
    ----------
    force_magnitudes : sequence of float
        Per-atom force magnitudes (eV/Angstrom or equivalent).
    n_bins : int
        Number of logarithmic bins.
    a_min, a_max : float
        Histogram bounds (magnitudes below a_min go into bin 0;
        above a_max go into the last bin).

    Returns
    -------
    AmplitudeScale
        Fully populated amplitude histogram.
    """
    n = len(force_magnitudes)
    if n == 0:
        return AmplitudeScale()

    # Build log-spaced bin edges
    log_lo = math.log10(max(a_min, 1e-20))
    log_hi = math.log10(max(a_max, a_min * 10))
    edges = [10.0 ** (log_lo + i * (log_hi - log_lo) / n_bins) for i in range(n_bins + 1)]

    bins = [AmplitudeBin(lo=edges[i], hi=edges[i + 1]) for i in range(n_bins)]

    # Assign atoms to bins
    for mag in force_magnitudes:
        if mag <= edges[0]:
            bins[0].count += 1
        elif mag >= edges[-1]:
            bins[-1].count += 1
        else:
            # Binary-ish search via log position
            if mag > 0:
                idx = int((math.log10(mag) - log_lo) / (log_hi - log_lo) * n_bins)
                idx = max(0, min(n_bins - 1, idx))
            else:
                idx = 0
            bins[idx].count += 1

    # Fractions
    for b in bins:
        b.fraction = b.count / n

    # Statistics
    f_min = min(force_magnitudes)
    f_max = max(force_magnitudes)
    f_mean = sum(force_magnitudes) / n
    f_rms = math.sqrt(sum(f * f for f in force_magnitudes) / n)
    f_std = math.sqrt(sum((f - f_mean) ** 2 for f in force_magnitudes) / n)

    # Peak bin
    peak_idx = max(range(n_bins), key=lambda i: bins[i].count)

    # Shannon spectral entropy (bits)
    entropy = 0.0
    for b in bins:
        if b.fraction > 0:
            entropy -= b.fraction * math.log2(b.fraction)

    return AmplitudeScale(
        bins=bins,
        n_atoms=n,
        F_min=f_min,
        F_max=f_max,
        F_mean=f_mean,
        F_rms=f_rms,
        F_std=f_std,
        peak_bin=peak_idx,
        spectral_entropy=entropy,
    )


# =====================================================================
# Chaos Sub-Scores
# =====================================================================

def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def score_displacement(
    positions: Sequence[tuple[float, float, float]],
    ideal_positions: Sequence[tuple[float, float, float]],
    lattice_a: float,
    d_max_frac: float = D_MAX_FRAC,
) -> float:
    """S_disp: positional displacement disorder.

    sigma_disp = RMS of |r_i - r_ideal_i|
    S_disp = sigma_disp / (a * d_max_frac)   clamped to [0, 1]

    Parameters
    ----------
    positions : sequence of (x, y, z) tuples
        Actual atom positions.
    ideal_positions : sequence of (x, y, z) tuples
        Reference crystallographic positions.
    lattice_a : float
        Lattice parameter (Angstroms).
    d_max_frac : float
        Fraction of lattice parameter at which S_disp saturates.

    Returns
    -------
    float in [0, 1]
    """
    n = len(positions)
    if n == 0 or lattice_a <= 0:
        return 0.0

    sum_d2 = 0.0
    for (px, py, pz), (ix, iy, iz) in zip(positions, ideal_positions):
        dx = px - ix
        dy = py - iy
        dz = pz - iz
        sum_d2 += dx * dx + dy * dy + dz * dz

    sigma = math.sqrt(sum_d2 / n)
    threshold = lattice_a * d_max_frac
    if threshold <= 0:
        return 0.0
    return _clamp01(sigma / threshold)


def score_force(force_magnitudes: Sequence[float]) -> float:
    """S_force: force-field non-uniformity.

    S_force = 1 - (F_min / F_max)  if F_max > 0 else 0

    A perfectly uniform force field yields S_force = 0.
    Highly non-uniform yields S_force -> 1.

    Parameters
    ----------
    force_magnitudes : sequence of float
        Per-atom force magnitudes.

    Returns
    -------
    float in [0, 1]
    """
    if not force_magnitudes:
        return 0.0
    f_max = max(force_magnitudes)
    if f_max <= 0:
        return 0.0
    f_min = min(force_magnitudes)
    return _clamp01(1.0 - f_min / f_max)


def score_thermal(T: float, T_ref: float, T_melt: float) -> float:
    """S_therm: thermal exceedance.

    S_therm = (T - T_ref) / (T_melt - T_ref)   clamped to [0, 1]

    Parameters
    ----------
    T : float
        Current temperature (K).
    T_ref : float
        Reference temperature (K), typically 298 K.
    T_melt : float
        Melting point (K).

    Returns
    -------
    float in [0, 1]
    """
    denom = T_melt - T_ref
    if denom <= 0:
        return 1.0 if T > T_ref else 0.0
    return _clamp01((T - T_ref) / denom)


def score_anisotropy(a: float, b: float, c: float) -> float:
    """S_aniso: lattice anisotropy.

    S_aniso = std(a, b, c) / mean(a, b, c)   clamped to [0, 1]

    Perfectly cubic lattice -> 0.
    Highly distorted -> 1.

    Parameters
    ----------
    a, b, c : float
        Lattice parameters (Angstroms).

    Returns
    -------
    float in [0, 1]
    """
    mean_abc = (a + b + c) / 3.0
    if mean_abc <= 0:
        return 0.0
    var = ((a - mean_abc) ** 2 + (b - mean_abc) ** 2 + (c - mean_abc) ** 2) / 3.0
    std = math.sqrt(var)
    return _clamp01(std / mean_abc)


# =====================================================================
# Chaos Factor (composite)
# =====================================================================

@dataclass
class ChaosResult:
    """Full chaos-factor evaluation — every sub-score inspectable.

    Attributes
    ----------
    chi : float
        Composite chaos factor in [0, 1].
    S_disp : float
        Displacement sub-score.
    S_force : float
        Force non-uniformity sub-score.
    S_therm : float
        Thermal exceedance sub-score.
    S_aniso : float
        Lattice anisotropy sub-score.
    weights : ScaleWeights
        The weights used for this evaluation.
    amplitude : AmplitudeScale
        Full amplitude histogram for this frame.
    label : str
        Optional human label (e.g. element symbol).
    """
    chi: float = 0.0
    S_disp: float = 0.0
    S_force: float = 0.0
    S_therm: float = 0.0
    S_aniso: float = 0.0
    weights: ScaleWeights = field(default_factory=ScaleWeights)
    amplitude: AmplitudeScale = field(default_factory=AmplitudeScale)
    label: str = ""

    @property
    def grade(self) -> str:
        """Human-readable stability grade."""
        if self.chi < 0.10:
            return "STABLE"
        if self.chi < 0.25:
            return "LOW-CHAOS"
        if self.chi < 0.50:
            return "MODERATE"
        if self.chi < 0.75:
            return "HIGH-CHAOS"
        return "CRITICAL"

    def dominant_channel(self) -> str:
        """Name of the sub-score contributing most to chi."""
        contributions = {
            "disp":  self.weights.w_disp * self.S_disp,
            "force": self.weights.w_force * self.S_force,
            "therm": self.weights.w_therm * self.S_therm,
            "aniso": self.weights.w_aniso * self.S_aniso,
        }
        return max(contributions, key=contributions.get)

    def contributions(self) -> dict[str, float]:
        """Weighted contribution of each channel to chi."""
        return {
            "disp":  self.weights.w_disp * self.S_disp,
            "force": self.weights.w_force * self.S_force,
            "therm": self.weights.w_therm * self.S_therm,
            "aniso": self.weights.w_aniso * self.S_aniso,
        }

    def to_dict(self) -> dict:
        return {
            "chi": self.chi,
            "grade": self.grade,
            "S_disp": self.S_disp,
            "S_force": self.S_force,
            "S_therm": self.S_therm,
            "S_aniso": self.S_aniso,
            "dominant": self.dominant_channel(),
            "contributions": self.contributions(),
            "weights": self.weights.to_dict(),
            "amplitude": self.amplitude.to_dict(),
            "label": self.label,
        }


def compute_chaos(
    positions: Sequence[tuple[float, float, float]],
    ideal_positions: Sequence[tuple[float, float, float]],
    force_magnitudes: Sequence[float],
    lattice_a: float,
    lattice_b: float,
    lattice_c: float,
    T: float,
    T_ref: float = 298.0,
    T_melt: float = 1000.0,
    weights: Optional[ScaleWeights] = None,
    label: str = "",
) -> ChaosResult:
    """Compute the full chaos factor for one atomistic frame.

    Parameters
    ----------
    positions : sequence of (x, y, z)
        Actual atom positions.
    ideal_positions : sequence of (x, y, z)
        Reference crystallographic positions.
    force_magnitudes : sequence of float
        Per-atom force magnitudes.
    lattice_a, lattice_b, lattice_c : float
        Lattice parameters.
    T : float
        Current temperature (K).
    T_ref : float
        Reference temperature (K).
    T_melt : float
        Melting point (K).
    weights : ScaleWeights or None
        Channel weights (defaults to DEFAULT_WEIGHTS).
    label : str
        Human label for this evaluation.

    Returns
    -------
    ChaosResult
        Fully inspectable chaos-factor result.
    """
    w = weights or ScaleWeights()

    s_disp  = score_displacement(positions, ideal_positions, lattice_a)
    s_force = score_force(force_magnitudes)
    s_therm = score_thermal(T, T_ref, T_melt)
    s_aniso = score_anisotropy(lattice_a, lattice_b, lattice_c)

    chi = (w.w_disp  * s_disp
         + w.w_force * s_force
         + w.w_therm * s_therm
         + w.w_aniso * s_aniso)

    amp = build_amplitude_scale(force_magnitudes)

    return ChaosResult(
        chi=chi,
        S_disp=s_disp,
        S_force=s_force,
        S_therm=s_therm,
        S_aniso=s_aniso,
        weights=w,
        amplitude=amp,
        label=label,
    )


# =====================================================================
# Batch evaluation (for metal sweep integration)
# =====================================================================

@dataclass
class ChaosSweepSummary:
    """Summary of chaos-factor evaluation across multiple elements.

    Attributes
    ----------
    results : list[ChaosResult]
        One result per element.
    n_elements : int
        Number of elements evaluated.
    chi_min : float
        Minimum chaos factor.
    chi_max : float
        Maximum chaos factor.
    chi_mean : float
        Mean chaos factor.
    most_stable : str
        Label of the element with lowest chi.
    most_chaotic : str
        Label of the element with highest chi.
    grade_distribution : dict[str, int]
        Count of elements in each grade.
    """
    results: list[ChaosResult] = field(default_factory=list)
    n_elements: int = 0
    chi_min: float = 0.0
    chi_max: float = 0.0
    chi_mean: float = 0.0
    most_stable: str = ""
    most_chaotic: str = ""
    grade_distribution: dict[str, int] = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "n_elements": self.n_elements,
            "chi_min": self.chi_min,
            "chi_max": self.chi_max,
            "chi_mean": self.chi_mean,
            "most_stable": self.most_stable,
            "most_chaotic": self.most_chaotic,
            "grade_distribution": self.grade_distribution,
            "results": [r.to_dict() for r in self.results],
        }


def summarise_chaos(results: Sequence[ChaosResult]) -> ChaosSweepSummary:
    """Aggregate chaos results into a sweep summary."""
    if not results:
        return ChaosSweepSummary()

    n = len(results)
    chis = [r.chi for r in results]
    chi_min = min(chis)
    chi_max = max(chis)
    chi_mean = sum(chis) / n

    min_idx = chis.index(chi_min)
    max_idx = chis.index(chi_max)

    grades: dict[str, int] = {}
    for r in results:
        g = r.grade
        grades[g] = grades.get(g, 0) + 1

    return ChaosSweepSummary(
        results=list(results),
        n_elements=n,
        chi_min=chi_min,
        chi_max=chi_max,
        chi_mean=chi_mean,
        most_stable=results[min_idx].label,
        most_chaotic=results[max_idx].label,
        grade_distribution=grades,
    )


# =====================================================================
# Weight sensitivity analysis
# =====================================================================

@dataclass
class SensitivityResult:
    """How much chi changes when each weight is perturbed by delta.

    Attributes
    ----------
    base_chi : float
        Chi with the original weights.
    sensitivities : dict[str, float]
        d(chi)/d(w_i) estimated by finite difference.
    delta : float
        Perturbation size used.
    """
    base_chi: float = 0.0
    sensitivities: dict[str, float] = field(default_factory=dict)
    delta: float = 0.01

    def most_sensitive(self) -> str:
        """Channel whose weight change most affects chi."""
        return max(self.sensitivities, key=lambda k: abs(self.sensitivities[k]))

    def to_dict(self) -> dict:
        return {
            "base_chi": self.base_chi,
            "sensitivities": self.sensitivities,
            "delta": self.delta,
            "most_sensitive": self.most_sensitive(),
        }


def weight_sensitivity(
    positions: Sequence[tuple[float, float, float]],
    ideal_positions: Sequence[tuple[float, float, float]],
    force_magnitudes: Sequence[float],
    lattice_a: float,
    lattice_b: float,
    lattice_c: float,
    T: float,
    T_ref: float = 298.0,
    T_melt: float = 1000.0,
    base_weights: Optional[ScaleWeights] = None,
    delta: float = 0.01,
) -> SensitivityResult:
    """Estimate d(chi)/d(w_i) by symmetric finite differences.

    For each weight w_i, computes chi at (w_i + delta) and (w_i - delta)
    with the other weights rescaled to maintain sum = 1.

    Parameters
    ----------
    (same as compute_chaos, plus:)
    delta : float
        Perturbation size for each weight.

    Returns
    -------
    SensitivityResult
    """
    w = base_weights or ScaleWeights()
    base = compute_chaos(
        positions, ideal_positions, force_magnitudes,
        lattice_a, lattice_b, lattice_c,
        T, T_ref, T_melt, w,
    )

    # Sub-scores are weight-independent; chi = sum(w_i * S_i)
    # So d(chi)/d(w_i) at constant sum(w) is:
    #   partial chi / partial w_i with renormalisation
    # Direct analytical approach for linear combination:
    scores = {
        "disp":  base.S_disp,
        "force": base.S_force,
        "therm": base.S_therm,
        "aniso": base.S_aniso,
    }
    weight_vals = {
        "disp":  w.w_disp,
        "force": w.w_force,
        "therm": w.w_therm,
        "aniso": w.w_aniso,
    }
    channels = ["disp", "force", "therm", "aniso"]

    sensitivities: dict[str, float] = {}
    for ch in channels:
        # Perturb w_ch up by delta, renormalise others down
        w_up = dict(weight_vals)
        w_up[ch] = min(w_up[ch] + delta, 1.0)
        remainder = 1.0 - w_up[ch]
        other_sum = sum(w_up[k] for k in channels if k != ch)
        if other_sum > 0:
            for k in channels:
                if k != ch:
                    w_up[k] = w_up[k] * remainder / other_sum
        chi_up = sum(w_up[k] * scores[k] for k in channels)

        # Perturb down
        w_dn = dict(weight_vals)
        w_dn[ch] = max(w_dn[ch] - delta, 0.0)
        remainder = 1.0 - w_dn[ch]
        other_sum = sum(w_dn[k] for k in channels if k != ch)
        if other_sum > 0:
            for k in channels:
                if k != ch:
                    w_dn[k] = w_dn[k] * remainder / other_sum
        chi_dn = sum(w_dn[k] * scores[k] for k in channels)

        denom = w_up[ch] - w_dn[ch]
        if abs(denom) > 1e-15:
            sensitivities[ch] = (chi_up - chi_dn) / denom
        else:
            sensitivities[ch] = 0.0

    return SensitivityResult(
        base_chi=base.chi,
        sensitivities=sensitivities,
        delta=delta,
    )
