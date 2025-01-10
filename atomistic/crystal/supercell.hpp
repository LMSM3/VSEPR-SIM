#pragma once
/**
 * supercell.hpp
 * -------------
 * Supercell construction and validation for atomistic::State.
 *
 * Implements §10 of the methodology:
 *   r_{i,pqr} = r_i + p·a + q·b + r·c
 *
 * Key operations:
 *   construct_supercell(unit_cell, na, nb, nc) → State
 *   validate_supercell(unit_cell, supercell)   → ConstructionReport
 *
 * Construction follows the documented protocol:
 *   1. Geometric replication (no bond topology)
 *   2. Scaled box vectors
 *   3. Optional FIRE relaxation (fixed lattice)
 *   4. Strain detection validation
 */

#include "lattice.hpp"
#include "unit_cell.hpp"
#include "../core/state.hpp"
#include <string>
#include <vector>

namespace atomistic {
namespace crystal {

// ============================================================================
// Construction Recipe (provenance chain)
// ============================================================================

struct ConstructionStep {
    std::string op;          // "load", "supercell", "relax", "annotate"
    std::string detail;      // Human-readable detail
};

struct ConstructionRecipe {
    std::vector<ConstructionStep> steps;

    void add(const std::string& op, const std::string& detail) {
        steps.push_back({op, detail});
    }
};

// ============================================================================
// Supercell Result
// ============================================================================

struct SupercellResult {
    State   state;         // Supercell atomistic state (PBC enabled)
    Lattice lattice;       // Expanded lattice
    int     na, nb, nc;    // Replication factors
    uint32_t total_atoms;  // N_unit × na × nb × nc
    ConstructionRecipe recipe;
};

// ============================================================================
// Construction
// ============================================================================

/**
 * Construct a supercell from a UnitCell.
 *
 * Replicates all basis atoms by (na, nb, nc) along lattice vectors.
 * The resulting State has PBC enabled with scaled box dimensions.
 * Atomic properties (mass, charge, type) are replicated identically.
 * Bond topology is NOT replicated — must be re-inferred.
 *
 * @param uc   Unit cell definition
 * @param na   Replications along a
 * @param nb   Replications along b
 * @param nc   Replications along c
 * @return     SupercellResult with State and provenance
 */
SupercellResult construct_supercell(const UnitCell& uc, int na, int nb, int nc);

/**
 * Construct a supercell from an existing State and Lattice.
 * Used when the unit cell was built manually (not from a preset).
 */
SupercellResult construct_supercell(const State& unit_state,
                                    const Lattice& lattice,
                                    int na, int nb, int nc);

// ============================================================================
// Validation
// ============================================================================

struct ConstructionReport {
    // Strain detection: |E_super/N - E_unit/N| / |E_unit/N|
    double strain;
    bool   strain_pass;        // strain < 0.01

    // Bond count consistency: |n_bonds_super - n_bonds_unit × na×nb×nc| / expected
    double bond_count_error;
    bool   bond_count_pass;    // error < 0.05

    // Coordination number consistency
    double mean_coordination;  // Average coordination number
    double coord_stddev;       // Standard deviation of coordination

    bool all_passed() const { return strain_pass && bond_count_pass; }
};

/**
 * Validate a supercell against its unit cell.
 * Requires energy evaluation (caller provides per-atom energies).
 *
 * @param E_unit_per_atom   Energy per atom of unit cell
 * @param E_super_per_atom  Energy per atom of supercell
 * @param n_bonds_unit      Bonds inferred in unit cell
 * @param n_bonds_super     Bonds inferred in supercell
 * @param replication       na × nb × nc
 */
ConstructionReport validate_construction(
    double E_unit_per_atom,
    double E_super_per_atom,
    uint32_t n_bonds_unit,
    uint32_t n_bonds_super,
    int replication_product);

/**
 * Infer bonds from distance: pairs closer than f × (r_cov_i + r_cov_j)
 * Returns edge list and count.
 *
 * @param s         State with positions
 * @param lattice   Lattice for MIC distance calculation
 * @param f         Tolerance factor (default 1.2)
 */
struct InferredBonds {
    std::vector<Edge> edges;
    uint32_t count;
};

InferredBonds infer_bonds_from_distances(const State& s,
                                         const Lattice& lattice,
                                         double f = 1.2);

/**
 * Compute coordination number for each atom.
 * Uses distance-based bond inference.
 */
std::vector<uint32_t> coordination_numbers(const State& s,
                                           const Lattice& lattice,
                                           double f = 1.2);

/**
 * Wrap all positions in a State to the primary cell [0, L).
 * Modifies positions in-place. Only for orthogonal boxes.
 */
void wrap_positions(State& s);

/**
 * Wrap positions for triclinic lattice (fractional-space wrap).
 * Converts to fractional, wraps to [0,1), converts back.
 */
void wrap_positions(State& s, const Lattice& lattice);

} // namespace crystal
} // namespace atomistic
