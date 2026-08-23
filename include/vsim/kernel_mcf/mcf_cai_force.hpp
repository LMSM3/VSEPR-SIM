#pragma once
// mcf_cai_force.hpp  -  MCF-CAI Kernel Fork  -  LJ + Coulomb force evaluator
// Phase 2a | v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE
//
// ForceEvaluator implementation for McfCaiWorld.
// Reads: obj.pos(), obj.charge(), obj.chem_c.Z
// Writes: obj.force[]  (Carrier cells only)
// Returns: potential energy [eV]
//
// Data tables reused from existing vsepr:: element archives:
//   vsepr::ATOMIC_MASSES, vsepr::LJ_EPSILON (kcal/mol), vsepr::VDW_RADII (A)
// No Molecule, no SimulationState — fork-clean.

#include "mcf_cai_object.hpp"
#include "mcf_cai_world.hpp"

// vsepr:: element data tables (header-only)
#include "../../../src/pot/lj_epsilon_params.hpp"
#include "../../../src/pot/vdw_radii.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace vsim::kernel_mcf {

// ============================================================================
// Unit conversion constants
// ============================================================================

// Coulomb constant in eV·Å/e²
// k_e = 14.3996 eV·Å/e²  (SI: 1/(4πε₀) = 8.9875e9 N·m²/C²)
static constexpr double kCoulomb_eVA = 14.3996;

// kcal/mol → eV
static constexpr double kKcalToEV = 0.043364;

// ============================================================================
// McfCaiForceParams  -  tuning knobs for the force evaluator
// ============================================================================

struct McfCaiForceParams {
    double cutoff_A       = 12.0;   // pair cutoff [Å]
    bool   use_lj         = true;   // Lennard-Jones 12-6
    bool   use_coulomb    = true;   // 1/r Coulomb
    bool   use_ewald      = false;  // Ewald summation (PBC only)
    double ewald_alpha    = 0.3;    // Ewald splitting [Å⁻¹]
    int    ewald_kmax     = 5;      // k-vector range per axis
    // Mixing rules: "lorentz_berthelot" or "geometric"
    std::string mixing    = "lorentz_berthelot";
};

// ============================================================================
// Per-species LJ parameters  -  built from vsepr:: data tables
// ============================================================================

struct LJSpeciesParams {
    double sigma_A  = 3.0;  // [Å]  σ = 2 * r_vdw
    double eps_eV   = 0.0;  // [eV] ε
};

/// Look up σ and ε for element Z from existing vsepr:: tables.
inline LJSpeciesParams lj_params_for_Z(int Z) {
    if (Z < 1 || Z > 118) return { 3.0, 0.001 };
    const double r_vdw  = vsepr::VDW_RADII[static_cast<std::size_t>(Z)]; // Å
    const double eps_kc = vsepr::LJ_EPSILON[static_cast<std::size_t>(Z)]; // kcal/mol
    return { 2.0 * r_vdw, eps_kc * kKcalToEV };
}

/// Lorentz-Berthelot mixing: σ_ij = (σ_i+σ_j)/2, ε_ij = sqrt(ε_i·ε_j)
inline LJSpeciesParams mix_lb(const LJSpeciesParams& a, const LJSpeciesParams& b) {
    return { 0.5*(a.sigma_A + b.sigma_A), std::sqrt(a.eps_eV * b.eps_eV) };
}
/// Geometric mixing: σ_ij = sqrt(σ_i·σ_j), ε_ij = sqrt(ε_i·ε_j)
inline LJSpeciesParams mix_geo(const LJSpeciesParams& a, const LJSpeciesParams& b) {
    return { std::sqrt(a.sigma_A * b.sigma_A), std::sqrt(a.eps_eV * b.eps_eV) };
}

// ============================================================================
// McfCaiForceEvaluator  -  the concrete ForceEvaluator for McfCaiWorld
// ============================================================================

class McfCaiForceEvaluator {
public:
    explicit McfCaiForceEvaluator(McfCaiForceParams p = {}) : params_(std::move(p)) {}

    const McfCaiForceParams& params() const noexcept { return params_; }
    McfCaiForceParams&       params()       noexcept { return params_; }

    /// operator() satisfies the ForceEvaluator callback contract.
    /// Zeroes all forces, evaluates LJ + Coulomb pair potentials,
    /// writes forces to obj.force[], returns total PE [eV].
    double operator()(McfCaiWorld& world);

    double last_pe() const noexcept { return last_pe_; }
    double last_lj_energy()  const noexcept { return last_lj_;  }
    double last_coul_energy() const noexcept { return last_coul_; }

private:
    McfCaiForceParams params_;
    double last_pe_   = 0.0;
    double last_lj_   = 0.0;
    double last_coul_ = 0.0;

    // Per-step LJ pair evaluation (returns energy contribution, accumulates forces)
    static double eval_lj_pair(McfCaiObject& oi, McfCaiObject& oj,
                               const Vec3d& dr, double r2,
                               const McfCaiForceParams& p);

    // Per-step Coulomb pair evaluation
    static double eval_coul_pair(McfCaiObject& oi, McfCaiObject& oj,
                                 const Vec3d& dr, double r);
};

} // namespace vsim::kernel_mcf
