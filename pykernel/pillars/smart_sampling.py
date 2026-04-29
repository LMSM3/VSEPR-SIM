"""
SmartSampling — Universal Adaptive Sampling Manager
=====================================================

Pillar D of the VSEPR-SIM Five Pillars architecture.

Core mission: use limited compute to explore the highest-value regions first.

Scoring equation:
  score_total = w1*novelty + w2*objective_match + w3*uncertainty
              + w4*physical_plausibility + w5*instability_interest
              + w6*compute_affordability

Roles:
  - Adaptive grid refinement
  - Weirdness/novelty score prioritization
  - Uncertainty-driven resampling
  - Novelty-biased exploration
  - Edge-of-stability probing
  - Objective-biased candidate ranking
  - Stochastic but reproducible branch selection

Cross-pillar usage:
  Steam tables  → refine near phase boundaries, critical behavior
  Crystals      → focus on low-energy but structurally distinct candidates
  Metals        → alloy composition windows near target property fronts
  Organics      → prioritize high-payoff torsions/side-chains

Anti-black-box: every sampling decision is logged with score breakdown.
Deterministic: same seed → identical sampling sequence.

VSEPR-SIM Five Pillars v1.0
"""

from __future__ import annotations

import math
import json
import random
import hashlib
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Tuple, Optional, Callable, Any
from enum import Enum, auto


# ============================================================================
# Sampling tiers
# ============================================================================

class SamplingTier(Enum):
    COARSE = "coarse"           # Initial sweep
    STANDARD = "standard"       # Normal resolution
    REFINED = "refined"         # Phase-boundary refinement
    CRITICAL = "critical"       # Near critical / exotic regions
    STRESS_TEST = "stress_test" # Randomized interpolation stress


# ============================================================================
# Score components
# ============================================================================

@dataclass
class SampleScore:
    """Complete score breakdown for one candidate sample point."""
    sample_id: str
    coordinates: Dict[str, float]   # {"T": 400, "P": 1e6, "x_Cu": 0.3, ...}
    novelty: float = 0.0            # How different from existing samples
    objective_match: float = 0.0    # How well it matches mission goals
    uncertainty: float = 0.0        # How uncertain the prediction is
    physical_plausibility: float = 0.0  # Does it obey physics?
    instability_interest: float = 0.0   # Is the region exotic/metastable?
    compute_affordability: float = 0.0  # How expensive to evaluate?
    total: float = 0.0
    tier: SamplingTier = SamplingTier.STANDARD
    accepted: bool = False
    metadata: Dict[str, Any] = field(default_factory=dict)

    @property
    def provenance_hash(self) -> str:
        raw = json.dumps(self.coordinates, sort_keys=True)
        return hashlib.sha256(raw.encode()).hexdigest()[:10]


@dataclass
class SamplingWeights:
    """Configurable weights for the scoring equation."""
    w_novelty: float = 0.20
    w_objective: float = 0.25
    w_uncertainty: float = 0.20
    w_plausibility: float = 0.15
    w_instability: float = 0.10
    w_affordability: float = 0.10

    def normalize(self):
        total = (self.w_novelty + self.w_objective + self.w_uncertainty
                 + self.w_plausibility + self.w_instability + self.w_affordability)
        if total > 0:
            self.w_novelty /= total
            self.w_objective /= total
            self.w_uncertainty /= total
            self.w_plausibility /= total
            self.w_instability /= total
            self.w_affordability /= total


# ============================================================================
# Novelty detector
# ============================================================================

class NoveltyDetector:
    """Track existing samples and score novelty of new candidates."""

    def __init__(self):
        self.seen: List[Dict[str, float]] = []
        self.dim_ranges: Dict[str, Tuple[float, float]] = {}

    def add_sample(self, coords: Dict[str, float]):
        self.seen.append(dict(coords))
        for k, v in coords.items():
            if k not in self.dim_ranges:
                self.dim_ranges[k] = (v, v)
            else:
                lo, hi = self.dim_ranges[k]
                self.dim_ranges[k] = (min(lo, v), max(hi, v))

    def novelty_score(self, coords: Dict[str, float]) -> float:
        """0-100: how far is this point from all existing samples (normalized)."""
        if not self.seen:
            return 100.0

        # Normalize coordinates to [0,1] using observed ranges
        def normalize(k, v):
            if k not in self.dim_ranges:
                return 0.5
            lo, hi = self.dim_ranges[k]
            span = hi - lo
            if span < 1e-30:
                return 0.5
            return (v - lo) / span

        min_dist = float('inf')
        norm_coords = {k: normalize(k, v) for k, v in coords.items()}

        for existing in self.seen:
            norm_existing = {k: normalize(k, v) for k, v in existing.items()}
            dist_sq = 0.0
            keys = set(norm_coords) | set(norm_existing)
            for k in keys:
                a = norm_coords.get(k, 0.5)
                b = norm_existing.get(k, 0.5)
                dist_sq += (a - b)**2
            dist = math.sqrt(dist_sq / max(len(keys), 1))
            if dist < min_dist:
                min_dist = dist

        return min(100.0, min_dist * 200.0)


