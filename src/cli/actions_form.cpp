#include "cli/actions.hpp"
#include "cli/emit_crystal.hpp"
#include "cli/emit_output.hpp"
#include "cli/metrics_rdf.hpp"
#include "cli/metrics_coordination.hpp"
#include "cli/viewer_launcher.hpp"
#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace vsepr {
namespace cli {

// ============================================================================
// PHYSICS CONSTANTS (High Precision)
// ============================================================================

// Boltzmann constant (kcal/(mol·K))
// Value: R / N_A = 8.314462618 J/(mol·K) / 4184 J/kcal
static constexpr double k_B = 0.0019872041;

// Kinetic energy conversion factor: amu·Å²/fs² → kcal/mol
// Derivation:
//   E(J) = 0.5 * m(amu) * v²(Å/fs) * 1.66054e-27 kg/amu * (1e-10 m/Å)² / (1e-15 s/fs)²
//        = 0.5 * m(amu) * v²(Å/fs) * 1.66054e-17 J
//   E(kcal/mol) = E(J) * N_A / (4184 J/kcal)
//               = 0.5 * m(amu) * v²(Å/fs) * 1.66054e-17 * 6.02214e23 / 4184
//               = 0.5 * m(amu) * v²(Å/fs) * 2390.057361
// NOTE: This includes the 0.5 factor!
static constexpr double KE_CONV = 2390.057361;

// ============================================================================
// HELPER: Compute Temperature from State
// ============================================================================

double compute_temperature_from_state(const atomistic::State& state) {
    if (state.N == 0 || state.V.size() != state.N || state.M.size() != state.N) {
        return 0.0;
    }

    // Compute kinetic energy
    // NOTE: KE_CONV already includes the 0.5 factor, so we don't apply it here!
    double KE = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double v2 = state.V[i].x * state.V[i].x + 
                    state.V[i].y * state.V[i].y + 
                    state.V[i].z * state.V[i].z;
        KE += state.M[i] * v2 * KE_CONV;
    }

    // Temperature from equipartition theorem: KE = (3/2) * N * k_B * T
    // Therefore: T = 2 * KE / (3 * N * k_B)
    double T = (2.0 * KE) / (3.0 * static_cast<double>(state.N) * k_B);

    return T;
}

// ============================================================================
// ATOM ↔ STATE CONVERSION
// ============================================================================

// Convert element symbol to atomic number (Z)
int element_to_Z(const std::string& element) {
    static const std::map<std::string, int> symbol_to_Z = {
        {"H", 1}, {"He", 2}, {"Li", 3}, {"Be", 4}, {"B", 5},
        {"C", 6}, {"N", 7}, {"O", 8}, {"F", 9}, {"Ne", 10},
        {"Na", 11}, {"Mg", 12}, {"Al", 13}, {"Si", 14}, {"P", 15},
        {"S", 16}, {"Cl", 17}, {"Ar", 18}, {"K", 19}, {"Ca", 20},
        {"Ti", 22}, {"Fe", 26}, {"Cu", 29}, {"Zn", 30}, {"Ge", 32},
        {"As", 33}, {"Se", 34}, {"Br", 35}, {"Rb", 37}, {"Sr", 38},
        {"Sn", 50}, {"I", 53}, {"Cs", 55}, {"Ba", 56}, {"La", 57},
        {"Ce", 58}, {"Pr", 59}, {"Nd", 60}, {"Ru", 44}, {"Rh", 45}
    };

    auto it = symbol_to_Z.find(element);
    if (it != symbol_to_Z.end()) {
        return it->second;
    }
    return 1;  // Default to H
}

// Convert atomic number to element symbol
std::string Z_to_element(int Z) {
    static const std::map<int, std::string> Z_to_symbol = {
        {1, "H"}, {2, "He"}, {3, "Li"}, {4, "Be"}, {5, "B"},
        {6, "C"}, {7, "N"}, {8, "O"}, {9, "F"}, {10, "Ne"},
        {11, "Na"}, {12, "Mg"}, {13, "Al"}, {14, "Si"}, {15, "P"},
        {16, "S"}, {17, "Cl"}, {18, "Ar"}, {19, "K"}, {20, "Ca"},
        {22, "Ti"}, {26, "Fe"}, {29, "Cu"}, {30, "Zn"}, {32, "Ge"},
        {33, "As"}, {34, "Se"}, {35, "Br"}, {37, "Rb"}, {38, "Sr"},
        {44, "Ru"}, {45, "Rh"}, {50, "Sn"}, {53, "I"}, {55, "Cs"},
        {56, "Ba"}, {57, "La"}, {58, "Ce"}, {59, "Pr"}, {60, "Nd"}
    };

    auto it = Z_to_symbol.find(Z);
    if (it != Z_to_symbol.end()) {
        return it->second;
    }
    return "H";  // Default
}

