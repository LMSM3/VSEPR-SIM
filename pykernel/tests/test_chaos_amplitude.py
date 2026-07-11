"""
Tests for chaos_amplitude — Chaos Factor, Amplitude Scale, Scale Weights.

VSEPR-SIM 3.0.0
"""

import math
import pytest

from pykernel.chaos_amplitude import (
    ScaleWeights,
    AmplitudeBin,
    AmplitudeScale,
    ChaosResult,
    ChaosSweepSummary,
    SensitivityResult,
    build_amplitude_scale,
    score_displacement,
    score_force,
    score_thermal,
    score_anisotropy,
    compute_chaos,
    summarise_chaos,
    weight_sensitivity,
    D_MAX_FRAC,
    A_MIN,
    A_MAX,
    N_BINS,
    _clamp01,
)


# =====================================================================
# ScaleWeights
# =====================================================================


class TestScaleWeights:
    def test_default_sum(self):
        w = ScaleWeights()
        assert abs(w.w_disp + w.w_force + w.w_therm + w.w_aniso - 1.0) < 1e-12

    def test_default_values(self):
        w = ScaleWeights()
        assert w.w_disp == 0.35
        assert w.w_force == 0.25
        assert w.w_therm == 0.25
        assert w.w_aniso == 0.15

    def test_bad_sum_raises(self):
        with pytest.raises(ValueError, match="must sum to 1.0"):
            ScaleWeights(0.5, 0.5, 0.5, 0.5)

    def test_zero_sum_raises(self):
        with pytest.raises(ValueError):
            ScaleWeights(0.0, 0.0, 0.0, 0.0)

    def test_uniform(self):
        w = ScaleWeights.uniform()
        assert w.w_disp == 0.25
        assert w.w_force == 0.25
        assert w.w_therm == 0.25
        assert w.w_aniso == 0.25

    def test_displacement_heavy(self):
        w = ScaleWeights.displacement_heavy()
        assert w.w_disp > w.w_force
        assert w.w_disp > w.w_therm
        assert w.w_disp > w.w_aniso
        assert abs(w.w_disp + w.w_force + w.w_therm + w.w_aniso - 1.0) < 1e-12

    def test_thermal_heavy(self):
        w = ScaleWeights.thermal_heavy()
        assert w.w_therm > w.w_disp
        assert abs(sum(w.to_dict().values()) - 1.0) < 1e-12

    def test_force_heavy(self):
        w = ScaleWeights.force_heavy()
        assert w.w_force > w.w_disp
        assert abs(sum(w.to_dict().values()) - 1.0) < 1e-12

    def test_to_dict(self):
        w = ScaleWeights()
        d = w.to_dict()
        assert set(d.keys()) == {"w_disp", "w_force", "w_therm", "w_aniso"}

    def test_from_dict_roundtrip(self):
        w = ScaleWeights(0.4, 0.3, 0.2, 0.1)
        d = w.to_dict()
        w2 = ScaleWeights.from_dict(d)
        assert w2 == w

    def test_frozen(self):
        w = ScaleWeights()
        with pytest.raises(AttributeError):
            w.w_disp = 0.5


# =====================================================================
# Amplitude Scale
# =====================================================================


