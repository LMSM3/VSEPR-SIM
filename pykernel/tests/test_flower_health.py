"""
test_flower_health -- Tests for the flower health indicator.
Mirrors the 40 C++ tests plus Python-specific extras.
VSEPR-SIM 3.0.1
"""

import math
import pytest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from pykernel.flower_health import (
    HealthBand, HealthTrend, HealthDeduction,
    NodeMetrics, HealthScore,
    compute_health, classify_health,
    band_name, trend_name,
    Colour, FlowerTheme, FlowerPalette,
    build_theme, resolve_palette, apply_trend,
    FlowerAnim, FlowerAnimState,
    pick_animation, advance_animation, apply_animation,
    render_flower, render_deductions,
    health_notification,
)


# =====================================================================
# T1-T15: Scoring
# =====================================================================

class TestScoring:
    def test_default_perfect(self):
        h = compute_health(NodeMetrics())
        assert h.score == 100
        assert h.band == HealthBand.BLOOM
        assert len(h.deductions) == 0

    def test_cpu_warm(self):
        assert compute_health(NodeMetrics(cpu_temp_C=65.0)).score == 95

    def test_cpu_high(self):
        assert compute_health(NodeMetrics(cpu_temp_C=80.0)).score == 85

    def test_cpu_critical(self):
        assert compute_health(NodeMetrics(cpu_temp_C=95.0)).score == 70

    def test_memory_pressure(self):
        assert compute_health(NodeMetrics(memory_usage_pct=85.0)).score == 90

    def test_memory_critical(self):
        assert compute_health(NodeMetrics(memory_usage_pct=95.0)).score == 75

    def test_swap_creep(self):
        assert compute_health(NodeMetrics(swap_usage_pct=20.0)).score == 97

    def test_swap_heavy(self):
        assert compute_health(NodeMetrics(swap_usage_pct=50.0)).score == 90

    def test_disk_pressure(self):
        assert compute_health(NodeMetrics(disk_usage_pct=85.0)).score == 92

    def test_disk_critical(self):
        assert compute_health(NodeMetrics(disk_usage_pct=95.0)).score == 80

    def test_service_down(self):
        assert compute_health(NodeMetrics(failed_services=1)).score == 95

    def test_services_failing(self):
        assert compute_health(NodeMetrics(failed_services=5)).score == 85

    def test_zombies(self):
        assert compute_health(NodeMetrics(zombie_processes=3)).score == 97

    def test_zombie_swarm(self):
        assert compute_health(NodeMetrics(zombie_processes=10)).score == 90

    def test_load_saturation(self):
        assert compute_health(NodeMetrics(load_per_core=2.0)).score == 95

    def test_load_extreme(self):
        assert compute_health(NodeMetrics(load_per_core=3.0)).score == 90

    def test_iowait(self):
        assert compute_health(NodeMetrics(iowait_pct=10.0)).score == 95

    def test_iowait_choked(self):
        assert compute_health(NodeMetrics(iowait_pct=20.0)).score == 88

    def test_gpu_warm(self):
        assert compute_health(NodeMetrics(gpu_present=True, gpu_temp_C=75.0)).score == 95

    def test_gpu_high(self):
        assert compute_health(NodeMetrics(gpu_present=True, gpu_temp_C=90.0)).score == 85

    def test_gpu_throttling(self):
        assert compute_health(NodeMetrics(gpu_present=True, gpu_throttling=True)).score == 70

    def test_gpu_absent(self):
        assert compute_health(NodeMetrics(gpu_present=False, gpu_temp_C=99.0)).score == 100

    def test_log_oversized(self):
        assert compute_health(NodeMetrics(log_oversized=True)).score == 96

    def test_log_explosion(self):
        assert compute_health(NodeMetrics(log_severe_growth=True)).score == 92

    def test_thermal_drift(self):
        assert compute_health(NodeMetrics(thermal_drift=True)).score == 95

    def test_thermal_drift_suspicious(self):
        assert compute_health(NodeMetrics(thermal_drift_suspicious=True)).score == 90

    def test_cleanup_overdue(self):
        assert compute_health(NodeMetrics(cleanup_overdue=True)).score == 97

    def test_cleanup_backlog(self):
        assert compute_health(NodeMetrics(cleanup_serious=True)).score == 92

    def test_cache_pressure(self):
        assert compute_health(NodeMetrics(cache_usage_pct=85.0)).score == 95

    def test_cache_critical(self):
        assert compute_health(NodeMetrics(cache_usage_pct=95.0)).score == 88

    def test_stacking(self):
        m = NodeMetrics(cpu_temp_C=80.0, memory_usage_pct=85.0,
                        swap_usage_pct=50.0, cleanup_overdue=True)
        h = compute_health(m)
        assert h.score == 62
        assert len(h.deductions) == 4

    def test_clamp_zero(self):
        m = NodeMetrics(
            cpu_temp_C=95.0, memory_usage_pct=95.0, disk_usage_pct=95.0,
            failed_services=5, zombie_processes=10, load_per_core=3.0,
            iowait_pct=20.0, gpu_present=True, gpu_throttling=True)
        h = compute_health(m)
        assert h.score == 0
        assert h.band == HealthBand.CRITICAL


