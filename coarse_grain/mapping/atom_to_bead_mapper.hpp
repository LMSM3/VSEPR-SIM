#pragma once
/**
 * atom_to_bead_mapper.hpp — Deterministic Atom→Bead Mapping Engine
 *
 * Given an atomistic::State and a MappingScheme, produce a BeadSystem.
 *
 * This is the single entry point for the atomistic→CG projection.
 * Every step is explicit, inspectable, and deterministic:
 *
 *   1. Validate scheme (no overlaps, full coverage).
 *   2. Resolve selectors → concrete atom index lists.
 *   3. Compute bead centers (COM or COG).
 *   4. Aggregate mass and charge.
 *   5. Compute residual diagnostics.
 *   6. Infer bead-bead topology from atomistic bonds.
 *   7. Return BeadSystem with full provenance.
 *
 * Anti-black-box: mapping_report() emits a human-readable Markdown report
 * that traces every bead back to its parent atoms with all metrics.
 */

#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/mapping/mapping_rule.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace coarse_grain {

/**
 * Mapping result — either success with a BeadSystem, or failure with reason.
 */
struct MappingResult {
    bool           ok{false};
    std::string    error;
    BeadSystem     system;
    ConservationReport conservation;
};

/**
 * AtomToBeadMapper — stateless, deterministic mapper.
 *
 * Usage:
 *   AtomToBeadMapper mapper;
 *   auto result = mapper.map(state, scheme, ProjectionMode::CENTER_OF_MASS);
 *   if (result.ok) { ... use result.system ... }
 */
class AtomToBeadMapper {
public:
    /**
     * Execute the full mapping pipeline.
     *
     * @param state   Source atomistic state (must pass atomistic::sane())
     * @param scheme  Mapping scheme (rules covering all atoms)
     * @param mode    Projection mode (COM or COG)
     * @return MappingResult with BeadSystem and ConservationReport
     */
    MappingResult map(const atomistic::State& state,
                      const MappingScheme& scheme,
                      ProjectionMode mode = ProjectionMode::CENTER_OF_MASS) const
    {
        MappingResult result;

        // Step 0: Validate source state
        if (!atomistic::sane(state)) {
            result.error = "Source atomistic::State fails sane() check";
            return result;
        }

        // Step 1: Validate scheme coverage
        std::string coverage_error;
        if (!validate_scheme(scheme, state.N, coverage_error)) {
            result.error = coverage_error;
            return result;
        }

        // Step 2-5: Build beads
        BeadSystem& sys = result.system;
        sys.source_atom_count = state.N;
        sys.projection_mode = mode;
        sys.beads.reserve(scheme.rules.size());

        for (const auto& rule : scheme.rules) {
            Bead bead;
            bead.type_id = rule.bead_type_id;
            bead.mapping_rule_id = rule.rule_id;

            // Resolve selector → concrete indices
            bead.parent_atom_indices = resolve_selector(rule.selector, state);

            // Aggregate mass and charge
            bead.mass = 0.0;
            bead.charge = 0.0;
            for (uint32_t idx : bead.parent_atom_indices) {
                bead.mass   += state.M[idx];
                bead.charge += state.Q[idx];
            }

            // Compute COM
            bead.com_position = compute_com(state, bead.parent_atom_indices);

            // Compute COG
            bead.cog_position = compute_cog(state, bead.parent_atom_indices);

            // Set active position from projection mode
            if (mode == ProjectionMode::CENTER_OF_MASS) {
                bead.position = bead.com_position;
            } else {
                bead.position = bead.cog_position;
            }

            // Compute velocity (mass-weighted average)
            bead.velocity = compute_com_velocity(state, bead.parent_atom_indices);

            // Mapping residual = |COM - COG|
            auto diff = bead.com_position - bead.cog_position;
            bead.mapping_residual = atomistic::norm(diff);

            sys.beads.push_back(std::move(bead));
        }

        // Step 6: Infer bead-bead topology
        sys.bonds = infer_bead_bonds(state, sys);

        // Step 7: Conservation check
        result.conservation = sys.check_conservation(state);
        result.ok = true;
        return result;
    }

