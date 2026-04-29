/**
 * demo_code_trail.cpp  —  Code Trail Wind v0.1  —  Semi-Visual Demo
 * ====================================================================
 * VSEPR-SIM 3.0.1
 *
 * Performs a realistic Lennard-Jones 6-12 pair interaction calculation
 * for a 4-atom cluster, trails every arithmetic step through CodeTrail,
 * then renders:
 *
 *   1. Live step-by-step formula build (ANSI colour-coded)
 *   2. Full CSV dump to stdout
 *   3. Aggregate statistics panel
 *   4. Particle geometry diagram (ASCII)
 *
 * Self-contained — depends only on vsepr_trail (code_trail.hpp/cpp).
 * No graphics libraries required.
 *
 * Build:
 *   cmake --build build --target demo_code_trail
 * Run:
 *   ./build/demo_code_trail
 */

#include "core/code_trail.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>

using namespace vsepr::trail;

// ============================================================================
// ANSI escape codes
// ============================================================================

namespace ansi {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* DIM     = "\033[2m";
    const char* CYAN    = "\033[0;36m";
    const char* GREEN   = "\033[0;32m";
    const char* YELLOW  = "\033[1;33m";
    const char* RED     = "\033[0;31m";
    const char* MAGENTA = "\033[0;35m";
    const char* WHITE   = "\033[0;37m";
    const char* GRAY    = "\033[0;90m";
    const char* BG_DARK = "\033[48;5;234m";
}

// ============================================================================
// Rendering helpers
// ============================================================================

static void hline(int width = 68) {
    std::cout << ansi::DIM;
    for (int i = 0; i < width; ++i) std::cout << "\xe2\x94\x80";
    std::cout << ansi::RESET << "\n";
}

static void boxed_header(const std::string& title, const std::string& subtitle = "") {
    const int W = 68;
    std::cout << ansi::MAGENTA;
    std::cout << "\xe2\x95\x94";
    for (int i = 0; i < W; ++i) std::cout << "\xe2\x95\x90";
    std::cout << "\xe2\x95\x97\n";

    std::cout << "\xe2\x95\x91  " << ansi::BOLD << std::left << std::setw(W - 4)
              << title << ansi::MAGENTA << "  \xe2\x95\x91\n";
    if (!subtitle.empty()) {
        std::cout << "\xe2\x95\x91  " << ansi::RESET << ansi::MAGENTA
                  << std::left << std::setw(W - 4)
                  << subtitle << "  \xe2\x95\x91\n";
    }
    std::cout << "\xe2\x95\x9a";
    for (int i = 0; i < W; ++i) std::cout << "\xe2\x95\x90";
    std::cout << "\xe2\x95\x9d" << ansi::RESET << "\n";
}

static void section(const std::string& title) {
    std::cout << "\n" << ansi::CYAN << ansi::BOLD
              << "  \xe2\x94\x8c\xe2\x94\x80 " << title << ansi::RESET << "\n";
}

static void kv(const std::string& key, const std::string& val, int kw = 24) {
    std::cout << ansi::CYAN << "  \xe2\x94\x82 " << std::left << std::setw(kw)
              << key << ansi::RESET << val << "\n";
}

static void kv_num(const std::string& key, double val,
                   const std::string& unit = "", int kw = 24) {
    std::ostringstream oss;
    oss << std::setprecision(10) << val;
    if (!unit.empty()) oss << " " << unit;
    kv(key, oss.str(), kw);
}

static void bullet(const std::string& msg) {
    std::cout << ansi::GREEN << "  \xe2\x96\xb6 " << ansi::RESET << msg << "\n";
}

// ============================================================================
// Simple 3D vector for demo (no external deps)
// ============================================================================

