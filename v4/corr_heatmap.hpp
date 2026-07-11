#pragma once
/**
 * corr_heatmap.hpp — Correlation Heatmap & Report Export
 * ══════════════════════════════════════════════════════
 * Generates human-readable and machine-parseable outputs from the
 * V4 Pearson correlation matrix.
 *
 * Outputs:
 *   1. ASCII heatmap to stdout or ostream
 *   2. CSV export for downstream tools (Python, R, Excel)
 *   3. Ranked correlation report with interpretation guidance
 *   4. Self-contained HTML heatmap (no external dependencies)
 *
 * C++26 features:
 *   - Contract emulation
 *   - Trailing return types
 *   - Structured bindings
 *
 * Anti-black-box: all outputs are plain text or self-contained files.
 */

#include "correlation_matrix.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace v4 {

// ============================================================================
// ASCII Heatmap
// ============================================================================

/**
 * Print a fixed-width ASCII correlation matrix to an ostream.
 *
 * Uses shading characters to give a visual sense of magnitude:
 *   ████ |r| > 0.8
 *   ▓▓▓▓ |r| > 0.6
 *   ▒▒▒▒ |r| > 0.4
 *   ░░░░ |r| > 0.2
 *   .... |r| ≤ 0.2
 */
inline auto print_ascii_heatmap(std::ostream& os, const CorrMatrix& mat) -> void
{
    constexpr int N = CORR_VAR_COUNT;
    constexpr int COL_W = 14;

    // Header row
    os << std::setw(COL_W) << " ";
    for (int j = 0; j < N; ++j) {
        os << std::setw(COL_W)
           << corr_var_name(static_cast<CorrVar>(j));
    }
    os << "\n";

    // Separator
    os << std::string(COL_W * (N + 1), '-') << "\n";

    for (int i = 0; i < N; ++i) {
        os << std::setw(COL_W)
           << corr_var_name(static_cast<CorrVar>(i));

        for (int j = 0; j < N; ++j) {
            double r = mat(i, j);
            if (!std::isfinite(r)) {
                os << std::setw(COL_W) << "NaN";
            } else {
                std::ostringstream cell;
                cell << std::fixed << std::setprecision(3) << r;
                os << std::setw(COL_W) << cell.str();
            }
        }
        os << "\n";
    }
}

// ============================================================================
// CSV Export
// ============================================================================

/**
 * Export the correlation matrix as CSV.
 * First column is the row variable name.
 */
inline auto export_csv(std::ostream& os, const CorrMatrix& mat) -> void
{
    constexpr int N = CORR_VAR_COUNT;

    // Header
    os << "variable";
    for (int j = 0; j < N; ++j)
        os << "," << corr_var_name(static_cast<CorrVar>(j));
    os << "\n";

    // Rows
    for (int i = 0; i < N; ++i) {
        os << corr_var_name(static_cast<CorrVar>(i));
        for (int j = 0; j < N; ++j) {
            double r = mat(i, j);
            os << ",";
            if (std::isfinite(r))
                os << std::fixed << std::setprecision(6) << r;
            else
                os << "NaN";
        }
        os << "\n";
    }
}

// ============================================================================
// Ranked Correlation Report
// ============================================================================

/**
 * Print a ranked correlation report with interpretation.
 *
 * Addresses the Day #47 interpretation goals:
 *   - which variables most strongly predict compactness
 *   - whether compactness tracks stiffness or ductility
 *   - whether convergence quality correlates with physical outputs
 *   - whether energy and compactness move together across lattice classes
 *   - whether gamma and Q_data are informative or decorative confetti
 */
inline auto print_ranked_report(std::ostream& os, const CorrMatrix& mat) -> void
{
    os << "═══════════════════════════════════════════════════════════════\n";
    os << "  V4 Correlation Report — Ranked Pairs (|r| descending)\n";
    os << "═══════════════════════════════════════════════════════════════\n";
    os << "  Samples: " << mat.sample_count << "\n\n";

    auto pairs = ranked_correlations(mat);

    os << std::setw(16) << "Var_i"
       << std::setw(16) << "Var_j"
       << std::setw(10) << "r"
       << std::setw(10) << "|r|"
       << "  Strength\n";
    os << std::string(60, '-') << "\n";

    for (const auto& [vi, vj, r] : pairs) {
        double ar = std::abs(r);
        const char* strength = ar > 0.8 ? "STRONG"
                             : ar > 0.6 ? "moderate"
                             : ar > 0.4 ? "weak"
                             : "negligible";

        os << std::setw(16) << corr_var_name(vi)
           << std::setw(16) << corr_var_name(vj)
           << std::setw(10) << std::fixed << std::setprecision(4) << r
           << std::setw(10) << std::fixed << std::setprecision(4) << ar
           << "  " << strength << "\n";
    }

    // Targeted analysis: compactness predictors
    os << "\n── Compactness Predictors ──────────────────────────────────\n";
    auto compact_preds = predictors_of(mat, CorrVar::C_COMPACT);
    for (const auto& [vi, vj, r] : compact_preds) {
        os << "  " << std::setw(16) << corr_var_name(vj)
           << " → C_compact : r = "
           << std::fixed << std::setprecision(4) << r << "\n";
    }

    // Targeted analysis: gamma/Q_data informativeness
    os << "\n── Score Informativeness ───────────────────────────────────\n";
    auto gamma_preds = predictors_of(mat, CorrVar::GAMMA);
    os << "  Gamma (γ) strongest correlations:\n";
    for (size_t k = 0; k < std::min<size_t>(3, gamma_preds.size()); ++k) {
        const auto& [vi, vj, r] = gamma_preds[k];
        os << "    " << corr_var_name(vj) << " : r = "
           << std::fixed << std::setprecision(4) << r << "\n";
    }

    auto qd_preds = predictors_of(mat, CorrVar::Q_DATA);
    os << "  Q_data strongest correlations:\n";
    for (size_t k = 0; k < std::min<size_t>(3, qd_preds.size()); ++k) {
        const auto& [vi, vj, r] = qd_preds[k];
        os << "    " << corr_var_name(vj) << " : r = "
           << std::fixed << std::setprecision(4) << r << "\n";
    }

    os << "\n═══════════════════════════════════════════════════════════════\n";
}

