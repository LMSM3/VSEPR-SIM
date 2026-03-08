#pragma once
/**
 * physics_status.hpp
 * ------------------
 * Authoritative record of which physics terms are active, incomplete,
 * or explicitly deferred in the current build.
 *
 * Every entry here must match the actual runtime behaviour.
 * Do not mark something ACTIVE if it is zeroed in eval().
 *
 * This header is the single source of truth queried by:
 *   - phase8_deferred_physics audit
 *   - EngineAdapter status reporting
 *   - documentation generator
 */

namespace atomistic {

enum class PhysicsState {
    ACTIVE,      // implemented, evaluated every step, passes verification
    PARTIAL,     // interface exists, logic incomplete or approximate
    DEFERRED,    // interface intact, eval contribution = 0 by design
    ABSENT       // not yet started
};

struct PhysicsTerm {
    const char*  name;
    PhysicsState state;
    const char*  note;
};

// ============================================================================
// Registry — add entries here when a term changes state.
// ============================================================================

inline constexpr PhysicsTerm PHYSICS_REGISTRY[] = {
    {
        "LJ 12-6 nonbonded",
        PhysicsState::ACTIVE,
        "UFF parameters, Lorentz-Berthelot rules, quintic switching, 1-2 exclusions"
    },
    {
        "Harmonic bonds (bonded model)",
        PhysicsState::ACTIVE,
        "BondedModel with generic parameters; composed via CompositeModel. "
        "Tight molecular convergence requires matched FF parameters."
    },
    {
        "Harmonic angles",
        PhysicsState::ACTIVE,
        "BondedModel implemented, composed via CompositeModel. Angle gradient "
        "sign bug fixed; force-energy consistency verified."
    },
    {
        "Periodic torsions",
        PhysicsState::PARTIAL,
        "BondedModel implemented; not yet composed"
    },
    {
        "Coulomb electrostatics",
        PhysicsState::ACTIVE,
        "Re-enabled with dielectric screening via EnvironmentContext.coulomb_scale(). "
        "k_eff = k_coul / eps_r. Switching function applied at cutoff."
    },
    {
        "Dipole-dipole interactions",
        PhysicsState::PARTIAL,
        "SCFPolarizationSolver implemented (Applequist/Thole). Coulomb now active. "
        "Not yet wired into default eval chain."
    },
    {
        "Induced dipoles / polarization",
        PhysicsState::PARTIAL,
        "SCF solver implemented; Upol in energy ledger. Excluded from default "
        "eval chain pending Coulomb stabilisation."
    },
    {
        "Environment screening (dielectric)",
        PhysicsState::ACTIVE,
        "EnvironmentContext::coulomb_scale() applied in LJ+Coulomb eval. "
        "k_eff = k_coul / eps_r. Verified vacuum vs solution."
    },
    {
        "PBC / minimum image convention",
        PhysicsState::ACTIVE,
        "BoxPBC in State, delta() via MIC. Used in crystal and supercell eval."
    },
    {
        "FIRE energy minimisation",
        PhysicsState::ACTIVE,
        "FIRE integrator with dt_max, alpha decay, nmin. Verified on Ar3."
    },
    {
        "Velocity Verlet NVE MD",
        PhysicsState::ACTIVE,
        "VelocityVerlet integrator. Maxwell-Boltzmann init. Verified on Ar clusters."
    },
    {
        "Langevin NVT MD",
        PhysicsState::ACTIVE,
        "LangevinDynamics with friction and stochastic thermostat."
    },
};

inline constexpr int PHYSICS_REGISTRY_SIZE =
    (int)(sizeof(PHYSICS_REGISTRY) / sizeof(PHYSICS_REGISTRY[0]));

// Convenience: count by state at compile time
template<PhysicsState S>
inline constexpr int count_terms()
{
    int n = 0;
    for (int i = 0; i < PHYSICS_REGISTRY_SIZE; ++i)
        if (PHYSICS_REGISTRY[i].state == S) ++n;
    return n;
}

} // namespace atomistic