// Simple charge assignment (placeholder - can be refined later)
double get_charge(int Z) {
    // Ionic charges for common elements
    if (Z == 11) return +1.0;  // Na⁺
    if (Z == 12) return +2.0;  // Mg²⁺
    if (Z == 20) return +2.0;  // Ca²⁺
    if (Z == 22) return +4.0;  // Ti⁴⁺
    if (Z == 57) return +3.0;  // La³⁺
    if (Z == 58) return +3.0;  // Ce³⁺
    if (Z == 59) return +3.0;  // Pr³⁺
    if (Z == 60) return +3.0;  // Nd³⁺

    if (Z == 9) return -1.0;   // F⁻
    if (Z == 17) return -1.0;  // Cl⁻
    if (Z == 8) return -2.0;   // O²⁻

    return 0.0;  // Neutral default
}

// Convert Atom vector → atomistic::State
atomistic::State atoms_to_state(const std::vector<Atom>& atoms) {
    atomistic::State state;
    state.N = static_cast<uint32_t>(atoms.size());

    state.X.resize(state.N);
    state.V.resize(state.N, {0, 0, 0});  // Start at rest
    state.F.resize(state.N, {0, 0, 0});
    state.type.resize(state.N);
    state.Q.resize(state.N);
    state.M.resize(state.N, 1.0);  // Unit mass for now

    for (uint32_t i = 0; i < state.N; ++i) {
        state.X[i] = {atoms[i].x, atoms[i].y, atoms[i].z};

        int Z = element_to_Z(atoms[i].element);
        state.type[i] = static_cast<uint32_t>(Z);
        state.Q[i] = get_charge(Z);
    }

    // Initialize energy terms to zero
    state.E = atomistic::EnergyTerms{};

    return state;
}

// Convert atomistic::State → Atom vector
std::vector<Atom> state_to_atoms(const atomistic::State& state) {
    std::vector<Atom> atoms;
    atoms.reserve(state.N);

    for (uint32_t i = 0; i < state.N; ++i) {
        Atom atom;
        atom.element = Z_to_element(static_cast<int>(state.type[i]));
        atom.x = state.X[i].x;
        atom.y = state.X[i].y;
        atom.z = state.X[i].z;
        atoms.push_back(atom);
    }

    return atoms;
}

// ============================================================================
// SNAPSHOT OUTPUT
// ============================================================================

void write_snapshot(
    const std::string& output_dir,
    int step,
    const std::vector<Atom>& atoms,
    const ParsedCommand& cmd,
    double temperature,
    double energy,
    int seed
) {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir);

    // Generate filename: snap_000000.xyz
    std::ostringstream filename;
    filename << output_dir << "/snap_" << std::setw(6) << std::setfill('0') << step << ".xyz";

    std::ofstream file(filename.str());
    if (!file) {
        std::cerr << "WARNING: Failed to write snapshot " << filename.str() << "\n";
        return;
    }

    // Write XYZ format
    file << atoms.size() << "\n";

    // Comment line with metadata
    file << "step=" << step 
         << " T=" << std::fixed << std::setprecision(1) << temperature << "K"
         << " E=" << std::setprecision(3) << energy << " kcal/mol"
         << " seed=" << seed
         << " formula=" << cmd.spec.formula()
         << " preset=" << cmd.action_params.preset
         << "\n";

    // Atom lines
    for (const auto& atom : atoms) {
        file << std::setw(2) << std::left << atom.element << "  "
             << std::fixed << std::setprecision(6) << std::setw(12) << atom.x << "  "
             << std::setw(12) << atom.y << "  "
             << std::setw(12) << atom.z << "\n";
    }
}

// ============================================================================
// TEMPERATURE SCHEDULE
// ============================================================================