class TestBuildAmplitudeScale:
    def test_empty(self):
        amp = build_amplitude_scale([])
        assert amp.n_atoms == 0
        assert amp.bins == []

    def test_single_atom(self):
        amp = build_amplitude_scale([1.0])
        assert amp.n_atoms == 1
        assert amp.F_min == 1.0
        assert amp.F_max == 1.0
        total_count = sum(b.count for b in amp.bins)
        assert total_count == 1

    def test_bin_count(self):
        amp = build_amplitude_scale([0.1, 1.0, 10.0], n_bins=10)
        assert len(amp.bins) == 10

    def test_fractions_sum_to_one(self):
        mags = [0.01 * i for i in range(1, 101)]
        amp = build_amplitude_scale(mags)
        total_frac = sum(b.fraction for b in amp.bins)
        assert abs(total_frac - 1.0) < 1e-10

    def test_all_counts_accounted(self):
        mags = [0.5, 1.5, 2.5, 5.0, 10.0]
        amp = build_amplitude_scale(mags)
        total_count = sum(b.count for b in amp.bins)
        assert total_count == 5

    def test_statistics(self):
        mags = [1.0, 2.0, 3.0, 4.0, 5.0]
        amp = build_amplitude_scale(mags)
        assert amp.F_min == 1.0
        assert amp.F_max == 5.0
        assert abs(amp.F_mean - 3.0) < 1e-10
        expected_rms = math.sqrt((1 + 4 + 9 + 16 + 25) / 5)
        assert abs(amp.F_rms - expected_rms) < 1e-10

    def test_std(self):
        mags = [2.0, 2.0, 2.0]
        amp = build_amplitude_scale(mags)
        assert abs(amp.F_std) < 1e-12

    def test_spectral_entropy_uniform(self):
        # Uniform distribution across bins -> high entropy
        mags = [10 ** (i * 0.2) * A_MIN for i in range(N_BINS)]
        amp = build_amplitude_scale(mags)
        assert amp.spectral_entropy > 0

    def test_spectral_entropy_single_bin(self):
        # All atoms in one magnitude -> low entropy
        mags = [1.0] * 100
        amp = build_amplitude_scale(mags)
        # Entropy should be 0 (all in one bin)
        assert amp.spectral_entropy < 1e-10

    def test_peak_bin(self):
        mags = [1.0] * 50 + [10.0] * 5
        amp = build_amplitude_scale(mags)
        # Peak bin should be the one containing magnitude 1.0
        pb = amp.bins[amp.peak_bin]
        assert pb.count >= 50

    def test_to_dict(self):
        amp = build_amplitude_scale([1.0, 2.0, 3.0])
        d = amp.to_dict()
        assert "bins" in d
        assert "F_min" in d
        assert "spectral_entropy" in d
        assert len(d["bins"]) == N_BINS

    def test_custom_bounds(self):
        amp = build_amplitude_scale([0.5, 5.0], n_bins=5, a_min=0.1, a_max=10.0)
        assert len(amp.bins) == 5


# =====================================================================
# Sub-Scores
# =====================================================================


class TestScoreDisplacement:
    def test_zero_displacement(self):
        pos = [(0.0, 0.0, 0.0), (1.0, 1.0, 1.0)]
        ideal = [(0.0, 0.0, 0.0), (1.0, 1.0, 1.0)]
        assert score_displacement(pos, ideal, 3.0) == 0.0

    def test_max_displacement(self):
        a = 4.0
        # displace all atoms by a * D_MAX_FRAC in x
        d = a * D_MAX_FRAC
        pos = [(d, 0.0, 0.0), (1.0 + d, 0.0, 0.0)]
        ideal = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0)]
        s = score_displacement(pos, ideal, a)
        assert abs(s - 1.0) < 1e-6

    def test_half_displacement(self):
        a = 4.0
        d = a * D_MAX_FRAC * 0.5
        pos = [(d, 0.0, 0.0)]
        ideal = [(0.0, 0.0, 0.0)]
        s = score_displacement(pos, ideal, a)
        assert abs(s - 0.5) < 1e-6

    def test_beyond_max_clamped(self):
        a = 4.0
        d = a * 2.0  # way past D_MAX_FRAC
        pos = [(d, 0.0, 0.0)]
        ideal = [(0.0, 0.0, 0.0)]
        s = score_displacement(pos, ideal, a)
        assert s == 1.0

    def test_empty(self):
        assert score_displacement([], [], 3.0) == 0.0

    def test_zero_lattice(self):
        assert score_displacement([(1, 0, 0)], [(0, 0, 0)], 0.0) == 0.0


