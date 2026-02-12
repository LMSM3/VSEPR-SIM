#include "atomistic/analysis/pmf.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace atomistic {

// Element symbol to atomic number mapping
static int element_to_z(const std::string& symbol) {
    // Common elements only
    if (symbol == "H") return 1;
    if (symbol == "He") return 2;
    if (symbol == "Li") return 3;
    if (symbol == "Be") return 4;
    if (symbol == "B") return 5;
    if (symbol == "C") return 6;
    if (symbol == "N") return 7;
    if (symbol == "O") return 8;
    if (symbol == "F") return 9;
    if (symbol == "Ne") return 10;
    if (symbol == "Na") return 11;
    if (symbol == "Mg") return 12;
    if (symbol == "Al") return 13;
    if (symbol == "Si") return 14;
    if (symbol == "P") return 15;
    if (symbol == "S") return 16;
    if (symbol == "Cl") return 17;
    if (symbol == "Ar") return 18;
    if (symbol == "K") return 19;
    if (symbol == "Ca") return 20;
    
    throw std::runtime_error("Unknown element symbol: " + symbol);
}

static std::string z_to_element(int z) {
    switch(z) {
        case 1: return "H";
        case 2: return "He";
        case 3: return "Li";
        case 4: return "Be";
        case 5: return "B";
        case 6: return "C";
        case 7: return "N";
        case 8: return "O";
        case 9: return "F";
        case 10: return "Ne";
        case 11: return "Na";
        case 12: return "Mg";
        case 13: return "Al";
        case 14: return "Si";
        case 15: return "P";
        case 16: return "S";
        case 17: return "Cl";
        case 18: return "Ar";
        case 19: return "K";
        case 20: return "Ca";
        default: return "X";
    }
}

// ============================================================================
// PairType Implementation
// ============================================================================

PairType PairType::from_string(const std::string& spec) {
    // Parse "Element1:Element2" (e.g., "Mg:F")
    size_t colon = spec.find(':');
    if (colon == std::string::npos) {
        throw std::runtime_error("Invalid pair spec (expected Element1:Element2): " + spec);
    }

    std::string elem1 = spec.substr(0, colon);
    std::string elem2 = spec.substr(colon + 1);

    int z1 = element_to_z(elem1);
    int z2 = element_to_z(elem2);

    PairType pt;
    // CANONICALIZE: Always store (min, max) so Mg:F == F:Mg
    pt.type1 = std::min(z1, z2);
    pt.type2 = std::max(z1, z2);

    return pt;
}

std::string PairType::to_string() const {
    // Format as "Element1_Element2" for filenames
    return z_to_element(type1) + "_" + z_to_element(type2);
}

bool PairType::matches(int z1, int z2) const {
    // Check if (z1, z2) matches this pair (either order)
    return (type1 == z1 && type2 == z2) || (type1 == z2 && type2 == z1);
}

// ============================================================================
// PMFCalculator Implementation
// ============================================================================

PMFResult PMFCalculator::compute_from_rdf(
    const std::vector<double>& r_bins,
    const std::vector<double>& g_r,
    PairType pair,
    double temperature,
    double g_min,
    double tail_fraction
) {
    if (r_bins.size() != g_r.size()) {
        throw std::runtime_error("PMF: r_bins and g_r must have same size");
    }

    if (r_bins.empty()) {
        throw std::runtime_error("PMF: Empty input arrays");
    }

    if (g_min <= 0.0) {
        throw std::runtime_error("PMF: g_min must be positive");
    }

    PMFResult result;
    result.pair = pair;
    result.temperature = temperature;
    result.k_B = 0.001987204;  // kcal/mol/K
    result.n_samples = 0;  // Will be set by caller if available
    result.r_max = r_bins.back();
    result.g_min_floor = g_min;
    result.floored_bins = 0;

    // Compute bin width (assume uniform)
    if (r_bins.size() > 1) {
        result.bin_width = r_bins[1] - r_bins[0];
    } else {
        result.bin_width = 0.1;
    }

    // Copy input data
    result.r = r_bins;
    result.g_r = g_r;

    // Compute PMF(r) = -k_B T ln(g_eff(r)) where g_eff = max(g, g_min)
    result.pmf.resize(g_r.size());

    double k_B_T = result.k_B * temperature;

    for (size_t i = 0; i < g_r.size(); ++i) {
        double g_eff = g_r[i];

        if (g_eff < g_min) {
            g_eff = g_min;
            result.floored_bins++;
        }

        result.pmf[i] = -k_B_T * std::log(g_eff);
    }

    // Compute tail reference and shift PMF
    // Tail = last tail_fraction of data
    int n_bins = static_cast<int>(result.pmf.size());
    int tail_start = static_cast<int>(n_bins * (1.0 - tail_fraction));
    if (tail_start < n_bins / 2) tail_start = n_bins / 2;  // At least half

    result.tail_start_index = tail_start;

    // Average PMF in tail
    double tail_sum = 0.0;
    int tail_count = 0;

    for (int i = tail_start; i < n_bins; ++i) {
        if (std::isfinite(result.pmf[i])) {
            tail_sum += result.pmf[i];
            tail_count++;
        }
    }

    result.tail_mean = (tail_count > 0) ? (tail_sum / tail_count) : 0.0;
    result.pmf_shift = result.tail_mean;

    // Shift PMF so tail → 0
    for (size_t i = 0; i < result.pmf.size(); ++i) {
        if (std::isfinite(result.pmf[i])) {
            result.pmf[i] -= result.pmf_shift;
        }
    }

    // Extract features
    result.basin_depth = find_basin_depth(result.pmf, result.basin_index);
    result.basin_position = (result.basin_index >= 0) ? result.r[result.basin_index] : 0.0;

    result.barrier_height = find_barrier_height(result.pmf, result.basin_index, result.barrier_index);
    result.has_barrier = (result.barrier_index >= 0);

    // Basin depth is reported as POSITIVE (magnitude of well depth)
    if (result.basin_depth < 0.0) {
        result.basin_depth = -result.basin_depth;
    }

    return result;
}