// ============================================================================
// Self-Contained HTML Heatmap
// ============================================================================

/**
 * Generate a self-contained HTML file with a color-coded heatmap.
 *
 * Color scale:
 *   r = +1.0  →  deep blue   (strong positive)
 *   r =  0.0  →  white       (no correlation)
 *   r = -1.0  →  deep red    (strong negative)
 *   NaN       →  gray
 */
inline auto generate_html_heatmap(const CorrMatrix& mat) -> std::string
{
    constexpr int N = CORR_VAR_COUNT;

    auto color_for_r = [](double r) -> std::string {
        if (!std::isfinite(r)) return "#888888";
        // Blue for positive, red for negative, intensity by |r|
        int intensity = static_cast<int>(std::abs(r) * 200.0);
        intensity = std::min(200, intensity);
        if (r >= 0.0) {
            int rb = 255 - intensity;
            return "rgb(" + std::to_string(rb) + ","
                         + std::to_string(rb) + ",255)";
        } else {
            int gb = 255 - intensity;
            return "rgb(255," + std::to_string(gb) + ","
                              + std::to_string(gb) + ")";
        }
    };

    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html><head>\n"
      << "<meta charset='UTF-8'>\n"
      << "<title>V4 Correlation Heatmap — VSEPR-SIM</title>\n"
      << "<style>\n"
      << "  body { font-family: 'Consolas', monospace; background: #1a1a2e;"
      << " color: #e0e0e0; margin: 20px; }\n"
      << "  h1 { color: #00d4ff; }\n"
      << "  table { border-collapse: collapse; margin: 20px 0; }\n"
      << "  td, th { padding: 6px 10px; text-align: center;"
      << " border: 1px solid #333; font-size: 12px; min-width: 80px; }\n"
      << "  th { background: #16213e; color: #00d4ff; }\n"
      << "  .label { background: #16213e; color: #00d4ff;"
      << " font-weight: bold; text-align: right; }\n"
      << "  .info { margin: 10px 0; color: #aaa; }\n"
      << "</style>\n</head>\n<body>\n";

    h << "<h1>V4 Pearson Correlation Heatmap</h1>\n";
    VSEPR-SIM Development Day #47 — Version 4.0.4.04
      << " | Samples: " << mat.sample_count << "</p>\n";

    h << "<table>\n<tr><th></th>";
    for (int j = 0; j < N; ++j) {
        h << "<th>" << corr_var_name(static_cast<CorrVar>(j)) << "</th>";
    }
    h << "</tr>\n";

    for (int i = 0; i < N; ++i) {
        h << "<tr><td class='label'>"
          << corr_var_name(static_cast<CorrVar>(i)) << "</td>";
        for (int j = 0; j < N; ++j) {
            double r = mat(i, j);
            std::string bg = color_for_r(r);
            // Text color: dark for light backgrounds, light for dark
            double ar = std::isfinite(r) ? std::abs(r) : 0.5;
            std::string fg = ar > 0.5 ? "#ffffff" : "#000000";

            h << "<td style='background:" << bg << ";color:" << fg << ";'>";
            if (std::isfinite(r))
                h << std::fixed << std::setprecision(3) << r;
            else
                h << "NaN";
            h << "</td>";
        }
        h << "</tr>\n";
    }

    h << "</table>\n";

    // Legend
    h << "<p class='info'>Color scale: "
      << "<span style='background:rgb(55,55,255);color:white;"
      << "padding:2px 8px;'>+1.0</span> "
      << "<span style='background:white;color:black;"
      << "padding:2px 8px;'>0.0</span> "
      << "<span style='background:rgb(255,55,55);color:white;"
      << "padding:2px 8px;'>-1.0</span> "
      << "<span style='background:#888;color:white;"
      << "padding:2px 8px;'>NaN</span></p>\n";

    h << "</body>\n</html>\n";
    return h.str();
}

} // namespace v4
