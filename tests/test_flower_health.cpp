/**
 * test_flower_health.cpp -- Flower health indicator tests
 * VSEPR-SIM 3.0.1
 *
 * T1-T15:  Scoring engine (deductions, stacking, clamp)
 * T16:     Band classification
 * T17-T19: Trend detection
 * T20-T25: 5-stage withering palette
 * T26-T28: Trend modifier
 * T29-T30: Animation state machine
 * T31-T33: Render (flower, deductions, dashboard)
 * T34-T35: Notifications (drop, recovery)
 * T36-T37: Name coverage
 * T38:     Theme generation from primary petal
 * T39:     Sub-band interpolation smoothness
 * T40:     Full pipeline: metrics -> score -> theme -> palette -> render
 */

#include "atomistic/tui/flower_health.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace atomistic::tui;

// ---- T1: Default metrics -> perfect health ----

static void test_default_perfect() {
    NodeMetrics m;
    auto h = compute_health(m);
    assert(h.score == 100);
    assert(h.band == HealthBand::BLOOM);
    assert(h.deductions.empty());
    std::printf("  T1  PASS: default metrics = 100 BLOOM\n");
}

// ---- T2: CPU thermal bands ----

static void test_cpu_thermal() {
    { NodeMetrics m; m.cpu_temp_C = 65.0;
      assert(compute_health(m).score == 95); }
    { NodeMetrics m; m.cpu_temp_C = 80.0;
      assert(compute_health(m).score == 85); }
    { NodeMetrics m; m.cpu_temp_C = 95.0;
      assert(compute_health(m).score == 70); }
    std::printf("  T2  PASS: CPU thermal bands\n");
}

// ---- T3: Memory pressure ----

static void test_memory_pressure() {
    { NodeMetrics m; m.memory_usage_pct = 85.0;
      assert(compute_health(m).score == 90); }
    { NodeMetrics m; m.memory_usage_pct = 95.0;
      assert(compute_health(m).score == 75); }
    std::printf("  T3  PASS: memory pressure\n");
}

// ---- T4: Swap creep ----

static void test_swap_creep() {
    { NodeMetrics m; m.swap_usage_pct = 20.0;
      assert(compute_health(m).score == 97); }
    { NodeMetrics m; m.swap_usage_pct = 50.0;
      assert(compute_health(m).score == 90); }
    std::printf("  T4  PASS: swap creep\n");
}

// ---- T5: Disk usage ----

static void test_disk_usage() {
    { NodeMetrics m; m.disk_usage_pct = 85.0;
      assert(compute_health(m).score == 92); }
    { NodeMetrics m; m.disk_usage_pct = 95.0;
      assert(compute_health(m).score == 80); }
    std::printf("  T5  PASS: disk usage\n");
}

// ---- T6: Services and zombies ----

static void test_services_zombies() {
    { NodeMetrics m; m.failed_services = 1;
      assert(compute_health(m).score == 95); }
    { NodeMetrics m; m.failed_services = 5;
      assert(compute_health(m).score == 85); }
    { NodeMetrics m; m.zombie_processes = 3;
      assert(compute_health(m).score == 97); }
    { NodeMetrics m; m.zombie_processes = 10;
      assert(compute_health(m).score == 90); }
    std::printf("  T6  PASS: services + zombies\n");
}

// ---- T7: Load saturation ----

static void test_load_saturation() {
    { NodeMetrics m; m.load_per_core = 2.0;
      assert(compute_health(m).score == 95); }
    { NodeMetrics m; m.load_per_core = 3.0;
      assert(compute_health(m).score == 90); }
    std::printf("  T7  PASS: load saturation\n");
}

// ---- T8: I/O wait ----

static void test_iowait() {
    { NodeMetrics m; m.iowait_pct = 10.0;
      assert(compute_health(m).score == 95); }
    { NodeMetrics m; m.iowait_pct = 20.0;
      assert(compute_health(m).score == 88); }
    std::printf("  T8  PASS: I/O wait\n");
}

// ---- T9: GPU thermal / throttling ----

static void test_gpu_thermal() {
    { NodeMetrics m; m.gpu_present = true; m.gpu_temp_C = 75.0;
      assert(compute_health(m).score == 95); }
    { NodeMetrics m; m.gpu_present = true; m.gpu_temp_C = 90.0;
      assert(compute_health(m).score == 85); }
    { NodeMetrics m; m.gpu_present = true; m.gpu_throttling = true;
      assert(compute_health(m).score == 70); }
    { NodeMetrics m; m.gpu_present = false; m.gpu_temp_C = 99.0;
      assert(compute_health(m).score == 100); }
    std::printf("  T9  PASS: GPU thermal + throttling\n");
}

