#pragma once
/**
 * sim_command.hpp
 * ---------------
 * Command queue for renderer → simulation thread communication.
 * Thread-safe, lock-free SPSC (single producer, single consumer) queue.
 * 
 * Architecture: Path-based parameter system
 * -----------------------------------------
 * Instead of a monolithic CmdSetParams struct with 50+ optionals,
 * we use CmdSet/CmdGet with hierarchical paths:
 * 
 *   set fire.dt_init 0.1
 *   set pbc.enabled true
 *   set lj.epsilon 0.01
 *   get temperature
 * 
 * This scales to arbitrary complexity without changing the transport layer.
 */

#include <string>
#include <variant>
#include <atomic>
#include <array>
#include <optional>
#include <vector>
#include <utility>

namespace vsepr {

// ============================================================================
// Parameter Value Type
// ============================================================================

// Union of all parameter value types
using ParamValue = std::variant<bool, int, double, std::string>;

// ============================================================================
// Session / Mode Commands
// ============================================================================

enum class SimMode {
    IDLE,           // Not running
    VSEPR,          // VSEPR geometry optimization
    OPTIMIZE,       // General structure optimization  
    MD,             // Molecular dynamics
    CRYSTAL         // Periodic crystal optimization
};

struct CmdSetMode {
    SimMode mode;
};

struct CmdReset {
    std::string config_id;  // Config file or preset name
    int seed = 0;           // Random seed
};

struct CmdShutdown {
    // Graceful shutdown signal
};

// ============================================================================
// I/O Commands
// ============================================================================

struct CmdLoad {
    std::string filepath;
};

struct CmdSave {
    std::string filepath;
    bool snapshot = false;  // true = current frame only, false = full state
};

// ============================================================================
// Build System Commands  
// ============================================================================

struct CmdInitMolecule {
    std::vector<double> coords;
    std::vector<int> atomic_numbers;
    std::vector<std::pair<int, int>> bonds;
};

// Heuristic guesses for initial structure
enum class GeometryGuess {
    VSEPR,      // VSEPR-based layout (default for molecules)
    CHAIN,      // Linear chain
    RING,       // Cyclic ring
    CRYSTAL,    // Crystalline lattice
    RANDOM      // Random positions
};

/**
 * CmdSolve - Generate structure from formula and minimize
 * Example: solve H2O --guess vsepr --seed 1
 */
struct CmdSolve {
    std::string formula;
    GeometryGuess guess = GeometryGuess::VSEPR;
    int charge = 0;
    int seed = 0;
    int max_iters = 1000;
};

/**
 * CmdBuild - Generate structure from formula (no minimization)
 * Example: build C6H12 --guess ring
 */
struct CmdBuild {
    std::string formula;
    GeometryGuess guess = GeometryGuess::VSEPR;
    int charge = 0;
    int seed = 0;
};

enum class SpawnType {
    GAS,        // Random gas particles
    CRYSTAL,    // Crystalline lattice (FCC, BCC, SC)
    LATTICE     // Custom lattice
};

enum class LatticeType {
    SC,         // Simple cubic
    BCC,        // Body-centered cubic
    FCC         // Face-centered cubic
};

/**
 * CmdSpawn - Load preset structure from JSON template
 * Example: spawn h2o, spawn butane, spawn sio2 --variant alpha_quartz
 */
struct CmdSpawn {   
    std::string preset_name;   // Name of preset (h2o, butane, sio2, etc.)
    std::string variant = "";  // Optional variant (e.g., alpha_quartz)
    