class TestScoreForce:
    def test_uniform_forces(self):
        mags = [1.0, 1.0, 1.0]
        assert score_force(mags) == 0.0

    def test_zero_forces(self):
        assert score_force([0.0, 0.0]) == 0.0

    def test_one_zero(self):
        mags = [0.0, 1.0]
        assert score_force(mags) == 1.0

    def test_partial(self):
        mags = [0.5, 1.0]
        assert abs(score_force(mags) - 0.5) < 1e-10

    def test_empty(self):
        assert score_force([]) == 0.0


class TestScoreThermal:
    def test_at_reference(self):
        assert score_thermal(298.0, 298.0, 1000.0) == 0.0

    def test_at_melting(self):
        assert abs(score_thermal(1000.0, 298.0, 1000.0) - 1.0) < 1e-10

    def test_midpoint(self):
        T_ref = 300.0
        T_melt = 1300.0
        T = 800.0
        expected = (800 - 300) / (1300 - 300)
        assert abs(score_thermal(T, T_ref, T_melt) - expected) < 1e-10

    def test_below_reference(self):
        assert score_thermal(200.0, 298.0, 1000.0) == 0.0

    def test_above_melting(self):
        assert score_thermal(2000.0, 298.0, 1000.0) == 1.0

    def test_degenerate_melt(self):
        # T_melt == T_ref
        assert score_thermal(500.0, 298.0, 298.0) == 1.0
        assert score_thermal(200.0, 298.0, 298.0) == 0.0


class TestScoreAnisotropy:
    def test_cubic(self):
        assert score_anisotropy(4.0, 4.0, 4.0) == 0.0

    def test_orthorhombic(self):
        s = score_anisotropy(3.0, 4.0, 5.0)
        assert 0.0 < s < 1.0

    def test_extreme(self):
        s = score_anisotropy(1.0, 1.0, 100.0)
        assert s > 0.5

    def test_zero_lattice(self):
        assert score_anisotropy(0.0, 0.0, 0.0) == 0.0


# =====================================================================
# Composite Chaos Factor
# =====================================================================