// ---- T10: Log explosion ----

static void test_log_explosion() {
    { NodeMetrics m; m.log_oversized = true;
      assert(compute_health(m).score == 96); }
    { NodeMetrics m; m.log_severe_growth = true;
      assert(compute_health(m).score == 92); }
    std::printf("  T10 PASS: log explosion\n");
}

// ---- T11: Thermal drift ----

static void test_thermal_drift() {
    { NodeMetrics m; m.thermal_drift = true;
      assert(compute_health(m).score == 95); }
    { NodeMetrics m; m.thermal_drift_suspicious = true;
      assert(compute_health(m).score == 90); }
    std::printf("  T11 PASS: thermal drift\n");
}

// ---- T12: Cleanup backlog ----

static void test_cleanup_backlog() {
    { NodeMetrics m; m.cleanup_overdue = true;
      assert(compute_health(m).score == 97); }
    { NodeMetrics m; m.cleanup_serious = true;
      assert(compute_health(m).score == 92); }
    std::printf("  T12 PASS: cleanup backlog\n");
}

// ---- T13: Cache overload ----

static void test_cache_overload() {
    { NodeMetrics m; m.cache_usage_pct = 85.0;
      assert(compute_health(m).score == 95); }
    { NodeMetrics m; m.cache_usage_pct = 95.0;
      assert(compute_health(m).score == 88); }
    std::printf("  T13 PASS: cache overload\n");
}

// ---- T14: Multiple deductions stack ----

static void test_stacking() {
    NodeMetrics m;
    m.cpu_temp_C = 80.0;        // -15
    m.memory_usage_pct = 85.0;  // -10
    m.swap_usage_pct = 50.0;    // -10
    m.cleanup_overdue = true;   // -3
    auto h = compute_health(m);
    assert(h.score == 62);
    assert(h.deductions.size() == 4);
    std::printf("  T14 PASS: deductions stack (score=%d, n=%zu)\n",
                h.score, h.deductions.size());
}

// ---- T15: Score clamps to 0 ----

static void test_clamp_zero() {
    NodeMetrics m;
    m.cpu_temp_C = 95.0;
    m.memory_usage_pct = 95.0;
    m.disk_usage_pct = 95.0;
    m.failed_services = 5;
    m.zombie_processes = 10;
    m.load_per_core = 3.0;
    m.iowait_pct = 20.0;
    m.gpu_present = true;
    m.gpu_throttling = true;
    auto h = compute_health(m);
    assert(h.score == 0);
    assert(h.band == HealthBand::CRITICAL);
    std::printf("  T15 PASS: score clamps to 0\n");
}

// ---- T16: Band classification ----

static void test_band_classify() {
    assert(classify_health(100) == HealthBand::BLOOM);
    assert(classify_health(90)  == HealthBand::BLOOM);
    assert(classify_health(89)  == HealthBand::STRONG);
    assert(classify_health(75)  == HealthBand::STRONG);
    assert(classify_health(74)  == HealthBand::STRAINED);
    assert(classify_health(60)  == HealthBand::STRAINED);
    assert(classify_health(59)  == HealthBand::WILTING);
    assert(classify_health(40)  == HealthBand::WILTING);
    assert(classify_health(39)  == HealthBand::FAILING);
    assert(classify_health(20)  == HealthBand::FAILING);
    assert(classify_health(19)  == HealthBand::CRITICAL);
    assert(classify_health(0)   == HealthBand::CRITICAL);
    std::printf("  T16 PASS: band classification\n");
}

// ---- T17: Trend -- stable ----

static void test_trend_stable() {
    auto h = compute_health(NodeMetrics(), {100, 100, 100});
    assert(h.trend == HealthTrend::STABLE);
    std::printf("  T17 PASS: trend stable\n");
}

// ---- T18: Trend -- falling ----

static void test_trend_falling() {
    NodeMetrics m; m.cpu_temp_C = 95.0;  // score 70
    auto h = compute_health(m, {95, 90, 85});
    assert(h.trend == HealthTrend::FALLING);
    std::printf("  T18 PASS: trend falling\n");
}

// ---- T19: Trend -- improving ----

static void test_trend_improving() {
    auto h = compute_health(NodeMetrics(), {60, 65, 70});
    assert(h.trend == HealthTrend::IMPROVING);
    std::printf("  T19 PASS: trend improving\n");
}

// ---- T20: 5-stage withering -- BLOOM palette ----

