#pragma once
// mcf_cai_parser.hpp  -  MCF-CAI Kernel Fork  -  .vsim object block parser bridge
// Phase 3b | v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE
//
// Parses [object.<name>.macro.carrier], [object.<name>.chemical.*],
// [object.<name>.fundamental.*] TOML-style blocks from a .vsim script
// and populates a McfCaiWorld.
//
// Entry point: McfCaiParser::parse_file(path) or parse_stream(istream).
//
// Activation: gated by the presence of [mcf_kernel] section or
// use_mcf_kernel = true in [run]. When absent, the existing
// SimulationState path is unaffected.
//
// Constraint: does NOT import SimulationState, Molecule, or VsimDocument.
// It is a self-contained text parser for the [object.*] subtree only.

#include "mcf_cai_object.hpp"
#include "mcf_cai_world.hpp"

#include <istream>
#include <string>
#include <vector>

namespace vsim::kernel_mcf {

// ============================================================================
// ParseError
// ============================================================================

struct McfCaiParseError {
    int         line   = 0;
    std::string message;
    bool ok() const noexcept { return message.empty(); }
};

// ============================================================================
// McfCaiParser
// ============================================================================

class McfCaiParser {
public:
    /// Parse a .vsim file.  Populates world with all [object.*] blocks found.
    /// Returns parse errors (may be empty = success).
    static std::vector<McfCaiParseError> parse_file(const std::string& path, McfCaiWorld& world);

    /// Parse from an already-open stream.
    static std::vector<McfCaiParseError> parse_stream(std::istream& in, McfCaiWorld& world);

    /// Check if a .vsim file/stream enables the MCF kernel.
    /// Returns true if [mcf_kernel] section present OR
    ///   [run] contains use_mcf_kernel = true.
    static bool is_mcf_kernel_enabled(const std::string& path);
    static bool is_mcf_kernel_enabled_stream(std::istream& in);
};

} // namespace vsim::kernel_mcf