    // Legacy parameters for backward compatibility
    SpawnType type = SpawnType::GAS;
    std::string species = "Ar";
    int seed = 0;
    double box_x = 20.0;
    double box_y = 20.0;
    double box_z = 20.0;
    int n_particles = 100;
    LatticeType lattice = LatticeType::FCC;
    int nx = 4, ny = 4, nz = 4;
    double lattice_constant = 4.0;
};

// ============================================================================
// Parameter Commands (Path-Based)
// ============================================================================

/**
 * CmdSet - Set a parameter by path
 * 
 * Paths are hierarchical, dot-separated:
 *   fire.dt_init, fire.dt_max, fire.alpha
 *   md.temperature, md.timestep, md.damping
 *   pbc.enabled, pbc.box.x, pbc.box.y, pbc.box.z
 *   lj.epsilon, lj.sigma, lj.cutoff
 *   neighbor.skin, neighbor.rebuild_frequency
 *   energy.use_bonds, energy.use_angles, energy.use_torsions
 *   ...
 * 
 * This replaces the old CmdSetParams monster.
 */
struct CmdSet {
    std::string path;
    ParamValue value;
};

/**
 * CmdGet - Request a parameter value
 * Response comes back via the frame snapshot or a separate response queue.
 */
struct CmdGet {
    std::string path;
};

/**
 * CmdListParams - List available parameters (optional)
 * Useful for introspection / autocomplete.
 */
struct CmdListParams {
    std::string prefix;  // Filter by prefix, e.g. "fire." or ""
};

// ============================================================================
// Runtime Control Commands
// ============================================================================

struct CmdPause {};

struct CmdResume {};

struct CmdSingleStep {
    int n_steps = 1;
};

struct CmdRun {
    int steps = -1;        // -1 = run indefinitely
    double max_time = -1;  // -1 = no time limit (seconds of sim time)
};

/**
 * CmdMinimize - Run FIRE minimizer
 * Example: minimize --iters 500 --tol 1e-6
 */
struct CmdMinimize {
    int max_iters = 1000;
    double tol = 1e-6;      // Convergence tolerance (force magnitude)
    double dt = 0.1;        // Initial timestep
    double max_step = 0.5;  // Maximum step size
};

/**
 * CmdMD - Run molecular dynamics
 * Example: md run 1000 --T 300 --dt 0.001
 */
struct CmdMD {
    int steps;
    double temperature = 300.0;  // Kelvin
    double dt = 0.001;           // Timestep in ps
};

// ============================================================================
// Visualization & Analysis Commands
// ============================================================================

/**
 * CmdAnimate - Control whether minimization streams to renderer
 */
struct CmdAnimate {
    bool enabled;
};

/**
 * CmdProgress - Print current iteration/energy/force info
 */
struct CmdProgress {};

/**
 * CmdSummary - Print system summary (atoms, bonds, mode, energy, etc.)
 */
struct CmdSummary {};

/**
 * CmdMeasure - Measure geometry (bonds, angles, torsions)
 */
enum class MeasureType {
    BONDS,
    ANGLES,
    TORSIONS
};

struct CmdMeasure {
    MeasureType type;
    int atom_id = -1;  // -1 = all, otherwise filter by atom
};

/**
 * CmdEnergy - Print energy breakdown
 */
struct CmdEnergy {
    bool breakdown = false;  // Show component energies
};

/**
 * CmdSelect - Highlight atom/bond in renderer
 */
enum class SelectType {
    ATOM,
    BOND
};

struct CmdSelect {
    SelectType type;
    int id;
};

/**
 * CmdTrace - Enable/disable trajectory drawing
 */
struct CmdTrace {
    bool enabled;
};

/**
 * CmdExport - Export to various formats
 */
struct CmdExport {
    std::string format;   // csv, xyz, cif, pdb
    std::string filepath;
};

// ============================================================================
// UI Window Control Commands
// ============================================================================

enum class WindowAction {
    SHOW,
    HIDE,
    TOGGLE
};

struct CmdWindowControl {
    std::string panel_name;
    WindowAction action;
};

// ============================================================================
// Command Variant
// ============================================================================

using SimCommand = std::variant<
    // Session / mode
    CmdSetMode,
    CmdReset,
    CmdShutdown,
    
    // I/O
    CmdLoad,
    CmdSave,
    
    // Build systems
    CmdInitMolecule,
    CmdSolve,
    CmdBuild,
    CmdSpawn,
    
    // Parameters (path-based)
    CmdSet,
    CmdGet,
    CmdListParams,
    
    // Runtime control
    CmdPause,
    CmdResume,
    CmdSingleStep,
    CmdRun,
    CmdMinimize,
    CmdMD,
    
    // Visualization & analysis
    CmdAnimate,
    CmdProgress,
    CmdSummary,
    CmdMeasure,
    CmdEnergy,
    CmdSelect,
    CmdTrace,
    CmdExport,
    
    // UI
    CmdWindowControl
>;

// ============================================================================
// Command Queue
// ============================================================================

// Note: SPSCQueue and CommandQueue are defined in command_router.hpp
// SPSCQueue is a lock-free single-producer-single-consumer queue template
// CommandQueue is defined as SPSCQueue<CmdEnvelope, 256>

} // namespace vsepr