# ============================================================================
# Uncertainty estimator
# ============================================================================

class UncertaintyEstimator:
    """Estimate prediction uncertainty based on data density."""

    def __init__(self, kernel_radius: float = 0.1):
        self.kernel_radius = kernel_radius
        self.data_points: List[Dict[str, float]] = []

    def add_observation(self, coords: Dict[str, float]):
        self.data_points.append(dict(coords))

    def uncertainty_score(self, coords: Dict[str, float]) -> float:
        """0-100: high = sparse region, low = dense region."""
        if not self.data_points:
            return 100.0

        count_near = 0
        for pt in self.data_points:
            dist_sq = 0.0
            for k in coords:
                a = coords.get(k, 0)
                b = pt.get(k, 0)
                dist_sq += (a - b)**2
            if math.sqrt(dist_sq) < self.kernel_radius:
                count_near += 1

        # Fewer neighbors → higher uncertainty
        return max(0.0, min(100.0, 100.0 - count_near * 10.0))


# ============================================================================
# SmartSampler core engine
# ============================================================================

class SmartSampler:
    """Universal adaptive sampling engine for all pillars.

    Usage:
        sampler = SmartSampler(seed=42)
        sampler.set_domain({"T": (200, 800), "P": (1e4, 1e7)})
        sampler.set_objective(lambda coords: objective_fn(coords))
        sampler.set_plausibility(lambda coords: physics_check(coords))

        candidates = sampler.generate_candidates(n=100)
        ranked = sampler.rank_candidates(candidates)
        top = sampler.select_top(ranked, n=20)
    """

    def __init__(self, seed: int = 42, weights: Optional[SamplingWeights] = None):
        self.seed = seed
        self.rng = random.Random(seed)
        self.weights = weights or SamplingWeights()
        self.weights.normalize()

        self.domain: Dict[str, Tuple[float, float]] = {}
        self.novelty_detector = NoveltyDetector()
        self.uncertainty_estimator = UncertaintyEstimator()

        self.objective_fn: Optional[Callable] = None
        self.plausibility_fn: Optional[Callable] = None
        self.instability_fn: Optional[Callable] = None
        self.cost_fn: Optional[Callable] = None

        self.history: List[SampleScore] = []
        self.iteration: int = 0

    def set_domain(self, ranges: Dict[str, Tuple[float, float]]):
        self.domain = dict(ranges)

    def set_objective(self, fn: Callable[[Dict[str, float]], float]):
        """fn(coords) → 0-100 objective match score."""
        self.objective_fn = fn

    def set_plausibility(self, fn: Callable[[Dict[str, float]], float]):
        self.plausibility_fn = fn

    def set_instability(self, fn: Callable[[Dict[str, float]], float]):
        self.instability_fn = fn

    def set_cost(self, fn: Callable[[Dict[str, float]], float]):
        self.cost_fn = fn

    def _random_point(self) -> Dict[str, float]:
        coords = {}
        for dim, (lo, hi) in self.domain.items():
            coords[dim] = lo + self.rng.random() * (hi - lo)
        return coords

    def _latin_hypercube(self, n: int) -> List[Dict[str, float]]:
        """Latin Hypercube Sampling for better coverage."""
        dims = list(self.domain.keys())
        nd = len(dims)
        # Generate permuted interval indices
        intervals: Dict[str, List[int]] = {}
        for d in dims:
            perm = list(range(n))
            self.rng.shuffle(perm)
            intervals[d] = perm

        points = []
        for i in range(n):
            coords = {}
            for d in dims:
                lo, hi = self.domain[d]
                idx = intervals[d][i]
                u = (idx + self.rng.random()) / n
                coords[d] = lo + u * (hi - lo)
            points.append(coords)
        return points

    def generate_candidates(self, n: int = 100,
                            tier: SamplingTier = SamplingTier.STANDARD,
                            use_lhs: bool = True) -> List[SampleScore]:
        """Generate candidate sample points."""
        if use_lhs and len(self.domain) >= 1:
            points = self._latin_hypercube(n)
        else:
            points = [self._random_point() for _ in range(n)]

        candidates = []
        for i, coords in enumerate(points):
            sid = f"S{self.iteration:04d}-{i:04d}"

            nov = self.novelty_detector.novelty_score(coords)
            unc = self.uncertainty_estimator.uncertainty_score(coords)
            obj = self.objective_fn(coords) if self.objective_fn else 50.0
            pla = self.plausibility_fn(coords) if self.plausibility_fn else 80.0
            ins = self.instability_fn(coords) if self.instability_fn else 20.0
            aff = self.cost_fn(coords) if self.cost_fn else 90.0

            w = self.weights
            total = (w.w_novelty * nov + w.w_objective * obj
                     + w.w_uncertainty * unc + w.w_plausibility * pla
                     + w.w_instability * ins + w.w_affordability * aff)

            score = SampleScore(
                sample_id=sid, coordinates=coords,
                novelty=nov, objective_match=obj, uncertainty=unc,
                physical_plausibility=pla, instability_interest=ins,
                compute_affordability=aff, total=total, tier=tier)
            candidates.append(score)

        return candidates

    def rank_candidates(self, candidates: List[SampleScore]) -> List[SampleScore]:
        return sorted(candidates, key=lambda s: s.total, reverse=True)

    def select_top(self, ranked: List[SampleScore],
                   n: int = 20) -> List[SampleScore]:
        """Select top-n candidates and register them as sampled."""
        top = ranked[:n]
        for s in top:
            s.accepted = True
            self.novelty_detector.add_sample(s.coordinates)
            self.uncertainty_estimator.add_observation(s.coordinates)
            self.history.append(s)
        self.iteration += 1
        return top

    def adaptive_refine(self, focus_region: Dict[str, Tuple[float, float]],
                        n: int = 50) -> List[SampleScore]:
        """Refine sampling in a specific sub-region."""
        old_domain = dict(self.domain)
        self.set_domain(focus_region)
        candidates = self.generate_candidates(n, tier=SamplingTier.REFINED)
        self.set_domain(old_domain)
        return candidates

    def stochastic_stress_test(self, n: int = 30) -> List[SampleScore]:
        """Randomized stress test with jittered points at domain edges."""
        candidates = []
        for i in range(n):
            coords = {}
            for dim, (lo, hi) in self.domain.items():
                choice = self.rng.choice(["lo", "hi", "mid", "random"])
                if choice == "lo":
                    coords[dim] = lo + self.rng.gauss(0, (hi-lo)*0.02)
                elif choice == "hi":
                    coords[dim] = hi + self.rng.gauss(0, (hi-lo)*0.02)
                elif choice == "mid":
                    coords[dim] = (lo+hi)/2 + self.rng.gauss(0, (hi-lo)*0.05)
                else:
                    coords[dim] = lo + self.rng.random() * (hi - lo)
                coords[dim] = max(lo, min(hi, coords[dim]))

            sid = f"STRESS-{self.iteration:04d}-{i:03d}"
            nov = self.novelty_detector.novelty_score(coords)
            unc = self.uncertainty_estimator.uncertainty_score(coords)

            score = SampleScore(
                sample_id=sid, coordinates=coords,
                novelty=nov, uncertainty=unc,
                physical_plausibility=50.0,
                compute_affordability=100.0,
                total=0.5 * nov + 0.5 * unc,
                tier=SamplingTier.STRESS_TEST)
            candidates.append(score)

        return candidates


