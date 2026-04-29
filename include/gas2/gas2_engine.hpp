/**
 * gas2_engine.hpp
 * ---------------
 * Unified Gas2 Engine — Top-Level API and CLI Dispatch.
 *
 * This is the main entry point for the gas2 module. Composes:
 *   - gas2_constants.hpp  (physical constants)
 *   - gas2_species.hpp    (species database)
 *   - gas2_eos.hpp        (equations of state)
 *   - gas2_kinetic.hpp    (kinetic theory, MB sampling)
 *   - gas2_heat.hpp       (heat capacity, adiabatic, JT)
 *
 * Provides:
 *   - Full gas analysis (EOS + kinetic + thermal in one call)
 *   - Comparison across EOS types
 *   - CLI dispatch for `vsepr gas2 <subcommand>`
 *   - Formatted report output
 *
 * Mirrors pattern: report_engine.hpp       (AutonomousEngine, orchestrator)
 *                  pipe_thermal_engine.hpp  (solve + report)
 *                  thermal_runner.hpp       (config + run + output)
 *
 * This header is the single include for external consumers.
 */

#pragma once

#include "gas2_constants.hpp"
#include "gas2_species.hpp"
#include "gas2_eos.hpp"
#include "gas2_kinetic.hpp"
#include "gas2_heat.hpp"
#include "gas2_thermo.hpp"
#include "gas2_potential.hpp"
#include "gas2_nuclear.hpp"

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace vsepr {
namespace gas2 {

// ============================================================================
// Full analysis result
// ============================================================================

struct Gas2Analysis {
    // Input
    std::string formula;
    double T_K;
    double P_Pa;
    double n_mol;
    uint64_t seed;

    // Species — molecular DB match (nullptr if unknown / monatomic fallback)
    const GasSpecies* species;
    // Monatomic fallback storage (populated when species==nullptr but nuclear match found)
    std::optional<GasSpecies> monatomic_storage;
    // Nuclear species reference (non-null when formula matches an element Z=2..102)
    const NuclearSpecies* nuclear_species_ref;

    // EOS results (all three for comparison)
    EOSResult eos_ideal;
    EOSResult eos_vdw;
    EOSResult eos_rk;

    // Kinetic theory
    DOFPartition dof;
    double v_rms;
    double v_mean;
    double v_mp;
    double ke_translational;      // J per molecule
    double ke_total;              // J per molecule
    double ke_translational_Eh;   // Eh per molecule (Hartree)
    double ke_total_Eh;           // Eh per molecule (Hartree)
    double mean_free_path_m;
    double collision_freq;
    double viscosity;
    double diffusion;

    // Heat
    double Cv_calc;
    double Cp_calc;
    double gamma_calc;
    double c_sound;
    double mu_JT;
    double T_inversion;

    // Formatting
    std::string format_full_report() const;
    std::string format_compact() const;
    std::string to_json() const;
};

// ============================================================================
// Core API
// ============================================================================

// Full analysis: species lookup + all EOS + kinetic + thermal
Gas2Analysis analyze(const std::string& formula, double T_K, double P_atm,
                     double n_mol = 1.0, uint64_t seed = 42);

// Atomic species analysis: wraps analyze() using element symbol from nuclear DB
// Enables sweeping Z=2..102 directly. Returns nullopt if Z out of range.
std::optional<Gas2Analysis> analyze_element(int Z, double T_K = 1000.0,
                                             double P_atm = 1.0,
                                             double n_mol = 1.0,
                                             uint64_t seed = 42);

// Thermal report for a species
ThermalReport thermal_report(const std::string& formula, double T_K);

// ============================================================================
// CLI dispatch: vsepr gas2 <subcommand> [args...]
// ============================================================================

int gas2_dispatch(int argc, char** argv);

} // namespace gas2
} // namespace vsepr
