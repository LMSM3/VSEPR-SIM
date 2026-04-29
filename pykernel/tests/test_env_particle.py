"""Tests for pykernel.env_particle — Environmental Particle Extension.

Mirrors C++ test_env_particle.cpp structure.
VSEPR-SIM 3.0.1
"""

import math
import pytest

from pykernel.env_particle import (
    Vec3, EnvParticleKind, EnvParticle, BeadProps,
    PlantEnvResponse, shade_factor, kind_name,
    sun_deposition, wind_force, drying_kernel,
    interact_env_particle,
    RootLocalState, RootChaosCoeffs, root_chaos_factor,
    PolySegment, PiecewiseRootPoly, default_root_poly,
    RootGrowthLimits, root_growth_modifier,
    LeafGateCoeffs, LeafLocalState,
    leaf_generation_gate, leaf_gate_signal, leaf_expansion_rate,
    EnvEnergyTerms, accumulate_env, advance_particle,
)


# =====================================================================
# Particle construction
# =====================================================================

class TestConstruction:
    def test_default_kind(self):
        p = EnvParticle()
        assert p.kind == EnvParticleKind.Sun

    def test_wind_kind(self):
        p = EnvParticle(kind=EnvParticleKind.Wind)
        assert p.kind == EnvParticleKind.Wind

    def test_kind_name_sun(self):
        assert kind_name(EnvParticleKind.Sun) == "SUN"

    def test_kind_name_wind(self):
        assert kind_name(EnvParticleKind.Wind) == "WIND"

    def test_fields(self):
        p = EnvParticle(intensity=2.5, energy=100, lifetime=5)
        assert p.intensity == 2.5
        assert p.energy == 100
        assert p.lifetime == 5


# =====================================================================
# Sun deposition
# =====================================================================

class TestSunDeposition:
    def test_basic(self):
        bead = BeadProps(projected_area=2.0, transmissivity=0.5, photo_response=1.0)
        sun = EnvParticle(kind=EnvParticleKind.Sun, intensity=3.0)
        assert sun_deposition(bead, sun) == pytest.approx(3.0)

    def test_half_shade(self):
        bead = BeadProps(projected_area=2.0, transmissivity=0.5, photo_response=1.0)
        sun = EnvParticle(kind=EnvParticleKind.Sun, intensity=3.0)
        assert sun_deposition(bead, sun, 0.5) == pytest.approx(1.5)

    def test_full_occlusion(self):
        bead = BeadProps(projected_area=2.0, transmissivity=0.5, photo_response=1.0)
        sun = EnvParticle(kind=EnvParticleKind.Sun, intensity=3.0)
        assert sun_deposition(bead, sun, 1.0) == pytest.approx(0.0)

    def test_zero_area(self):
        bead = BeadProps(projected_area=0.0, transmissivity=0.5)
        sun = EnvParticle(kind=EnvParticleKind.Sun, intensity=3.0)
        assert sun_deposition(bead, sun) == pytest.approx(0.0)

    def test_high_photo(self):
        bead = BeadProps(projected_area=1.0, transmissivity=0.5, photo_response=3.0)
        sun = EnvParticle(kind=EnvParticleKind.Sun, intensity=3.0)
        assert sun_deposition(bead, sun) == pytest.approx(4.5)


# =====================================================================
# Wind force
# =====================================================================

class TestWindForce:
    def test_x_direction(self):
        bead = BeadProps(drag_coeff=0.5, projected_area=1.0, flexibility=1.0)
        wind = EnvParticle(kind=EnvParticleKind.Wind, velocity=Vec3(10, 0, 0))
        F = wind_force(bead, wind)
        assert F.x == pytest.approx(50.0)
        assert F.y == pytest.approx(0.0)

    def test_diagonal(self):
        bead = BeadProps(drag_coeff=0.5, projected_area=1.0, flexibility=1.0)
        wind = EnvParticle(velocity=Vec3(3, 4, 0))
        F = wind_force(bead, wind)
        assert F.norm() == pytest.approx(12.5)

    def test_direction_matches(self):
        bead = BeadProps(drag_coeff=0.5, projected_area=1.0, flexibility=1.0)
        wind = EnvParticle(velocity=Vec3(3, 4, 0))
        F = wind_force(bead, wind)
        Fn = F.norm()
        assert F.x / Fn == pytest.approx(0.6, abs=0.01)
        assert F.y / Fn == pytest.approx(0.8, abs=0.01)

    def test_zero_relative(self):
        bead = BeadProps(velocity=Vec3(10, 0, 0))
        wind = EnvParticle(velocity=Vec3(10, 0, 0))
        F = wind_force(bead, wind)
        assert F.norm() == pytest.approx(0.0)

    def test_flexibility(self):
        bead = BeadProps(drag_coeff=0.5, projected_area=1.0, flexibility=0.5)
        wind = EnvParticle(velocity=Vec3(10, 0, 0))
        F = wind_force(bead, wind)
        assert F.x == pytest.approx(25.0)


