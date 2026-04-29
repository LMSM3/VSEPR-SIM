/**
 * module_registry.hpp
 * -------------------
 * Unified Module Registry for VSEPR-SIM.
 *
 * Provides a single registry that catalogs every loadable subsystem
 * (atomistic, coarse-grain, gas, thermal, report, trail, tracker, etc.)
 * with unified help, status, and dispatch.
 *
 * Design goals:
 *   - Zero-configuration discovery: modules self-register at static-init time
 *   - Unified help: `vsepr modules` lists everything, `vsepr help <module>` drills in
 *   - Status introspection: each module reports readiness
 *   - Branch loading: modules can declare dependencies on other modules
 *
 * Anti-black-box: the registry is a flat, inspectable vector. No hidden state.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace vsepr {

// ============================================================================
// Module descriptor
// ============================================================================

enum class ModuleStatus {
    Ready,          // Fully operational
    Degraded,       // Works but missing optional deps
    Unavailable,    // Cannot run (missing required deps)
    Experimental    // Under development
};

inline std::string status_string(ModuleStatus s) {
    switch (s) {
        case ModuleStatus::Ready:        return "READY";
        case ModuleStatus::Degraded:     return "DEGRADED";
        case ModuleStatus::Unavailable:  return "UNAVAILABLE";
        case ModuleStatus::Experimental: return "EXPERIMENTAL";
    }
    return "UNKNOWN";
}

inline std::string status_color(ModuleStatus s) {
    switch (s) {
        case ModuleStatus::Ready:        return "\033[0;32m"; // green
        case ModuleStatus::Degraded:     return "\033[1;33m"; // yellow
        case ModuleStatus::Unavailable:  return "\033[0;31m"; // red
        case ModuleStatus::Experimental: return "\033[0;36m"; // cyan
    }
    return "\033[0m";
}

struct ModuleInfo {
    std::string name;           // e.g. "atomistic", "gas", "coarse-grain"
    std::string version;        // e.g. "1.0.0"
    std::string description;    // One-line summary
    std::string category;       // "simulation", "analysis", "infrastructure", "reporting"
    ModuleStatus status;
    std::vector<std::string> dependencies;  // other module names
    std::string detailed_help;  // Multi-line help text

    // CLI entry: if non-empty, the subcommand that activates this module
    std::string cli_command;

    // Quick-start example
    std::string example;
};

// ============================================================================
// Registry (singleton)
// ============================================================================

class ModuleRegistry {
public:
    static ModuleRegistry& instance() {
        static ModuleRegistry reg;
        return reg;
    }

    void register_module(const ModuleInfo& info) {
        modules_.push_back(info);
    }

    const std::vector<ModuleInfo>& modules() const { return modules_; }

    const ModuleInfo* find(const std::string& name) const {
        for (const auto& m : modules_) {
            if (m.name == name || m.cli_command == name) return &m;
        }
        return nullptr;
    }

    std::vector<const ModuleInfo*> by_category(const std::string& cat) const {
        std::vector<const ModuleInfo*> result;
        for (const auto& m : modules_) {
            if (m.category == cat) result.push_back(&m);
        }
        return result;
    }

    // Print full module table to stdout
    void print_table() const {
        std::cout << "\033[1;35m"
                  << "╔════════════════════════════════════════════════════════════════╗\n"
                  << "║  VSEPR-SIM Module Registry                                    ║\n"
                  << "╚════════════════════════════════════════════════════════════════╝\n"
                  << "\033[0m\n";

        // Group by category
        std::map<std::string, std::vector<const ModuleInfo*>> grouped;
        for (const auto& m : modules_) {
            grouped[m.category].push_back(&m);
        }

        for (const auto& [cat, mods] : grouped) {
            std::cout << "\033[1;36m┌─ " << cat << "\033[0m\n";
            for (const auto* m : mods) {
                std::cout << "│  "
                          << status_color(m->status) << "●\033[0m "
                          << "\033[1m" << std::setw(18) << std::left << m->name << "\033[0m"
                          << " v" << std::setw(8) << std::left << m->version
                          << " " << m->description;
                if (!m->cli_command.empty()) {
                    std::cout << "  \033[0;90m[vsepr " << m->cli_command << "]\033[0m";
                }
                std::cout << "\n";
            }
            std::cout << "│\n";
        }

        std::cout << "Total: " << modules_.size() << " modules registered\n";
    }

    // Print detailed help for one module
    void print_help(const std::string& name) const {
        const ModuleInfo* m = find(name);
        if (!m) {
            std::cerr << "\033[0;31m✗\033[0m Module not found: " << name << "\n";
            std::cerr << "  Run 'vsepr modules' to see all available modules.\n";
            return;
        }

        std::cout << "\033[1;35m"
                  << "╔════════════════════════════════════════════════════════════════╗\n"
                  << "║  Module: " << std::setw(53) << std::left << m->name << "  ║\n"
                  << "╚════════════════════════════════════════════════════════════════╝\n"
                  << "\033[0m\n";

        std::cout << "  Version:     " << m->version << "\n";
        std::cout << "  Category:    " << m->category << "\n";
        std::cout << "  Status:      " << status_color(m->status)
                  << status_string(m->status) << "\033[0m\n";
        std::cout << "  Description: " << m->description << "\n";

        if (!m->dependencies.empty()) {
            std::cout << "  Depends on:  ";
            for (size_t i = 0; i < m->dependencies.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << m->dependencies[i];
            }
            std::cout << "\n";
        }

        if (!m->cli_command.empty()) {
            std::cout << "  CLI Command: vsepr " << m->cli_command << "\n";
        }

        if (!m->example.empty()) {
            std::cout << "\n  \033[1;36mQuick Start:\033[0m\n    " << m->example << "\n";
        }

        if (!m->detailed_help.empty()) {
            std::cout << "\n" << m->detailed_help << "\n";
        }
    }

private:
    ModuleRegistry() = default;
    std::vector<ModuleInfo> modules_;
};

// ============================================================================
// Auto-registration helper
// ============================================================================

struct ModuleRegistrar {
    explicit ModuleRegistrar(const ModuleInfo& info) {
        ModuleRegistry::instance().register_module(info);
    }
};

// Convenience macro for module self-registration
#define VSEPR_REGISTER_MODULE(var_name, ...)                     \
    static ::vsepr::ModuleRegistrar var_name(::vsepr::ModuleInfo{__VA_ARGS__})

// ============================================================================
// Built-in module registrations
// ============================================================================

namespace modules {

inline void register_builtin_modules() {
    auto& reg = ModuleRegistry::instance();

    // --- Simulation modules ---
    reg.register_module({
        "atomistic", "3.0.1",
        "Full atomistic simulation: emit, relax, form, test",
        "simulation", ModuleStatus::Ready, {},
        R"(  ATOMISTIC MODULE
  ----------------
  The core simulation engine for atomistic structure generation,
  energy minimization (FIRE), and molecular testing.

  Subcommands (via main CLI):
    vsepr <FORMULA>@gas emit --cloud N --box x,y,z
    vsepr <FORMULA>@molecule relax --steps N
    vsepr <FORMULA>@crystal emit --preset <id>
    vsepr <FORMULA> test --preset <id>

  Supports: gas, molecule, crystal, bulk domain modes.
  Integrators: FIRE, BAOAB Langevin, Velocity-Verlet.
  Potentials: LJ, harmonic bond/angle, VSEPR domain repulsion.)",
        "", // cli_command handled by main dispatch
        "vsepr H2O@molecule relax --steps 2000 --out water.xyz"
    });

    reg.register_module({
        "coarse-grain", "2.0.0",
        "Bead-based reduced-representation modeling",
        "simulation", ModuleStatus::Ready, {"atomistic"},
        R"(  COARSE-GRAIN MODULE
  -------------------
  Multi-scale modeling with bead-based reduced representations.
  Supports environment descriptors, orientation potentials,
  anisotropic interactions, and FIRE minimization on bead systems.

  Subcommands:
    vsepr cg scene    -- Build/load bead configurations
    vsepr cg inspect  -- Inspect bead/system state
    vsepr cg env      -- Run environment update pipeline
    vsepr cg interact -- Evaluate pairwise interactions
    vsepr cg fire     -- LL-FIRE minimization
    vsepr cg viz      -- Lightweight bead viewer (requires BUILD_VIS)

  Models: hourglass, lookglass, unified potential, seed-bead stepper.
  Mapping: atom-to-bead, fragment bridge, multi-channel, surface mapper.)",
        "cg",
        "vsepr cg scene --preset pair --spacing 4.0"
    });

    reg.register_module({
        "gas", "1.0.0",
        "Ideal and real gas modeling, kinetic theory, Maxwell-Boltzmann sampling",
        "simulation", ModuleStatus::Ready, {"atomistic"},
        R"(  GAS MODULE
  ----------
  Dedicated gas-phase simulation and analysis toolkit.

  Features:
    - Ideal gas law (PV=nRT) calculations
    - Van der Waals equation of state
    - Maxwell-Boltzmann speed distribution sampling
    - Kinetic energy and RMS speed calculations
    - Mean free path estimation
    - Gas cloud generation with thermal velocity initialization

  Subcommands:
    vsepr gas props <FORMULA> -T <K> -P <atm>   -- Gas properties
    vsepr gas sample <FORMULA> -T <K> -N <count> -- Sample velocities
    vsepr gas cloud <FORMULA> --box x,y,z -N <n> -- Generate gas cloud
    vsepr gas help                                -- Full help

  All calculations are deterministic given seed.)",
        "gas",
        "vsepr gas props Ar -T 300 -P 1.0"
    });

    reg.register_module({
        "gas2", "1.0.0",
        "Advanced heat and gas analysis — 3-EOS comparison, Chapman-Enskog transport, Joule-Thomson",
        "simulation", ModuleStatus::Ready, {"gas"},
        R"(  GAS2 MODULE — Advanced Heat and Gas Analysis
  ---------------------------------------------
  Next-generation gas-phase modeling with unified species database,
  three equation-of-state solvers, and full kinetic/thermal analysis.

  Features:
    - 14-species database (noble, diatomic, polyatomic) with full
      thermodynamic, transport, EOS, and critical-point data
    - Ideal Gas, Van der Waals, and Redlich-Kwong EOS with
      Newton solvers and convergence diagnostics
    - Degrees-of-freedom partitioning (trans/rot/vib)
    - Heat capacity (Cv, Cp) from DOF with tabulated comparison
    - Adiabatic compression/expansion calculations
    - Speed of sound, Joule-Thomson coefficient, inversion temperature
    - Chapman-Enskog transport: viscosity, thermal conductivity, diffusion
    - Mean free path, collision frequency
    - Maxwell-Boltzmann velocity sampling (deterministic, seeded)
    - Full JSON output for pipeline integration

  Subcommands:
    vsepr gas2 analyze <FORMULA> -T <K> -P <atm>   Full analysis
    vsepr gas2 thermal <FORMULA> -T <K>            Thermal report
    vsepr gas2 compare <FORMULA> -T <K> -P <atm>   EOS comparison
    vsepr gas2 sample <FORMULA> -T <K> -N <count>  MB sampling
    vsepr gas2 species [FORMULA]                    Database query

  All calculations are deterministic. Anti-black-box: every
  intermediate (Re, Nu, Z, DOF) is exposed in the output.)",
        "gas2",
        "vsepr gas2 analyze Ar -T 300 -P 1.0"
    });

    // --- Analysis modules ---
    reg.register_module({
        "thermal", "2.0.0",
        "Thermal property analysis, Debye model, heat capacity",
        "analysis", ModuleStatus::Ready, {"atomistic"},
        R"(  THERMAL MODULE
  --------------
  Analyze thermal properties of molecular and crystal structures.

  Features:
    - Debye model thermal analysis
    - Heat capacity calculations (Cv, Cp)
    - Thermal evolution over generations
    - Bonding decomposition
    - Thermal object file generation

  Subcommands:
    vsepr therm <input.xyz> [options]
    vsepr therm <input.xyz> -T 500
    vsepr therm <input.xyz> --generate-object --viz)",
        "therm",
        "vsepr therm molecule.xyz -T 500 --viz"
    });

    reg.register_module({
        "tracker", "1.0.0",
        "Persistent particle identity tracking and element picking",
        "analysis", ModuleStatus::Ready, {},
        R"(  TRACKER MODULE
  --------------
  Maintains persistent particle identity across simulation steps.
  Supports random element picking with configurable weight profiles.

  Features:
    - Bidirectional particle ID mapping
    - Event logging (add, remove, transform)
    - TrackerMetrics for statistical introspection
    - Random element picker (3 modes: uniform, weighted, preset)
    - Molecule alias resolution)",
        "",
        ""
    });

    reg.register_module({
        "trail", "0.1.0",
        "Code Trail Wind — deterministic operation audit trail with CSV",
        "analysis", ModuleStatus::Ready, {},
        R"(  TRAIL MODULE (Code Trail Wind v0.1)
  ------------------------------------
  Append-only operation audit trail for scientific reproducibility.

  Features:
    - TrailEntry recording (compute, transform, measure, validate, annotate)
    - TrailScope RAII guard for automatic begin/end tracking
    - CSV serialization with preamble metadata
    - TrailWriter for streaming output
    - Trail statistics and filtering)",
        "",
        ""
    });

    reg.register_module({
        "units", "1.0.0",
        "Energy unit system (Hartree canonical) + physical quantity reporting",
        "analysis", ModuleStatus::Ready, {},
        R"(  UNITS MODULE
  ------------
  Canonical Hartree energy unit system with CODATA 2018 constants.

  Features:
    - Energy conversion: Hartree ↔ eV ↔ kcal/mol ↔ kJ/mol
    - Thermal energy kBT calculations
    - Thermal accessibility assessment
    - ReportedQuantity: co-report mass, energy, composition, moles
    - 118-element IUPAC 2021 mass table)",
        "",
        ""
    });

    // --- Reporting modules ---
    reg.register_module({
        "report", "1.0.0",
        "Autonomous thermal-materials experiment and report generation",
        "reporting", ModuleStatus::Ready, {},
        R"(  REPORT MODULE
  -------------
  Continual autonomous report-generation system for randomized
  thermal and materials science digital experiments.

  Complexity levels (1-5):
    L1: Simple uniform single-element materials
    L2: Binary/ternary alloys and compounds
    L3: Anisotropic, graded, or layered materials
    L4: Transient thermal loading with material variation
    L5: Rare/unstable synthetic configurations

  Launch: report_generator [--complexity N] [--seed S] [--count C])",
        "",
        "report_generator --complexity 3 --seed 42 --count 10"
    });

    // --- Infrastructure modules ---
    reg.register_module({
        "infra", "1.0.0",
        "Bootstrap probe, GPU detection, NVIDIA TUI, MOTD",
        "infrastructure", ModuleStatus::Ready, {},
        R"(  INFRASTRUCTURE MODULE
  ---------------------
  System bootstrap and hardware discovery.

  Features:
    - 6-step GPU detection cascade (nvidia-smi shell-out)
    - Bootstrap probe with seed file reading
    - NVIDIA TUI for live GPU monitoring
    - Message of the day (MOTD) system)",
        "",
        ""
    });

    reg.register_module({
        "live-server", "1.0.0",
        "Zero-input HTTP live analysis stream on port 99998",
        "infrastructure", ModuleStatus::Ready, {"report", "tracker", "units"},
        R"(  LIVE SERVER MODULE
  ------------------
  Automatic zero-input HTTP server that streams random analysis
  output as server-sent events (SSE) and JSON snapshots.

  Features:
    - Starts on port 99998 with zero configuration
    - Generates random molecular analysis every cycle
    - Streams SSE events to any browser or HTTP client
    - JSON snapshot endpoint for polling clients
    - Built-in HTML dashboard at /
    - Full provenance: every analysis has seed, timestamp, trail

  Launch:
    vsepr serve                    -- Start on port 99998
    vsepr serve --port 8080        -- Custom port
    vsepr_live                     -- Standalone launcher

  Endpoints:
    GET /              HTML dashboard with auto-updating display
    GET /stream        SSE event stream (text/event-stream)
    GET /snapshot      Latest analysis as JSON
    GET /status        Server status and uptime)",
        "serve",
        "vsepr serve"
    });
}

} // namespace modules
} // namespace vsepr
