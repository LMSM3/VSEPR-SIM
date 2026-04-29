#pragma once
/**
 * flower_health.hpp -- Living health indicator for the TUI dashboard
 * =================================================================
 * VSEPR-SIM 3.0.1
 *
 * Top-left ASCII flower that reflects node vitality through staged
 * colour degradation.  Leaves degrade FIRST.  Petals fade AFTER.
 *
 * 5-Stage withering arc
 * ---------------------
 *   Stage 1  Leaves dull slightly
 *   Stage 2  Leaves become olive, stem dims
 *   Stage 3  Petals begin losing saturation
 *   Stage 4  Petals fade toward beige
 *   Stage 5  Browning across whole symbol
 *
 * Health bands
 * ------------
 *   BLOOM    90-100  Stage 1 boundary (rich green, bright petals)
 *   STRONG   75-89   Late Stage 1 / early Stage 2
 *   STRAINED 60-74   Stage 2-3 (olive leaves, petals start fading)
 *   WILTING  40-59   Stage 3-4 (brown-green leaves, pale tan petals)
 *   FAILING  20-39   Stage 4-5 (brown leaves, dusty faded petals)
 *   CRITICAL  0-19   Stage 5 (dark brown across whole symbol)
 *
 * Score = 100 - deductions.  Each deduction is named and traceable.
 * Trend (STABLE / IMPROVING / FALLING) modifies visual state so two
 * nodes at score 72 can look different depending on trajectory.
 *
 * Anti-black-box: every mapping decision, every penalty, every
 * intermediate colour is explicitly inspectable and deterministic.
 */

#include "crystal_tui.hpp"   // Colour, FrameBuffer, Cell, RESET, BOLD, DIM
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <cstdint>

namespace atomistic {
namespace tui {

// ============================================================================
// Health bands
// ============================================================================

enum class HealthBand : uint8_t {
    BLOOM     = 5,   // 90-100
    STRONG    = 4,   // 75-89
    STRAINED  = 3,   // 60-74
    WILTING   = 2,   // 40-59
    FAILING   = 1,   // 20-39
    CRITICAL  = 0    //  0-19
};

inline const char* band_name(HealthBand b) {
    switch (b) {
        case HealthBand::BLOOM:     return "BLOOM";
        case HealthBand::STRONG:    return "STRONG";
        case HealthBand::STRAINED:  return "STRAINED";
        case HealthBand::WILTING:   return "WILTING";
        case HealthBand::FAILING:   return "FAILING";
        case HealthBand::CRITICAL:  return "CRITICAL";
    }
    return "UNKNOWN";
}

inline HealthBand classify_health(int score) {
    if (score >= 90) return HealthBand::BLOOM;
    if (score >= 75) return HealthBand::STRONG;
    if (score >= 60) return HealthBand::STRAINED;
    if (score >= 40) return HealthBand::WILTING;
    if (score >= 20) return HealthBand::FAILING;
    return HealthBand::CRITICAL;
}

// ============================================================================
// Trend direction (derived from recent score history)
// ============================================================================

enum class HealthTrend : uint8_t {
    STABLE    = 0,
    IMPROVING = 1,
    FALLING   = 2
};

inline const char* trend_name(HealthTrend t) {
    switch (t) {
        case HealthTrend::STABLE:    return "stable";
        case HealthTrend::IMPROVING: return "improving";
        case HealthTrend::FALLING:   return "falling";
    }
    return "unknown";
}

// ============================================================================
// Deduction: one named penalty against the health score
// ============================================================================

struct HealthDeduction {
    std::string reason;
    int         penalty{};   // positive value subtracted from 100
};

// ============================================================================
// NodeMetrics: raw readings fed into the health scorer
// ============================================================================

struct NodeMetrics {
    // CPU thermal
    double cpu_temp_C          = 45.0;

    // Memory
    double memory_usage_pct    = 50.0;
    double swap_usage_pct      = 0.0;

    // Disk
    double disk_usage_pct      = 40.0;

    // Services
    int    failed_services     = 0;

    // Processes
    int    zombie_processes    = 0;

    // Load (as multiple of core count)
    double load_per_core       = 0.5;

    // I/O wait
    double iowait_pct          = 0.0;

    // GPU
    double gpu_temp_C          = 0.0;
    bool   gpu_present         = false;
    bool   gpu_throttling      = false;