# =====================================================================
# Drying kernel
# =====================================================================

class TestDrying:
    def test_basic(self):
        bead = BeadProps(projected_area=2.0)
        wind = EnvParticle(velocity=Vec3(5, 0, 0))
        assert drying_kernel(bead, wind, 0.05) == pytest.approx(0.5)

    def test_zero_wind(self):
        bead = BeadProps(projected_area=2.0)
        wind = EnvParticle()
        assert drying_kernel(bead, wind, 0.05) == pytest.approx(0.0)

    def test_custom_coeff(self):
        bead = BeadProps(projected_area=2.0)
        wind = EnvParticle(velocity=Vec3(10, 0, 0))
        assert drying_kernel(bead, wind, 0.1) == pytest.approx(2.0)


# =====================================================================
# Unified interaction
# =====================================================================

class TestUnified:
    def test_sun_deposits(self):
        bead = BeadProps(projected_area=1.5, transmissivity=0.9)
        sun = EnvParticle(kind=EnvParticleKind.Sun, intensity=2.0)
        r = interact_env_particle(bead, sun)
        assert r.dE_sun > 0
        assert r.photo_bias == pytest.approx(r.dE_sun * 0.15)
        assert r.dF_wind.norm() == pytest.approx(0.0)

    def test_wind_forces(self):
        bead = BeadProps(projected_area=1.5, drag_coeff=0.47)
        wind = EnvParticle(kind=EnvParticleKind.Wind, velocity=Vec3(8, 0, 0))
        r = interact_env_particle(bead, wind)
        assert r.dF_wind.x > 0
        assert r.drying_rate > 0
        assert r.stress_bias > 0
        assert r.dE_sun == pytest.approx(0.0)

    def test_response_addition(self):
        a = PlantEnvResponse(dE_sun=1, dF_wind=Vec3(2, 0, 0), drying_rate=0.1)
        b = PlantEnvResponse(dE_sun=3, dF_wind=Vec3(0, 1, 0), stress_bias=0.5)
        c = a + b
        assert c.dE_sun == pytest.approx(4.0)
        assert c.dF_wind.x == pytest.approx(2.0)
        assert c.dF_wind.y == pytest.approx(1.0)


# =====================================================================
# Root chaos factor
# =====================================================================

class TestRootChaos:
    def test_default(self):
        s = RootLocalState()
        assert root_chaos_factor(s) == pytest.approx(1.05)

    def test_high_moisture(self):
        s = RootLocalState(moisture=1.0)
        assert root_chaos_factor(s) == pytest.approx(1.2)

    def test_dense_soil_clamp(self):
        s = RootLocalState(soil_density=1.0)
        assert root_chaos_factor(s) == pytest.approx(0.98)

    def test_extreme_chaos_clamp(self):
        s = RootLocalState(soil_density=0.0, chaos_perturb=50.0)
        assert root_chaos_factor(s) == pytest.approx(1.8)

    def test_custom_bounds(self):
        cc = RootChaosCoeffs(alpha0=2.5, lo=0.5, hi=3.0)
        s = RootLocalState()
        g = root_chaos_factor(s, cc)
        assert 0.5 <= g <= 3.0


# =====================================================================
# Piecewise polynomial
# =====================================================================

class TestPiecewisePoly:
    def test_dry_zone(self):
        p = default_root_poly()
        assert p.eval(0.1) == pytest.approx(0.08)

    def test_intercept(self):
        p = default_root_poly()
        assert p.eval(0.0) == pytest.approx(0.05)

    def test_optimal(self):
        p = default_root_poly()
        assert p.eval(0.5) == pytest.approx(0.8875)

    def test_saturated(self):
        p = default_root_poly()
        assert p.eval(0.9) == pytest.approx(0.885)

    def test_optimal_beats_dry(self):
        p = default_root_poly()
        assert p.eval(0.5) > p.eval(0.1)


# =====================================================================
# Root growth modifier
# =====================================================================