# ============================================================================
# Cross-pillar sampling adapters
# ============================================================================

def create_steam_sampler(material_formula: str,
                         seed: int = 42) -> SmartSampler:
    """Pre-configured sampler for steam-table thermophysical exploration."""
    from .steam_tables import get_material, antoine_P_sat

    mat = get_material(material_formula)
    sampler = SmartSampler(seed=seed)

    if mat:
        Tc = mat.critical.T_c
        Pc = mat.critical.P_c
        sampler.set_domain({
            "T": (Tc * 0.35, Tc * 1.3),
            "P": (Pc * 0.005, Pc * 1.5)
        })

        def phase_boundary_interest(coords):
            T, P = coords["T"], coords["P"]
            Psat = antoine_P_sat(mat, T)
            rel_diff = abs(P - Psat) / max(Psat, 1.0)
            if rel_diff < 0.02:
                return 95.0  # Very close to saturation
            elif rel_diff < 0.1:
                return 70.0
            elif rel_diff < 0.3:
                return 40.0
            return 20.0

        def critical_region_interest(coords):
            T, P = coords["T"], coords["P"]
            Tr = T / Tc
            Pr = P / Pc
            dist = math.sqrt((Tr - 1)**2 + (Pr - 1)**2)
            if dist < 0.05:
                return 100.0
            return max(0, 80 - dist * 200)

        sampler.set_objective(phase_boundary_interest)
        sampler.set_instability(critical_region_interest)
        sampler.set_plausibility(lambda c: 90.0 if c["T"] > 0 and c["P"] > 0 else 0.0)

    return sampler


