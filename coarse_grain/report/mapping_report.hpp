#pragma once
/**
 * mapping_report.hpp — Standalone Mapping Report Generator
 *
 * Produces a Markdown file documenting every mapping decision.
 * This is the file-export counterpart to the visual inspector.
 *
 * Anti-black-box: every atom, every bead, every rule, every metric
 * is written to a human-readable document.
 */

#include "coarse_grain/mapping/atom_to_bead_mapper.hpp"
#include <fstream>
#include <string>

namespace coarse_grain {

/**
 * Write a mapping report to a Markdown file.
 *
 * @param path    Output file path (e.g. "mapping_report.md")
 * @param state   Source atomistic state
 * @param scheme  Mapping scheme used
 * @param system  Resulting BeadSystem
 * @param cons    Conservation report
 * @return true on success
 */
inline bool write_mapping_report(const std::string& path,
                                  const atomistic::State& state,
                                  const MappingScheme& scheme,
                                  const BeadSystem& system,
                                  const ConservationReport& cons)
{
    std::string content = AtomToBeadMapper::mapping_report(state, scheme, system, cons);

    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << content;
    return out.good();
}

} // namespace coarse_grain
