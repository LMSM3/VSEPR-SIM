/**
 * gas_module.cpp
 * --------------
 * Implementation of gas-phase simulation and analysis module.
 *
 * All constants: CODATA 2018. Deterministic given seed.
 */

#include "core/gas_module.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cstring>

namespace vsepr {
namespace gas {

// ============================================================================
// compute_properties
// ============================================================================

GasProperties compute_properties(const std::string& formula,
                                 double T_K, double P_atm, double n_mol) {
    GasProperties gp{};
    gp.formula = formula;
    gp.temperature_K = T_K;
    gp.pressure_Pa = P_atm * atm_to_Pa;
    gp.moles = n_mol;

    // Molar mass lookup
    auto it_mm = gas_molar_mass().find(formula);
    if (it_mm != gas_molar_mass().end()) {
        gp.molar_mass_kg = it_mm->second / 1000.0;  // g/mol → kg/mol
    } else {
        // Fallback: assume 28 g/mol (N2-like)
        gp.molar_mass_kg = 0.028;
    }

    // Ideal gas
    gp.ideal_volume_m3 = ideal_gas_volume(n_mol, T_K, gp.pressure_Pa);

    // Van der Waals
    auto it_vdw = vdw_database().find(formula);
    if (it_vdw != vdw_database().end()) {
        gp.has_vdw = true;
        gp.vdw_volume_m3 = vdw_volume(n_mol, T_K, gp.pressure_Pa,
                                        it_vdw->second.a, it_vdw->second.b);
        gp.compressibility_Z = gp.pressure_Pa * gp.vdw_volume_m3 / (n_mol * R_gas * T_K);
    } else {
        gp.has_vdw = false;
        gp.vdw_volume_m3 = 0.0;
        gp.compressibility_Z = 1.0;
    }

    // Kinetic theory
    gp.rms_speed_ms = rms_speed(T_K, gp.molar_mass_kg);
    gp.mean_speed_ms = mean_speed(T_K, gp.molar_mass_kg);
    gp.most_probable_speed_ms = most_probable_speed(T_K, gp.molar_mass_kg);
    gp.avg_kinetic_energy_J = avg_kinetic_energy(T_K);
    gp.mean_free_path_m = mean_free_path(T_K, gp.pressure_Pa);

    return gp;
}

// ============================================================================
// Maxwell-Boltzmann sampling
// ============================================================================

std::vector<VelocitySample> sample_maxwell_boltzmann(
    double T_K, double M_kg_per_mol, size_t count, uint64_t seed) {

    double m_kg = M_kg_per_mol / N_A;  // per-molecule mass
    double sigma = std::sqrt(kB * T_K / m_kg);

    std::mt19937_64 rng(seed);
    std::normal_distribution<double> gauss(0.0, sigma);

    std::vector<VelocitySample> samples(count);
    for (size_t i = 0; i < count; ++i) {
        samples[i].vx = gauss(rng);
        samples[i].vy = gauss(rng);
        samples[i].vz = gauss(rng);
    }

    return samples;
}

// ============================================================================
// Speed histogram
// ============================================================================

std::string format_speed_histogram(const std::vector<VelocitySample>& samples,
                                   int bins, int width) {
    if (samples.empty()) return "(no samples)\n";

    // Compute speeds
    std::vector<double> speeds(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        speeds[i] = samples[i].speed();
    }

    double max_speed = *std::max_element(speeds.begin(), speeds.end());
    double bin_width = max_speed / bins;
    if (bin_width < 1e-30) return "(degenerate speeds)\n";

    std::vector<int> counts(bins, 0);
    for (double s : speeds) {
        int b = static_cast<int>(s / bin_width);
        if (b >= bins) b = bins - 1;
        counts[b]++;
    }

    int max_count = *std::max_element(counts.begin(), counts.end());
    if (max_count == 0) return "(empty histogram)\n";

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0);
    ss << "  Maxwell-Boltzmann Speed Distribution (" << samples.size() << " samples)\n";
    ss << "  " << std::string(width + 20, '-') << "\n";

    for (int i = 0; i < bins; ++i) {
        double lo = i * bin_width;
        double hi = (i + 1) * bin_width;
        int bar_len = static_cast<int>(static_cast<double>(counts[i]) / max_count * width);

        ss << "  " << std::setw(6) << static_cast<int>(lo) << "-"
           << std::setw(6) << static_cast<int>(hi) << " |";
        for (int j = 0; j < bar_len; ++j) ss << "█";
        ss << " " << counts[i] << "\n";
    }

    // Stats
    double sum = std::accumulate(speeds.begin(), speeds.end(), 0.0);
    double avg = sum / speeds.size();
    double sq_sum = 0.0;
    for (double s : speeds) sq_sum += s * s;
    double rms = std::sqrt(sq_sum / speeds.size());

    ss << "\n  Mean speed:  " << std::setprecision(1) << avg << " m/s\n";
    ss << "  RMS speed:   " << rms << " m/s\n";
    ss << "  Max speed:   " << max_speed << " m/s\n";