struct TemperatureSchedule {
    double T_start;
    double T_peak;
    double T_end;
    
    double get_temperature(int step, int total_steps) const {
        double progress = static_cast<double>(step) / total_steps;
        
        if (progress < 0.5) {
            // First half: ramp up (T_start → T_peak)
            return T_start + (T_peak - T_start) * (progress * 2.0);
        } else {
            // Second half: ramp down (T_peak → T_end)
            return T_peak + (T_end - T_peak) * ((progress - 0.5) * 2.0);
        }
    }
};

TemperatureSchedule parse_temperature_schedule(const std::string& schedule_str) {
    // Parse "T_start:T_peak:T_end" format
    TemperatureSchedule schedule;
    
    size_t first_colon = schedule_str.find(':');
    size_t second_colon = schedule_str.find(':', first_colon + 1);
    
    if (first_colon == std::string::npos || second_colon == std::string::npos) {
        throw std::runtime_error("Invalid temperature schedule format. Use: T_start:T_peak:T_end");
    }
    
    schedule.T_start = std::stod(schedule_str.substr(0, first_colon));
    schedule.T_peak = std::stod(schedule_str.substr(first_colon + 1, second_colon - first_colon - 1));
    schedule.T_end = std::stod(schedule_str.substr(second_colon + 1));
    
    return schedule;
}

// ============================================================================
// DIFFUSION COEFFICIENTS (DEPRECATED - NOT USED ANYMORE)
// ============================================================================
// 
// NOTE: The old diffusion-based approach was replaced with proper Langevin MD.
// These functions are kept for reference but are no longer called.
//
// OLD APPROACH (WRONG):
//   - Pure Brownian motion (random walk)
//   - No force evaluation
//   - Unphysical at T=0
//
// NEW APPROACH (CORRECT):
//   - Langevin dynamics with forces
//   - Proper thermostat
//   - Energy conserving
// ============================================================================

/*
double get_diffusion_coefficient(const std::string& element, double T, double scale_factor) {
    // D(T) = D₀ · exp(-E_a / k_B·T) · scale_factor
    //
    // Where:
    // - D₀: pre-exponential factor (element-specific)
    // - E_a: activation energy (eV)
    // - k_B: Boltzmann constant (eV/K)
    // - scale_factor: user-controlled multiplier (--diffusion flag)
    
    static const std::map<std::string, double> D0_map = {
        // Cations (slow, large, highly charged)
        {"Ce", 1e-5},
        {"La", 1e-5},
        {"Pr", 1e-5},
        {"Nd", 1e-5},
        {"Mg", 1e-5},
        {"Ca", 1e-5},
        {"Ti", 1e-5},
        {"Sn", 1e-5},
        
        // Anions (faster, smaller)
        {"F", 1e-4},
        {"O", 1e-4},
        {"Cl", 1e-4}
    };
    
    // Default D₀ for unlisted elements
    double D0 = 1e-5;
    auto it = D0_map.find(element);
    if (it != D0_map.end()) {
        D0 = it->second;
    }
    
    // Constants
    constexpr double k_B = 8.617e-5;  // eV/K
    constexpr double E_a = 0.1;       // eV (placeholder activation energy)
    
    // Arrhenius relation
    double D = D0 * std::exp(-E_a / (k_B * T));

    return D * scale_factor;
}

// ============================================================================
// DIFFUSION STEP (DEPRECATED - NOT USED ANYMORE)
// ============================================================================

void apply_diffusion_step(
    std::vector<Atom>& atoms,
    double dt,
    double temperature,
    double diffusion_scale,
    const std::vector<double>& cell,
    std::mt19937& rng
) {
    // For each atom: Δr_i ~ N(0, 2·D_i(T)·Δt)
    
    for (auto& atom : atoms) {
        double D = get_diffusion_coefficient(atom.element, temperature, diffusion_scale);
        
        // Variance: σ² = 2·D·Δt
        double sigma = std::sqrt(2.0 * D * dt);
        
        std::normal_distribution<double> noise(0.0, sigma);
        
        // Apply random displacement
        atom.x += noise(rng);
        atom.y += noise(rng);
        atom.z += noise(rng);
        
        // PBC wrapping (if cell is defined)
        if (!cell.empty() && cell.size() == 3) {
            double Lx = cell[0];
            double Ly = cell[1];
            double Lz = cell[2];
            
            // Wrap into [0, L)
            atom.x = std::fmod(atom.x + 10.0 * Lx, Lx);  // +10*Lx ensures positive before mod
            atom.y = std::fmod(atom.y + 10.0 * Ly, Ly);
            atom.z = std::fmod(atom.z + 10.0 * Lz, Lz);
        }
    }
}
*/  // End of deprecated diffusion code

