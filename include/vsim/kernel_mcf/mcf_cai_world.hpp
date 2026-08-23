#pragma once
/**
 * mcf_cai_world.hpp
 * -----------------
 * MCF-CAI Kernel Fork  —  World container and simulation interface
 *
 * McfCaiWorld is the top-level container for the MCF-CAI kernel fork.
 * It owns a flat vector of McfCaiObjects.  Every object in the simulation
 * has exactly one entry in this vector.
 *
 * Responsibilities:
 *   - Own and provide access to all McfCaiObjects
 *   - Own simulation time and step counter
 *   - Own the periodic box (if PBC is active)
 *   - Provide object lookup by id and by name
 *   - Provide SoA extraction for integration hot paths
 *   - Provide snapshot export (for .xyza / dynx / report pipelines)
 *   - Drive the step loop (delegates force evaluation to McfCaiIntegrator)
 *
 * What McfCaiWorld does NOT do:
 *   - Force evaluation  (see McfCaiIntegrator)
 *   - Sidecar/information column updates  (see McfCaiSidecar)
 *   - File I/O  (see mcf_cai_io.hpp, planned WO-77 Phase 3)
 *
 * Theory reference: docs/Theoretical/MCF_CAI_State_Vector_Model.tex
 * WO: docs/wo/WO-77-MCF-CAI-Kernel-Fork.md
 *
 * v1.0  |  VSPER-SIM v5.0.0-main  |  Status: BLUE (provisional fork)
 */

#include "mcf_cai_object.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vsim::kernel_mcf {

// ============================================================================
// Periodic box (orthorhombic only in v1.0)
// ============================================================================

struct OrthoBox {
    Vec3d lengths = {0.0, 0.0, 0.0};   // [Å]
    bool  pbc_x   = false;
    bool  pbc_y   = false;
    bool  pbc_z   = false;

    bool any_pbc() const noexcept { return pbc_x || pbc_y || pbc_z; }
    bool all_pbc() const noexcept { return pbc_x && pbc_y && pbc_z; }
    bool active()  const noexcept { return lengths[0] > 0.0 && lengths[1] > 0.0 && lengths[2] > 0.0; }
};

// ============================================================================
// World-level simulation parameters
// ============================================================================

struct McfCaiParams {
    // Integration
    double  dt_ps           = 0.001;        // timestep [ps]
    double  temperature_K   = 300.0;        // target temperature [K]
    std::string thermostat  = "berendsen";  // "none" | "berendsen" | "langevin"
    double  tau_thermo_ps   = 0.1;          // thermostat coupling time [ps]

    // Convergence (optimizer modes)
    double  tol_rms_force   = 1e-3;         // [eV/Å]
    double  tol_max_force   = 1e-3;
    int     max_steps       = 1000;

    // Sidecar update frequency
    int     sidecar_update_every = 10;      // update Information column every N steps

    // Snapshot publication
    int     publish_every   = 2;
    int     print_every     = 10;
};

// ============================================================================
// World-level statistics (read-only output per step)
// ============================================================================

struct McfCaiStats {
    uint64_t    step           = 0;
    double      time_ps        = 0.0;
    double      kinetic_energy = 0.0;   // [eV]
    double      potential_energy = 0.0; // [eV]
    double      total_energy   = 0.0;   // [eV]
    double      temperature    = 0.0;   // [K]
    double      rms_force      = 0.0;   // [eV/Å]
    double      max_force      = 0.0;   // [eV/Å]
    bool        converged      = false;

    // Information-column aggregate (updated at sidecar_update_every interval)
    double      mean_dist_chem = 1.0;   // ⟨𝔇_chem⟩ over all objects
    double      mean_dist_fund = 1.0;   // ⟨𝔇_fund⟩ over all objects
    double      total_entropy_loss = 0.0; // Σ entropy_loss across all FundInfo cells
};

// ============================================================================
// SoA extraction result (for integration hot-paths)
// ============================================================================

/// Extracted Structure-of-Arrays view of kernel-integrable quantities.
/// These are COPIES; writing back requires McfCaiWorld::commit_soa().
struct McfCaiSoA {
    std::vector<double> px, py, pz;     // positions [Å]
    std::vector<double> vx, vy, vz;     // velocities [Å/ps]
    std::vector<double> fx, fy, fz;     // forces (current) [eV/Å]
    std::vector<double> fx_p, fy_p, fz_p; // forces (previous step) [eV/Å]
    std::vector<double> mass;           // [amu]
    std::vector<double> charge;         // [e]
    std::vector<ObjectId> ids;          // parallel id array

    std::size_t size() const noexcept { return ids.size(); }
};

// ============================================================================
// Frame snapshot (for .xyza / renderer / report export)
// ============================================================================

struct McfCaiFrame {
    uint64_t    step;
    double      time_ps;
    double      total_energy;
    double      temperature;
    OrthoBox    box;

