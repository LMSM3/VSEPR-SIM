#pragma once

#include "parse.hpp"
#include <string>
#include <random>

namespace vsepr {
namespace cli {

// Domain resolution rules
struct DomainRules {
    bool pbc_enabled;      // Periodic boundary conditions
    bool pbc_mandatory;    // Cannot be disabled
    bool requires_cell;    // Must specify --cell
    bool requires_box;     // Must specify --box (if no cell)
    
    std::string rationale; // Why these rules apply
};

// Runtime context for action execution
struct RunContext {
    // From global flags
    std::string output_path;
    int seed;
    bool viz_enabled;     // Launch viewer after completion
    bool watch_enabled;   // Launch viewer in live mode

    // Domain resolution
    DomainRules rules;
    
    // Validated domain parameters
    std::vector<double> cell_or_box;
    bool has_cell;  // true = cell, false = box
    
    // RNG (seeded consistently)
    std::mt19937 rng;
    
    // Construction
    static RunContext from_parsed(const ParsedCommand& cmd);
    
private:
    RunContext() = default;
    static DomainRules resolve_domain(const Spec& spec, const DomainParams& params);
    static void validate_domain_params(const DomainRules& rules, const DomainParams& params);
};

} // namespace cli
} // namespace vsepr