// ============================================================================
// FORM ACTION (Molecular Dynamics Formation)
// ============================================================================

int action_form(const ParsedCommand& cmd, RunContext& ctx) {
    std::cout << "=== VSEPR FORM (PMF Formation Sandbox) ===\n\n";
    
    // ========================================================================
    // 1. VALIDATE PARAMETERS
    // ========================================================================
    
    if (cmd.action_params.temperature_schedule.empty()) {
        std::cerr << "ERROR: --T <schedule> required (format: T_start:T_peak:T_end)\n";
        std::cerr << "Example: --T 300:600:300\n";
        return 1;
    }
    
    if (ctx.cell_or_box.empty()) {
        std::cerr << "ERROR: --cell <a,b,c> required for crystal formation\n";
        return 1;
    }
    
    TemperatureSchedule T_schedule;
    try {
        T_schedule = parse_temperature_schedule(cmd.action_params.temperature_schedule);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to parse temperature schedule: " << e.what() << "\n";
        return 1;
    }
    
    // ========================================================================
    // 2. GENERATE INITIAL STRUCTURE (or load from file)
    // ========================================================================

    std::vector<Atom> atoms;

    if (!cmd.action_params.preset.empty()) {
        // Generate from preset + cell
        std::cout << "Initializing from preset: " << cmd.action_params.preset << "\n";

        double a = ctx.cell_or_box[0];
        double b = ctx.cell_or_box[1];
        double c = ctx.cell_or_box[2];

        atoms = generate_crystal_atoms(cmd.action_params.preset, cmd, a, b, c);

        if (atoms.empty()) {
            std::cerr << "ERROR: Failed to generate structure from preset\n";
            return 1;
        }

        std::cout << "  Generated " << atoms.size() << " atoms from " 
                  << cmd.action_params.preset << " preset\n";

    } else if (!cmd.action_params.input_file.empty()) {
        // Load structure from XYZ file
        std::cout << "Loading structure from: " << cmd.action_params.input_file << "\n";

        std::ifstream file(cmd.action_params.input_file);
        if (!file.is_open()) {
            std::cerr << "ERROR: Failed to open file: " << cmd.action_params.input_file << "\n";
            return 1;
        }

        // Read XYZ format
        int n_atoms;
        file >> n_atoms;

        if (n_atoms <= 0) {
            std::cerr << "ERROR: Invalid atom count in XYZ file: " << n_atoms << "\n";
            return 1;
        }

        file.ignore(1000, '\n');  // Skip to end of line

        // Read comment line (ignore for now)
        std::string comment;
        std::getline(file, comment);

        // Read atoms
        atoms.reserve(n_atoms);
        for (int i = 0; i < n_atoms; ++i) {
            std::string element;
            double x, y, z;

            file >> element >> x >> y >> z;

            if (file.fail()) {
                std::cerr << "ERROR: Failed to parse atom " << (i+1) << " in XYZ file\n";
                return 1;
            }

            Atom atom;
            atom.element = element;
            atom.x = x;
            atom.y = y;
            atom.z = z;
            atoms.push_back(atom);
        }

        if (atoms.empty()) {
            std::cerr << "ERROR: No atoms loaded from file\n";
            return 1;
        }

        std::cout << "  Loaded " << atoms.size() << " atoms from " 
                  << cmd.action_params.input_file << "\n";
        std::cout << "  Comment: " << comment << "\n";

    } else {
        std::cerr << "ERROR: Must specify --preset <ID> or --in <file>\n";
        return 1;
    }
    
    // ========================================================================
    // 3. SETUP SIMULATION (MD with Langevin thermostat)
    // ========================================================================

    int total_steps = cmd.action_params.form_steps;
    int checkpoint_freq = cmd.action_params.checkpoint;
    double dt = 1.0;  // fs (timestep)
    double gamma = 0.1;  // 1/fs (Langevin friction - standard value)

    // If diffusion_scale = 0, disable thermostat (NVE ensemble)
    if (cmd.action_params.diffusion_scale == 0.0) {
        gamma = 0.0;
        std::cout << "NOTE: diffusion_scale=0 → NVE ensemble (no thermostat)\n";
    }

    std::cout << "Formation parameters:\n";
    std::cout << "  Formula: " << cmd.spec.formula() << "\n";
    std::cout << "  Temperature schedule: " << T_schedule.T_start << "K → " 
              << T_schedule.T_peak << "K → " << T_schedule.T_end << "K\n";
    std::cout << "  Total steps: " << total_steps << "\n";
    std::cout << "  Checkpoint every: " << checkpoint_freq << " steps\n";
    std::cout << "  Timestep: " << dt << " fs\n";
    std::cout << "  Langevin gamma: " << gamma << " /fs\n";
    std::cout << "  RNG seed: " << ctx.seed << "\n";
    std::cout << "\n";

    // Initialize RNG
    std::mt19937 rng(ctx.seed);

    // ========================================================================
    // 4. SETUP MD SYSTEM
    // ========================================================================

    // Convert Atom → State
    atomistic::State state = atoms_to_state(atoms);

    // DEBUG: Check charges (to verify they're in correct units)
    bool has_charges = false;
    std::cout << "DEBUG: Charges after atoms_to_state():\n";
    for (uint32_t i = 0; i < std::min(state.N, 10u); ++i) {
        std::cout << "  Atom " << i << ": Q = " << state.Q[i] << " e\n";
        if (std::abs(state.Q[i]) > 0.001) has_charges = true;
    }
    std::cout << "\n";

    // WARN: Ionic systems not yet supported
    if (has_charges) {
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ⚠️  WARNING: CHARGED SYSTEM DETECTED                     ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        std::cout << "  Coulomb forces are currently DISABLED due to known bug.\n";
        std::cout << "  Only LJ forces will be computed.\n";
        std::cout << "\n";
        std::cout << "  STATUS: Ionic MD has systematic instability (T explodes).\n";
        std::cout << "          After 7+ hours debugging, integrator proven to work\n";
        std::cout << "          but Coulomb-integrator coupling has fundamental issue.\n";
        std::cout << "\n";
        std::cout << "  WORKAROUND: Use neutral molecules only (Ar, Kr, etc.)\n";
        std::cout << "  SEE: IONIC_MD_BLOCKED.md for full technical analysis\n";
        std::cout << "\n";
        std::cout << "  Press Ctrl+C to cancel, or any key to continue (LJ only)...\n";
        std::cin.get();
        std::cout << "\n";
    }

    // Setup PBC box
    if (ctx.cell_or_box.size() >= 3) {
        state.box.enabled = true;
        state.box.L = {ctx.cell_or_box[0], ctx.cell_or_box[1], ctx.cell_or_box[2]};
        state.box.invL = {1.0/ctx.cell_or_box[0], 1.0/ctx.cell_or_box[1], 1.0/ctx.cell_or_box[2]};
    } else {
        std::cerr << "WARNING: No cell defined, PBC disabled\n";
    }

    // Initialize velocities (Maxwell-Boltzmann at T_start)
    atomistic::initialize_velocities_thermal(state, T_schedule.T_start, rng);

    // Create force field model
    auto model = atomistic::create_lj_coulomb_model();
    atomistic::ModelParams mp;
    mp.rc = 10.0;  // 10 Å cutoff

    // DEBUG: Check initial velocities
    double initial_max_v = 0.0;
    double initial_avg_v = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double v = std::sqrt(state.V[i].x * state.V[i].x + 
                             state.V[i].y * state.V[i].y + 
                             state.V[i].z * state.V[i].z);
        initial_max_v = std::max(initial_max_v, v);
        initial_avg_v += v;
    }
    initial_avg_v /= state.N;
    std::cout << "DEBUG: Initial velocities after Maxwell-Boltzmann:\n";
    std::cout << "    Max: " << initial_max_v << " Å/fs\n";
    std::cout << "    Avg: " << initial_avg_v << " Å/fs\n";
    std::cout << "    Expected ~0.005-0.01 Å/fs for " << T_schedule.T_start << " K\n\n";

    // DEBUG: Check initial forces
    model->eval(state, mp);
    double initial_max_f = 0.0;
    double initial_avg_f = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double f = std::sqrt(state.F[i].x * state.F[i].x + 
                             state.F[i].y * state.F[i].y + 
                             state.F[i].z * state.F[i].z);
        initial_max_f = std::max(initial_max_f, f);
        initial_avg_f += f;
    }
    initial_avg_f /= state.N;
    std::cout << "DEBUG: Initial forces:\n";
    std::cout << "    Max: " << initial_max_f << " kcal/(mol·Å)\n";
    std::cout << "    Avg: " << initial_avg_f << " kcal/(mol·Å)\n";
    std::cout << "    Expected ~1-100 kcal/(mol·Å)\n\n";

    // Create Langevin dynamics integrator
    atomistic::LangevinDynamics langevin(*model, mp);

    // ========================================================================
    // 5. FORMATION LOOP (Proper MD)
    // ========================================================================

    std::cout << "Starting MD formation loop (Langevin dynamics)...\n\n";

    // Write initial snapshot
    atoms = state_to_atoms(state);  // Convert back for output

    // Compute initial energy
    model->eval(state, mp);
    double E_initial = state.E.UvdW + state.E.UCoul;

    write_snapshot(ctx.output_path, 0, atoms, cmd, T_schedule.T_start, E_initial, ctx.seed);
    std::cout << "  Initial snapshot: " << ctx.output_path << "/snap_000000.xyz\n";
    std::cout << "  Initial energy: " << std::fixed << std::setprecision(2) << E_initial << " kcal/mol\n\n";

    // If watch mode, launch viewer now (will auto-reload)
    if (ctx.watch_enabled) {
        std::string current_path = ctx.output_path + "/current.xyz";
        write_snapshot(current_path, 0, atoms, cmd, T_schedule.T_start, E_initial, ctx.seed);
        ViewerLauncher::launch_watch(current_path);
        std::cout << "  Live viewer launched (watching " << current_path << ")\n\n";
    }

    // Open log file
    std::string log_path = ctx.output_path + "/formation.log";
    std::filesystem::create_directories(ctx.output_path);
    std::ofstream log(log_path);
    log << "# Formation log (MD with Langevin thermostat)\n";
    log << "# step,T(K),E_total(kcal/mol),E_per_atom,crystallinity,coord_avg,rdf_peak_height\n";

    // Main MD simulation loop
    // NOTE: For now, testing with single integrate() call instead of multiple
    // to isolate whether repeated calls are causing instability

    std::cout << "DEBUG: Running all " << total_steps << " steps in single integrate() call...\n\n";

    atomistic::LangevinParams lp;
    lp.dt = dt;
    lp.n_steps = total_steps;  // All steps at once!
    lp.T_target = T_schedule.T_start;  // Constant temperature (for now)
    lp.gamma = gamma;
    lp.verbose = false;
    lp.print_freq = 100;  // Print every 100 steps

    auto stats = langevin.integrate(state, lp, rng);

    std::cout << "\nDEBUG: Integration complete. Final temperature: " << stats.T_avg << " K\n\n";

    // Final checkpoint
    int checkpoint_count = 1;
    int step = total_steps;

    // Evaluate current energy and forces
    model->eval(state, mp);
    double E_current = state.E.UvdW + state.E.UCoul;
    double E_per_atom = E_current / state.N;

    // Compute instantaneous temperature
    double T_inst = compute_temperature_from_state(state);

    // DEBUG: Check final velocities
    double max_v = 0.0;
    double avg_v = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double v = std::sqrt(state.V[i].x * state.V[i].x + 
                             state.V[i].y * state.V[i].y + 
                             state.V[i].z * state.V[i].z);
        max_v = std::max(max_v, v);
        avg_v += v;
    }
    avg_v /= state.N;
    std::cout << "  DEBUG: Final velocity check:\n";
    std::cout << "    Max velocity: " << max_v << " Å/fs\n";
    std::cout << "    Avg velocity: " << avg_v << " Å/fs\n";
    std::cout << "    Expected ~0.005-0.05 Å/fs for " << T_schedule.T_start << " K\n";

    // Convert State → Atom for output
    atoms = state_to_atoms(state);

            // ========================================================
            // COMPUTE METRICS (Phase B)
            // ========================================================

            // RDF computation with AUTO r_max (SAFETY: prevents r_max > cell/2)
            RDFParams rdf_params;

            // Auto-detect safe r_max from cell dimensions (min(L)/2)
            if (ctx.cell_or_box.size() >= 3) {
                double min_cell = std::min({ctx.cell_or_box[0], ctx.cell_or_box[1], ctx.cell_or_box[2]});
                rdf_params.rmax = min_cell / 2.0;

                // Ensure r_max is reasonable (at least 3 Å, at most 15 Å)
                rdf_params.rmax = std::max(3.0, std::min(rdf_params.rmax, 15.0));
            } else {
                // Fallback if cell not defined (shouldn't happen)
                rdf_params.rmax = 10.0;
                std::cerr << "WARNING: Cell dimensions not available, using r_max = 10.0 Å\n";
            }

            rdf_params.bin_width = 0.1;
            rdf_params.compute_pairs = true;
            rdf_params.find_peaks = true;

            RDFResult rdf = compute_rdf(state, rdf_params);

            // Crystallinity score (RDF-based)
            double crystallinity = compute_crystallinity(rdf);

            // Coordination analysis (uses RDF minima for cutoffs)
            CoordinationResult coord = compute_coordination(state, rdf);

            // Average coordination (now from actual CN computation)
            double coord_avg = 0.0;
            int n_pairs = 0;
            for (const auto& [pair, mean] : coord.mean_CN) {
                coord_avg += mean;
                n_pairs++;
            }
            if (n_pairs > 0) coord_avg /= n_pairs;

            // Write snapshot
            write_snapshot(ctx.output_path, step, atoms, cmd, T_inst, E_current, ctx.seed);

            // If watch mode, also update current.xyz for live viewer
            if (ctx.watch_enabled) {
                write_snapshot(ctx.output_path + "/current.xyz", 0, atoms, cmd, T_inst, E_current, ctx.seed);
            }

            // Write RDF to separate file
            std::ostringstream rdf_filename;
            rdf_filename << ctx.output_path << "/snap_" 
                        << std::setw(6) << std::setfill('0') << step << "_rdf.csv";
            write_rdf_csv(rdf_filename.str(), rdf, step, T_inst);

            // Write coordination to separate file
            std::ostringstream coord_filename;
            coord_filename << ctx.output_path << "/snap_" 
                          << std::setw(6) << std::setfill('0') << step << "_coord.csv";
            write_coordination_csv(coord_filename.str(), coord, step, T_inst);

            // Log metrics (enhanced with RDF and crystallinity)
            log << step << "," << T_inst << "," << E_current << "," << E_per_atom << ","
                << crystallinity << "," << coord_avg << ","
                << (rdf.peaks.empty() ? 0.0 : rdf.peaks[0].g_peak) << "\n";
            log.flush();

            // Print progress
            std::cout << "  Checkpoint " << checkpoint_count << " (step " << step << "/" << total_steps << ")"
                      << "  T = " << std::fixed << std::setprecision(1) << T_inst << " K"
                      << "  (target: " << T_schedule.T_start << " K)"
                      << "  E = " << std::setprecision(2) << E_current << " kcal/mol"
                      << "  cryst = " << std::setprecision(3) << crystallinity
                      << "\n";

    log.close();

    std::cout << "\n=== Formation Complete (MD with forces!) ===\n";
    std::cout << "  Snapshots: " << ctx.output_path << "/snap_*.xyz\n";
    std::cout << "  RDF files: " << ctx.output_path << "/snap_*_rdf.csv\n";
    std::cout << "  Coordination files: " << ctx.output_path << "/snap_*_coord.csv\n";
    std::cout << "  Log: " << log_path << "\n";
    std::cout << "  Total checkpoints: " << checkpoint_count << "\n";
    std::cout << "  Simulation type: Langevin MD (gamma=" << gamma << " /fs)\n";

    // Launch viewer if requested
    if (ctx.viz_enabled) {
        std::string final_snap = ctx.output_path + "/snap_" 
            + std::to_string(total_steps).insert(0, 6 - std::to_string(total_steps).length(), '0') 
            + ".xyz";
        ViewerLauncher::launch_static(final_snap);
    }

    return 0;
}

} // namespace cli
} // namespace vsepr