    // Log / journal
    bool   log_oversized       = false;
    bool   log_severe_growth   = false;

    // Thermal drift
    bool   thermal_drift       = false;
    bool   thermal_drift_suspicious = false;

    // Maintenance
    bool   cleanup_overdue     = false;
    bool   cleanup_serious     = false;

    // Cache / scratch
    double cache_usage_pct     = 0.0;
};

// ============================================================================
// HealthScore: computed result with full audit trail
// ============================================================================

struct HealthScore {
    int                          score = 100;
    HealthBand                   band  = HealthBand::BLOOM;
    HealthTrend                  trend = HealthTrend::STABLE;
    std::vector<HealthDeduction> deductions;
    std::vector<int>             history;
};

// ============================================================================
// compute_health -- deterministic, additive scoring
// ============================================================================

inline HealthScore compute_health(const NodeMetrics& m,
                                  const std::vector<int>& prev_history = {})
{
    HealthScore h;
    h.score = 100;

    auto deduct = [&](int penalty, const std::string& reason) {
        if (penalty <= 0) return;
        h.deductions.push_back({reason, penalty});
        h.score -= penalty;
    };

    // CPU thermal
    if (m.cpu_temp_C >= 95.0)       deduct(30, "CPU critical");
    else if (m.cpu_temp_C >= 80.0)  deduct(15, "CPU high");
    else if (m.cpu_temp_C >= 65.0)  deduct(5,  "CPU warm");

    // Memory pressure
    if (m.memory_usage_pct >= 95.0)       deduct(25, "memory critical");
    else if (m.memory_usage_pct >= 85.0)  deduct(10, "memory pressure");

    // Swap creep
    if (m.swap_usage_pct >= 50.0)       deduct(10, "swap heavy");
    else if (m.swap_usage_pct >= 20.0)  deduct(3,  "swap creep");

    // Disk usage
    if (m.disk_usage_pct >= 95.0)       deduct(20, "disk critical");
    else if (m.disk_usage_pct >= 85.0)  deduct(8,  "disk pressure");

    // Failed services
    if (m.failed_services >= 5)       deduct(15, "services failing");
    else if (m.failed_services >= 1)  deduct(5,  "service down");

    // Zombie processes
    if (m.zombie_processes >= 10)     deduct(10, "zombie swarm");
    else if (m.zombie_processes >= 3) deduct(3,  "zombies");

    // Load saturation
    if (m.load_per_core >= 3.0)       deduct(10, "load extreme");
    else if (m.load_per_core >= 2.0)  deduct(5,  "load saturation");

    // I/O wait
    if (m.iowait_pct >= 20.0)       deduct(12, "I/O choked");
    else if (m.iowait_pct >= 10.0)  deduct(5,  "I/O wait");

    // GPU thermal / fault
    if (m.gpu_present) {
        if (m.gpu_throttling)               deduct(30, "GPU throttling");
        else if (m.gpu_temp_C >= 90.0)      deduct(15, "GPU high");
        else if (m.gpu_temp_C >= 75.0)      deduct(5,  "GPU warm");
    }

    // Log explosion
    if (m.log_severe_growth) deduct(8,  "log explosion");
    else if (m.log_oversized) deduct(4, "log oversized");

    // Thermal drift
    if (m.thermal_drift_suspicious)   deduct(10, "thermal drift suspicious");
    else if (m.thermal_drift)         deduct(5,  "thermal drift");

    // Stale cleanup
    if (m.cleanup_serious)            deduct(8,  "cleanup backlog");
    else if (m.cleanup_overdue)       deduct(3,  "cleanup overdue");

    // Cache / scratch overload
    if (m.cache_usage_pct >= 95.0)       deduct(12, "cache critical");
    else if (m.cache_usage_pct >= 85.0)  deduct(5,  "cache pressure");

    // Clamp
    if (h.score < 0) h.score = 0;

    h.band = classify_health(h.score);

    // Trend from history
    h.history = prev_history;
    h.history.push_back(h.score);
    if (h.history.size() > 10)
        h.history.erase(h.history.begin(),
                        h.history.begin() + static_cast<long>(h.history.size() - 10));

    if (h.history.size() >= 3) {
        int oldest = h.history.front();
        int newest = h.history.back();
        int delta  = newest - oldest;
        if (delta <= -8)      h.trend = HealthTrend::FALLING;
        else if (delta >= 8)  h.trend = HealthTrend::IMPROVING;
        else                  h.trend = HealthTrend::STABLE;
    }

    return h;
}

// ============================================================================
// Theme: named palette stages for future theming system
// ============================================================================

struct FlowerTheme {
    // Petal stages (from theme.primary_petal)
    Colour petal_full;    // full theme colour
    Colour petal_soft;    // 10-15% softened
    Colour petal_faded;   // midway to beige
    Colour petal_tan;     // strongly faded toward pale tan
    Colour petal_dead;    // dead beige-brown

