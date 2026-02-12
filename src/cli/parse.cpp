#include "cli/parse.hpp"
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <algorithm>

namespace vsepr {
namespace cli {

std::string Spec::formula() const {
    std::string result;
    for (const auto& [elem, count] : composition.elements) {
        result += elem;
        if (count > 1) result += std::to_string(count);
    }
    return result;
}

std::string Spec::mode_string() const {
    switch (mode) {
        case DomainMode::Gas: return "gas";
        case DomainMode::Crystal: return "crystal";
        case DomainMode::Bulk: return "bulk";
        case DomainMode::Molecule: return "molecule";
    }
    return "unknown";
}

ParsedCommand CommandParser::parse(int argc, char** argv) {
    ParsedCommand cmd;
    
    if (argc < 3) {
        throw std::runtime_error("Usage: vsepr <SPEC> <ACTION> [OPTIONS]");
    }
    
    // Parse SPEC (argv[1])
    cmd.spec = parse_spec(argv[1]);
    
    // Parse ACTION (argv[2])
    cmd.action = parse_action(argv[2]);
    if (cmd.action == Action::Unknown) {
        throw std::runtime_error("Unknown action: " + std::string(argv[2]));
    }
    
    // Parse remaining args
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Check if it's a domain param
        if (arg == "--cell" || arg == "--box" || arg == "--pbc") {
            parse_domain_params(i, argc, argv, cmd.domain);
            continue;
        }
        
        // Check if it's a global flag
        if (arg == "--out" || arg == "--seed" || arg == "--viz" || arg == "--watch") {
            parse_global_flags(i, argc, argv, cmd.globals);
            continue;
        }
        
        // Otherwise it's action-specific
        parse_action_params(i, argc, argv, cmd.action_params, cmd.action);
    }
    