class TestComputeChaos:
    @pytest.fixture
    def perfect_cubic(self):
        """Perfect FCC-like positions with no displacement."""
        pos = [(0, 0, 0), (2, 2, 0), (2, 0, 2), (0, 2, 2)]
        return pos, pos  # positions == ideal

    def test_perfect_crystal_low_chaos(self, perfect_cubic):
        pos, ideal = perfect_cubic
        fmags = [1.0] * 4
        result = compute_chaos(
            pos, ideal, fmags,
            lattice_a=4.0, lattice_b=4.0, lattice_c=4.0,
            T=298.0, T_melt=1500.0,
        )
        assert result.chi < 0.05
        assert result.grade == "STABLE"

    def test_high_temperature_increases_chaos(self, perfect_cubic):
        pos, ideal = perfect_cubic
        fmags = [1.0] * 4
        r_low = compute_chaos(pos, ideal, fmags, 4, 4, 4, T=300.0, T_melt=1500.0)
        r_high = compute_chaos(pos, ideal, fmags, 4, 4, 4, T=1400.0, T_melt=1500.0)
        assert r_high.chi > r_low.chi

    def test_displacement_increases_chaos(self):
        ideal = [(0, 0, 0), (1, 0, 0)]
        displaced = [(0.5, 0.3, 0.2), (1.4, 0.2, 0.1)]
        fmags = [1.0, 1.0]
        r = compute_chaos(displaced, ideal, fmags, 4, 4, 4, T=300.0, T_melt=1500.0)
        r0 = compute_chaos(ideal, ideal, fmags, 4, 4, 4, T=300.0, T_melt=1500.0)
        assert r.chi > r0.chi

    def test_force_nonuniformity_increases_chaos(self):
        pos = [(0, 0, 0), (1, 0, 0)]
        fmags_uniform = [1.0, 1.0]
        fmags_varied = [0.0, 5.0]
        r_u = compute_chaos(pos, pos, fmags_uniform, 4, 4, 4, T=300.0, T_melt=1500.0)
        r_v = compute_chaos(pos, pos, fmags_varied, 4, 4, 4, T=300.0, T_melt=1500.0)
        assert r_v.chi > r_u.chi

    def test_anisotropy_increases_chaos(self):
        pos = [(0, 0, 0)]
        fmags = [1.0]
        r_cubic = compute_chaos(pos, pos, fmags, 4, 4, 4, T=300.0, T_melt=1500.0)
        r_aniso = compute_chaos(pos, pos, fmags, 4, 4, 10, T=300.0, T_melt=1500.0)
        assert r_aniso.chi > r_cubic.chi

    def test_custom_weights(self, perfect_cubic):
        pos, ideal = perfect_cubic
        fmags = [0.0, 5.0, 0.0, 5.0]
        w_force = ScaleWeights.force_heavy()
        w_therm = ScaleWeights.thermal_heavy()
        r_f = compute_chaos(pos, ideal, fmags, 4, 4, 4, T=300.0, T_melt=1500.0, weights=w_force)
        r_t = compute_chaos(pos, ideal, fmags, 4, 4, 4, T=300.0, T_melt=1500.0, weights=w_therm)
        # Force-heavy weights should amplify force contribution
        assert r_f.contributions()["force"] > r_t.contributions()["force"]

    def test_result_fields(self, perfect_cubic):
        pos, ideal = perfect_cubic
        fmags = [1.0, 2.0, 3.0, 4.0]
        r = compute_chaos(pos, ideal, fmags, 4, 4, 4, T=500.0, T_melt=1500.0, label="Fe")
        assert r.label == "Fe"
        assert 0.0 <= r.chi <= 1.0
        assert 0.0 <= r.S_disp <= 1.0
        assert 0.0 <= r.S_force <= 1.0
        assert 0.0 <= r.S_therm <= 1.0
        assert 0.0 <= r.S_aniso <= 1.0
        assert isinstance(r.amplitude, AmplitudeScale)
        assert isinstance(r.weights, ScaleWeights)

    def test_contributions_sum_to_chi(self, perfect_cubic):
        pos, ideal = perfect_cubic
        fmags = [0.5, 1.0, 2.0, 3.0]
        r = compute_chaos(pos, ideal, fmags, 4, 5, 4, T=600.0, T_melt=1500.0)
        total = sum(r.contributions().values())
        assert abs(total - r.chi) < 1e-12

    def test_dominant_channel(self, perfect_cubic):
        pos, ideal = perfect_cubic
        fmags = [1.0] * 4
        # High T near melting -> thermal should dominate
        r = compute_chaos(pos, ideal, fmags, 4, 4, 4, T=1400.0, T_melt=1500.0)
        assert r.dominant_channel() == "therm"

    def test_to_dict(self, perfect_cubic):
        pos, ideal = perfect_cubic
        r = compute_chaos(pos, ideal, [1.0] * 4, 4, 4, 4, T=400.0, T_melt=1500.0, label="Cu")
        d = r.to_dict()
        assert "chi" in d
        assert "grade" in d
        assert "dominant" in d
        assert "contributions" in d
        assert "amplitude" in d
        assert "weights" in d

    def test_grade_boundaries(self):
        pos = [(0, 0, 0)]
        ideal = [(0, 0, 0)]
        fmags = [1.0]
        # chi ~ 0 -> STABLE
        r = compute_chaos(pos, ideal, fmags, 4, 4, 4, T=298.0, T_melt=10000.0)
        assert r.grade == "STABLE"


# =====================================================================
# Sweep Summary
# =====================================================================