struct V3 {
    double x, y, z;
    V3 operator-(const V3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    double len() const { return std::sqrt(x*x + y*y + z*z); }
};

// ============================================================================
// LJ 6-12 walk-through — one pair, fully instrumented
// ============================================================================

static double lj_pair_trail(CodeTrail& trail, int i, int j,
                            double eps, double sig,
                            const V3& ri, const V3& rj) {
    std::string tag = "pair_" + std::to_string(i) + "_" + std::to_string(j);
    TrailScope scope(trail, tag);

    // Step: compute separation vector components
    V3 dr = ri - rj;
    trail.record_binary("subtract", ri.x, rj.x, dr.x, 0.0,
        "dx = x_i - x_j", "Angstrom");
    trail.record_binary("subtract", ri.y, rj.y, dr.y, 0.0,
        "dy = y_i - y_j", "Angstrom");
    trail.record_binary("subtract", ri.z, rj.z, dr.z, 0.0,
        "dz = z_i - z_j", "Angstrom");

    // Step: r^2 = dx^2 + dy^2 + dz^2
    double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
    trail.record_custom("dot_product", r2, r2,
        "r^2 = dx^2 + dy^2 + dz^2", "Angstrom^2");

    // Step: r = sqrt(r^2)
    double r = std::sqrt(r2);
    trail.record_unary("sqrt", r2, r, r, "r = sqrt(r^2)", "Angstrom");

    // Step: sigma/r ratio
    double sr = sig / r;
    trail.record_binary("divide", sig, r, sr, sr, "sr = sigma / r");

    // Step: sr^2
    double sr2 = sr * sr;
    trail.record_unary("square", sr, sr2, sr2, "sr2 = sr^2");

    // Step: sr^6 = (sr^2)^3
    double sr6 = sr2 * sr2 * sr2;
    trail.record_custom("cube", sr6, sr6, "sr6 = (sr^2)^3 = (sigma/r)^6");

    // Step: sr^12 = (sr^6)^2
    double sr12 = sr6 * sr6;
    trail.record_unary("square", sr6, sr12, sr12, "sr12 = sr6^2 = (sigma/r)^12");

    // Step: attractive - repulsive bracketed term
    double bracket = sr12 - sr6;
    trail.record_binary("subtract", sr12, sr6, bracket, bracket,
        "bracket = sr12 - sr6");

    // Step: U_ij = 4 * epsilon * bracket
    double U = 4.0 * eps * bracket;
    trail.record_binary("multiply", 4.0 * eps, bracket, U, U,
        "U_ij = 4 * epsilon * (sr12 - sr6)", "kcal/mol");

    // Step: force magnitude  F = 24*eps/r * (2*sr12 - sr6)
    double F = 24.0 * eps / r * (2.0 * sr12 - sr6);
    trail.record_custom("LJ_force", F, U,
        "F_ij = 24*epsilon/r * (2*sr12 - sr6)", "kcal/mol/Angstrom");

    return U;
}

// ============================================================================
// Wind perturbation walk-through — single atom, fully instrumented
// ============================================================================

static double wind_trail(CodeTrail& trail, int atom_idx,
                         const V3& pos, const V3& wind_dir, double strength,
                         double accumulator) {
    std::string tag = "wind_atom_" + std::to_string(atom_idx);
    TrailScope scope(trail, tag);

    // Step: normalize wind direction
    double dn = wind_dir.len();
    trail.record_unary("norm", dn, dn, accumulator,
        "|d_wind| = sqrt(dx^2 + dy^2 + dz^2)");

    double inv_dn = 1.0 / dn;
    trail.record_unary("reciprocal", dn, inv_dn, accumulator,
        "inv_norm = 1 / |d_wind|");

    // Step: force = strength * direction_hat
    V3 F_wind = {strength * wind_dir.x * inv_dn,
                 strength * wind_dir.y * inv_dn,
                 strength * wind_dir.z * inv_dn};
    trail.record_binary("multiply", strength, inv_dn, F_wind.x, accumulator,
        "Fx_wind = strength * dx_hat", "kcal/mol/Angstrom");

    // Step: energy = -F · r
    double U_wind = -(F_wind.x * pos.x + F_wind.y * pos.y + F_wind.z * pos.z);
    accumulator += U_wind;
    trail.record_custom("dot_product", U_wind, accumulator,
        "U_wind = -(F_wind . r_i)", "kcal/mol");

    return U_wind;
}

// ============================================================================
// Step-by-step playback renderer
// ============================================================================

static void render_step_playback(const CodeTrail& trail) {
    section("Step-by-Step Formula Build");
    std::cout << "\n";

    const auto& entries = trail.entries();

    std::string last_tag;
    for (const auto& e : entries) {
        // Section break when source_tag changes
        if (e.source_tag != last_tag) {
            if (!last_tag.empty()) std::cout << "\n";
            std::cout << ansi::YELLOW << "    ── " << e.source_tag
                      << " ──" << ansi::RESET << "\n";
            last_tag = e.source_tag;
        }

        // Step number
        std::cout << ansi::GRAY << "    [" << std::setw(3) << e.step << "] "
                  << ansi::RESET;

        // Operation kind color
        const char* kind_col = ansi::WHITE;
        switch (e.kind) {
            case OpKind::BINARY:  kind_col = ansi::GREEN;   break;
            case OpKind::UNARY:   kind_col = ansi::CYAN;    break;
            case OpKind::ASSIGN:  kind_col = ansi::YELLOW;  break;
            case OpKind::COMPARE: kind_col = ansi::RED;     break;
            case OpKind::CUSTOM:  kind_col = ansi::MAGENTA; break;
        }

        // Op label
        std::cout << kind_col << std::left << std::setw(14) << e.op_label
                  << ansi::RESET;

        // Formula notation (the core readable expression)
        std::cout << ansi::BOLD << e.formula_notation << ansi::RESET;

        // Result value
        if (!std::isnan(e.result)) {
            std::cout << ansi::DIM << "  = " << std::setprecision(8) << e.result;
            if (!e.unit_label.empty())
                std::cout << " " << e.unit_label;
            std::cout << ansi::RESET;
        }

        std::cout << "\n";
    }
}

// ============================================================================
// ASCII geometry diagram
// ============================================================================

static void render_geometry(const std::vector<V3>& pos,
                            const std::vector<std::string>& labels) {
    section("Cluster Geometry (XY projection, 1 char = 0.5 Angstrom)");
    std::cout << "\n";

    const int W = 50, H = 20;
    std::vector<std::string> grid(H, std::string(W, ' '));

    // Find bounds
    double xmin = 1e20, xmax = -1e20, ymin = 1e20, ymax = -1e20;
    for (const auto& p : pos) {
        xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
    }
    double xr = xmax - xmin + 2.0;
    double yr = ymax - ymin + 2.0;

    for (size_t i = 0; i < pos.size(); ++i) {
        int cx = static_cast<int>(((pos[i].x - xmin + 1.0) / xr) * (W - 4)) + 2;
        int cy = static_cast<int>(((pos[i].y - ymin + 1.0) / yr) * (H - 2)) + 1;
        cx = std::max(0, std::min(cx, W - 1));
        cy = std::max(0, std::min(cy, H - 1));

        // Place label
        const std::string& lbl = labels[i];
        for (size_t k = 0; k < lbl.size() && (cx + (int)k) < W; ++k) {
            grid[cy][cx + k] = lbl[k];
        }
    }

    // Draw frame
    std::cout << ansi::DIM << "    +";
    for (int i = 0; i < W; ++i) std::cout << '-';
    std::cout << "+\n";
    for (int r = 0; r < H; ++r) {
        std::cout << "    |" << ansi::RESET << ansi::GREEN;
        std::cout << grid[r];
        std::cout << ansi::RESET << ansi::DIM << "|\n";
    }
    std::cout << "    +";
    for (int i = 0; i < W; ++i) std::cout << '-';
    std::cout << "+" << ansi::RESET << "\n";
}

// ============================================================================
// CSV preview renderer
// ============================================================================

static void render_csv_preview(const CodeTrail& trail, int max_rows = 12) {
    section("CSV Output Preview (first " + std::to_string(max_rows) + " data rows)");
    std::cout << "\n";

    // Header
    std::cout << ansi::BOLD << "    "
              << std::left << std::setw(5)  << "step"
              << std::setw(9)  << "kind"
              << std::setw(16) << "op_label"
              << std::setw(12) << "lhs"
              << std::setw(12) << "rhs"
              << std::setw(14) << "result"
              << std::setw(14) << "accum"
              << ansi::RESET << "\n";

    std::cout << ansi::DIM << "    ";
    for (int i = 0; i < 80; ++i) std::cout << "\xe2\x94\x80";
    std::cout << ansi::RESET << "\n";

    const auto& entries = trail.entries();
    int shown = 0;
    for (const auto& e : entries) {
        if (shown >= max_rows) {
            std::cout << ansi::DIM << "    ... ("
                      << (entries.size() - max_rows) << " more rows)\n"
                      << ansi::RESET;
            break;
        }

        std::cout << ansi::GRAY << "    " << ansi::RESET;
        std::cout << std::left << std::setw(5) << e.step;

        const char* kc = ansi::WHITE;
        switch (e.kind) {
            case OpKind::BINARY:  kc = ansi::GREEN;   break;
            case OpKind::UNARY:   kc = ansi::CYAN;    break;
            case OpKind::ASSIGN:  kc = ansi::YELLOW;  break;
            case OpKind::COMPARE: kc = ansi::RED;     break;
            case OpKind::CUSTOM:  kc = ansi::MAGENTA; break;
        }
        std::cout << kc << std::setw(9) << op_kind_str(e.kind) << ansi::RESET;
        std::cout << std::setw(16) << e.op_label;

        auto fmt = [](double v) -> std::string {
            if (std::isnan(v)) return "-";
            std::ostringstream o;
            o << std::setprecision(6) << v;
            return o.str();
        };

        std::cout << std::setw(12) << fmt(e.lhs)
                  << std::setw(12) << fmt(e.rhs)
                  << std::setw(14) << fmt(e.result)
                  << std::setw(14) << fmt(e.accumulator)
                  << "\n";
        ++shown;
    }
}

// ============================================================================
// Stats panel
// ============================================================================

static void render_stats(const CodeTrail& trail) {
    TrailStats st = trail.stats();
    section("Trail Statistics");

    kv("Trail name",         st.trail_name);
    kv("Total steps",        std::to_string(st.total_steps));
    kv("Binary operations",  std::to_string(st.binary_ops));
    kv("Unary operations",   std::to_string(st.unary_ops));
    kv("Assign operations",  std::to_string(st.assign_ops));
    kv("Compare operations", std::to_string(st.compare_ops));
    kv("Custom operations",  std::to_string(st.custom_ops));
    kv_num("Final accumulator", st.final_accumulator, "kcal/mol");
}

// ============================================================================
// Formula sheet preview (v0.2 teaser)
// ============================================================================

static void render_formula_sheet(const CodeTrail& trail) {
    section("Formula Sheet (plain text — LaTeX in v0.2)");
    std::cout << "\n";

    std::cout << ansi::DIM
              << "    Lennard-Jones 6-12 Pair Potential\n"
              << "    ─────────────────────────────────\n" << ansi::RESET;

    // Extract only the formula_notation fields that contain '='
    int eq_num = 1;
    const auto& entries = trail.entries();
    for (const auto& e : entries) {
        if (e.formula_notation.find('=') != std::string::npos) {
            std::cout << ansi::YELLOW << "    (" << eq_num++ << ") "
                      << ansi::RESET << ansi::BOLD
                      << e.formula_notation << ansi::RESET;
            if (!e.unit_label.empty()) {
                std::cout << ansi::DIM << "   [" << e.unit_label << "]"
                          << ansi::RESET;
            }
            std::cout << "\n";
        }
    }

    std::cout << "\n" << ansi::DIM
              << "    These formulas are paper-solvable with the given inputs.\n"
              << "    v0.2 will emit this as a compilable LaTeX document.\n"
              << ansi::RESET;
}

// ============================================================================
// Main demo
// ============================================================================

int main() {
    std::cout << "\n";
    boxed_header("Code Trail Wind v0.1  —  Semi-Visual Demo",
                 "VSEPR-SIM 3.0.1  |  Deterministic Operation Audit");
    std::cout << "\n";

    // ── Setup: 4-atom Ar cluster with LJ parameters ──
    const double eps = 0.238;     // kcal/mol  (argon)
    const double sig = 3.4;       // Angstrom  (argon)

    std::vector<V3> pos = {
        { 0.0,  0.0,  0.0},
        { 4.0,  0.0,  0.0},
        { 2.0,  3.5,  0.0},
        { 2.0,  1.2,  3.0}
    };
    std::vector<std::string> labels = {"Ar[0]", "Ar[1]", "Ar[2]", "Ar[3]"};

    section("System Setup");
    kv("Cluster",       "4-atom Argon (LJ 6-12)");
    kv_num("epsilon",   eps, "kcal/mol");
    kv_num("sigma",     sig, "Angstrom");
    kv("Wind",          "+x direction, strength 0.3 kcal/mol/A");
    std::cout << "\n";

    for (size_t i = 0; i < pos.size(); ++i) {
        std::ostringstream oss;
        oss << "(" << pos[i].x << ", " << pos[i].y << ", " << pos[i].z << ") Angstrom";
        kv(labels[i], oss.str());
    }

    // ── Record a full trail ──
    CodeTrail trail("LJ_cluster_4Ar_plus_wind");

    // Phase 1: Parameter assignments
    {
        TrailScope scope(trail, "parameters");
        trail.record_assign("epsilon", eps, 0.0,
            "epsilon = 0.238 kcal/mol", "kcal/mol");
        trail.record_assign("sigma", sig, 0.0,
            "sigma = 3.4 Angstrom", "Angstrom");
    }

    // Phase 2: LJ pair interactions (all 6 unique pairs)
    double U_total = 0.0;
    int pair_count = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            double U_ij = lj_pair_trail(trail, i, j, eps, sig, pos[i], pos[j]);
            U_total += U_ij;
            ++pair_count;

            // Record the running sum
            TrailScope scope(trail, "accumulate");
            trail.record_binary("add", U_total - U_ij, U_ij, U_total, U_total,
                "U_total += U_" + std::to_string(i) + std::to_string(j),
                "kcal/mol");
        }
    }

    // Phase 3: Wind perturbation
    V3 wind_dir = {1.0, 0.0, 0.0};
    double wind_strength = 0.3;

    for (int i = 0; i < 4; ++i) {
        double U_w = wind_trail(trail, i, pos[i], wind_dir, wind_strength, U_total);
        U_total += U_w;

        TrailScope scope(trail, "accumulate_wind");
        trail.record_binary("add", U_total - U_w, U_w, U_total, U_total,
            "U_total += U_wind_" + std::to_string(i), "kcal/mol");
    }

    // Phase 4: Final comparison
    {
        TrailScope scope(trail, "final_check");
        trail.record_compare("check_negative", U_total, 0.0, U_total < 0.0, U_total,
            "U_total < 0  (bound state?)");
    }

    // ── Render all panels ──

    render_geometry(pos, labels);
    render_step_playback(trail);
    render_csv_preview(trail);
    render_stats(trail);
    render_formula_sheet(trail);

    // ── Flush CSV to disk ──
    const std::string csv_path = "code_trail_demo_output.csv";
    bool ok = trail.flush_csv(csv_path);

    std::cout << "\n";
    hline();
    if (ok) {
        bullet("CSV written: " + csv_path + " (" +
               std::to_string(trail.step_count()) + " steps)");
    } else {
        std::cout << ansi::RED << "  \xe2\x9c\x97 Failed to write CSV\n"
                  << ansi::RESET;
    }
    bullet("Total energy: " + [&]{
        std::ostringstream o;
        o << std::setprecision(10) << U_total << " kcal/mol";
        return o.str();
    }());
    bullet(std::string("Bound state: ") + (U_total < 0.0 ? "YES" : "NO"));
    std::cout << "\n";

    return 0;
}
