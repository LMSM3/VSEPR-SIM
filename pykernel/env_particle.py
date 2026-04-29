"""
env_particle -- Environmental particle extension for plant-bead framework.

Dual-kind carrier model:
  P_k = (id, kind, r_k, v_k, E_k, I_k, omega_k, chi_k, tau_k)

Particle kinds:
  SUN  — radiative energy packets  (directional, intensity-weighted)
  WIND — advective momentum + transport disturbance packets

Subsystems:
  - Sun deposition:  dE = A_proj * I * T * S_photo * Phi_shade
  - Wind force:      dF = C_d * A_eff * |v_rel|^2 * n_hat * Psi_flex
  - Root chaos:      Gamma in [0.98, 1.8]
  - Leaf generation: Heaviside step gate + continuous expansion
  - Piecewise polynomial root response

Mirrors the C++ atomistic::environment system.
VSEPR-SIM 3.0.1
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional


# =====================================================================
# Vec3 (local lightweight copy — avoids circular import)
# =====================================================================

@dataclass
class Vec3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def __add__(self, o: Vec3) -> Vec3:
        return Vec3(self.x + o.x, self.y + o.y, self.z + o.z)

    def __sub__(self, o: Vec3) -> Vec3:
        return Vec3(self.x - o.x, self.y - o.y, self.z - o.z)

    def __mul__(self, s: float) -> Vec3:
        return Vec3(self.x * s, self.y * s, self.z * s)

    def dot(self, o: Vec3) -> float:
        return self.x * o.x + self.y * o.y + self.z * o.z

    def norm(self) -> float:
        return math.sqrt(self.dot(self))


# =====================================================================
# Particle Kind
# =====================================================================

class EnvParticleKind(IntEnum):
    Sun  = 0
    Wind = 1


_KIND_NAMES = {
    EnvParticleKind.Sun:  "SUN",
    EnvParticleKind.Wind: "WIND",
}


def kind_name(k: EnvParticleKind) -> str:
    return _KIND_NAMES.get(k, "UNKNOWN")


# =====================================================================
# Environmental Particle
# =====================================================================

@dataclass
class EnvParticle:
    id: int = 0
    kind: EnvParticleKind = EnvParticleKind.Sun
    position: Vec3 = field(default_factory=Vec3)
    velocity: Vec3 = field(default_factory=Vec3)
    energy: float = 0.0
    intensity: float = 1.0
    omega: float = 0.0
    chaos: float = 0.0
    lifetime: float = 1.0


# =====================================================================
# Bead Properties
# =====================================================================

@dataclass
class BeadProps:
    position: Vec3 = field(default_factory=Vec3)
    velocity: Vec3 = field(default_factory=Vec3)
    projected_area: float = 1.0
    transmissivity: float = 0.8
    photo_response: float = 1.0
    drag_coeff: float = 0.47
    flexibility: float = 1.0


# =====================================================================
# Shade Factor
# =====================================================================

def shade_factor(bead: BeadProps, p: EnvParticle,
                 occlusion: float = 0.0) -> float:
    return max(0.0, min(1.0, 1.0 - occlusion))


# =====================================================================
# Plant Environment Response
# =====================================================================

@dataclass
class PlantEnvResponse:
    dE_sun: float = 0.0
    dF_wind: Vec3 = field(default_factory=Vec3)
    drying_rate: float = 0.0
    photo_bias: float = 0.0
    stress_bias: float = 0.0

    def __add__(self, o: PlantEnvResponse) -> PlantEnvResponse:
        return PlantEnvResponse(
            dE_sun=self.dE_sun + o.dE_sun,
            dF_wind=self.dF_wind + o.dF_wind,
            drying_rate=self.drying_rate + o.drying_rate,
            photo_bias=self.photo_bias + o.photo_bias,
            stress_bias=self.stress_bias + o.stress_bias,
        )


# =====================================================================
# Sun Deposition Kernel
# =====================================================================

def sun_deposition(bead: BeadProps, p: EnvParticle,
                   occlusion: float = 0.0) -> float:
    """dE = A_proj * I * T * S_photo * Phi_shade."""
    return (bead.projected_area
            * p.intensity
            * bead.transmissivity
            * bead.photo_response
            * shade_factor(bead, p, occlusion))


# =====================================================================
# Wind Force Kernel
# =====================================================================

def wind_force(bead: BeadProps, p: EnvParticle) -> Vec3:
    """dF = C_d * A_eff * |v_rel|^2 * n_hat * Psi_flex."""
    v_rel = p.velocity - bead.velocity
    v2 = v_rel.dot(v_rel)
    v_mag = math.sqrt(v2)
    if v_mag < 1e-30:
        return Vec3()
    n_hat = v_rel * (1.0 / v_mag)
    F_mag = bead.drag_coeff * bead.projected_area * v2 * bead.flexibility
    return n_hat * F_mag


def drying_kernel(bead: BeadProps, p: EnvParticle,
                  drying_coeff: float = 0.05) -> float:
    """Drying rate proportional to relative velocity."""
    v_rel = p.velocity - bead.velocity
    return drying_coeff * v_rel.norm() * bead.projected_area


# =====================================================================
# Unified Interaction
# =====================================================================

def interact_env_particle(bead: BeadProps, p: EnvParticle,
                          occlusion: float = 0.0) -> PlantEnvResponse:
    out = PlantEnvResponse()

    if p.kind == EnvParticleKind.Sun:
        out.dE_sun = sun_deposition(bead, p, occlusion)
        out.photo_bias = out.dE_sun * 0.15

    if p.kind == EnvParticleKind.Wind:
        out.dF_wind = wind_force(bead, p)
        out.drying_rate = drying_kernel(bead, p)
        out.stress_bias = out.dF_wind.norm() * 0.08

    return out


# =====================================================================
# Root Chaos Factor
# =====================================================================

@dataclass
class RootLocalState:
    moisture: float = 0.5
    water_gradient: float = 0.0
    soil_density: float = 0.5
    chaos_perturb: float = 0.0
    damage: float = 0.0
    compaction: float = 0.0
    nutrient_grad: float = 0.0
    sun_coupling: float = 0.5


@dataclass
class RootChaosCoeffs:
    alpha0: float = 1.0
    alpha1: float = 0.3
    alpha2: float = 0.15
    alpha3: float = -0.2
    alpha4: float = 0.1
    lo: float = 0.98
    hi: float = 1.80


def root_chaos_factor(s: RootLocalState,
                      c: Optional[RootChaosCoeffs] = None) -> float:
    """Gamma_i = clamp(a0 + a1*M + a2*nablaW + a3*rho + a4*chi, lo, hi)."""
    if c is None:
        c = RootChaosCoeffs()
    raw = (c.alpha0
           + c.alpha1 * s.moisture
           + c.alpha2 * s.water_gradient
           + c.alpha3 * s.soil_density
           + c.alpha4 * s.chaos_perturb)
    return max(c.lo, min(c.hi, raw))


# =====================================================================
# Piecewise Polynomial Root Response
# =====================================================================

@dataclass
class PolySegment:
    x_lo: float = 0.0
    x_hi: float = 1.0
    a: float = 0.0   # x^3
    b: float = 0.0   # x^2
    c: float = 0.0   # x^1
    d: float = 0.0   # x^0

    def eval(self, x: float) -> float:
        return self.a * x**3 + self.b * x**2 + self.c * x + self.d


@dataclass
class PiecewiseRootPoly:
    dry: PolySegment = field(default_factory=PolySegment)
    optimal: PolySegment = field(default_factory=PolySegment)
    saturated: PolySegment = field(default_factory=PolySegment)
    x_dry: float = 0.2
    x_opt: float = 0.7

    def eval(self, x: float) -> float:
        if x < self.x_dry:
            return self.dry.eval(x)
        if x < self.x_opt:
            return self.optimal.eval(x)
        return self.saturated.eval(x)


def default_root_poly() -> PiecewiseRootPoly:
    p = PiecewiseRootPoly()
    p.x_dry = 0.2
    p.x_opt = 0.7
    p.dry = PolySegment(0.0, 0.2, 0.0, 2.0, 0.1, 0.05)
    p.optimal = PolySegment(0.2, 0.7, -0.5, 1.2, -0.3, 0.8)
    p.saturated = PolySegment(0.7, 1.0, 0.0, -1.5, 2.0, 0.3)
    return p


@dataclass
class RootGrowthLimits:
    damage_limit: float = 0.5
    damage_mult: float = 0.55
    compact_limit: float = 0.6
    compact_mult: float = 0.72
    nutrient_trig: float = 0.3
    nutrient_mult: float = 1.18
    low_sun_thresh: float = 0.2
    low_sun_mult: float = 0.91
    final_lo: float = 0.05
    final_hi: float = 2.25


def root_growth_modifier(s: RootLocalState,
                         poly: Optional[PiecewiseRootPoly] = None,
                         cc: Optional[RootChaosCoeffs] = None,
                         lim: Optional[RootGrowthLimits] = None) -> float:
    """Full root growth: polynomial * logic gates * chaos factor."""
    if poly is None:
        poly = default_root_poly()
    if cc is None:
        cc = RootChaosCoeffs()
    if lim is None:
        lim = RootGrowthLimits()

    p = poly.eval(s.moisture)

    if s.damage > lim.damage_limit:
        p *= lim.damage_mult
    if s.compaction > lim.compact_limit:
        p *= lim.compact_mult
    if s.nutrient_grad > lim.nutrient_trig:
        p *= lim.nutrient_mult
    if s.sun_coupling < lim.low_sun_thresh:
        p *= lim.low_sun_mult

    gamma = root_chaos_factor(s, cc)
    return max(lim.final_lo, min(lim.final_hi, p * gamma))


# =====================================================================
# Leaf Generation Gate
# =====================================================================

@dataclass
class LeafGateCoeffs:
    beta1: float = 1.0
    beta2: float = 0.8
    beta3: float = 0.6
    beta4: float = 1.5
    theta: float = 2.0


@dataclass
class LeafLocalState:
    energy_local: float = 0.0
    sunlight: float = 0.0
    hydration: float = 0.0
    damage_sum: float = 0.0


def leaf_gate_signal(s: LeafLocalState,
                     c: Optional[LeafGateCoeffs] = None) -> float:
    if c is None:
        c = LeafGateCoeffs()
    return (c.beta1 * s.energy_local
            + c.beta2 * s.sunlight
            + c.beta3 * s.hydration
            - c.beta4 * s.damage_sum
            - c.theta)


def leaf_generation_gate(s: LeafLocalState,
                         c: Optional[LeafGateCoeffs] = None) -> bool:
    """H(signal) — Heaviside step function for leaf initiation."""
    return leaf_gate_signal(s, c) >= 0.0


def leaf_expansion_rate(s: LeafLocalState,
                        c: Optional[LeafGateCoeffs] = None,
                        base_rate: float = 0.1) -> float:
    """Post-initiation continuous growth."""
    if c is None:
        c = LeafGateCoeffs()
    if not leaf_generation_gate(s, c):
        return 0.0
    resource = max(0.0, s.energy_local + s.sunlight + s.hydration - s.damage_sum)
    return base_rate * resource


# =====================================================================
# Energy Decomposition
# =====================================================================

@dataclass
class EnvEnergyTerms:
    U_wind: float = 0.0
    U_sun: float = 0.0
    U_dry: float = 0.0
    U_photo: float = 0.0
    U_stress: float = 0.0

    def total(self) -> float:
        return self.U_wind + self.U_sun + self.U_dry + self.U_photo + self.U_stress


def accumulate_env(responses: list[PlantEnvResponse],
                   beads: list[BeadProps]) -> EnvEnergyTerms:
    E = EnvEnergyTerms()
    for r, b in zip(responses, beads):
        E.U_sun += r.dE_sun
        E.U_wind += -r.dF_wind.dot(b.position)
        E.U_dry += r.drying_rate
        E.U_photo += r.photo_bias
        E.U_stress += r.stress_bias
    return E


# =====================================================================
# Particle Lifetime
# =====================================================================

def advance_particle(p: EnvParticle, dt: float) -> bool:
    """Advect position, decay lifetime.  Returns False if dead."""
    p.position = p.position + p.velocity * dt
    p.lifetime -= dt

    if abs(p.omega) > 1e-30:
        p.intensity *= (1.0 + 0.1 * math.sin(p.omega * dt))
        p.intensity = max(0.0, p.intensity)

    return p.lifetime > 0.0