# =====================================================================
# T16: Band classification
# =====================================================================

class TestBands:
    @pytest.mark.parametrize("score,expected", [
        (100, HealthBand.BLOOM),   (90, HealthBand.BLOOM),
        (89, HealthBand.STRONG),   (75, HealthBand.STRONG),
        (74, HealthBand.STRAINED), (60, HealthBand.STRAINED),
        (59, HealthBand.WILTING),  (40, HealthBand.WILTING),
        (39, HealthBand.FAILING),  (20, HealthBand.FAILING),
        (19, HealthBand.CRITICAL), (0, HealthBand.CRITICAL),
    ])
    def test_classify(self, score, expected):
        assert classify_health(score) == expected


# =====================================================================
# T17-T19: Trend
# =====================================================================

class TestTrend:
    def test_stable(self):
        h = compute_health(NodeMetrics(), [100, 100, 100])
        assert h.trend == HealthTrend.STABLE

    def test_falling(self):
        h = compute_health(NodeMetrics(cpu_temp_C=95.0), [95, 90, 85])
        assert h.trend == HealthTrend.FALLING

    def test_improving(self):
        h = compute_health(NodeMetrics(), [60, 65, 70])
        assert h.trend == HealthTrend.IMPROVING


# =====================================================================
# T20-T26: 5-stage withering palette
# =====================================================================

class TestWithering:
    def test_bloom(self):
        theme = build_theme()
        pal = resolve_palette(95, theme)
        assert pal.leaf.g > 150
        assert pal.leaf.g > pal.leaf.r
        assert pal.petal.r > 200
        assert pal.center.r > 220

    def test_strong_muted(self):
        theme = build_theme()
        bloom = resolve_palette(95, theme)
        strong = resolve_palette(80, theme)
        assert strong.leaf.g < bloom.leaf.g
        assert strong.petal.r <= bloom.petal.r

    def test_strained_olive(self):
        theme = build_theme()
        pal = resolve_palette(65, theme)
        assert pal.leaf.r >= 100
        assert 100 <= pal.leaf.g <= 160

    def test_wilting_dry(self):
        theme = build_theme()
        pal = resolve_palette(50, theme)
        assert pal.leaf.r > pal.leaf.g - 30
        assert pal.petal.r < 200

    def test_failing_brown(self):
        theme = build_theme()
        pal = resolve_palette(25, theme)
        assert pal.leaf.r >= pal.leaf.g

    def test_critical_dead(self):
        theme = build_theme()
        pal = resolve_palette(5, theme)
        assert pal.leaf.r >= pal.leaf.g
        assert pal.petal.r < 160
        assert pal.center.r < 140

    def test_leaves_degrade_before_petals(self):
        theme = build_theme()
        bloom = resolve_palette(95, theme)
        strained = resolve_palette(65, theme)
        leaf_d = abs(bloom.leaf.g - strained.leaf.g) + abs(bloom.leaf.r - strained.leaf.r)
        petal_d = abs(bloom.petal.r - strained.petal.r) + abs(bloom.petal.g - strained.petal.g)
        assert leaf_d > petal_d


