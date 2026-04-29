#pragma once
/**
 * comparison_tables.hpp
 *
 * Electrohydrodynamic Simulation — Stage 5: Postprocessing
 *
 * Generates comparison tables between two geometry cases (e.g. straight vs.
 * modulated channel).  Computes enhancement factors for each engineering metric.
 *
 * Output format: structured text table and CSV, suitable for embedding in
 * research reports.
 *
 * Metrics compared:
 *   - ΔP          pressure drop
 *   - E_max       maximum electric field
 *   - E_avg       volume-averaged field
 *   - Ṅ_i,out     species outlet molar flux
 *   - A_i         accumulation index
 *   - η           enhancement factor (modulated / baseline)
 */

#include "sim/ehd/physics/coupled_solver.hpp"
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace post {

struct ComparisonRow {
    std::string metric_name;
    std::string units;
    double baseline_value;
    double modulated_value;
    double enhancement;     // modulated / baseline
};

struct ComparisonTable {
    std::string baseline_id;
    std::string modulated_id;
    std::vector<ComparisonRow> rows;

    void add(const std::string& name, const std::string& units,
             double base, double mod) {
        double eta = (std::abs(base) > 1e-30) ? mod / base : 0.0;
        rows.push_back({name, units, base, mod, eta});
    }
};

/**
 * Build a comparison table from two solver metric sets.
 */
inline ComparisonTable build_comparison(
    const physics::SolverMetrics& baseline,
    const physics::SolverMetrics& modulated,
    const std::string& baseline_id  = "Straight",
    const std::string& modulated_id = "Modulated")
{
    ComparisonTable table;
    table.baseline_id  = baseline_id;
    table.modulated_id = modulated_id;

    table.add("ΔP",    "Pa",   baseline.delta_P, modulated.delta_P);
    table.add("E_max", "V/m",  baseline.E_max,   modulated.E_max);
    table.add("E_avg", "V/m",  baseline.E_avg,   modulated.E_avg);
    table.add("u_max", "m/s",  baseline.u_max,   modulated.u_max);
    table.add("Re",    "-",    baseline.Re,       modulated.Re);

    size_t n_sp = std::min(baseline.outlet_flux.size(),
                           modulated.outlet_flux.size());
    for (size_t i = 0; i < n_sp; ++i) {
        table.add("N_out[" + std::to_string(i) + "]", "mol/s",
                  baseline.outlet_flux[i], modulated.outlet_flux[i]);
        table.add("A_idx[" + std::to_string(i) + "]", "-",
                  baseline.accumulation_idx[i], modulated.accumulation_idx[i]);
    }

    return table;
}

/**
 * Write comparison table as formatted text.
 */
inline bool write_comparison_text(const ComparisonTable& table,
                                   const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "# ============================================================================\n";
    out << "# EHD Geometry Comparison: "
        << table.baseline_id << " vs " << table.modulated_id << "\n";
    out << "# ============================================================================\n\n";

    out << std::left << std::setw(20) << "Metric"
        << std::right << std::setw(14) << table.baseline_id
        << std::setw(14) << table.modulated_id
        << std::setw(14) << "η (enh.)"
        << std::setw(10) << "Units"
        << "\n";
    out << std::string(72, '-') << "\n";

    out << std::scientific << std::setprecision(4);
    for (const auto& row : table.rows) {
        out << std::left  << std::setw(20) << row.metric_name
            << std::right << std::setw(14) << row.baseline_value
            << std::setw(14) << row.modulated_value
            << std::fixed  << std::setprecision(3)
            << std::setw(14) << row.enhancement
            << std::setw(10) << row.units
            << "\n";
        out << std::scientific << std::setprecision(4);
    }

    return true;
}

/**
 * Write comparison table as CSV.
 */
inline bool write_comparison_csv(const ComparisonTable& table,
                                  const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "metric,units," << table.baseline_id << ","
        << table.modulated_id << ",enhancement\n";

    out << std::scientific << std::setprecision(8);
    for (const auto& row : table.rows) {
        out << row.metric_name << "," << row.units << ","
            << row.baseline_value << "," << row.modulated_value << ","
            << row.enhancement << "\n";
    }
    return true;
}

/**
 * Compute a weighted objective for geometry optimization:
 *   J = w1·Ē + w2·Ṅ_out - w3·ΔP
 */
inline double optimization_objective(
    const physics::SolverMetrics& metrics,
    double w1_field = 1.0,
    double w2_flux  = 1.0,
    double w3_dp    = 1.0,
    int species_index = 0)
{
    double flux = 0.0;
    if (species_index >= 0
        && static_cast<size_t>(species_index) < metrics.outlet_flux.size()) {
        flux = metrics.outlet_flux[static_cast<size_t>(species_index)];
    }
    return w1_field * metrics.E_avg
         + w2_flux  * flux
         - w3_dp    * metrics.delta_P;
}

} // namespace post
} // namespace ehd
} // namespace vsepr