static void test_stage_bloom() {
    auto theme = build_theme();
    auto pal = resolve_palette(95, theme);
    // Leaves should be vibrant green
    assert(pal.leaf.g > 150);
    assert(pal.leaf.g > pal.leaf.r);
    // Petals should be full theme colour
    assert(pal.petal.r > 200);
    // Center should be bright gold
    assert(pal.center.r > 220);
    std::printf("  T20 PASS: BLOOM palette (stage 1)\n");
}

// ---- T21: STRONG palette (stage 1-2) ----

static void test_stage_strong() {
    auto theme = build_theme();
    auto bloom = resolve_palette(95, theme);
    auto strong = resolve_palette(80, theme);
    // Leaves should be slightly muted compared to BLOOM
    assert(strong.leaf.g < bloom.leaf.g);
    // Petals should be softened
    assert(strong.petal.r <= bloom.petal.r);
    std::printf("  T21 PASS: STRONG palette (stage 1-2)\n");
}

// ---- T22: STRAINED palette (stage 2-3) ----

static void test_stage_strained() {
    auto theme = build_theme();
    auto pal = resolve_palette(65, theme);
    // Leaves should be olive (r > 100, g between 100-160)
    assert(pal.leaf.r >= 100);
    assert(pal.leaf.g >= 100 && pal.leaf.g <= 160);
    std::printf("  T22 PASS: STRAINED palette (stage 2-3)\n");
}

// ---- T23: WILTING palette (stage 3-4) ----

static void test_stage_wilting() {
    auto theme = build_theme();
    auto pal = resolve_palette(50, theme);
    // Leaves should be dry (brownish-green)
    assert(pal.leaf.r > pal.leaf.g - 30);
    // Petals should be faded toward tan
    assert(pal.petal.r < 200);
    std::printf("  T23 PASS: WILTING palette (stage 3-4)\n");
}

// ---- T24: FAILING palette (stage 4-5) ----

static void test_stage_failing() {
    auto theme = build_theme();
    auto pal = resolve_palette(25, theme);
    // Leaves should be brown
    assert(pal.leaf.r >= pal.leaf.g);
    std::printf("  T24 PASS: FAILING palette (stage 4-5)\n");
}

// ---- T25: CRITICAL palette (stage 5) ----

static void test_stage_critical() {
    auto theme = build_theme();
    auto pal = resolve_palette(5, theme);
    // Leaves brown, petals dead, center dead
    assert(pal.leaf.r >= pal.leaf.g);
    assert(pal.petal.r < 160);
    assert(pal.center.r < 140);
    std::printf("  T25 PASS: CRITICAL palette (stage 5)\n");
}

// ---- T26: Leaves degrade before petals ----

static void test_leaves_first() {
    auto theme = build_theme();
    auto bloom = resolve_palette(95, theme);
    auto strained = resolve_palette(65, theme);
    double leaf_delta = std::abs(bloom.leaf.g - strained.leaf.g)
                      + std::abs(bloom.leaf.r - strained.leaf.r);
    double petal_delta = std::abs(bloom.petal.r - strained.petal.r)
                       + std::abs(bloom.petal.g - strained.petal.g);
    assert(leaf_delta > petal_delta);
    std::printf("  T26 PASS: leaves degrade before petals (leaf=%.0f petal=%.0f)\n",
                leaf_delta, petal_delta);
}

// ---- T27: Trend falling dims petals ----

static void test_trend_falling_dims() {
    auto theme = build_theme();
    auto pal = resolve_palette(70, theme);
    auto dimmed = apply_trend(pal, HealthTrend::FALLING);
    int before = pal.petal.r + pal.petal.g + pal.petal.b;
    int after  = dimmed.petal.r + dimmed.petal.g + dimmed.petal.b;
    assert(after < before);
    std::printf("  T27 PASS: falling trend dims petals\n");
}

// ---- T28: Trend improving brightens petals ----

static void test_trend_improving_brightens() {
    auto theme = build_theme();
    auto pal = resolve_palette(70, theme);
    auto bright = apply_trend(pal, HealthTrend::IMPROVING);
    int before = pal.petal.r + pal.petal.g + pal.petal.b;
    int after  = bright.petal.r + bright.petal.g + bright.petal.b;
    assert(after > before);
    std::printf("  T28 PASS: improving trend brightens petals\n");
}

// ---- T29: Animation -- BLOOM triggers PULSE_CENTER ----

