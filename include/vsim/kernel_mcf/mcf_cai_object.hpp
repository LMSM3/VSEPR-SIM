#pragma once
/**
 * mcf_cai_object.hpp
 * ------------------
 * MCF-CAI State Vector Model  --  primary persistent object state
 *
 * Every simulated object in the MCF-CAI kernel fork is represented as a
 * single McfCaiObject.  This struct IS the persistent state.  Nothing else
 * owns authoritative object data.
 *
 * Layout: the 3x3 MCF-CAI grid
 *
 *              C (Carrier)      A (Action)       I (Information)
 *   Macro      MacroCarrier     MacroAction      MacroInfo
 *   Chemical   ChemCarrier      ChemAction       ChemInfo
 *   Fundamental FundCarrier     FundAction       FundInfo
 *
 * Doctrine (non-negotiable):
 *   - Carrier cells own all integrable / ground-truth quantities.
 *   - Action  cells own per-step transient coupling state.
 *   - Information cells are derived audit fields.  They are NEVER used as
 *     force inputs and are NEVER written back into Carrier or Action cells.
 *     They map 1:1 onto IdentitySidecarRecord fields.
 *
 * Namespace note:
 *   "CAI" here = Carrier / Action / Information (the identity basis).
 *   The force channel formerly called cai_channel has been renamed
 *   caf_channel (WO-76 prerequisite).  Do not conflate.
 *
 * Theory reference: docs/Theoretical/MCF_CAI_State_Vector_Model.tex
 * WO: docs/wo/WO-77-MCF-CAI-Kernel-Fork.md
 *
 * v1.0  |  VSPER-SIM v5.0.0-main  |  Status: BLUE (provisional fork)
 */

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vsim::kernel_mcf {

// ============================================================================
// Shared primitive types
// ============================================================================

using Vec3d  = std::array<double, 3>;
using Vec3f  = std::array<float,  3>;
using Mat3d  = std::array<double, 9>;   // row-major 3x3

static constexpr Vec3d kZeroVec3d = {0.0, 0.0, 0.0};
static constexpr Vec3f kZeroVec3f = {0.0f, 0.0f, 0.0f};
static constexpr Mat3d kZeroMat3d = {
    0.0, 0.0, 0.0,
    0.0, 0.0, 0.0,
    0.0, 0.0, 0.0
};

// ============================================================================
// MACRO LAYER
// ============================================================================

/// 𝓜_C — Macro Carrier / Configuration state
/// Ground-truth bulk/continuum geometry at the material scale.
struct MacroCarrier {
    Vec3d   position       = kZeroVec3d;   // centroid position [Å]
    Vec3d   orientation    = kZeroVec3d;   // Euler angles or quaternion axis [rad]
    double  phase_fraction = 1.0;          // crystalline fraction  0..1
    std::string phase      = "unknown";    // "solid" | "liquid" | "gas" | "plasma"
    std::string geometry   = "sphere";     // primitive shape tag for renderer
    double  volume_A3      = 0.0;          // bounding volume [Å³]

    // Grain structure (non-zero only for poly-crystalline beads)
    double  grain_size_A   = 0.0;          // mean grain radius [Å]
    int     grain_count    = 0;
};

/// 𝓜_A — Macro Action / Coupling dynamics
/// Per-step bulk process state.  Updated every integration step.
/// Never stored as truth across steps (use MacroCarrier for persistent geometry).
struct MacroAction {
    Vec3d   stress_diag    = kZeroVec3d;   // diagonal stress [Pa]:  σ_xx, σ_yy, σ_zz
    Mat3d   stress_tensor  = kZeroMat3d;   // full Cauchy stress tensor [Pa]
    Vec3d   heat_flux      = kZeroVec3d;   // heat flux vector [W/m²]
    Vec3d   flow_velocity  = kZeroVec3d;   // continuum flow velocity [m/s]
    double  deformation    = 0.0;          // scalar deformation measure (0 = undeformed)
    double  fracture_prob  = 0.0;          // instantaneous fracture probability 0..1
    double  diffusion_rate = 0.0;          // effective diffusion coefficient [Å²/ps]
    double  temperature    = 0.0;          // local temperature [K] (from MD kernel)
};

/// 𝓜_I — Macro Information / entropy-loss audit
/// Derived from trajectory history.  Sidecar-only: never used as force input.
struct MacroInfo {
    double  formation_age      = 0.0;   // simulation time when object was formed [ps]
    double  defect_memory      = 0.0;   // fractional defect content retained  0..1
    double  coarse_grain_loss  = 0.0;   // information lost at this coarse-graining level
    double  measured_state_age = 0.0;   // age of last external measurement event [ps]
    int     phase_transition_count = 0; // cumulative phase transitions observed
};

// ============================================================================
// CHEMICAL LAYER
// ============================================================================

/// 𝓒_C — Chemical Carrier / Configuration
/// Atom/ion/molecule identity and bonding topology.
struct ChemCarrier {
    int         Z              = 0;      // atomic number (0 = unset / coarse bead)
    double      mass_amu       = 0.0;    // atomic/molecular mass [amu]
    double      charge_e       = 0.0;    // net formal charge [e]
    int         oxidation_state = 0;     // formal oxidation state

    // Coordination
    int         coordination   = 0;      // coordination number
    std::vector<std::string> bonds;      // bond type labels: "sigma", "pi", "ionic", ...
    std::string hybridization  = "none"; // "sp", "sp2", "sp3", "none"

    // Molecular context (may be empty for isolated atoms / coarse beads)
    std::string symbol         = "X";    // element symbol or bead tag
    std::string residue        = "";     // residue / molecule name
    int         residue_index  = -1;     // residue sequence index (-1 = none)
};

/// 𝓒_A — Chemical Action / coupling
/// Per-step reaction and bonding dynamics.
struct ChemAction {
    bool    reaction_enabled   = false;
    bool    bond_exchange      = false;
    double  reaction_rate      = 0.0;   // per-step reaction probability 0..1
    double  solvation_energy   = 0.0;   // [eV]
    double  ionisation_energy  = 0.0;   // [eV]
    double  catalytic_activity = 0.0;   // dimensionless activity index 0..1

    // Bond-breaking / formation events this step
    int     bonds_formed       = 0;
    int     bonds_broken       = 0;
};

/// 𝓒_I — Chemical Information / entropy-loss audit
struct ChemInfo {
    std::string formation_route       = "";   // textual pathway tag, e.g. "graphitic_precursor"
    double      reaction_entropy_loss = 0.0;  // [eV/K] cumulative reaction entropy change
    double      dist_chem             = 1.0;  // 𝔇_chem: chemical distinguishability  0..1
    int         reaction_event_count  = 0;    // cumulative bond-change events
    double      bond_entropy          = 0.0;  // Shannon entropy over bond-type distribution
};

// ============================================================================
// FUNDAMENTAL LAYER
// ============================================================================

/// 𝓕_C — Fundamental Carrier
/// Charge, spin, nuclear, and colour-force bookkeeping at the sub-atomic proxy level.
struct FundCarrier {
    double  net_charge_e       = 0.0;          // net charge [e] (redundant with ChemCarrier for audit)
    double  spin_proxy         = 0.0;          // effective spin magnitude (proxy, not QM)
    int     isotope_A          = 0;            // mass number (0 = natural abundance)
    int     nuclear_state      = 0;            // nuclear isomer index (0 = ground state)

    // Colour-force bookkeeping vector (normalised RGB proxy)
    // caf_channel: colour-averaged-force channel (renamed from cai_channel per WO-76)
    Vec3f   caf_channel        = kZeroVec3f;   // [r,g,b] colour fractions, sum ≤ 1
};

/// 𝓕_A — Fundamental Action
/// Field coupling and sub-atomic process flags.
struct FundAction {
    bool    em_coupling        = true;    // electromagnetic coupling active
    bool    strong_proxy       = false;   // strong-force proxy channel active (DEM scale)
    bool    weak_proxy         = false;   // weak-force proxy channel (reserved)
    bool    decay_enabled      = false;   // radioactive / isomeric decay flag
    bool    annihilation_flag  = false;   // set when annihilation event is pending

    double  field_strength     = 0.0;    // local external field magnitude [V/Å or T]
    Vec3d   field_direction    = kZeroVec3d;  // local field unit vector
};

/// 𝓕_I — Fundamental Information / entropy-loss audit
/// Deepest information cell.  Maps to IKK IV hidden residual.
/// SIDECAR ONLY.  Never used as a force input.  Never written back to FundCarrier.
struct FundInfo {
    double  hidden_W           = 0.0;    // W-coordinate hidden residual magnitude
    double  projection_loss    = 0.0;    // |Ψ^hid| = (I - Π_{a←b})|Ψ_a| magnitude
    double  entropy_loss       = 0.0;    // cumulative entropy-loss trace [eV/K]
    double  dist_fund          = 1.0;    // 𝔇_fund: fundamental distinguishability  0..1
    double  identity_residual  = 0.0;    // ‖𝔍_true − 𝔍_top‖ at fundamental layer
};

// ============================================================================
// McfCaiObject  —  the primary persistent state of a single simulated object
// ============================================================================

/// Unique object identifier within a McfCaiWorld.
using ObjectId = uint32_t;
static constexpr ObjectId kNullObjectId = 0xFFFFFFFFu;

/// McfCaiObject
///
/// This struct IS the kernel.  Every simulated bead, atom, ion, molecule, or
/// coarse object is represented as exactly one McfCaiObject.
///
/// Integration state (positions, velocities, forces) lives in MacroCarrier
/// and ChemCarrier.  The Information column is derived and audit-only.
///
/// SoA note: McfCaiWorld stores objects as an AoS vector for prototype
/// correctness.  SoA layouts for hot-path arrays (position, velocity, force)
/// can be derived via McfCaiWorld::extract_soa() for integration kernels that
/// require them.
struct McfCaiObject {
    // ---- Identity -----------------------------------------------------------
    ObjectId    id            = kNullObjectId;
    std::string name;          // human-readable tag, e.g. "C_001" or "H2O_042"

    // ---- Velocities and forces (kernel-integrable quantities) ---------------
    // Stored at object level for direct AoS access.  Also accessible via
    // MacroCarrier::position for positional truth.
    Vec3d       velocity      = kZeroVec3d;   // [Å/ps]
    Vec3d       force         = kZeroVec3d;   // [eV/Å] (current step)
    Vec3d       force_prev    = kZeroVec3d;   // [eV/Å] (previous step, for Verlet)

    // ---- The 3x3 MCF-CAI grid -----------------------------------------------
    MacroCarrier    macro_c;    // 𝓜_C
    MacroAction     macro_a;    // 𝓜_A
    MacroInfo       macro_i;    // 𝓜_I   (sidecar-only)

    ChemCarrier     chem_c;     // 𝓒_C
    ChemAction      chem_a;     // 𝓒_A
    ChemInfo        chem_i;     // 𝓒_I   (sidecar-only)

    FundCarrier     fund_c;     // 𝓕_C
    FundAction      fund_a;     // 𝓕_A
    FundInfo        fund_i;     // 𝓕_I   (sidecar-only)

    // ---- Convenience accessors (positional truth is MacroCarrier::position) -

    const Vec3d& pos()  const noexcept { return macro_c.position; }
    Vec3d&       pos()        noexcept { return macro_c.position; }

    double mass() const noexcept { return chem_c.mass_amu; }
    double charge() const noexcept { return chem_c.charge_e; }
    int    Z()      const noexcept { return chem_c.Z; }

    bool has_fundamental_bookkeeping() const noexcept {
        return fund_c.caf_channel[0] > 0.0f
            || fund_c.caf_channel[1] > 0.0f
            || fund_c.caf_channel[2] > 0.0f
            || fund_c.spin_proxy != 0.0;
    }

    bool has_information_content() const noexcept {
        return fund_i.hidden_W != 0.0
            || fund_i.projection_loss != 0.0
            || chem_i.dist_chem < 1.0
            || fund_i.dist_fund < 1.0;
    }

    // ---- Serialisation tag --------------------------------------------------
    // Returns a compact one-line debug string for logging.
    std::string debug_tag() const {
        return "[" + std::to_string(id) + "|" + name
             + "|Z=" + std::to_string(chem_c.Z)
             + "|phase=" + macro_c.phase
             + "|D_c=" + std::to_string(chem_i.dist_chem).substr(0,5)
             + "]";
    }
};

// ============================================================================
// McfCaiObjectBuilder  —  factory helpers for common object archetypes
// ============================================================================

/// Build a minimal atom object from Z and position.
inline McfCaiObject make_atom(ObjectId id,
                               int Z,
                               double mass_amu,
                               Vec3d position,
                               double charge_e = 0.0)
{
    McfCaiObject obj;
    obj.id                 = id;
    obj.name               = "atom_" + std::to_string(id);
    obj.macro_c.position   = position;
    obj.macro_c.phase      = "unknown";
    obj.macro_c.geometry   = "sphere";
    obj.chem_c.Z           = Z;
    obj.chem_c.mass_amu    = mass_amu;
    obj.chem_c.charge_e    = charge_e;
    obj.fund_c.net_charge_e = charge_e;
    // Information column: fully unresolved / pristine at construction
    obj.chem_i.dist_chem   = 1.0;
    obj.fund_i.dist_fund   = 1.0;
    return obj;
}

/// Build a coarse bead (no Z, macro-only carrier).
inline McfCaiObject make_bead(ObjectId id,
                               std::string tag,
                               double mass_amu,
                               Vec3d position)
{
    McfCaiObject obj;
    obj.id                 = id;
    obj.name               = std::move(tag);
    obj.macro_c.position   = position;
    obj.macro_c.phase      = "solid";
    obj.macro_c.geometry   = "sphere";
    obj.chem_c.Z           = 0;        // coarse bead: no element identity
    obj.chem_c.mass_amu    = mass_amu;
    obj.chem_i.dist_chem   = 1.0;
    obj.fund_i.dist_fund   = 1.0;
    return obj;
}

} // namespace vsim::kernel_mcf