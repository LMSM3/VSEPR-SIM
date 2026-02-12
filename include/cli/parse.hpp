#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace vsepr {
namespace cli {

// Domain mode hint
enum class DomainMode {
    Gas,      // @gas
    Crystal,  // @crystal
    Bulk,     // @bulk
    Molecule  // @molecule (default if no hint)
};

// Parsed composition (element, count)
struct Composition {
    std::vector<std::pair<std::string, int>> elements;
};

// Parsed specification
struct Spec {
    Composition composition;
    DomainMode mode = DomainMode::Molecule;  // Default
    
    std::string formula() const;
    std::string mode_string() const;
};

// Action type
enum class Action {
    Emit,
    Relax,
    Form,
    Test,
    Unknown
};

// Domain parameters
struct DomainParams {
    std::optional<std::vector<double>> cell;  // a,b,c for unit cell
    std::optional<std::vector<double>> box;   // x,y,z for bounding box
    bool pbc_override = false;  // Explicit --pbc flag
};

// Global flags
struct GlobalFlags {
    std::string out_path;
    int seed = -1;
    bool viz = false;     // Launch viewer after completion (static snapshot)
    bool watch = false;   // Launch viewer in live mode (updates during sim)
};

// Action-specific parameters
struct ActionParams {
    // emit
    int cloud_size = 0;
    double density = 0.0;
    std::string preset;

    // relax
    int steps = 1000;
    double dt = 0.001;
    std::string input_file;
    std::string config_file;

    // form (PMF formation sandbox)
    int form_steps = 20000;        // Total simulation steps
    int checkpoint = 1000;         // Checkpoint frequency
    std::string temperature_schedule; // Format: "T_start:T_peak:T_end"
    double diffusion_scale = 1.0;  // D(T) multiplier (0 = no diffusion)

    // PMF analysis flags
    std::string pmf_pair;          // Pair for PMF (e.g., "Mg:F", empty = disabled)
    std::vector<std::string> pmf_pairs; // Multiple pairs (e.g., ["Mg:F", "F:F"])
    int pmf_bins = 100;            // Number of histogram bins
    double pmf_rmax = 10.0;        // Maximum distance for PMF (Angstroms)
    double pmf_gmin = 1e-10;       // Floor for g(r) to avoid ln(0)

    // Collect all extra args
    std::map<std::string, std::string> extras;
};

// Complete parsed command
struct ParsedCommand {
    Spec spec;
    Action action = Action::Unknown;
    DomainParams domain;
    GlobalFlags globals;
    ActionParams action_params;
};

// Parser
class CommandParser {
public:
    ParsedCommand parse(int argc, char** argv);
    
private:
    Spec parse_spec(const std::string& spec_str);
    Composition parse_formula(const std::string& formula);
    DomainMode parse_mode_hint(const std::string& hint);
    Action parse_action(const std::string& action_str);
    
    void parse_domain_params(int& i, int argc, char** argv, DomainParams& params);
    void parse_global_flags(int& i, int argc, char** argv, GlobalFlags& flags);
    void parse_action_params(int& i, int argc, char** argv, ActionParams& params, Action action);
    
    std::vector<double> parse_triple(const std::string& str);
};

} // namespace cli
} // namespace vsepr
