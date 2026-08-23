#include "cli/run_context.hpp"
#include <stdexcept>
#include <chrono>
#include <iostream>

namespace vsepr {
namespace cli {

RunContext RunContext::from_parsed(const ParsedCommand& cmd) {
    RunContext ctx;
    
    // Extract global flags
    ctx.output_path = cmd.globals.out_path.empty() ? "out.xyz" : cmd.globals.out_path;
    ctx.viz_enabled = cmd.globals.viz;
    ctx.watch_enabled = cmd.globals.watch;

    // Seed RNG
    if (cmd.globals.seed >= 0) {
        ctx.seed = cmd.globals.seed;
    } else {
        // Use current time as seed
        auto now = std::chrono::high_resolution_clock::now();
        ctx.seed = static_cast<int>(now.time_since_epoch().count() % 1000000);
    }
    ctx.rng.seed(ctx.seed);
    
    // Resolve domain rules
    ctx.rules = resolve_domain(cmd.spec, cmd.domain);
    
    // Validate domain parameters against rules
    validate_domain_params(ctx.rules, cmd.domain);
    
    // Extract cell or box
    if (cmd.domain.cell.has_value()) {
        ctx.cell_or_box = cmd.domain.cell.value();
        ctx.has_cell = true;
    } else if (cmd.domain.box.has_value()) {
        ctx.cell_or_box = cmd.domain.box.value();
        ctx.has_cell = false;
    }
    
    return ctx;
}

DomainRules RunContext::resolve_domain(const Spec& spec, const DomainParams& params) {
    DomainRules rules;
    
    switch (spec.mode) {
        case DomainMode::Crystal:
            rules.pbc_enabled = true;
            rules.pbc_mandatory = true;
            rules.requires_cell = true;
            rules.requires_box = false;
            rules.rationale = "@crystal: PBC mandatory, requires --cell";
            break;
            
        case DomainMode::Bulk:
            rules.pbc_enabled = true;
            rules.pbc_mandatory = true;
            rules.requires_cell = true;
            rules.requires_box = false;
            rules.rationale = "@bulk: PBC mandatory, requires --cell";
            break;
            
        case DomainMode::Gas:
            rules.pbc_enabled = params.pbc_override;
            rules.pbc_mandatory = false;
            rules.requires_cell = false;
            rules.requires_box = params.pbc_override;  // If PBC enabled, need box or cell
            rules.rationale = "@gas: PBC optional, requires --box if --pbc enabled";
            break;
            
        case DomainMode::Molecule:
            rules.pbc_enabled = params.pbc_override;
            rules.pbc_mandatory = false;
            rules.requires_cell = false;
            rules.requires_box = params.pbc_override;  // If PBC enabled, need box or cell
            rules.rationale = "@molecule: PBC optional, requires --box if --pbc enabled";
            break;
    }
    
    return rules;
}

void RunContext::validate_domain_params(const DomainRules& rules, const DomainParams& params) {
    // Check if PBC override is forbidden
    if (params.pbc_override && rules.pbc_mandatory) {
        throw std::runtime_error(
            "ERROR: --pbc flag is redundant for crystal/bulk domains.\n"
            "PBC is always enabled for @crystal and @bulk.\n"
            "Rationale: " + rules.rationale
        );
    }
    
    // Check if cell is required
    if (rules.requires_cell && !params.cell.has_value()) {
        throw std::runtime_error(
            "ERROR: --cell required for this domain.\n"
            "Rationale: " + rules.rationale
        );
    }
    
    // Check if box is required (when PBC enabled for non-crystal)
    if (rules.requires_box && !params.box.has_value() && !params.cell.has_value()) {
        throw std::runtime_error(
            "ERROR: --box or --cell required when PBC is enabled.\n"
            "Rationale: " + rules.rationale
        );
    }
    
    // Check for conflicting parameters
    if (params.cell.has_value() && params.box.has_value()) {
        throw std::runtime_error(
            "ERROR: Cannot specify both --cell and --box.\n"
            "Use --cell for crystal lattice or --box for confinement."
        );
    }
    
    // Validate cell/box values
    if (params.cell.has_value()) {
        for (double val : params.cell.value()) {
            if (val <= 0) {
                throw std::runtime_error("ERROR: --cell values must be positive");
            }
        }
    }
    if (params.box.has_value()) {
        for (double val : params.box.value()) {
            if (val <= 0) {
                throw std::runtime_error("ERROR: --box values must be positive");
            }
        }
    }
}

} // namespace cli
} // namespace vsepr
