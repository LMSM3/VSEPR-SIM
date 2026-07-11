"""
flower_health -- Living health indicator for TUI dashboards.

Visual + numeric health scoring.  The ASCII flower reflects node
vitality through staged colour degradation: leaves brown first,
then petals fade.

5-stage withering arc:
  Stage 1: Leaves dull slightly
  Stage 2: Leaves become olive, stem dims
  Stage 3: Petals begin losing saturation
  Stage 4: Petals fade toward beige
  Stage 5: Browning across whole symbol

Mirrors the C++ atomistic::tui::flower_health system.
VSEPR-SIM 3.0.1
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional


# =====================================================================
# Health bands
# =====================================================================

class HealthBand(IntEnum):
    CRITICAL = 0   #  0-19
    FAILING  = 1   # 20-39
    WILTING  = 2   # 40-59
    STRAINED = 3   # 60-74
    STRONG   = 4   # 75-89
    BLOOM    = 5   # 90-100


_BAND_NAMES = {
    HealthBand.BLOOM:    "BLOOM",
    HealthBand.STRONG:   "STRONG",
    HealthBand.STRAINED: "STRAINED",
    HealthBand.WILTING:  "WILTING",
    HealthBand.FAILING:  "FAILING",
    HealthBand.CRITICAL: "CRITICAL",
}


def band_name(b: HealthBand) -> str:
    return _BAND_NAMES.get(b, "UNKNOWN")


def classify_health(score: int) -> HealthBand:
    if score >= 90: return HealthBand.BLOOM
    if score >= 75: return HealthBand.STRONG
    if score >= 60: return HealthBand.STRAINED
    if score >= 40: return HealthBand.WILTING
    if score >= 20: return HealthBand.FAILING
    return HealthBand.CRITICAL


# =====================================================================
# Trend
# =====================================================================

class HealthTrend(IntEnum):
    STABLE    = 0
    IMPROVING = 1
    FALLING   = 2


_TREND_NAMES = {
    HealthTrend.STABLE:    "stable",
    HealthTrend.IMPROVING: "improving",
    HealthTrend.FALLING:   "falling",
}


def trend_name(t: HealthTrend) -> str:
    return _TREND_NAMES.get(t, "unknown")


# =====================================================================
# Deduction
# =====================================================================

@dataclass
class HealthDeduction:
    reason: str
    penalty: int


# =====================================================================
# Node metrics
# =====================================================================

@dataclass
class NodeMetrics:
    cpu_temp_C: float = 45.0
    memory_usage_pct: float = 50.0
    swap_usage_pct: float = 0.0
    disk_usage_pct: float = 40.0
    failed_services: int = 0
    zombie_processes: int = 0
    load_per_core: float = 0.5
    iowait_pct: float = 0.0
    gpu_temp_C: float = 0.0
    gpu_present: bool = False
    gpu_throttling: bool = False
    log_oversized: bool = False
    log_severe_growth: bool = False
    thermal_drift: bool = False
    thermal_drift_suspicious: bool = False
    cleanup_overdue: bool = False
    cleanup_serious: bool = False
    cache_usage_pct: float = 0.0


# =====================================================================
# Health score result
# =====================================================================

@dataclass
class HealthScore:
    score: int = 100
    band: HealthBand = HealthBand.BLOOM
    trend: HealthTrend = HealthTrend.STABLE
    deductions: list[HealthDeduction] = field(default_factory=list)
    history: list[int] = field(default_factory=list)


# =====================================================================
# Health computation
# =====================================================================

def compute_health(m: NodeMetrics,
                   prev_history: Optional[list[int]] = None) -> HealthScore:
    """Deterministic additive health scoring."""
    h = HealthScore()
    h.score = 100

    def deduct(penalty: int, reason: str) -> None:
        if penalty <= 0:
            return
        h.deductions.append(HealthDeduction(reason, penalty))
        h.score -= penalty

    if m.cpu_temp_C >= 95.0:      deduct(30, "CPU critical")
    elif m.cpu_temp_C >= 80.0:    deduct(15, "CPU high")
    elif m.cpu_temp_C >= 65.0:    deduct(5,  "CPU warm")

    if m.memory_usage_pct >= 95.0:    deduct(25, "memory critical")
    elif m.memory_usage_pct >= 85.0:  deduct(10, "memory pressure")

    if m.swap_usage_pct >= 50.0:      deduct(10, "swap heavy")
    elif m.swap_usage_pct >= 20.0:    deduct(3,  "swap creep")

    if m.disk_usage_pct >= 95.0:      deduct(20, "disk critical")
    elif m.disk_usage_pct >= 85.0:    deduct(8,  "disk pressure")

    if m.failed_services >= 5:        deduct(15, "services failing")
    elif m.failed_services >= 1:      deduct(5,  "service down")

    if m.zombie_processes >= 10:      deduct(10, "zombie swarm")
    elif m.zombie_processes >= 3:     deduct(3,  "zombies")

    if m.load_per_core >= 3.0:        deduct(10, "load extreme")
    elif m.load_per_core >= 2.0:      deduct(5,  "load saturation")

    if m.iowait_pct >= 20.0:         deduct(12, "I/O choked")
    elif m.iowait_pct >= 10.0:       deduct(5,  "I/O wait")

    if m.gpu_present:
        if m.gpu_throttling:          deduct(30, "GPU throttling")
        elif m.gpu_temp_C >= 90.0:    deduct(15, "GPU high")
        elif m.gpu_temp_C >= 75.0:    deduct(5,  "GPU warm")

    if m.log_severe_growth:           deduct(8,  "log explosion")
    elif m.log_oversized:             deduct(4,  "log oversized")

    if m.thermal_drift_suspicious:    deduct(10, "thermal drift suspicious")
    elif m.thermal_drift:             deduct(5,  "thermal drift")

    if m.cleanup_serious:             deduct(8,  "cleanup backlog")
    elif m.cleanup_overdue:           deduct(3,  "cleanup overdue")

    if m.cache_usage_pct >= 95.0:     deduct(12, "cache critical")
    elif m.cache_usage_pct >= 85.0:   deduct(5,  "cache pressure")

    h.score = max(0, h.score)
    h.band = classify_health(h.score)

    h.history = list(prev_history) if prev_history else []
    h.history.append(h.score)
    if len(h.history) > 10:
        h.history = h.history[-10:]

    if len(h.history) >= 3:
        delta = h.history[-1] - h.history[0]
        if delta <= -8:
            h.trend = HealthTrend.FALLING
        elif delta >= 8:
            h.trend = HealthTrend.IMPROVING
        else:
            h.trend = HealthTrend.STABLE

    return h


# =====================================================================
# Colour helpers
# =====================================================================

@dataclass(frozen=True)
class Colour:
    r: int = 180
    g: int = 180
    b: int = 180

    def fg(self) -> str:
        return f"\033[38;2;{self.r};{self.g};{self.b}m"

    @staticmethod
    def lerp(a: "Colour", b: "Colour", t: float) -> "Colour":
        t = max(0.0, min(1.0, t))
        return Colour(
            int(a.r + t * (b.r - a.r)),
            int(a.g + t * (b.g - a.g)),
            int(a.b + t * (b.b - a.b)),
        )


RESET = "\033[0m"
BOLD = "\033[1m"


# =====================================================================
# Theme: named palette stages for future theming
# =====================================================================

@dataclass(frozen=True)
class FlowerTheme:
    petal_full: Colour
    petal_soft: Colour
    petal_faded: Colour
    petal_tan: Colour
    petal_dead: Colour
    leaf_vibrant: Colour
    leaf_healthy: Colour
    leaf_olive: Colour
    leaf_dry: Colour
    leaf_brown: Colour
    center_bright: Colour
    center_mid: Colour
    center_dim: Colour
    center_dead: Colour


def build_theme(primary_petal: Colour = Colour(230, 100, 140)) -> FlowerTheme:
    """Generate the living and dying palette from one input colour."""
    p = primary_petal
    return FlowerTheme(
        petal_full=p,
        petal_soft=Colour.lerp(p, Colour(220, 200, 190), 0.15),
        petal_faded=Colour((p.r + 180) // 2, (p.g + 160) // 2, (p.b + 140) // 2),
        petal_tan=Colour.lerp(
            Colour((p.r + 180) // 2, (p.g + 160) // 2, (p.b + 140) // 2),
            Colour(170, 150, 120), 0.60,
        ),
        petal_dead=Colour(140, 120, 95),
        leaf_vibrant=Colour(60, 180, 60),
        leaf_healthy=Colour(80, 160, 55),
        leaf_olive=Colour(120, 140, 40),
        leaf_dry=Colour(140, 120, 40),
        leaf_brown=Colour(100, 70, 30),
        center_bright=Colour(240, 200, 60),
        center_mid=Colour(200, 170, 55),
        center_dim=Colour(150, 130, 60),
        center_dead=Colour(120, 110, 70),
    )


# =====================================================================
# FlowerPalette: resolved colours for one frame
# =====================================================================

@dataclass(frozen=True)
class FlowerPalette:
    leaf: Colour
    petal: Colour
    center: Colour
    stem: Colour
    text: Colour
    border: Colour


# =====================================================================
# 5-stage withering: resolve palette from score + theme
# =====================================================================

def resolve_palette(score: int, theme: FlowerTheme) -> FlowerPalette:
    """Leaves degrade first. That is the most important design choice."""
    band = classify_health(score)

    _MAP = {
        HealthBand.BLOOM: dict(
            leaf=theme.leaf_vibrant, petal=theme.petal_full,
            center=theme.center_bright, stem=theme.leaf_vibrant,
            text=Colour(200, 220, 200), border=Colour(80, 180, 120)),
        HealthBand.STRONG: dict(
            leaf=theme.leaf_healthy, petal=theme.petal_soft,
            center=theme.center_bright, stem=theme.leaf_healthy,
            text=Colour(190, 210, 190), border=Colour(70, 160, 110)),
        HealthBand.STRAINED: dict(
            leaf=theme.leaf_olive, petal=theme.petal_faded,
            center=theme.center_mid,
            stem=Colour.lerp(theme.leaf_olive, theme.leaf_dry, 0.3),
            text=Colour(170, 180, 160), border=Colour(150, 150, 80)),
        HealthBand.WILTING: dict(
            leaf=theme.leaf_dry, petal=theme.petal_tan,
            center=theme.center_dim, stem=theme.leaf_dry,
            text=Colour(150, 140, 120), border=Colour(160, 120, 60)),
        HealthBand.FAILING: dict(
            leaf=theme.leaf_brown,
            petal=Colour.lerp(theme.petal_tan, theme.petal_dead, 0.5),
            center=theme.center_dim, stem=theme.leaf_brown,
            text=Colour(130, 110, 90), border=Colour(140, 90, 50)),
        HealthBand.CRITICAL: dict(
            leaf=theme.leaf_brown, petal=theme.petal_dead,
            center=theme.center_dead,
            stem=Colour.lerp(theme.leaf_brown, Colour(80, 60, 30), 0.4),
            text=Colour(120, 100, 80), border=Colour(120, 70, 40)),
    }
    base = _MAP[band]

    # Sub-band interpolation
    _RANGES = {
        HealthBand.BLOOM:    (90, 100), HealthBand.STRONG:   (75, 89),
        HealthBand.STRAINED: (60, 74),  HealthBand.WILTING:  (40, 59),
        HealthBand.FAILING:  (20, 39),  HealthBand.CRITICAL: (0, 19),
    }
    lo, hi = _RANGES[band]
    frac = (score - lo) / max(hi - lo, 1)
    frac = max(0.0, min(1.0, frac))
    dim_amount = (1.0 - frac) * 0.08
    dimmer = Colour(40, 35, 25)

    return FlowerPalette(
        leaf=Colour.lerp(base["leaf"], dimmer, dim_amount),
        petal=Colour.lerp(base["petal"], dimmer, dim_amount),
        center=base["center"],
        stem=base["stem"],
        text=base["text"],
        border=base["border"],
    )


# =====================================================================
# Trend modifier
# =====================================================================

def apply_trend(pal: FlowerPalette, trend: HealthTrend) -> FlowerPalette:
    if trend == HealthTrend.FALLING:
        return FlowerPalette(
            leaf=pal.leaf,
            petal=Colour.lerp(Colour(100, 90, 75), pal.petal, 0.85),
            center=Colour.lerp(Colour(90, 80, 50), pal.center, 0.90),
            stem=pal.stem, text=pal.text, border=pal.border)
    if trend == HealthTrend.IMPROVING:
        return FlowerPalette(
            leaf=pal.leaf,
            petal=Colour.lerp(pal.petal, Colour(255, 240, 230), 0.10),
            center=Colour.lerp(pal.center, Colour(250, 210, 80), 0.08),
            stem=pal.stem, text=pal.text, border=pal.border)
    return pal


# =====================================================================
# Animation
# =====================================================================

class FlowerAnim(IntEnum):
    NONE          = 0
    PULSE_CENTER  = 1
    DIM_FLICKER   = 2
    RECOVERY_GLOW = 3
    WARNING_FLASH = 4
    LEAF_DROOP    = 5


@dataclass
class FlowerAnimState:
    active: FlowerAnim = FlowerAnim.NONE
    frame: int = 0
    duration: int = 0
    prev_score: int = -1


def pick_animation(hs: HealthScore, prev: FlowerAnimState) -> FlowerAnim:
    if prev.prev_score >= 0 and hs.score < prev.prev_score - 10:
        return FlowerAnim.WARNING_FLASH
    if prev.prev_score >= 0 and hs.score > prev.prev_score + 5:
        return FlowerAnim.RECOVERY_GLOW
    if hs.band == HealthBand.BLOOM:
        return FlowerAnim.PULSE_CENTER
    if hs.band in (HealthBand.CRITICAL, HealthBand.FAILING):
        return FlowerAnim.LEAF_DROOP
    if hs.trend == HealthTrend.FALLING:
        return FlowerAnim.DIM_FLICKER
    return FlowerAnim.NONE


_ANIM_DURATIONS = {
    FlowerAnim.PULSE_CENTER:  60,
    FlowerAnim.DIM_FLICKER:   20,
    FlowerAnim.RECOVERY_GLOW: 15,
    FlowerAnim.WARNING_FLASH: 10,
    FlowerAnim.LEAF_DROOP:    40,
    FlowerAnim.NONE:           0,
}


def advance_animation(state: FlowerAnimState,
                      hs: HealthScore) -> FlowerAnimState:
    nxt = pick_animation(hs, state)
    done = state.duration > 0 and state.frame >= state.duration
    interrupt = nxt in (FlowerAnim.WARNING_FLASH, FlowerAnim.RECOVERY_GLOW)

    if done or interrupt or state.active == FlowerAnim.NONE:
        state = FlowerAnimState(
            active=nxt, frame=0,
            duration=_ANIM_DURATIONS.get(nxt, 0),
            prev_score=hs.score)
    else:
        state = FlowerAnimState(
            active=state.active, frame=state.frame + 1,
            duration=state.duration, prev_score=hs.score)
    return state


def apply_animation(pal: FlowerPalette,
                    anim: FlowerAnimState) -> FlowerPalette:
    if anim.duration == 0:
        return pal
    t = anim.frame / max(anim.duration, 1)

    if anim.active == FlowerAnim.PULSE_CENTER:
        pulse = 0.5 + 0.5 * math.sin(t * 2.0 * math.pi)
        shift = pulse * 0.06
        return FlowerPalette(
            leaf=pal.leaf, petal=pal.petal,
            center=Colour.lerp(pal.center, Colour(255, 230, 100), shift),
            stem=pal.stem, text=pal.text, border=pal.border)

    if anim.active == FlowerAnim.DIM_FLICKER:
        flicker = 0.08 if 0.4 < t < 0.6 else 0.0
        dark = Colour(60, 50, 40)
        return FlowerPalette(
            leaf=pal.leaf,
            petal=Colour.lerp(pal.petal, dark, flicker),
            center=Colour.lerp(pal.center, dark, flicker),
            stem=pal.stem, text=pal.text, border=pal.border)

    if anim.active == FlowerAnim.RECOVERY_GLOW:
        glow = (1.0 - t) * 0.12
        return FlowerPalette(
            leaf=pal.leaf,
            petal=Colour.lerp(pal.petal, Colour(255, 220, 180), glow),
            center=pal.center, stem=pal.stem, text=pal.text, border=pal.border)

    if anim.active == FlowerAnim.WARNING_FLASH:
        flash = (1.0 - t) * 0.25
        return FlowerPalette(
            leaf=pal.leaf, petal=pal.petal, center=pal.center,
            stem=pal.stem, text=pal.text,
            border=Colour.lerp(pal.border, Colour(255, 60, 40), flash))

    if anim.active == FlowerAnim.LEAF_DROOP:
        droop = 0.5 + 0.5 * math.sin(t * 2.0 * math.pi)
        shift = droop * 0.05
        dark = Colour(70, 50, 25)
        return FlowerPalette(
            leaf=Colour.lerp(pal.leaf, dark, shift),
            petal=pal.petal, center=pal.center,
            stem=Colour.lerp(pal.stem, dark, shift),
            text=pal.text, border=pal.border)

    return pal


# =====================================================================
# ASCII flower renderer
# =====================================================================

def render_flower(hs: HealthScore, node: str,
                  pal: FlowerPalette) -> str:
    """Render the ASCII flower with ANSI colour codes."""
    R = RESET
    lf = pal.leaf.fg()
    pf = pal.petal.fg()
    cf = pal.center.fg()
    sf = pal.stem.fg()
    tf = pal.text.fg()
    bf = pal.border.fg()

    lines = [
        f"{bf}|{R}     {pf}{BOLD}*{R}",
        f"{bf}|{R}    {pf}/{R}{sf}|{R}{pf}\\{R}",
        f"{bf}|{R}   {pf}/{R} {sf}|{R} {pf}\\{R}",
        f"{bf}|{R}{pf}{BOLD}*{R}{sf}-{R}{pf}/{R}{sf}--{R}{cf}{BOLD}+{R}{sf}--{R}{pf}\\{R}{sf}-{R}{pf}{BOLD}*{R}",
        f"{bf}|{R}  {lf}/{R}  {sf}|{R}  {lf}\\{R}",
        f"{bf}|{R} {lf}/{R}   {sf}|{R}   {lf}\\{R}",
        f"{bf}|{R}     {sf}|{R}",
        "",
        f" {tf}{BOLD}{node}{R}",
        f" {tf}health: {hs.score}{R}",
        f" {tf}state: {band_name(hs.band)}{R}",
        f" {tf}trend: {trend_name(hs.trend)}{R}",
    ]
    return "\n".join(lines)


def render_deductions(hs: HealthScore) -> str:
    lines = [
        f"Health score: {hs.score}",
        f"{BOLD}Deductions:{RESET}",
    ]
    if not hs.deductions:
        lines.append("  (none)")
    else:
        for d in hs.deductions:
            penalty_str = f"-{d.penalty}".ljust(4)
            lines.append(f" {penalty_str} {d.reason}")
    return "\n".join(lines)


# =====================================================================
# Notifications
# =====================================================================

def health_notification(hs: HealthScore,
                        prev: Optional[HealthScore] = None) -> str:
    if prev is None:
        return ""

    if hs.score > prev.score:
        prev_set = {(d.reason, d.penalty) for d in prev.deductions}
        curr_set = {(d.reason, d.penalty) for d in hs.deductions}
        removed = prev_set - curr_set
        cause = next(iter(removed))[0] if removed else "conditions improved"
        return f"\u273e health improved to {hs.score}: {cause}"

    if hs.score < prev.score:
        prev_set = {(d.reason, d.penalty) for d in prev.deductions}
        cause = "conditions worsened"
        for d in hs.deductions:
            if (d.reason, d.penalty) not in prev_set:
                cause = d.reason
                break
        return f"\u2741 health dropped to {hs.score}: {cause}"

    return ""
