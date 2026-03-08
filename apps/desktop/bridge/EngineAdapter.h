#pragma once
/**
 * EngineAdapter.h — Bridge between atomistic engine and SceneDocument
 *
 * Converts in both directions:
 *   atomistic::State  ⟶  scene::FrameData   (for display)
 *   scene::FrameData  ⟶  atomistic::State   (for computation)
 *
 * Also wraps all engine operations behind a single uniform interface:
 *   submit(request) → SceneDocument
 *
 * The desktop never touches atomistic:: types directly.
 */

#include "scene/SceneDocument.h"
#include "atomistic/core/environment.hpp"
#include <functional>
#include <future>
#include <memory>
#include <string>

namespace bridge {

// ============================================================================
// Conversion: State ↔ FrameData
// ============================================================================

/// Forward declaration — actual atomistic types only included in .cpp
namespace detail { struct EngineCore; }

// ============================================================================
// Request / Result — uniform kernel invocation
// ============================================================================

/**
 * Every kernel operation the desktop can ask for.
 * New chemistry features are added here once, never in the UI code.
 */
enum class KernelOp {
    LoadXYZ,         // load file → SceneDocument
    SaveXYZ,         // write current frame to file
    Relax,           // FIRE minimization
    MD_NVE,          // velocity Verlet (constant energy)
    MD_NVT,          // Langevin thermostat (constant temperature)
    SinglePoint,     // evaluate energy & forces, no motion
    InferBonds,      // re-detect bonds from covalent radii
    // Crystal
    EmitCrystal,     // build crystal from preset + supercell
    // Future additions go here — the UI never changes.
};

struct KernelRequest {
    KernelOp op;

    // Parameters (not all used by every op)
    std::string file_path;               // LoadXYZ, SaveXYZ
    int         max_steps    = 1000;     // Relax, MD_*
    double      dt           = 1.0;      // MD_* timestep (fs)
    double      temperature  = 300.0;    // MD_NVT target T (K)
    double      force_tol    = 1e-4;     // Relax convergence
    std::string preset;                  // EmitCrystal preset name
    int         supercell[3] = {1,1,1};  // EmitCrystal replication

    // Physical context — controls dielectric screening, medium type, etc.
    // Defaults to NearVacuum if not set.
    atomistic::EnvironmentContext env;

    // Input frame (for operations that modify existing data)
    std::shared_ptr<scene::SceneDocument> input;
};

struct KernelResult {
    bool   success{false};
    std::string message;

    // Output — always a SceneDocument (possibly multi-frame for trajectories)
    std::shared_ptr<scene::SceneDocument> output;
};

// ============================================================================
// Progress callback (optional, for long-running operations)
// ============================================================================

/// step, max_steps, energy, force_rms
using ProgressFn = std::function<void(int step, int max_steps,
                                      double energy, double force_rms)>;

// ============================================================================
// EngineAdapter — the only class the desktop instantiates
// ============================================================================

class EngineAdapter {
public:
    EngineAdapter();
    ~EngineAdapter();

    // Synchronous execution (blocks calling thread)
    KernelResult run(const KernelRequest& req,
                     ProgressFn progress = nullptr);

    // Asynchronous execution (returns immediately, result via future)
    std::future<KernelResult> runAsync(const KernelRequest& req,
                                       ProgressFn progress = nullptr);

private:
    std::unique_ptr<detail::EngineCore> core_;
};

} // namespace bridge