def create_crystal_sampler(seed: int = 42) -> SmartSampler:
    """Pre-configured sampler for crystal discovery exploration."""
    sampler = SmartSampler(seed=seed)
    sampler.set_domain({
        "a_lattice": (2.0, 12.0),
        "c_over_a": (0.5, 3.0),
        "composition_x": (0.0, 1.0)
    })

    def structural_novelty(coords):
        # Prefer unusual c/a ratios
        ca = coords["c_over_a"]
        if 1.5 < ca < 1.7:  # HCP ideal
            return 30.0
        return min(100, abs(ca - 1.633) * 100)

    sampler.set_objective(structural_novelty)
    return sampler


def create_alloy_sampler(elements: List[str],
                         seed: int = 42) -> SmartSampler:
    """Pre-configured sampler for alloy composition exploration."""
    sampler = SmartSampler(seed=seed)
    domain = {}
    for elem in elements:
        domain[f"x_{elem}"] = (0.0, 1.0)
    sampler.set_domain(domain)

    def composition_valid(coords):
        total = sum(v for k, v in coords.items() if k.startswith("x_"))
        return max(0, 100 - abs(total - 1.0) * 1000)

    sampler.set_plausibility(composition_valid)
    return sampler


# ============================================================================
# Report generation
# ============================================================================

def generate_sampling_report_md(sampler: SmartSampler,
                                pillar_name: str = "General") -> str:
    lines = []
    lines.append(f"## SmartSampling Report: {pillar_name}\n")
    lines.append(f"**Seed**: {sampler.seed}")
    lines.append(f"**Iterations**: {sampler.iteration}")
    lines.append(f"**Total samples**: {len(sampler.history)}")
    lines.append(f"**Domain**: {sampler.domain}\n")

    w = sampler.weights
    lines.append("### Scoring Weights\n")
    lines.append("| Component | Weight |")
    lines.append("|-----------|--------|")
    lines.append(f"| Novelty | {w.w_novelty:.3f} |")
    lines.append(f"| Objective Match | {w.w_objective:.3f} |")
    lines.append(f"| Uncertainty | {w.w_uncertainty:.3f} |")
    lines.append(f"| Physical Plausibility | {w.w_plausibility:.3f} |")
    lines.append(f"| Instability Interest | {w.w_instability:.3f} |")
    lines.append(f"| Compute Affordability | {w.w_affordability:.3f} |")

    if sampler.history:
        lines.append("\n### Top Samples\n")
        lines.append("| ID | Total | Nov | Obj | Unc | Phys | Inst | Aff | Tier | Coords |")
        lines.append("|----|-------|-----|-----|-----|------|------|-----|------|--------|")
        top = sorted(sampler.history, key=lambda s: s.total, reverse=True)[:30]
        for s in top:
            coord_str = ", ".join(f"{k}={v:.2f}" for k, v in s.coordinates.items())
            lines.append(
                f"| {s.sample_id} | {s.total:.1f} "
                f"| {s.novelty:.1f} | {s.objective_match:.1f} "
                f"| {s.uncertainty:.1f} | {s.physical_plausibility:.1f} "
                f"| {s.instability_interest:.1f} | {s.compute_affordability:.1f} "
                f"| {s.tier.value} | {coord_str} |")

    lines.append("\n### Score Distribution\n")
    if sampler.history:
        scores = [s.total for s in sampler.history]
        lines.append(f"- Min: {min(scores):.1f}")
        lines.append(f"- Max: {max(scores):.1f}")
        lines.append(f"- Mean: {sum(scores)/len(scores):.1f}")
        lines.append(f"- Median: {sorted(scores)[len(scores)//2]:.1f}")

    return "\n".join(lines)