    // Leaf stages
    Colour leaf_vibrant;  // strong green
    Colour leaf_healthy;  // slightly muted green
    Colour leaf_olive;    // olive-green
    Colour leaf_dry;      // yellow-olive to brown-green
    Colour leaf_brown;    // brown

    // Center stages
    Colour center_bright; // warm gold
    Colour center_mid;    // slightly dimmer
    Colour center_dim;    // dull amber
    Colour center_dead;   // gray-yellow
};

inline FlowerTheme build_theme(Colour primary_petal = {230, 100, 140}) {
    FlowerTheme t;
    auto p = primary_petal;
    t.petal_full  = p;
    t.petal_soft  = Colour::lerp(p, Colour{220, 200, 190}, 0.15);
    t.petal_faded = Colour{
        static_cast<uint8_t>((p.r + 180) / 2),
        static_cast<uint8_t>((p.g + 160) / 2),
        static_cast<uint8_t>((p.b + 140) / 2)
    };
    t.petal_tan   = Colour::lerp(t.petal_faded, Colour{170, 150, 120}, 0.60);
    t.petal_dead  = {140, 120, 95};

    t.leaf_vibrant = {60,  180, 60};
    t.leaf_healthy = {80,  160, 55};
    t.leaf_olive   = {120, 140, 40};
    t.leaf_dry     = {140, 120, 40};
    t.leaf_brown   = {100, 70,  30};

    t.center_bright = {240, 200, 60};
    t.center_mid    = {200, 170, 55};
    t.center_dim    = {150, 130, 60};
    t.center_dead   = {120, 110, 70};

    return t;
}

// ============================================================================
// FlowerPalette: the resolved colours for one rendered frame
// ============================================================================

struct FlowerPalette {
    Colour leaf;
    Colour petal;
    Colour center;
    Colour stem;
    Colour text;
    Colour border;     // panel border colour tied to band
};

// ============================================================================
// 5-stage withering: resolve palette from score + theme
// ============================================================================

/**
 * Stage 1: Leaves dull slightly.
 * Stage 2: Leaves become olive, stem dims.
 * Stage 3: Petals begin losing saturation.
 * Stage 4: Petals fade toward beige.
 * Stage 5: Browning across whole symbol.
 *
 * Leaves degrade first.  That is the most important design choice.
 */
inline FlowerPalette resolve_palette(int score, const FlowerTheme& theme) {
    FlowerPalette pal;
    HealthBand band = classify_health(score);

    switch (band) {
        case HealthBand::BLOOM:     // 90-100: Stage 1 boundary
            pal.leaf   = theme.leaf_vibrant;
            pal.petal  = theme.petal_full;
            pal.center = theme.center_bright;
            pal.stem   = theme.leaf_vibrant;
            pal.text   = {200, 220, 200};
            pal.border = {80, 180, 120};
            break;

        case HealthBand::STRONG:    // 75-89: Stage 1-2
            pal.leaf   = theme.leaf_healthy;
            pal.petal  = theme.petal_soft;
            pal.center = theme.center_bright;
            pal.stem   = theme.leaf_healthy;
            pal.text   = {190, 210, 190};
            pal.border = {70, 160, 110};
            break;

        case HealthBand::STRAINED:  // 60-74: Stage 2-3
            pal.leaf   = theme.leaf_olive;
            pal.petal  = theme.petal_faded;
            pal.center = theme.center_mid;
            pal.stem   = Colour::lerp(theme.leaf_olive, theme.leaf_dry, 0.3);
            pal.text   = {170, 180, 160};
            pal.border = {150, 150, 80};
            break;

        case HealthBand::WILTING:   // 40-59: Stage 3-4
            pal.leaf   = theme.leaf_dry;
            pal.petal  = theme.petal_tan;
            pal.center = theme.center_dim;
            pal.stem   = theme.leaf_dry;
            pal.text   = {150, 140, 120};
            pal.border = {160, 120, 60};
            break;

        case HealthBand::FAILING:   // 20-39: Stage 4-5
            pal.leaf   = theme.leaf_brown;
            pal.petal  = Colour::lerp(theme.petal_tan, theme.petal_dead, 0.5);
            pal.center = theme.center_dim;
            pal.stem   = theme.leaf_brown;
            pal.text   = {130, 110, 90};
            pal.border = {140, 90, 50};
            break;

        case HealthBand::CRITICAL:  // 0-19: Stage 5
            pal.leaf   = theme.leaf_brown;
            pal.petal  = theme.petal_dead;
            pal.center = theme.center_dead;
            pal.stem   = Colour::lerp(theme.leaf_brown, Colour{80, 60, 30}, 0.4);
            pal.text   = {120, 100, 80};
            pal.border = {120, 70, 40};
            break;
    }

    // Sub-band interpolation for smooth transitions within each stage
    double band_lo = 0, band_hi = 0;
    switch (band) {
        case HealthBand::BLOOM:    band_lo = 90;  band_hi = 100; break;
        case HealthBand::STRONG:   band_lo = 75;  band_hi = 89;  break;
        case HealthBand::STRAINED: band_lo = 60;  band_hi = 74;  break;
        case HealthBand::WILTING:  band_lo = 40;  band_hi = 59;  break;
        case HealthBand::FAILING:  band_lo = 20;  band_hi = 39;  break;
        case HealthBand::CRITICAL: band_lo = 0;   band_hi = 19;  break;
    }
    double frac = (band_hi > band_lo)
        ? (score - band_lo) / (band_hi - band_lo)
        : 0.0;
    frac = std::clamp(frac, 0.0, 1.0);

    // Within each band, the lower end is slightly more degraded
    double dim_amount = (1.0 - frac) * 0.08;
    Colour dimmer = {40, 35, 25};
    pal.leaf  = Colour::lerp(pal.leaf,  dimmer, dim_amount);
    pal.petal = Colour::lerp(pal.petal, dimmer, dim_amount);

    return pal;
}

// ============================================================================
// Trend modifier: visual shift based on trajectory
// ============================================================================

inline FlowerPalette apply_trend(FlowerPalette pal, HealthTrend trend) {
    switch (trend) {
        case HealthTrend::FALLING:
            // Petals dim further -- flower looks like it is actively dying
            pal.petal = Colour::lerp(Colour{100, 90, 75}, pal.petal, 0.85);
            pal.center = Colour::lerp(Colour{90, 80, 50}, pal.center, 0.90);
            break;
        case HealthTrend::IMPROVING:
            // Petals recover slightly faster than leaves (intentional)
            pal.petal  = Colour::lerp(pal.petal, Colour{255, 240, 230}, 0.10);
            pal.center = Colour::lerp(pal.center, Colour{250, 210, 80}, 0.08);
            break;
        case HealthTrend::STABLE:
            break;
    }
    return pal;
}

// ============================================================================
// Animation state
// ============================================================================

enum class FlowerAnim : uint8_t {
    NONE          = 0,
    PULSE_CENTER  = 1,   // slow pulse when healthy
    DIM_FLICKER   = 2,   // slight dimming when unstable
    RECOVERY_GLOW = 3,   // brief colour recovery after cleanup
    WARNING_FLASH = 4,   // outline flash on major health drop
    LEAF_DROOP    = 5    // tiny leaf droop at very low health
};

struct FlowerAnimState {
    FlowerAnim  active     = FlowerAnim::NONE;
    int         frame      = 0;
    int         duration   = 0;
    int         prev_score = -1;
};

inline FlowerAnim pick_animation(const HealthScore& hs,
                                 const FlowerAnimState& prev)
{
    if (prev.prev_score >= 0 && hs.score < prev.prev_score - 10)
        return FlowerAnim::WARNING_FLASH;
    if (prev.prev_score >= 0 && hs.score > prev.prev_score + 5)
        return FlowerAnim::RECOVERY_GLOW;
    if (hs.band == HealthBand::BLOOM)
        return FlowerAnim::PULSE_CENTER;
    if (hs.band == HealthBand::CRITICAL || hs.band == HealthBand::FAILING)
        return FlowerAnim::LEAF_DROOP;
    if (hs.trend == HealthTrend::FALLING)
        return FlowerAnim::DIM_FLICKER;
    return FlowerAnim::NONE;
}

inline int anim_duration(FlowerAnim a) {
    switch (a) {
        case FlowerAnim::PULSE_CENTER:  return 60;
        case FlowerAnim::DIM_FLICKER:   return 20;
        case FlowerAnim::RECOVERY_GLOW: return 15;
        case FlowerAnim::WARNING_FLASH: return 10;
        case FlowerAnim::LEAF_DROOP:    return 40;
        case FlowerAnim::NONE:          return 0;
    }
    return 0;
}

inline FlowerAnimState advance_animation(FlowerAnimState state,
                                         const HealthScore& hs)
{
    FlowerAnim next = pick_animation(hs, state);
    bool done = (state.duration > 0 && state.frame >= state.duration);
    bool interrupt = (next == FlowerAnim::WARNING_FLASH
                   || next == FlowerAnim::RECOVERY_GLOW);

    if (done || interrupt || state.active == FlowerAnim::NONE) {
        state.active   = next;
        state.frame    = 0;
        state.duration = anim_duration(next);
    } else {
        state.frame++;
    }
    state.prev_score = hs.score;
    return state;
}

inline FlowerPalette apply_animation(FlowerPalette pal,
                                     const FlowerAnimState& anim)
{
    if (anim.duration == 0) return pal;
    double t = static_cast<double>(anim.frame) / std::max(anim.duration, 1);

    switch (anim.active) {
        case FlowerAnim::PULSE_CENTER: {
            double pulse = 0.5 + 0.5 * std::sin(t * 2.0 * 3.14159265358979);
            double shift = pulse * 0.06;
            pal.center = Colour::lerp(pal.center, Colour{255, 230, 100}, shift);
            break;
        }
        case FlowerAnim::DIM_FLICKER: {
            double flicker = (t > 0.4 && t < 0.6) ? 0.08 : 0.0;
            Colour dark = {60, 50, 40};
            pal.petal  = Colour::lerp(pal.petal, dark, flicker);
            pal.center = Colour::lerp(pal.center, dark, flicker);
            break;
        }
        case FlowerAnim::RECOVERY_GLOW: {
            double glow = (1.0 - t) * 0.12;
            pal.petal = Colour::lerp(pal.petal, Colour{255, 220, 180}, glow);
            break;
        }
        case FlowerAnim::WARNING_FLASH: {
            double flash = (1.0 - t) * 0.25;
            pal.border = Colour::lerp(pal.border, Colour{255, 60, 40}, flash);
            break;
        }
        case FlowerAnim::LEAF_DROOP: {
            double droop = 0.5 + 0.5 * std::sin(t * 2.0 * 3.14159265358979);
            double shift = droop * 0.05;
            Colour dark = {70, 50, 25};
            pal.leaf = Colour::lerp(pal.leaf, dark, shift);
            pal.stem = Colour::lerp(pal.stem, dark, shift);
            break;
        }
        case FlowerAnim::NONE:
            break;
    }
    return pal;
}

// ============================================================================
// render_flower -- draw the ASCII flower into a FrameBuffer
// ============================================================================

inline void render_flower(FrameBuffer& fb, int x0, int y0,
                          const HealthScore& hs,
                          const std::string& node,
                          const FlowerPalette& pal)
{
    int cx = x0 + 6;

    // Row 0: top petal
    fb.put(cx, y0, '*', pal.petal, true);

    // Row 1: upper petals + stem
    fb.put(cx - 1, y0 + 1, '/', pal.petal);
    fb.put(cx,     y0 + 1, '|', pal.stem);
    fb.put(cx + 1, y0 + 1, '\\', pal.petal);

    // Row 2: mid petals + stem
    fb.put(cx - 2, y0 + 2, '/', pal.petal);
    fb.put(cx,     y0 + 2, '|', pal.stem);
    fb.put(cx + 2, y0 + 2, '\\', pal.petal);

    // Row 3: side petals + center
    fb.put(cx - 4, y0 + 3, '*', pal.petal, true);
    fb.put(cx - 3, y0 + 3, '-', pal.stem);
    fb.put(cx - 2, y0 + 3, '/', pal.petal);
    fb.put(cx - 1, y0 + 3, '-', pal.stem);
    fb.put(cx,     y0 + 3, '+', pal.center, true);
    fb.put(cx + 1, y0 + 3, '-', pal.stem);
    fb.put(cx + 2, y0 + 3, '\\', pal.petal);
    fb.put(cx + 3, y0 + 3, '-', pal.stem);
    fb.put(cx + 4, y0 + 3, '*', pal.petal, true);

    // Row 4: upper leaves
    fb.put(cx - 2, y0 + 4, '/', pal.leaf);
    fb.put(cx,     y0 + 4, '|', pal.stem);
    fb.put(cx + 2, y0 + 4, '\\', pal.leaf);

    // Row 5: lower leaves (wider spread)
    fb.put(cx - 3, y0 + 5, '/', pal.leaf);
    fb.put(cx,     y0 + 5, '|', pal.stem);
    fb.put(cx + 3, y0 + 5, '\\', pal.leaf);

    // Row 6: stem base
    fb.put(cx, y0 + 6, '|', pal.stem);

    // Border accent on left edge
    for (int r = 0; r <= 6; ++r)
        fb.put(x0, y0 + r, '|', pal.border);

    // Labels below flower
    int ly = y0 + 8;
    fb.put_string(x0, ly,     node, pal.text, true);
    fb.put_string(x0, ly + 1, "health: " + std::to_string(hs.score), pal.text);
    fb.put_string(x0, ly + 2, "state: " + std::string(band_name(hs.band)), pal.text);
    fb.put_string(x0, ly + 3, "trend: " + std::string(trend_name(hs.trend)), pal.text);
}

// ============================================================================
// render_deductions -- health explanation panel
// ============================================================================

inline void render_deductions(FrameBuffer& fb, int x0, int y0,
                              const HealthScore& hs,
                              const FlowerPalette& pal)
{
    fb.put_string(x0, y0, "Health score: " + std::to_string(hs.score),
                  pal.text, true);

    fb.put_string(x0, y0 + 1, "Deductions:", {180, 180, 200}, true);

    if (hs.deductions.empty()) {
        fb.put_string(x0, y0 + 2, "  (none)", Colour{80, 180, 80});
        return;
    }

    Colour penalty_c = {200, 130, 90};
    int row = 2;
    for (const auto& d : hs.deductions) {
        std::string pen = " -" + std::to_string(d.penalty);
        while (pen.size() < 5) pen += ' ';
        fb.put_string(x0, y0 + row, pen, penalty_c);
        fb.put_string(x0 + static_cast<int>(pen.size()), y0 + row,
                      " " + d.reason, {160, 140, 120});
        ++row;
        if (y0 + row >= fb.H - 1) break;
    }
}

// ============================================================================
// Notification formatters
// ============================================================================

inline std::string health_notification(const HealthScore& hs,
                                       const HealthScore* prev = nullptr)
{
    if (!prev) return {};

    if (hs.score > prev->score) {
        std::string cause;
        for (const auto& pd : prev->deductions) {
            bool still = false;
            for (const auto& d : hs.deductions)
                if (d.reason == pd.reason && d.penalty == pd.penalty)
                    { still = true; break; }
            if (!still) { cause = pd.reason; break; }
        }
        if (cause.empty()) cause = "conditions improved";
        return "\xe2\x9c\xbe health improved to "
               + std::to_string(hs.score) + ": " + cause;
    }

    if (hs.score < prev->score) {
        std::string cause;
        for (const auto& d : hs.deductions) {
            bool existed = false;
            for (const auto& pd : prev->deductions)
                if (pd.reason == d.reason && pd.penalty == d.penalty)
                    { existed = true; break; }
            if (!existed) { cause = d.reason; break; }
        }
        if (cause.empty()) cause = "conditions worsened";
        return "\xe2\x9d\x81 health dropped to "
               + std::to_string(hs.score) + ": " + cause;
    }

    return {};
}

// ============================================================================
// render_flower_dashboard -- combined flower + deductions in one call
// ============================================================================

inline void render_flower_dashboard(FrameBuffer& fb,
                                    int x0, int y0,
                                    const HealthScore& hs,
                                    const std::string& node,
                                    const FlowerPalette& pal)
{
    render_flower(fb, x0, y0, hs, node, pal);
    render_deductions(fb, x0 + 16, y0, hs, pal);
}

} // namespace tui
} // namespace atomistic