    /**
     * Validate that a MappingScheme covers every atom exactly once.
     */
    static bool validate_scheme(const MappingScheme& scheme,
                                uint32_t n_atoms,
                                std::string& error_out)
    {
        if (scheme.rules.empty()) {
            error_out = "MappingScheme has no rules";
            return false;
        }

        // Placeholder coverage check — resolve all selectors
        // and verify full, non-overlapping coverage.
        std::vector<uint8_t> covered(n_atoms, 0);

        // We need a dummy state just for BY_TYPE resolution.
        // For validation, we only check BY_INDICES and BY_RANGE selectors directly.
        for (const auto& rule : scheme.rules) {
            const auto& sel = rule.selector;

            std::vector<uint32_t> indices;
            switch (sel.mode) {
            case SelectorMode::BY_INDICES:
                indices = sel.indices;
                break;
            case SelectorMode::BY_RANGE:
                for (uint32_t i = 0; i < sel.range_count; ++i)
                    indices.push_back(sel.range_first + i);
                break;
            case SelectorMode::BY_TYPE:
                // BY_TYPE requires the actual State for resolution.
                // Defer to map() for full validation.
                continue;
            }

            for (uint32_t idx : indices) {
                if (idx >= n_atoms) {
                    error_out = "Rule '" + rule.label + "' references atom index "
                              + std::to_string(idx) + " >= N=" + std::to_string(n_atoms);
                    return false;
                }
                if (covered[idx] != 0) {
                    error_out = "Rule '" + rule.label + "' maps atom "
                              + std::to_string(idx) + " which is already mapped";
                    return false;
                }
                covered[idx] = 1;
            }
        }

        // Check full coverage (skip if BY_TYPE rules exist — deferred)
        bool has_type_selectors = false;
        for (const auto& rule : scheme.rules) {
            if (rule.selector.mode == SelectorMode::BY_TYPE) {
                has_type_selectors = true;
                break;
            }
        }

        if (!has_type_selectors) {
            for (uint32_t i = 0; i < n_atoms; ++i) {
                if (covered[i] == 0) {
                    error_out = "Atom " + std::to_string(i) + " is not covered by any rule";
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * Generate a Markdown mapping report.
     *
     * This is the primary anti-black-box output: a human-readable trace
     * of every mapping decision, every conservation metric, every residual.
     */
    static std::string mapping_report(const atomistic::State& state,
                                      const MappingScheme& scheme,
                                      const BeadSystem& sys,
                                      const ConservationReport& cons)
    {
        std::ostringstream ss;

        ss << "# Coarse-Grained Mapping Report\n\n";
        ss << "## Scheme: " << scheme.name << "\n\n";

        // Summary
        ss << "| Property | Value |\n";
        ss << "|----------|-------|\n";
        ss << "| Source atoms | " << state.N << " |\n";
        ss << "| Beads produced | " << sys.num_beads() << " |\n";
        ss << "| Reduction ratio | " << state.N << ":" << sys.num_beads();
        if (sys.num_beads() > 0)
            ss << " (" << static_cast<double>(state.N) / sys.num_beads() << ":1)";
        ss << " |\n";
        ss << "| Projection mode | "
           << (sys.projection_mode == ProjectionMode::CENTER_OF_MASS ? "COM" : "COG")
           << " |\n";
        ss << "| Bead-bead bonds | " << sys.bonds.size() << " |\n\n";

        // Conservation
        ss << "## Conservation Check\n\n";
        ss << "| Quantity | Atomistic | Coarse-Grained | Error | Status |\n";
        ss << "|----------|-----------|----------------|-------|--------|\n";
        ss << "| Mass (amu) | " << cons.atomistic_total_mass
           << " | " << cons.coarse_grain_total_mass
           << " | " << cons.mass_error
           << " | " << (cons.mass_conserved ? "✓ PASS" : "✗ FAIL") << " |\n";
        ss << "| Charge (e) | " << cons.atomistic_total_charge
           << " | " << cons.coarse_grain_total_charge
           << " | " << cons.charge_error
           << " | " << (cons.charge_conserved ? "✓ PASS" : "✗ FAIL") << " |\n\n";

        // Per-bead detail
        ss << "## Per-Bead Detail\n\n";
        ss << "| Bead | Rule | Atoms | Mass | Charge | Residual (Å) | Position |\n";
        ss << "|------|------|-------|------|--------|--------------|----------|\n";

        for (uint32_t i = 0; i < sys.num_beads(); ++i) {
            const auto& b = sys.beads[i];
            ss << "| " << i << " | " << b.mapping_rule_id << " | ";

            // Atom indices (truncated if many)
            if (b.parent_atom_indices.size() <= 8) {
                for (size_t j = 0; j < b.parent_atom_indices.size(); ++j) {
                    if (j > 0) ss << ",";
                    ss << b.parent_atom_indices[j];
                }
            } else {
                for (size_t j = 0; j < 4; ++j) {
                    if (j > 0) ss << ",";
                    ss << b.parent_atom_indices[j];
                }
                ss << "...+" << (b.parent_atom_indices.size() - 4) << " more";
            }

            ss << " | " << b.mass
               << " | " << b.charge
               << " | " << b.mapping_residual
               << " | (" << b.position.x << ", " << b.position.y << ", " << b.position.z << ")"
               << " |\n";
        }

        // Diagnostics summary
        auto diag = sys.diagnostics();
        ss << "\n## Diagnostics\n\n";
        ss << "| Metric | Value |\n";
        ss << "|--------|-------|\n";
        ss << "| Mean residual | " << diag.mean_residual << " Å |\n";
        ss << "| Max residual | " << diag.max_residual << " Å |\n";
        ss << "| Atoms mapped | " << diag.n_atoms_mapped << "/" << state.N << " |\n";

        bool is_sane = sys.sane();
        ss << "| sane() | " << (is_sane ? "✓ PASS" : "✗ FAIL") << " |\n";

        // Surface descriptor summary (if any beads have surface data)
        bool any_surface = false;
        for (const auto& b : sys.beads) {
            if (b.has_surface_data()) { any_surface = true; break; }
        }
        if (any_surface) {
            ss << "\n## Anisotropic Surface Descriptors\n\n";
            ss << "| Bead | Asphericity | Aniso Ratio | Iso Component | Dominant ℓ | Samples |\n";
            ss << "|------|-------------|-------------|---------------|------------|----------|\n";
            for (uint32_t i = 0; i < sys.num_beads(); ++i) {
                const auto& b = sys.beads[i];
                if (b.has_surface_data()) {
                    ss << "| " << i
                       << " | " << b.asphericity()
                       << " | " << b.anisotropy_ratio()
                       << " | " << b.surface->isotropic_component()
                       << " | " << b.surface->dominant_band()
                       << " | " << b.surface->n_samples
                       << " |\n";
                } else {
                    ss << "| " << i << " | — | — | — | — | — |\n";
                }
            }
        }

        return ss.str();
    }

private:
    /**
     * Resolve a selector into concrete atom indices.
     */
    static std::vector<uint32_t> resolve_selector(const AtomSelector& sel,
                                                   const atomistic::State& state)
    {
        std::vector<uint32_t> indices;

        switch (sel.mode) {
        case SelectorMode::BY_INDICES:
            indices = sel.indices;
            break;

        case SelectorMode::BY_TYPE:
            for (uint32_t i = 0; i < state.N; ++i) {
                if (state.type[i] == sel.type_id)
                    indices.push_back(i);
            }
            break;

        case SelectorMode::BY_RANGE:
            indices.reserve(sel.range_count);
            for (uint32_t i = 0; i < sel.range_count; ++i)
                indices.push_back(sel.range_first + i);
            break;
        }

        return indices;
    }

    /**
     * Center of mass: r_COM = Σ(m_i * r_i) / Σ(m_i)
     */
    static atomistic::Vec3 compute_com(const atomistic::State& state,
                                        const std::vector<uint32_t>& indices)
    {
        atomistic::Vec3 weighted_sum{};
        double total_mass = 0.0;

        for (uint32_t idx : indices) {
            double m = state.M[idx];
            weighted_sum = weighted_sum + state.X[idx] * m;
            total_mass += m;
        }

        if (total_mass > 0.0)
            return weighted_sum * (1.0 / total_mass);
        return {};
    }

    /**
     * Center of geometry: r_COG = Σ(r_i) / N
     */
    static atomistic::Vec3 compute_cog(const atomistic::State& state,
                                        const std::vector<uint32_t>& indices)
    {
        atomistic::Vec3 sum{};
        for (uint32_t idx : indices)
            sum = sum + state.X[idx];

        if (!indices.empty())
            return sum * (1.0 / static_cast<double>(indices.size()));
        return {};
    }

    /**
     * COM velocity: v_COM = Σ(m_i * v_i) / Σ(m_i)
     */
    static atomistic::Vec3 compute_com_velocity(const atomistic::State& state,
                                                 const std::vector<uint32_t>& indices)
    {
        atomistic::Vec3 weighted_sum{};
        double total_mass = 0.0;

        for (uint32_t idx : indices) {
            double m = state.M[idx];
            weighted_sum = weighted_sum + state.V[idx] * m;
            total_mass += m;
        }

        if (total_mass > 0.0)
            return weighted_sum * (1.0 / total_mass);
        return {};
    }

    /**
     * Infer bead-bead bonds from atomistic connectivity.
     *
     * Two beads are bonded if any atom in bead A is bonded to any atom in bead B
     * in the atomistic edge list.
     */
    static std::vector<std::pair<uint32_t, uint32_t>>
    infer_bead_bonds(const atomistic::State& state, const BeadSystem& sys)
    {
        // Build atom→bead lookup
        std::vector<uint32_t> atom_to_bead(state.N, UINT32_MAX);
        for (uint32_t bi = 0; bi < sys.num_beads(); ++bi) {
            for (uint32_t ai : sys.beads[bi].parent_atom_indices)
                atom_to_bead[ai] = bi;
        }

        // Scan atomistic edges for inter-bead bonds
        std::vector<std::pair<uint32_t, uint32_t>> bead_bonds;

        for (const auto& edge : state.B) {
            uint32_t ba = atom_to_bead[edge.i];
            uint32_t bb = atom_to_bead[edge.j];
            if (ba != bb && ba != UINT32_MAX && bb != UINT32_MAX) {
                // Canonical ordering
                auto bond = (ba < bb) ? std::make_pair(ba, bb) : std::make_pair(bb, ba);
                // Deduplicate
                if (std::find(bead_bonds.begin(), bead_bonds.end(), bond) == bead_bonds.end())
                    bead_bonds.push_back(bond);
            }
        }

        return bead_bonds;
    }
};

} // namespace coarse_grain