class TestRootGrowth:
    def test_basic_bounds(self):
        s = RootLocalState(moisture=0.5)
        g = root_growth_modifier(s)
        assert 0.05 <= g <= 2.25

    def test_damage_reduces(self):
        s = RootLocalState(moisture=0.5)
        g0 = root_growth_modifier(s)
        s.damage = 0.8
        g1 = root_growth_modifier(s)
        assert g1 < g0

    def test_compaction_reduces(self):
        s = RootLocalState(moisture=0.5)
        g0 = root_growth_modifier(s)
        s.compaction = 0.8
        g1 = root_growth_modifier(s)
        assert g1 < g0

    def test_nutrient_boost(self):
        s = RootLocalState(moisture=0.5)
        g0 = root_growth_modifier(s)
        s.nutrient_grad = 0.5
        g1 = root_growth_modifier(s)
        assert g1 > g0

    def test_low_sun_reduces(self):
        s = RootLocalState(moisture=0.5)
        g0 = root_growth_modifier(s)
        s.sun_coupling = 0.1
        g1 = root_growth_modifier(s)
        assert g1 < g0


# =====================================================================
# Leaf generation gate
# =====================================================================

class TestLeafGate:
    def test_no_resources(self):
        assert not leaf_generation_gate(LeafLocalState())

    def test_sufficient(self):
        s = LeafLocalState(energy_local=2.0, sunlight=1.5, hydration=1.0)
        assert leaf_generation_gate(s)

    def test_damage_suppresses(self):
        s = LeafLocalState(energy_local=2.0, sunlight=1.5, hydration=1.0,
                           damage_sum=3.0)
        assert not leaf_generation_gate(s)

    def test_signal_positive(self):
        s = LeafLocalState(energy_local=2.0, sunlight=1.5, hydration=1.0)
        assert leaf_gate_signal(s) > 0

    def test_expansion_positive(self):
        s = LeafLocalState(energy_local=2.0, sunlight=1.5, hydration=1.0)
        assert leaf_expansion_rate(s) > 0

    def test_expansion_zero_below(self):
        s = LeafLocalState()
        assert leaf_expansion_rate(s) == pytest.approx(0.0)


# =====================================================================
# Energy decomposition
# =====================================================================

class TestEnergy:
    def test_accumulate(self):
        bead = BeadProps(position=Vec3(1, 0, 0))
        r = PlantEnvResponse(dE_sun=5, dF_wind=Vec3(2, 0, 0),
                             drying_rate=0.3, photo_bias=0.75, stress_bias=0.16)
        E = accumulate_env([r], [bead])
        assert E.U_sun == pytest.approx(5.0)
        assert E.U_wind < 0
        assert E.U_dry == pytest.approx(0.3)
        assert E.total() != 0


# =====================================================================
# Particle advection
# =====================================================================

class TestAdvection:
    def test_alive(self):
        p = EnvParticle(lifetime=2.0, velocity=Vec3(1, 0, 0))
        assert advance_particle(p, 0.5) is True
        assert p.position.x == pytest.approx(0.5)
        assert p.lifetime == pytest.approx(1.5)

    def test_dead(self):
        p = EnvParticle(lifetime=0.5, velocity=Vec3(1, 0, 0))
        assert advance_particle(p, 1.0) is False


# =====================================================================
# Full pipeline
# =====================================================================

class TestPipeline:
    def test_full(self):
        sun = EnvParticle(kind=EnvParticleKind.Sun, intensity=2.0,
                          velocity=Vec3(0, -1, 0))
        wind = EnvParticle(kind=EnvParticleKind.Wind,
                           velocity=Vec3(5, 0, 0))

        bead = BeadProps(projected_area=1.0, transmissivity=0.7,
                         photo_response=1.2, drag_coeff=0.5,
                         flexibility=0.9)

        r_sun = interact_env_particle(bead, sun)
        r_wind = interact_env_particle(bead, wind)
        combined = r_sun + r_wind

        rs = RootLocalState(moisture=0.6, sun_coupling=r_sun.dE_sun)
        gamma = root_chaos_factor(rs)
        growth = root_growth_modifier(rs)

        ls = LeafLocalState(energy_local=r_sun.dE_sun,
                            sunlight=sun.intensity,
                            hydration=rs.moisture)
        leaf = leaf_generation_gate(ls)

        assert combined.dE_sun > 0
        assert combined.dF_wind.norm() > 0
        assert 0.98 <= gamma <= 1.8
        assert growth > 0


# =====================================================================
# Parametrized sweep: chaos factor across soil densities
# =====================================================================

_DENSITIES = [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0]


class TestChaosSweep:
    @pytest.mark.parametrize("rho", _DENSITIES)
    def test_bounded(self, rho: float):
        s = RootLocalState(soil_density=rho)
        g = root_chaos_factor(s)
        assert 0.98 <= g <= 1.80

    def test_monotonic_decrease(self):
        """Higher soil density -> lower or equal gamma."""
        vals = [root_chaos_factor(RootLocalState(soil_density=r))
                for r in _DENSITIES]
        for i in range(1, len(vals)):
            assert vals[i] <= vals[i - 1] + 1e-9
