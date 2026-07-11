#pragma once
/**
 * include/vis/vsim_viz_adapter.hpp
 * ==================================
 * Terminal visualization adapter for .vsim-driven runs.
 *
 * Packs the two earlier animation attempts into a unified, dependency-free
 * terminal engine that is activated by [visualization] flags in a .vsim script.
 *
 * Packed patterns:
 *   A — Steady-state proxy overlay cycle  (from cg_anim_demo)
 *       Shows EnsembleProxy table, then animates convergence trace column
 *       by column while the FIRE loop runs. Prints "CONVERGED at step N" on
 *       steady_state, then prints the full overlay column sequence.
 *
 *   B — Post-run snapshot bar-chart       (from seed_bead_demo / SnapshotGraphCollector)
 *       After the run ends, prints an ASCII bar-chart of energy and eta over
 *       every captured frame, then prints the proxy summary row.
 *
 * Entry points (all take VisualizationSection from the parsed .vsim doc):
 *
 *   VsimVizAdapter::begin(doc)
 *       Call once before simulation loop. Prints header and labels.
 *
 *   VsimVizAdapter::step(doc, step, rms_force, energy, avg_eta, avg_C, converged)
 *       Call every simulation step. Handles spark-line, convergence marker.
 *
 *   VsimVizAdapter::convergence_table(doc, proxy_rows)
 *       Call after all scenes converge. Prints proxy summary table.
 *
 *   VsimVizAdapter::snapshot_chart(doc, energy_trace, eta_trace)
 *       Call with per-step vectors. Prints bar-chart of energy + eta.
 *
 *   VsimVizAdapter::event_panels(doc, log)
 *       Call with a KernelEventLog. Emits the panels declared in [visualization].
 *
 *   VsimVizAdapter::gl_config(doc)  ->  VsimGlConfig
 *       Extracts GL overlay config from doc.visualization for forwarding to
 *       CGVizViewer / SeedBeadViewer when BUILD_VISUALIZATION is defined.
 *
 * WO-56C  |  v5.0.0-beta.7.1
 *
 * NOTE: References doc.visual (VisualSection) after [export.visual] split.
 */

#include "vsim/vsim_document.hpp"
#include "kernel/kernel_event_log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// ANSI helpers (duplicated locally so this header is self-contained)
// ============================================================================

namespace viz_ansi {
    constexpr const char* reset   = "\033[0m";
    constexpr const char* bold    = "\033[1m";
    constexpr const char* dim     = "\033[2m";
    constexpr const char* green   = "\033[32m";
    constexpr const char* yellow  = "\033[33m";
    constexpr const char* cyan    = "\033[36m";
    constexpr const char* white   = "\033[37m";
    constexpr const char* magenta = "\033[35m";
    constexpr const char* red     = "\033[31m";
}

// ============================================================================
// ProxyRow — a converged scene result (from cg_anim_demo pattern A)
// ============================================================================

struct ProxyRow {
    std::string name;
    double cohesion      = 0.0;
    double uniformity    = 0.0;
    double texture       = 0.0;
    double stabilization = 0.0;
    double surf_sensitivity = 0.0;
    int    bead_count    = 0;
    bool   valid         = false;
};

// ============================================================================
// GL config forwarding struct (for BUILD_VISUALIZATION callers)
// ============================================================================

struct VsimGlConfig {
    bool   active          = false;  // false if output_type is not a GL mode
    bool   show_axes       = true;
    bool   show_neighbours = true;
    float  overlay_hold_s  = 2.5f;
    bool   auto_orbit      = true;
    int    window_width    = 1280;
    int    window_height   = 800;
    std::vector<std::string> overlay_sequence;
};

// ============================================================================
// VsimVizAdapter
// ============================================================================

class VsimVizAdapter {
public:

    // -----------------------------------------------------------------------
    // begin — print session header according to output_type
    // -----------------------------------------------------------------------
    static void begin(const VsimDocument& doc) {
        const auto& v = doc.visual;
        if (!v.is_any_mode()) return;

        using namespace viz_ansi;
        std::printf("\n%s%s══════════════════════════════════════════════════════%s\n",
            bold, cyan, reset);
        std::printf("%s%s  %s  |  viz: %s  |  anim: %s%s\n",
            bold, cyan,
            doc.project.name.c_str(),
            v.output_type.c_str(),
            v.animation_mode.c_str(),
            reset);
        std::printf("%s%s══════════════════════════════════════════════════════%s\n\n",
            bold, cyan, reset);

        if (v.output_type == "terminal_chart" && v.show_convergence_trace) {
            // Print convergence trace header (metal_sim pattern)
            std::printf("%s  %-8s  %-14s  %-12s  %-10s  %-10s  %s\n%s",
                white,
                "Step", "RMS Force", "Energy", "avg_eta", "avg_C", "Conv",
                reset);
            std::printf("  %s\n", std::string(70, '-').c_str());
        }
    }

    // -----------------------------------------------------------------------
    // step — called every simulation step
    // Pattern A: spark-line (animation_mode = "spark")
    // Pattern A: per-step row  (animation_mode = "bar" / convergence_trace)
    // -----------------------------------------------------------------------
    static void step(const VsimDocument& doc,
                     uint64_t step_n,
                     double rms_force,
                     double energy,
                     double avg_eta,
                     double avg_C,
                     bool   converged)
    {
        const auto& v = doc.visual;
        if (!v.is_terminal_mode()) return;

        using namespace viz_ansi;

        if (v.show_convergence_trace && v.output_type == "terminal_chart") {
            // Print row (mirrors metal_sim live trace)
            std::printf("  %-8llu  %-14.5f  %-12.4f  %-10.4f  %-10.2f",
                (unsigned long long)step_n,
                rms_force, energy, avg_eta, avg_C);
            if (converged)
                std::printf("  %s✓ CONVERGED%s", green, reset);
            std::printf("\n");
        } else if (v.animation_mode == "spark" && step_n % 25 == 0) {
            // Minimal spark-line: one char per 25 steps
            char c = energy < -100.0 ? '#' : energy < -50.0 ? '+' : energy < 0.0 ? '.' : '?';
            std::printf("%c", c);
            std::fflush(stdout);
        }

        if (converged && v.show_steady_state_marker) {
            std::printf("\n%s%s  ✓ STEADY STATE reached at step %llu%s\n\n",
                bold, green, (unsigned long long)step_n, reset);
        }
    }

    // -----------------------------------------------------------------------
    // convergence_table — print EnsembleProxy summary table (Pattern A)
    // from cg_anim_demo headless path
    // -----------------------------------------------------------------------
    static void convergence_table(const VsimDocument& doc,
                                  const std::vector<ProxyRow>& rows)
    {
        const auto& v = doc.visual;
        if (!v.show_proxy_table) return;
        if (!v.is_any_mode()) return;

        using namespace viz_ansi;
        std::printf("\n%s%s── Proxy Summary ──%s\n", bold, white, reset);
        std::printf("  %s%-42s  %9s  %9s  %9s  %9s  %9s  %5s  %5s%s\n",
            dim,
            "Scene", "cohesion", "uniform", "texture",
            "stab", "surf_sens", "N", "valid",
            reset);
        std::printf("  %s\n", std::string(110, '-').c_str());

        for (const auto& r : rows) {
            const char* vcol = r.valid ? green : red;
            std::printf("  %-42s  %9.4f  %9.4f  %9.4f  %9.4f  %9.4f  %5d  %s%5s%s\n",
                r.name.c_str(),
                r.cohesion, r.uniformity, r.texture,
                r.stabilization, r.surf_sensitivity,
                r.bead_count,
                vcol, r.valid ? "yes" : "no", reset);
        }
        std::printf("  %s\n\n", std::string(110, '-').c_str());
    }