# =====================================================================
# T27-T28: Trend modifier
# =====================================================================

class TestTrendModifier:
    def test_falling_dims(self):
        theme = build_theme()
        pal = resolve_palette(70, theme)
        dimmed = apply_trend(pal, HealthTrend.FALLING)
        before = pal.petal.r + pal.petal.g + pal.petal.b
        after = dimmed.petal.r + dimmed.petal.g + dimmed.petal.b
        assert after < before

    def test_improving_brightens(self):
        theme = build_theme()
        pal = resolve_palette(70, theme)
        bright = apply_trend(pal, HealthTrend.IMPROVING)
        before = pal.petal.r + pal.petal.g + pal.petal.b
        after = bright.petal.r + bright.petal.g + bright.petal.b
        assert after > before


# =====================================================================
# T29-T30: Animation
# =====================================================================

class TestAnimation:
    def test_bloom_pulse(self):
        hs = HealthScore(score=95, band=HealthBand.BLOOM)
        assert pick_animation(hs, FlowerAnimState()) == FlowerAnim.PULSE_CENTER

    def test_warning_flash(self):
        hs = HealthScore(score=60, band=HealthBand.STRAINED)
        prev = FlowerAnimState(prev_score=85)
        assert pick_animation(hs, prev) == FlowerAnim.WARNING_FLASH

    def test_advance_state(self):
        hs = HealthScore(score=95, band=HealthBand.BLOOM)
        state = FlowerAnimState()
        state = advance_animation(state, hs)
        assert state.active == FlowerAnim.PULSE_CENTER
        assert state.duration == 60
        assert state.prev_score == 95

    def test_apply_animation_no_crash(self):
        theme = build_theme()
        pal = resolve_palette(50, theme)
        state = FlowerAnimState(active=FlowerAnim.LEAF_DROOP,
                                frame=10, duration=40, prev_score=50)
        result = apply_animation(pal, state)
        assert 0 <= result.leaf.r <= 255
        assert 0 <= result.leaf.g <= 255


# =====================================================================
# T31-T33: Rendering
# =====================================================================

class TestRendering:
    def test_render_flower(self):
        hs = compute_health(NodeMetrics())
        theme = build_theme()
        pal = resolve_palette(hs.score, theme)
        output = render_flower(hs, "cmp-01", pal)
        assert "cmp-01" in output
        assert "100" in output
        assert "BLOOM" in output
        assert "\033[" in output
        assert "+" in output  # center character
        assert "*" in output  # petal character

    def test_render_deductions(self):
        hs = compute_health(NodeMetrics(cpu_temp_C=80.0, swap_usage_pct=50.0))
        output = render_deductions(hs)
        assert "Deductions" in output
        assert "CPU high" in output
        assert "swap heavy" in output

    def test_render_deductions_empty(self):
        hs = compute_health(NodeMetrics())
        output = render_deductions(hs)
        assert "(none)" in output


# =====================================================================
# T34-T35: Notifications
# =====================================================================

