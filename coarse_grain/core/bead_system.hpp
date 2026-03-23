#pragma once
/**
 * bead_system.hpp — Coarse-Grained Bead System
 *
 * The BeadSystem is the CG analogue of atomistic::State.
 * It holds the full collection of beads plus aggregate diagnostics.
 *
 * Invariant: every atom in the source State appears in exactly one bead.
 * This is checked by sane() and by the conservation validators.
 *
 * Philosophy: deterministic, inspectable, no hidden state.
 */

#include "coarse_grain/core/bead.hpp"
#include <cassert>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

namespace coarse_grain {

/**
 * Conservation report — verifiable mass/charge accounting.
 */
struct ConservationReport {
    double atomistic_total_mass{};
    double coarse_grain_total_mass{};
    double mass_error{};            // |atomistic - CG| (amu)

    double atomistic_total_charge{};
    double coarse_grain_total_charge{};
    double charge_error{};          // |atomistic - CG| (e)

    bool mass_conserved{};          // |error| < tolerance
    bool charge_conserved{};        // |error| < tolerance

    static constexpr double DEFAULT_MASS_TOL   = 1.0e-10;   // amu
    static constexpr double DEFAULT_CHARGE_TOL  = 1.0e-10;  // e

    bool all_conserved() const { return mass_conserved && charge_conserved; }
};

/**
 * Mapping diagnostics — per-bead quality metrics.
 */
struct MappingDiagnostics {
    double mean_residual{};     // Average |COM - COG| across beads (Å)
    double max_residual{};      // Worst-case residual (Å)
    double total_mass{};        // Sum of bead masses
    double total_charge{};      // Sum of bead charges
    uint32_t n_beads{};
    uint32_t n_atoms_mapped{};  // Should equal source State.N
};

/**
 * BeadSystem — the full coarse-grained representation.
 */
struct BeadSystem {
    std::vector<Bead>     beads;
    std::vector<BeadType> bead_types;

    // Topology (bead-bead bonds inferred from atomistic connectivity)
    std::vector<std::pair<uint32_t, uint32_t>> bonds;

    // Source provenance
    uint32_t source_atom_count{};   // N from the atomistic State

    // Active projection mode used to compute bead centers
    ProjectionMode projection_mode = ProjectionMode::CENTER_OF_MASS;

    // --- Inspectors ---

    uint32_t num_beads() const { return static_cast<uint32_t>(beads.size()); }

    /**
     * Compute conservation report against the source atomistic State.
     */
    ConservationReport check_conservation(const atomistic::State& source,
                                          double mass_tol  = ConservationReport::DEFAULT_MASS_TOL,
                                          double charge_tol = ConservationReport::DEFAULT_CHARGE_TOL) const
    {
        ConservationReport r;
        r.atomistic_total_mass   = std::accumulate(source.M.begin(), source.M.end(), 0.0);
        r.atomistic_total_charge = std::accumulate(source.Q.begin(), source.Q.end(), 0.0);

        for (const auto& b : beads) {
            r.coarse_grain_total_mass   += b.mass;
            r.coarse_grain_total_charge += b.charge;
        }

        r.mass_error   = std::abs(r.atomistic_total_mass   - r.coarse_grain_total_mass);
        r.charge_error = std::abs(r.atomistic_total_charge - r.coarse_grain_total_charge);
        r.mass_conserved   = r.mass_error   < mass_tol;
        r.charge_conserved = r.charge_error < charge_tol;
        return r;
    }

    /**
     * Compute per-bead mapping diagnostics.
     */
    MappingDiagnostics diagnostics() const {
        MappingDiagnostics d;
        d.n_beads = num_beads();
        d.max_residual = 0.0;
        double sum_residual = 0.0;

        for (const auto& b : beads) {
            d.total_mass   += b.mass;
            d.total_charge += b.charge;
            d.n_atoms_mapped += static_cast<uint32_t>(b.parent_atom_indices.size());
            sum_residual += b.mapping_residual;
            if (b.mapping_residual > d.max_residual)
                d.max_residual = b.mapping_residual;
        }

        d.mean_residual = (d.n_beads > 0) ? sum_residual / d.n_beads : 0.0;
        return d;
    }

    /**
     * Sanity check — mirrors atomistic::sane().
     *
     * Checks:
     *   1. At least one bead exists.
     *   2. Every bead has at least one parent atom.
     *   3. The union of parent atom indices covers [0, source_atom_count).
     *   4. No atom appears in more than one bead.
     */
    bool sane() const {
        if (beads.empty()) return false;
        if (source_atom_count == 0) return false;

        std::vector<uint8_t> covered(source_atom_count, 0);
        for (const auto& b : beads) {
            if (b.parent_atom_indices.empty()) return false;
            for (uint32_t idx : b.parent_atom_indices) {
                if (idx >= source_atom_count) return false;
                if (covered[idx] != 0) return false;  // duplicate mapping
                covered[idx] = 1;
            }
        }

        for (uint32_t i = 0; i < source_atom_count; ++i) {
            if (covered[i] == 0) return false;  // unmapped atom
        }
        return true;
    }
};

} // namespace coarse_grain