double PMFCalculator::find_basin_depth(const std::vector<double>& pmf, int& basin_idx) {
    // Find most negative (lowest) PMF value
    double min_pmf = std::numeric_limits<double>::infinity();
    basin_idx = -1;
    
    for (size_t i = 0; i < pmf.size(); ++i) {
        if (std::isfinite(pmf[i]) && pmf[i] < min_pmf) {
            min_pmf = pmf[i];
            basin_idx = static_cast<int>(i);
        }
    }
    
    return (basin_idx >= 0) ? min_pmf : 0.0;
}

double PMFCalculator::find_barrier_height(
    const std::vector<double>& pmf,
    int basin_idx,
    int& barrier_idx
) {
    if (basin_idx < 0 || basin_idx >= static_cast<int>(pmf.size())) {
        barrier_idx = -1;
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Find first local maximum after basin
    double max_pmf = pmf[basin_idx];  // Start from basin
    barrier_idx = -1;

    bool found_max = false;

    for (int i = basin_idx + 1; i < static_cast<int>(pmf.size()); ++i) {
        if (!std::isfinite(pmf[i])) continue;

        if (pmf[i] > max_pmf) {
            max_pmf = pmf[i];
            barrier_idx = i;
            found_max = true;
        }

        // Stop if we've found a max and PMF drops significantly
        // (i.e., we've crossed the barrier)
        if (found_max && pmf[i] < max_pmf - 0.1) {
            break;
        }
    }

    if (!found_max || barrier_idx < 0) {
        // No barrier found
        barrier_idx = -1;
        return std::numeric_limits<double>::quiet_NaN();
    }

    double barrier_height = max_pmf - pmf[basin_idx];

    // Barrier must be at least 0.1 kT to be considered real
    double k_B_T = 0.001987204 * 300.0;  // Assume 300 K for threshold
    if (barrier_height < 0.1 * k_B_T) {
        barrier_idx = -1;
        return std::numeric_limits<double>::quiet_NaN();
    }

    return barrier_height;
}

void PMFCalculator::save_csv(const PMFResult& pmf, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("PMF: Could not open file for writing: " + filename);
    }

    // Header
    file << "# PMF for " << pmf.pair.to_string() << " at " << pmf.temperature << " K\n";
    file << "# Units: r (Angstrom), g(r) (unitless), PMF (kcal/mol, SHIFTED)\n";
    file << "# PMF shifted by " << pmf.pmf_shift << " kcal/mol to make tail = 0\n";
    file << "# Basin depth: " << pmf.basin_depth << " kcal/mol at r = " << pmf.basin_position << " A\n";

    if (pmf.has_barrier) {
        file << "# Barrier height: " << pmf.barrier_height << " kcal/mol at r = " << pmf.r[pmf.barrier_index] << " A\n";
    } else {
        file << "# Barrier height: none detected\n";
    }

    file << "# g(r) floor: " << pmf.g_min_floor << " (applied to " << pmf.floored_bins << " bins)\n";
    file << "r,g(r),PMF(r)\n";

    // Data
    for (size_t i = 0; i < pmf.r.size(); ++i) {
        file << std::fixed << std::setprecision(4) << pmf.r[i] << ",";
        file << std::fixed << std::setprecision(6) << pmf.g_r[i] << ",";

        if (std::isfinite(pmf.pmf[i])) {
            file << std::fixed << std::setprecision(4) << pmf.pmf[i];
        } else {
            file << "inf";
        }

        file << "\n";
    }
}

void PMFCalculator::save_metadata_json(const PMFResult& pmf, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("PMF: Could not open file for writing: " + filename);
    }

    file << "{\n";
    file << "  \"pair\": \"" << z_to_element(pmf.pair.type1) << ":" << z_to_element(pmf.pair.type2) << "\",\n";
    file << "  \"temperature\": " << pmf.temperature << ",\n";
    file << "  \"k_B\": " << pmf.k_B << ",\n";
    file << "  \"n_samples\": " << pmf.n_samples << ",\n";
    file << "  \"r_max\": " << pmf.r_max << ",\n";
    file << "  \"bin_width\": " << pmf.bin_width << ",\n";
    file << "  \"pmf_shift\": " << pmf.pmf_shift << ",\n";
    file << "  \"tail_mean\": " << pmf.tail_mean << ",\n";
    file << "  \"tail_start_index\": " << pmf.tail_start_index << ",\n";
    file << "  \"g_min_floor\": " << pmf.g_min_floor << ",\n";
    file << "  \"floored_bins\": " << pmf.floored_bins << ",\n";
    file << "  \"basin_depth\": " << pmf.basin_depth << ",\n";
    file << "  \"basin_position\": " << pmf.basin_position << ",\n";
    file << "  \"basin_index\": " << pmf.basin_index << ",\n";

    if (pmf.has_barrier) {
        file << "  \"barrier_height\": " << pmf.barrier_height << ",\n";
        file << "  \"barrier_index\": " << pmf.barrier_index << ",\n";
        file << "  \"has_barrier\": true\n";
    } else {
        file << "  \"barrier_height\": null,\n";
        file << "  \"barrier_index\": -1,\n";
        file << "  \"has_barrier\": false\n";
    }

    file << "}\n";
}

} // namespace atomistic