class TestNotifications:
    def test_drop(self):
        prev = compute_health(NodeMetrics())
        curr = compute_health(NodeMetrics(cpu_temp_C=80.0))
        msg = health_notification(curr, prev)
        assert "dropped" in msg
        assert "85" in msg
        assert "CPU high" in msg

    def test_recovery(self):
        prev = compute_health(NodeMetrics(cpu_temp_C=80.0))
        curr = compute_health(NodeMetrics())
        msg = health_notification(curr, prev)
        assert "improved" in msg
        assert "CPU high" in msg

    def test_no_change(self):
        hs = compute_health(NodeMetrics())
        assert health_notification(hs, hs) == ""

    def test_no_prev(self):
        hs = compute_health(NodeMetrics())
        assert health_notification(hs) == ""


# =====================================================================
# T36-T37: Names
# =====================================================================

class TestNames:
    def test_band_names(self):
        assert band_name(HealthBand.BLOOM) == "BLOOM"
        assert band_name(HealthBand.STRONG) == "STRONG"
        assert band_name(HealthBand.STRAINED) == "STRAINED"
        assert band_name(HealthBand.WILTING) == "WILTING"
        assert band_name(HealthBand.FAILING) == "FAILING"
        assert band_name(HealthBand.CRITICAL) == "CRITICAL"

    def test_trend_names(self):
        assert trend_name(HealthTrend.STABLE) == "stable"
        assert trend_name(HealthTrend.IMPROVING) == "improving"
        assert trend_name(HealthTrend.FALLING) == "falling"


# =====================================================================
# T38: Theme generation
# =====================================================================

class TestTheme:
    def test_from_custom_petal(self):
        theme = build_theme(Colour(200, 80, 120))
        assert theme.petal_full.r == 200
        assert theme.petal_soft.g > theme.petal_full.g
        assert theme.petal_dead == Colour(140, 120, 95)
        assert theme.leaf_vibrant.g > theme.leaf_healthy.g
        assert theme.leaf_healthy.g > theme.leaf_olive.g

    def test_default_theme(self):
        theme = build_theme()
        assert theme.petal_full == Colour(230, 100, 140)


# =====================================================================
# T39: Sub-band smoothness
# =====================================================================

class TestSubBand:
    def test_within_band_dimming(self):
        theme = build_theme()
        top = resolve_palette(74, theme)
        bot = resolve_palette(60, theme)
        top_b = top.leaf.r + top.leaf.g + top.leaf.b
        bot_b = bot.leaf.r + bot.leaf.g + bot.leaf.b
        assert bot_b <= top_b


# =====================================================================
# T40: Full pipeline
# =====================================================================

class TestPipeline:
    def test_full(self):
        m = NodeMetrics(cpu_temp_C=75.0, swap_usage_pct=30.0, cleanup_overdue=True)
        hs = compute_health(m, [92, 91, 90])
        assert hs.score == 89
        assert hs.band == HealthBand.STRONG
        assert len(hs.deductions) == 3

        theme = build_theme()
        pal = resolve_palette(hs.score, theme)
        pal = apply_trend(pal, hs.trend)

        anim = FlowerAnimState()
        anim = advance_animation(anim, hs)
        pal = apply_animation(pal, anim)

        output = render_flower(hs, "cmp-03", pal)
        assert "cmp-03" in output
        assert "89" in output


# =====================================================================
# Extra: Palette sweep regression guard
# =====================================================================

class TestPaletteSweep:
    @pytest.mark.parametrize("score", [0, 5, 10, 20, 40, 50, 60, 65, 74, 75, 80, 89, 90, 95, 100])
    def test_valid_colours(self, score):
        theme = build_theme()
        pal = resolve_palette(score, theme)
        for c in [pal.leaf, pal.petal, pal.center, pal.stem, pal.text, pal.border]:
            assert 0 <= c.r <= 255
            assert 0 <= c.g <= 255
            assert 0 <= c.b <= 255

    def test_leaf_monotonic_degradation(self):
        theme = build_theme()
        prev_g = 999
        for score in range(100, -1, -5):
            pal = resolve_palette(score, theme)
            assert pal.leaf.g <= prev_g + 2  # +2 for sub-band rounding
            prev_g = pal.leaf.g