static void test_anim_bloom_pulse() {
    HealthScore hs; hs.score = 95; hs.band = HealthBand::BLOOM;
    FlowerAnimState prev;
    auto anim = pick_animation(hs, prev);
    assert(anim == FlowerAnim::PULSE_CENTER);
    std::printf("  T29 PASS: BLOOM triggers PULSE_CENTER\n");
}

// ---- T30: Animation -- major drop triggers WARNING_FLASH ----

static void test_anim_warning_flash() {
    HealthScore hs; hs.score = 60; hs.band = HealthBand::STRAINED;
    FlowerAnimState prev; prev.prev_score = 85;
    auto anim = pick_animation(hs, prev);
    assert(anim == FlowerAnim::WARNING_FLASH);
    std::printf("  T30 PASS: major drop triggers WARNING_FLASH\n");
}

// ---- T31: Render flower populates buffer ----

static void test_render_flower() {
    FrameBuffer fb(50, 20);
    fb.clear();
    auto hs = compute_health(NodeMetrics());
    auto theme = build_theme();
    auto pal = resolve_palette(hs.score, theme);
    render_flower(fb, 1, 1, hs, "cmp-01", pal);
    // Center '+' at (1+6, 1+3) = (7, 4)
    assert(fb.at(7, 4).ch == '+');
    // Top petal at (7, 1)
    assert(fb.at(7, 1).ch == '*');
    // Node label starts at (1, 9)
    assert(fb.at(1, 9).ch == 'c');
    std::printf("  T31 PASS: render_flower populates buffer\n");
}

// ---- T32: Render deductions ----

static void test_render_deductions() {
    FrameBuffer fb(60, 20);
    fb.clear();
    NodeMetrics m; m.cpu_temp_C = 80.0; m.swap_usage_pct = 50.0;
    auto hs = compute_health(m);
    auto theme = build_theme();
    auto pal = resolve_palette(hs.score, theme);
    render_deductions(fb, 1, 1, hs, pal);
    // "Health score:" at row 1
    assert(fb.at(1, 1).ch == 'H');
    // "Deductions:" at row 2
    assert(fb.at(1, 2).ch == 'D');
    // First deduction at row 3
    assert(fb.at(2, 3).ch == '-');
    std::printf("  T32 PASS: render_deductions\n");
}

// ---- T33: Dashboard combines flower + deductions ----

static void test_render_dashboard() {
    FrameBuffer fb(60, 20);
    fb.clear();
    NodeMetrics m; m.cpu_temp_C = 65.0;
    auto hs = compute_health(m);
    auto theme = build_theme();
    auto pal = resolve_palette(hs.score, theme);
    render_flower_dashboard(fb, 1, 1, hs, "sim-02", pal);
    // Flower center at (7, 4)
    assert(fb.at(7, 4).ch == '+');
    // Deductions panel at x=17, y=1 starts with 'H'
    assert(fb.at(17, 1).ch == 'H');
    std::printf("  T33 PASS: render_flower_dashboard\n");
}

// ---- T34: Notification on drop ----

static void test_notification_drop() {
    auto prev = compute_health(NodeMetrics());
    NodeMetrics m; m.cpu_temp_C = 80.0;
    auto curr = compute_health(m);
    auto msg = health_notification(curr, &prev);
    assert(!msg.empty());
    assert(msg.find("dropped") != std::string::npos);
    assert(msg.find("85") != std::string::npos);
    assert(msg.find("CPU high") != std::string::npos);
    std::printf("  T34 PASS: notification on drop\n");
}

// ---- T35: Notification on recovery ----

static void test_notification_recovery() {
    NodeMetrics m_sick; m_sick.cpu_temp_C = 80.0;
    auto prev = compute_health(m_sick);
    auto curr = compute_health(NodeMetrics());
    auto msg = health_notification(curr, &prev);
    assert(!msg.empty());
    assert(msg.find("improved") != std::string::npos);
    assert(msg.find("CPU high") != std::string::npos);
    std::printf("  T35 PASS: notification on recovery\n");
}

// ---- T36: Band names ----

static void test_band_names() {
    assert(std::string(band_name(HealthBand::BLOOM))    == "BLOOM");
    assert(std::string(band_name(HealthBand::STRONG))   == "STRONG");
    assert(std::string(band_name(HealthBand::STRAINED)) == "STRAINED");
    assert(std::string(band_name(HealthBand::WILTING))   == "WILTING");
    assert(std::string(band_name(HealthBand::FAILING))  == "FAILING");
    assert(std::string(band_name(HealthBand::CRITICAL)) == "CRITICAL");
    std::printf("  T36 PASS: all band names\n");
}

// ---- T37: Trend names ----