    struct AtomRecord {
        std::string symbol;
        Vec3d       position;
        Vec3d       velocity;
        Vec3d       force;
        double      charge;
        double      energy;          // per-atom potential energy contribution
        double      dist_chem;       // 𝔇_chem from ChemInfo
        double      dist_fund;       // 𝔇_fund from FundInfo
        double      projection_loss; // from FundInfo
        ObjectId    id;
    };

    std::vector<AtomRecord> atoms;
};

// ============================================================================
// McfCaiWorld
// ============================================================================

class McfCaiWorld {
public:

    // ---- Construction -------------------------------------------------------

    McfCaiWorld()  = default;
    ~McfCaiWorld() = default;

    McfCaiWorld(const McfCaiWorld&)            = delete;
    McfCaiWorld& operator=(const McfCaiWorld&) = delete;
    McfCaiWorld(McfCaiWorld&&)                 = default;
    McfCaiWorld& operator=(McfCaiWorld&&)      = default;

    // ---- Object management --------------------------------------------------

    /// Add an object.  Assigns the next sequential id if obj.id == kNullObjectId.
    /// Returns the assigned id.
    ObjectId add(McfCaiObject obj);

    /// Reserve capacity for N objects (optional performance hint).
    void reserve(std::size_t n) {
        objects_.reserve(n);
        id_index_.reserve(n);
    }

    std::size_t size()  const noexcept { return objects_.size(); }
    bool        empty() const noexcept { return objects_.empty(); }

    // ---- Object access (by index — fastest for integration loops) -----------

    McfCaiObject&       at(std::size_t i)       { return objects_[i]; }
    const McfCaiObject& at(std::size_t i) const { return objects_[i]; }

    McfCaiObject*       data()       noexcept { return objects_.data(); }
    const McfCaiObject* data() const noexcept { return objects_.data(); }

    // Range-for support
    auto begin()        { return objects_.begin(); }
    auto end()          { return objects_.end();   }
    auto begin()  const { return objects_.begin(); }
    auto end()    const { return objects_.end();   }

    // ---- Object lookup by id / name -----------------------------------------

    McfCaiObject*       find_by_id(ObjectId id);
    const McfCaiObject* find_by_id(ObjectId id) const;

    McfCaiObject*       find_by_name(const std::string& name);
    const McfCaiObject* find_by_name(const std::string& name) const;

    // ---- Box / PBC ----------------------------------------------------------

    OrthoBox&       box()       noexcept { return box_; }
    const OrthoBox& box() const noexcept { return box_; }

    void set_box(const OrthoBox& b) { box_ = b; }

    // Wrap all positions into [0, L) if PBC is active.
    void wrap_positions();

    // Minimum-image displacement vector between two objects.
    Vec3d min_image(const Vec3d& a, const Vec3d& b) const noexcept;

    // ---- Parameters and statistics ------------------------------------------

    McfCaiParams&       params()       noexcept { return params_; }
    const McfCaiParams& params() const noexcept { return params_; }

    const McfCaiStats& stats() const noexcept { return stats_; }

    uint64_t step()    const noexcept { return stats_.step; }
    double   time_ps() const noexcept { return stats_.time_ps; }

    // ---- SoA extraction and commit ------------------------------------------

    /// Extract all integrable quantities into a flat SoA copy.
    McfCaiSoA extract_soa() const;

    /// Write positions, velocities, and forces from a SoA back into the
    /// corresponding McfCaiObjects.  Indices must match those from extract_soa().
    void commit_soa(const McfCaiSoA& soa);

    // ---- Step loop ----------------------------------------------------------

    /// Advance by one step.
    /// Delegates to the registered integrator.  If no integrator is set,
    /// forces are left at zero and only time/step counters are advanced.
    void step_forward();

    /// Advance by n steps.
    void advance(uint64_t n_steps);

    bool is_converged() const noexcept { return stats_.converged; }

    // ---- Snapshot export ----------------------------------------------------

    /// Build a McfCaiFrame snapshot of the current state.
    McfCaiFrame snapshot() const;

    // ---- Bulk statistics update (called internally after each step) ---------
    void recompute_stats();

    // ---- Information column update (sidecar) --------------------------------
    /// Update MacroInfo, ChemInfo, FundInfo for all objects.
    /// Called automatically every params_.sidecar_update_every steps.
    /// Can also be called manually at any time.
    void update_information_column();

private:
    // ---- Internal helpers ---------------------------------------------------
    ObjectId next_id() noexcept { return next_id_++; }

    // ---- State --------------------------------------------------------------
    std::vector<McfCaiObject>              objects_;
    std::unordered_map<ObjectId, uint32_t> id_index_;   // id -> objects_ index

    OrthoBox     box_;
    McfCaiParams params_;
    McfCaiStats  stats_;

    ObjectId next_id_ = 1;  // 0 is reserved (kNullObjectId = 0xFFFFFFFF)
};

} // namespace vsim::kernel_mcf