    return cmd;
}

Spec CommandParser::parse_spec(const std::string& spec_str) {
    Spec spec;
    
    // Check for mode hint: H2O@crystal
    size_t at_pos = spec_str.find('@');
    std::string formula_part;
    
    if (at_pos != std::string::npos) {
        formula_part = spec_str.substr(0, at_pos);
        std::string mode_hint = spec_str.substr(at_pos + 1);
        spec.mode = parse_mode_hint(mode_hint);
    } else {
        formula_part = spec_str;
        spec.mode = DomainMode::Molecule;  // Default
    }
    
    // Parse formula
    spec.composition = parse_formula(formula_part);
    
    return spec;
}

Composition CommandParser::parse_formula(const std::string& formula) {
    Composition comp;
    
    size_t i = 0;
    while (i < formula.size()) {
        // Parse element (capital letter + optional lowercase)
        if (!std::isupper(formula[i])) {
            throw std::runtime_error("Invalid formula: expected element at position " + std::to_string(i));
        }
        
        std::string element(1, formula[i++]);
        while (i < formula.size() && std::islower(formula[i])) {
            element += formula[i++];
        }
        
        // Parse count (optional number)
        int count = 1;
        if (i < formula.size() && std::isdigit(formula[i])) {
            std::string count_str;
            while (i < formula.size() && std::isdigit(formula[i])) {
                count_str += formula[i++];
            }
            count = std::stoi(count_str);
        }
        
        comp.elements.push_back({element, count});
    }
    
    if (comp.elements.empty()) {
        throw std::runtime_error("Invalid formula: no elements found");
    }
    
    return comp;
}

DomainMode CommandParser::parse_mode_hint(const std::string& hint) {
    if (hint == "gas") return DomainMode::Gas;
    if (hint == "crystal") return DomainMode::Crystal;
    if (hint == "bulk") return DomainMode::Bulk;
    if (hint == "molecule") return DomainMode::Molecule;
    
    throw std::runtime_error("Unknown mode hint: @" + hint);
}

Action CommandParser::parse_action(const std::string& action_str) {
    if (action_str == "emit") return Action::Emit;
    if (action_str == "relax") return Action::Relax;
    if (action_str == "form") return Action::Form;
    if (action_str == "test") return Action::Test;
    return Action::Unknown;
}

void CommandParser::parse_domain_params(int& i, int argc, char** argv, DomainParams& params) {
    std::string arg = argv[i];
    
    if (arg == "--cell") {
        if (i + 1 >= argc) throw std::runtime_error("--cell requires value (a,b,c)");
        params.cell = parse_triple(argv[++i]);
    } else if (arg == "--box") {
        if (i + 1 >= argc) throw std::runtime_error("--box requires value (x,y,z)");
        params.box = parse_triple(argv[++i]);
    } else if (arg == "--pbc") {
        params.pbc_override = true;
    }
}

void CommandParser::parse_global_flags(int& i, int argc, char** argv, GlobalFlags& flags) {
    std::string arg = argv[i];

    if (arg == "--out") {
        if (i + 1 >= argc) throw std::runtime_error("--out requires path");
        flags.out_path = argv[++i];
    } else if (arg == "--seed") {
        if (i + 1 >= argc) throw std::runtime_error("--seed requires integer");
        flags.seed = std::stoi(argv[++i]);
    } else if (arg == "--viz") {
        flags.viz = true;
    } else if (arg == "--watch") {
        flags.watch = true;
    }
}

void CommandParser::parse_action_params(int& i, int argc, char** argv, ActionParams& params, Action action) {
    std::string arg = argv[i];
    
    // emit parameters
    if (arg == "--cloud") {
        if (i + 1 >= argc) throw std::runtime_error("--cloud requires integer");
        params.cloud_size = std::stoi(argv[++i]);
    } else if (arg == "--density") {
        if (i + 1 >= argc) throw std::runtime_error("--density requires float");
        params.density = std::stod(argv[++i]);
    } else if (arg == "--preset") {
        if (i + 1 >= argc) throw std::runtime_error("--preset requires ID");
        params.preset = argv[++i];
    }
    
    // relax and form parameters (shared: --steps)
    else if (arg == "--steps") {
        if (i + 1 >= argc) throw std::runtime_error("--steps requires integer");
        int step_value = std::stoi(argv[++i]);

        // For form action, use form_steps; for relax, use steps
        if (action == Action::Form) {
            params.form_steps = step_value;
        } else {
            params.steps = step_value;
        }
    } else if (arg == "--dt") {
        if (i + 1 >= argc) throw std::runtime_error("--dt requires float");
        params.dt = std::stod(argv[++i]);
    } else if (arg == "--in") {
        if (i + 1 >= argc) throw std::runtime_error("--in requires path");
        params.input_file = argv[++i];
    } else if (arg == "--config") {
        if (i + 1 >= argc) throw std::runtime_error("--config requires path");
        params.config_file = argv[++i];
    }

    // form parameters
    else if (arg == "--checkpoint") {
        if (i + 1 >= argc) throw std::runtime_error("--checkpoint requires integer");
        params.checkpoint = std::stoi(argv[++i]);
    } else if (arg == "--T") {
        if (i + 1 >= argc) throw std::runtime_error("--T requires schedule (T_start:T_peak:T_end)");
        params.temperature_schedule = argv[++i];
    } else if (arg == "--diffusion") {
        if (i + 1 >= argc) throw std::runtime_error("--diffusion requires float");
        params.diffusion_scale = std::stod(argv[++i]);
    }

    // PMF parameters
    else if (arg == "--pmf-report" || arg == "--pmf-pair") {
        if (i + 1 >= argc) throw std::runtime_error(arg + " requires pair spec (e.g., Mg:F)");
        params.pmf_pair = argv[++i];
    } else if (arg == "--pmf-pairs") {
        if (i + 1 >= argc) throw std::runtime_error("--pmf-pairs requires comma-separated list (e.g., Mg:F,F:F)");
        std::string pairs_str = argv[++i];
        std::stringstream ss(pairs_str);
        std::string pair;
        while (std::getline(ss, pair, ',')) {
            params.pmf_pairs.push_back(pair);
        }
    } else if (arg == "--pmf-bins") {
        if (i + 1 >= argc) throw std::runtime_error("--pmf-bins requires integer");
        params.pmf_bins = std::stoi(argv[++i]);
    } else if (arg == "--pmf-rmax") {
        if (i + 1 >= argc) throw std::runtime_error("--pmf-rmax requires float (Angstroms)");
        params.pmf_rmax = std::stod(argv[++i]);
    } else if (arg == "--pmf-gmin") {
        if (i + 1 >= argc) throw std::runtime_error("--pmf-gmin requires float");
        params.pmf_gmin = std::stod(argv[++i]);
    }

    // Unknown parameter - store in extras
    else {
        if (i + 1 < argc) {
            params.extras[arg] = argv[++i];
        } else {
            params.extras[arg] = "";
        }
    }
}

std::vector<double> CommandParser::parse_triple(const std::string& str) {
    std::vector<double> values;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        values.push_back(std::stod(token));
    }
    
    if (values.size() != 3) {
        throw std::runtime_error("Expected 3 values (a,b,c), got " + std::to_string(values.size()));
    }
    
    return values;
}

} // namespace cli
} // namespace vsepr