    // -----------------------------------------------------------------------
    // snapshot_chart — post-run bar chart of energy + eta (Pattern B)
    // from seed_bead_demo / SnapshotGraphCollector concept
    // -----------------------------------------------------------------------
    static void snapshot_chart(const VsimDocument& doc,
                                const std::vector<double>& energy_trace,
                                const std::vector<double>& eta_trace)
    {
        const auto& v = doc.visual;
        if (!v.show_snapshot_chart) return;
        if (!v.is_any_mode()) return;
        if (energy_trace.empty()) return;

        using namespace viz_ansi;

        // Normalize energy to [0,1] for bar width (invert — more negative = longer bar)
        double e_min = *std::min_element(energy_trace.begin(), energy_trace.end());
        double e_max = *std::max_element(energy_trace.begin(), energy_trace.end());
        double e_range = (e_max - e_min) < 1e-12 ? 1.0 : (e_max - e_min);

        double eta_max = *std::max_element(eta_trace.begin(), eta_trace.end());
        if (eta_max < 1e-12) eta_max = 1.0;

        constexpr int BAR_W = 40;
        constexpr int MAX_ROWS = 30;  // Downsample if longer

        std::printf("\n%s%s── Snapshot Chart (energy + eta) ──%s\n", bold, white, reset);
        std::printf("  %sframe  %-*s  %-*s%s\n",
            dim, BAR_W, "energy (neg=stable)", BAR_W, "eta", reset);
        std::printf("  %s\n", std::string(BAR_W * 2 + 10, '-').c_str());

        size_t n = energy_trace.size();
        size_t step_sz = std::max(size_t(1), n / MAX_ROWS);

        for (size_t i = 0; i < n; i += step_sz) {
            double e   = energy_trace[i];
            double eta = (i < eta_trace.size()) ? eta_trace[i] : 0.0;

            // Energy bar: invert so lower energy = longer bar
            int e_len = static_cast<int>(BAR_W * (e_max - e) / e_range);
            e_len = std::max(0, std::min(BAR_W, e_len));

            int eta_len = static_cast<int>(BAR_W * eta / eta_max);
            eta_len = std::max(0, std::min(BAR_W, eta_len));

            std::string e_bar(e_len, '#');
            std::string eta_bar(eta_len, '|');

            std::printf("  %5zu  %s%-*s%s  %s%-*s%s\n",
                i,
                cyan, BAR_W, e_bar.c_str(), reset,
                yellow, BAR_W, eta_bar.c_str(), reset);
        }
        std::printf("\n");
    }

    // -----------------------------------------------------------------------
    // overlay_cycle — print the declared overlay sequence as a text
    //                 column header list (terminal fallback for GL mode)
    // -----------------------------------------------------------------------
    static void overlay_cycle(const VsimDocument& doc) {
        const auto& v = doc.visual;
        if (v.overlay_sequence.empty()) return;

        using namespace viz_ansi;
        std::printf("\n%s%s── Overlay Cycle Sequence ──%s\n", bold, white, reset);
        for (size_t i = 0; i < v.overlay_sequence.size(); ++i) {
            std::printf("  %s[%zu]%s  %s%s%.2f s hold%s\n",
                bold, i + 1, reset,
                cyan, v.overlay_sequence[i].c_str(), reset,
                v.gl_overlay_hold_s, reset);
        }
        if (v.is_gl_mode()) {
            std::printf("  %s(GL path — forward to CGVizViewer / SeedBeadViewer)%s\n\n",
                dim, reset);
        } else {
            std::printf("  %s(terminal fallback — GL viewer not active)%s\n\n",
                dim, reset);
        }
    }