    return ss.str();
}

// ============================================================================
// CLI dispatch: vsepr gas <subcommand> [args...]
// ============================================================================

static void show_gas_help() {
    std::cout << R"(
GAS MODULE — Gas-Phase Simulation and Analysis
═══════════════════════════════════════════════

USAGE:
    vsepr gas <command> [options]

COMMANDS:
    props <FORMULA> [options]    Compute gas properties
    sample <FORMULA> [options]   Sample Maxwell-Boltzmann velocities
    help                         Show this help

OPTIONS (props):
    -T, --temperature <K>        Temperature (default: 298.15 K)
    -P, --pressure <atm>         Pressure (default: 1.0 atm)
    -n, --moles <mol>            Amount of substance (default: 1.0 mol)

OPTIONS (sample):
    -T, --temperature <K>        Temperature (default: 298.15 K)
    -N, --count <N>              Number of velocity samples (default: 1000)
    --seed <S>                   RNG seed (default: 42)
    --histogram                  Show speed distribution histogram

SUPPORTED GASES:
    H2, He, N2, O2, Ar, CO2, H2O, CH4, Ne, Kr, Xe, NH3, Cl2, SO2

EXAMPLES:
    vsepr gas props Ar -T 300 -P 1.0
    vsepr gas props CO2 -T 500 -P 2.0 -n 0.5
    vsepr gas sample N2 -T 300 -N 5000 --histogram
    vsepr gas sample Ar -T 77 -N 10000 --seed 123

)";
}

int gas_dispatch(int argc, char** argv) {
    // argv[0] = "vsepr", argv[1] = "gas", argv[2] = subcommand
    if (argc < 3) {
        show_gas_help();
        return 0;
    }

    std::string subcmd = argv[2];

    if (subcmd == "help" || subcmd == "--help" || subcmd == "-h") {
        show_gas_help();
        return 0;
    }

    if (subcmd == "props") {
        // vsepr gas props <FORMULA> [-T K] [-P atm] [-n mol]
        if (argc < 4) {
            std::cerr << "ERROR: 'vsepr gas props' requires a formula.\n";
            std::cerr << "Example: vsepr gas props Ar -T 300 -P 1.0\n";
            return 1;
        }

        std::string formula = argv[3];
        double T = 298.15;
        double P = 1.0;
        double n = 1.0;

        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-T" || arg == "--temperature") && i + 1 < argc) {
                T = std::stod(argv[++i]);
            } else if ((arg == "-P" || arg == "--pressure") && i + 1 < argc) {
                P = std::stod(argv[++i]);
            } else if ((arg == "-n" || arg == "--moles") && i + 1 < argc) {
                n = std::stod(argv[++i]);
            }
        }

        auto gp = compute_properties(formula, T, P, n);
        std::cout << gp.format_report();
        return 0;
    }

    if (subcmd == "sample") {
        // vsepr gas sample <FORMULA> [-T K] [-N count] [--seed S] [--histogram]
        if (argc < 4) {
            std::cerr << "ERROR: 'vsepr gas sample' requires a formula.\n";
            std::cerr << "Example: vsepr gas sample N2 -T 300 -N 5000 --histogram\n";
            return 1;
        }

        std::string formula = argv[3];
        double T = 298.15;
        size_t count = 1000;
        uint64_t seed = 42;
        bool show_hist = false;

        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-T" || arg == "--temperature") && i + 1 < argc) {
                T = std::stod(argv[++i]);
            } else if ((arg == "-N" || arg == "--count") && i + 1 < argc) {
                count = static_cast<size_t>(std::stoul(argv[++i]));
            } else if (arg == "--seed" && i + 1 < argc) {
                seed = static_cast<uint64_t>(std::stoull(argv[++i]));
            } else if (arg == "--histogram") {
                show_hist = true;
            }
        }

        // Look up molar mass
        double M = 0.028; // default N2-like
        auto it = gas_molar_mass().find(formula);
        if (it != gas_molar_mass().end()) {
            M = it->second / 1000.0;
        }

        auto samples = sample_maxwell_boltzmann(T, M, count, seed);

        std::cout << "\033[1;35m"
                  << "╔════════════════════════════════════════════════════════════════╗\n"
                  << "║  Maxwell-Boltzmann Velocity Sampling                          ║\n"
                  << "╚════════════════════════════════════════════════════════════════╝\n"
                  << "\033[0m\n";
        std::cout << "  Formula:     " << formula << "\n";
        std::cout << "  Temperature: " << T << " K\n";
        std::cout << "  Molar mass:  " << (M * 1000.0) << " g/mol\n";
        std::cout << "  Samples:     " << count << "\n";
        std::cout << "  Seed:        " << seed << "\n\n";

        if (show_hist) {
            std::cout << format_speed_histogram(samples) << "\n";
        }

        // Summary stats
        double sum_speed = 0.0, sum_sq = 0.0;
        for (const auto& s : samples) {
            double sp = s.speed();
            sum_speed += sp;
            sum_sq += sp * sp;
        }
        double mean = sum_speed / samples.size();
        double rms_val = std::sqrt(sum_sq / samples.size());
        double expected_rms = rms_speed(T, M);

        std::cout << "  Sampled mean speed:   " << std::fixed << std::setprecision(1) << mean << " m/s\n";
        std::cout << "  Sampled RMS speed:    " << rms_val << " m/s\n";
        std::cout << "  Theoretical RMS:      " << expected_rms << " m/s\n";
        std::cout << "  Relative error:       "
                  << std::setprecision(2)
                  << (std::abs(rms_val - expected_rms) / expected_rms * 100.0)
                  << " %\n";

        return 0;
    }

    std::cerr << "ERROR: Unknown gas subcommand: " << subcmd << "\n";
    std::cerr << "Run 'vsepr gas help' for usage.\n";
    return 1;
}

} // namespace gas
} // namespace vsepr