class TestSummariseChaos:
    def test_empty(self):
        s = summarise_chaos([])
        assert s.n_elements == 0

    def test_single(self):
        r = ChaosResult(chi=0.3, label="Fe")
        s = summarise_chaos([r])
        assert s.n_elements == 1
        assert s.chi_min == 0.3
        assert s.chi_max == 0.3
        assert s.most_stable == "Fe"
        assert s.most_chaotic == "Fe"

    def test_multiple(self):
        results = [
            ChaosResult(chi=0.1, label="Cu"),
            ChaosResult(chi=0.5, label="Fe"),
            ChaosResult(chi=0.9, label="Bi"),
        ]
        s = summarise_chaos(results)
        assert s.n_elements == 3
        assert s.chi_min == 0.1
        assert s.chi_max == 0.9
        assert abs(s.chi_mean - 0.5) < 1e-10
        assert s.most_stable == "Cu"
        assert s.most_chaotic == "Bi"

    def test_grade_distribution(self):
        results = [
            ChaosResult(chi=0.05, label="A"),  # STABLE
            ChaosResult(chi=0.15, label="B"),  # LOW-CHAOS
            ChaosResult(chi=0.35, label="C"),  # MODERATE
            ChaosResult(chi=0.60, label="D"),  # HIGH-CHAOS
            ChaosResult(chi=0.80, label="E"),  # CRITICAL
        ]
        s = summarise_chaos(results)
        assert s.grade_distribution["STABLE"] == 1
        assert s.grade_distribution["LOW-CHAOS"] == 1
        assert s.grade_distribution["MODERATE"] == 1
        assert s.grade_distribution["HIGH-CHAOS"] == 1
        assert s.grade_distribution["CRITICAL"] == 1

    def test_to_dict(self):
        results = [ChaosResult(chi=0.2, label="Fe")]
        s = summarise_chaos(results)
        d = s.to_dict()
        assert "n_elements" in d
        assert "chi_min" in d
        assert "most_stable" in d
        assert "grade_distribution" in d


# =====================================================================
# Weight Sensitivity
# =====================================================================


class TestWeightSensitivity:
    @pytest.fixture
    def typical_frame(self):
        pos = [(0, 0, 0), (2, 2, 0), (2, 0, 2), (0, 2, 2)]
        fmags = [0.5, 1.0, 2.0, 3.0]
        return pos, fmags

    def test_returns_all_channels(self, typical_frame):
        pos, fmags = typical_frame
        sr = weight_sensitivity(pos, pos, fmags, 4, 4, 4, T=600.0, T_melt=1500.0)
        assert set(sr.sensitivities.keys()) == {"disp", "force", "therm", "aniso"}

    def test_most_sensitive(self, typical_frame):
        pos, fmags = typical_frame
        sr = weight_sensitivity(pos, pos, fmags, 4, 4, 4, T=600.0, T_melt=1500.0)
        ms = sr.most_sensitive()
        assert ms in {"disp", "force", "therm", "aniso"}

    def test_sensitivity_magnitude(self, typical_frame):
        pos, fmags = typical_frame
        sr = weight_sensitivity(pos, pos, fmags, 4, 4, 4, T=600.0, T_melt=1500.0)
        # Sensitivities should be finite
        for v in sr.sensitivities.values():
            assert math.isfinite(v)

    def test_to_dict(self, typical_frame):
        pos, fmags = typical_frame
        sr = weight_sensitivity(pos, pos, fmags, 4, 4, 4, T=600.0, T_melt=1500.0)
        d = sr.to_dict()
        assert "base_chi" in d
        assert "sensitivities" in d
        assert "most_sensitive" in d


# =====================================================================
# Clamp utility
# =====================================================================


class TestClamp:
    def test_in_range(self):
        assert _clamp01(0.5) == 0.5

    def test_below(self):
        assert _clamp01(-1.0) == 0.0

    def test_above(self):
        assert _clamp01(2.0) == 1.0

    def test_boundary(self):
        assert _clamp01(0.0) == 0.0
        assert _clamp01(1.0) == 1.0