    // -----------------------------------------------------------------------
    // event_panels — emit kernel event panels declared in [visualization]
    //                Uses KernelEventLog directly (no GL dependency)
    // -----------------------------------------------------------------------
    static void event_panels(const VsimDocument& doc,
                              const vsepr::kernel::KernelEventLog& log)
    {
        const auto& v = doc.visual;
        if (!v.is_terminal_mode()) return;

        using namespace viz_ansi;
        using namespace vsepr::kernel;

        auto all = log.snapshot();
        if (all.empty()) return;

        // ── Event Timeline panel ──────────────────────────────────────────
        if (v.show_event_timeline) {
            constexpr int RULER_W = 50;
            uint64_t f_min = all.front().frame_id;
            uint64_t f_max = all.front().frame_id;
            for (const auto& e : all) {
                if (e.frame_id < f_min) f_min = e.frame_id;
                if (e.frame_id > f_max) f_max = e.frame_id;
            }
            uint64_t f_range = (f_max > f_min) ? (f_max - f_min) : 1;

            std::printf("\n%s%s── Event Timeline  [frame %llu … %llu] ──%s\n",
                bold, white,
                (unsigned long long)f_min,
                (unsigned long long)f_max,
                reset);

            const KernelEventKind kinds[] = {
                KernelEventKind::Reaction, KernelEventKind::ChemicalState,
                KernelEventKind::Formation, KernelEventKind::Defect,
                KernelEventKind::Transport, KernelEventKind::ContinualReport,
            };
            for (auto k : kinds) {
                auto filtered = log.filter_by_kind(k);
                if (filtered.empty()) continue;

                std::string row(RULER_W, '\xB7');  // middle dots
                for (const auto& e : filtered) {
                    int pos = static_cast<int>(
                        (e.frame_id - f_min) * (RULER_W - 1) / f_range);
                    pos = std::max(0, std::min(RULER_W - 1, pos));
                    row[pos] = '#';
                }
                std::printf("  %s%-18s%s  %s\n",
                    cyan, kind_name(k), reset, row.c_str());
            }
        }

        // ── Bar chart panel ───────────────────────────────────────────────
        if (v.show_bar_chart) {
            std::printf("\n%s%s── Per-Kind Event Count ──%s\n", bold, white, reset);
            const KernelEventKind kinds[] = {
                KernelEventKind::Reaction, KernelEventKind::ChemicalState,
                KernelEventKind::Formation, KernelEventKind::Defect,
                KernelEventKind::Transport, KernelEventKind::ContinualReport,
            };
            int max_count = 1;
            for (auto k : kinds)
                max_count = std::max(max_count, (int)log.filter_by_kind(k).size());

            for (auto k : kinds) {
                int n = (int)log.filter_by_kind(k).size();
                int bar = n * 40 / max_count;
                std::printf("  %s%-18s%s  %s%-*s%s  %d\n",
                    cyan, kind_name(k), reset,
                    yellow, bar, std::string(bar, '#').c_str(), reset,
                    n);
            }
        }

        // ── Symbolic trace panel ──────────────────────────────────────────
        if (v.show_symbolic_trace) {
            std::printf("\n%s%s── Symbolic Trace ──%s\n", bold, white, reset);
            for (const auto& e : all) {
                if (e.equation_symbolic.empty()) continue;
                std::printf("  %s[%llu]%s  %s%s%s  %s\n",
                    dim, (unsigned long long)e.event_id, reset,
                    cyan, kind_name(e.kind), reset,
                    e.source_formula.c_str());
                std::printf("    %ssymbolic: %s%s\n", dim, e.equation_symbolic.c_str(), reset);
                std::printf("    %snumeric:  %s%s\n", dim, e.equation_numeric.c_str(), reset);
            }
        }

        // ── Animation cue panel ───────────────────────────────────────────
        if (v.show_animation_cues) {
            overlay_cycle(doc);
        }

        // ── Audit table panel ─────────────────────────────────────────────
        if (v.show_audit_table) {
            std::printf("\n%s%s── Event Audit Table ──%s\n", bold, white, reset);
            std::printf("  %s%-6s  %-18s  %-7s  %-14s  %-12s  %-12s  %-5s%s\n",
                dim,
                "ID", "Kind", "Frame", "Formula", "Result", "Unit", "Valid",
                reset);
            std::printf("  %s\n", std::string(82, '-').c_str());
            for (const auto& e : all) {
                const char* vcol = e.is_valid ? green : red;
                std::printf("  %-6llu  %-18s  %-7llu  %-14s  %-12.4g  %-12s  %s%s%s\n",
                    (unsigned long long)e.event_id,
                    kind_name(e.kind),
                    (unsigned long long)e.frame_id,
                    e.source_formula.c_str(),
                    e.result_value,
                    e.result_unit.c_str(),
                    vcol, e.is_valid ? "OK" : "INVALID", reset);
            }
        }

        // ── Summary ───────────────────────────────────────────────────────
        std::printf("\n%s  %zu events | %s%s\n\n",
            dim, all.size(), doc.project.name.c_str(), reset);
    }

    // -----------------------------------------------------------------------
    // gl_config — extract GL forwarding config
    // -----------------------------------------------------------------------
    static VsimGlConfig gl_config(const VsimDocument& doc) {
        const auto& v = doc.visual;
        VsimGlConfig cfg;
        cfg.active           = v.is_gl_mode();
        cfg.show_axes        = v.gl_show_axes;
        cfg.show_neighbours  = v.gl_show_neighbours;
        cfg.overlay_hold_s   = v.gl_overlay_hold_s;
        cfg.auto_orbit       = v.gl_auto_orbit;
        cfg.window_width     = v.gl_window_width;
        cfg.window_height    = v.gl_window_height;
        cfg.overlay_sequence = v.overlay_sequence;
        return cfg;
    }
};

} // namespace vsim