static void test_trend_names() {
    assert(std::string(trend_name(HealthTrend::STABLE))    == "stable");
    assert(std::string(trend_name(HealthTrend::IMPROVING)) == "improving");
    assert(std::string(trend_name(HealthTrend::FALLING))   == "falling");
    std::printf("  T37 PASS: all trend names\n");
}

// ---- T38: Theme generation ----

static void test_theme_generation() {
    auto theme = build_theme({200, 80, 120});
    assert(theme.petal_full.r == 200);
    assert(theme.petal_full.g == 80);
    // Soft should be lighter (closer to beige)
    assert(theme.petal_soft.g > theme.petal_full.g);
    // Dead should be muted brown
    assert(theme.petal_dead.r == 140);
    assert(theme.petal_dead.g == 120);
    // Leaf stages should be ordered: vibrant > healthy > olive > dry > brown
    assert(theme.leaf_vibrant.g > theme.leaf_healthy.g);
    assert(theme.leaf_healthy.g > theme.leaf_olive.g);
    assert(theme.leaf_olive.g > theme.leaf_dry.g);
    std::printf("  T38 PASS: theme generation from primary petal\n");
}

// ---- T39: Sub-band interpolation ----

static void test_sub_band_smoothness() {
    auto theme = build_theme();
    auto top = resolve_palette(74, theme);    // top of STRAINED
    auto bot = resolve_palette(60, theme);    // bottom of STRAINED
    // Bottom should be slightly dimmer (sub-band dim)
    int top_bright = top.leaf.r + top.leaf.g + top.leaf.b;
    int bot_bright = bot.leaf.r + bot.leaf.g + bot.leaf.b;
    assert(bot_bright <= top_bright);
    std::printf("  T39 PASS: sub-band interpolation (top=%d bot=%d)\n",
                top_bright, bot_bright);
}

// ---- T40: Full pipeline ----

static void test_full_pipeline() {
    NodeMetrics m;
    m.cpu_temp_C = 75.0;      // -5
    m.swap_usage_pct = 30.0;  // -3
    m.cleanup_overdue = true;  // -3
    auto hs = compute_health(m, {92, 91, 90});
    assert(hs.score == 89);
    assert(hs.band == HealthBand::STRONG);
    assert(hs.deductions.size() == 3);

    auto theme = build_theme();
    auto pal = resolve_palette(hs.score, theme);
    pal = apply_trend(pal, hs.trend);

    FlowerAnimState anim;
    anim = advance_animation(anim, hs);
    pal = apply_animation(pal, anim);

    FrameBuffer fb(60, 20);
    fb.clear();
    render_flower_dashboard(fb, 1, 1, hs, "cmp-03", pal);
    assert(fb.at(7, 4).ch == '+');

    auto msg = health_notification(hs, nullptr);
    // No prev -> empty
    assert(msg.empty());

    std::printf("  T40 PASS: full pipeline (metrics->score->theme->palette->render)\n");
}

// ---- main ----

int main() {
    std::printf("=== Flower Health Indicator Tests (40) ===\n\n");

    test_default_perfect();      // T1
    test_cpu_thermal();          // T2
    test_memory_pressure();      // T3
    test_swap_creep();           // T4
    test_disk_usage();           // T5
    test_services_zombies();     // T6
    test_load_saturation();      // T7
    test_iowait();               // T8
    test_gpu_thermal();          // T9
    test_log_explosion();        // T10
    test_thermal_drift();        // T11
    test_cleanup_backlog();      // T12
    test_cache_overload();       // T13
    test_stacking();             // T14
    test_clamp_zero();           // T15
    test_band_classify();        // T16
    test_trend_stable();         // T17
    test_trend_falling();        // T18
    test_trend_improving();      // T19
    test_stage_bloom();          // T20
    test_stage_strong();         // T21
    test_stage_strained();       // T22
    test_stage_wilting();        // T23
    test_stage_failing();        // T24
    test_stage_critical();       // T25
    test_leaves_first();         // T26
    test_trend_falling_dims();   // T27
    test_trend_improving_brightens(); // T28
    test_anim_bloom_pulse();     // T29
    test_anim_warning_flash();   // T30
    test_render_flower();        // T31
    test_render_deductions();    // T32
    test_render_dashboard();     // T33
    test_notification_drop();    // T34
    test_notification_recovery();// T35
    test_band_names();           // T36
    test_trend_names();          // T37
    test_theme_generation();     // T38
    test_sub_band_smoothness();  // T39
    test_full_pipeline();        // T40

    std::printf("\n=== ALL 40 TESTS PASSED ===\n");
    return 0;
}
