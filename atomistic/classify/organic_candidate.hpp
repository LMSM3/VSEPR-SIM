#pragma once
/**
 * organic_candidate.hpp
 * =====================
 * Organic classification layer for the formation engine.
 *
 * Metals can often be summarised with: lattice type, lattice parameter,
 * coordination tendency, cohesion.
 *
 * Organics need:
 *   connectivity graph, bond order pattern, torsional DOF,
 *   ring strain, conjugation continuity, donor/acceptor map,
 *   steric hindrance, local polarity map, H-bond opportunities,
 *   likely reactive sites.
 *
 * This header provides:
 *   1. OrganicFamily enum — chemical family tags (not decorative labels)
 *   2. OrganicCandidate struct — extended identity card for organics
 *   3. OrganicClassifier — derives family + descriptors from State
 *
 * Design rules:
 *   - Organic family tags determine: likely bond patterns, steric
 *     constraints, rotational freedom, polarity, hydrogen bonding,
 *     decomposition routes, packing tendencies, kinetic bottlenecks.
 *   - This layer extends (does not replace) ElementCategory and
 *     ComprehensiveElementData for inorganic materials.
 *   - No data is hardcoded as a Z array.  All descriptors are derived
 *     from the connectivity graph + element properties.
 *
 * Compatibility: uses atomistic::State, fits alongside
 *   atomistic::kinetic::FormationKineticsEngine and
 *   atomistic::reaction::ReactionEngine.
 */

#include "../core/state.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace atomistic {
namespace classify {

// ============================================================================
// Organic family tags
// ============================================================================

/**
 * Chemical family classification for organic candidates.
 *
 * These are NOT decorative labels.  Each tag implies:
 *   - likely bond patterns           (single, double, aromatic, etc.)
 *   - steric constraints             (ring, chain, branching)
 *   - rotational freedom             (rigid ring vs flexible chain)
 *   - polarity                       (dipole moment tendency)
 *   - hydrogen bonding               (donor / acceptor sites)
 *   - decomposition routes           (thermal, oxidative, hydrolytic)
 *   - packing tendencies             (crystal packing, liquid structure)
 *   - kinetic bottlenecks            (steric barrier, orbital mismatch)
 */
enum class OrganicFamily : uint8_t {
    ALKANE,
    ALKENE,
    ALKYNE,
    AROMATIC,
    HETEROAROMATIC,
    ALCOHOL,
    ETHER,
    KETONE,
    ALDEHYDE,
    CARBOXYLIC_ACID,
    ESTER,
    AMIDE,
    AMINE,
    NITRILE,
    HALOGENATED_ORGANIC,
    ORGANOMETALLIC,
    POLYMER_PRECURSOR,
    CROSSLINKABLE_SPECIES,
    RADICAL_PRONE,
    CONJUGATED,
    RIGID_RING,
    FLEXIBLE_CHAIN,
    DONOR_RICH,
    ACCEPTOR_RICH,
    MIXED_ORGANIC,         // Multiple families apply
    UNKNOWN_ORGANIC,
};

const char* organic_family_name(OrganicFamily f);

// ============================================================================
// Organic candidate: the extended identity card
// ============================================================================

/**
 * OrganicCandidate carries chemistry-aware descriptors that go far
 * beyond atom positions.  This is what makes organic support real
 * instead of fake.
 *
 * All scores are [0, 1] unless otherwise noted.
 * All counts are non-negative integers.
 */
struct OrganicCandidate {
    // ── Identity ──────────────────────────────────────────────────────────
    std::string id_hash;                 // Deterministic hash of connectivity
    std::string formula;                 // Molecular formula (Hill order)
    OrganicFamily primary_family = OrganicFamily::UNKNOWN_ORGANIC;
    std::vector<OrganicFamily> families; // All applicable families
    std::vector<std::string> functional_groups;  // "hydroxyl", "carbonyl", etc.

    // ── Topology counts ───────────────────────────────────────────────────
    int ring_count            = 0;
    int aromatic_ring_count   = 0;
    int heteroatom_count      = 0;       // N, O, S, P, ... (non-C, non-H)
    int rotatable_bond_count  = 0;
    int chiral_centre_count   = 0;
    int sp3_count             = 0;
    int sp2_count             = 0;
    int sp_count              = 0;

    // ── Chemistry-aware descriptors ───────────────────────────────────────
    double steric_score       = 0.0;     // 0 = unhindered, 1 = heavily blocked
    double polarity_score     = 0.0;     // 0 = nonpolar, 1 = strongly polar
    double hbond_donor_score  = 0.0;     // 0 = no donors, 1 = rich in OH/NH
    double hbond_acceptor_score = 0.0;   // 0 = no acceptors, 1 = rich in lone pairs
    double conjugation_score  = 0.0;     // 0 = no conjugation, 1 = fully conjugated
    double strain_score       = 0.0;     // 0 = strain-free, 1 = severely strained

    // ── Formation engine integration ──────────────────────────────────────
    double thermo_score       = 0.0;     // Thermodynamic stability
    double kinetic_score      = 0.0;     // Kinetic formation likelihood
    double packing_score      = 0.0;     // Crystal / liquid packing tendency
    double decomposition_risk = 0.0;     // 0 = stable, 1 = decomposes readily
    double final_score        = 0.0;     // Combined S_form from kinetic engine

    // ── Reactive site map ─────────────────────────────────────────────────
    // Index → descriptor pairs for atoms with notable reactivity
    struct ReactiveSite {
        uint32_t atom_index;
        std::string element;
        double fukui_plus;          // Nucleophilic attack propensity
        double fukui_minus;         // Electrophilic attack propensity
        double local_softness;
        std::string site_type;      // "electrophilic_centre", "leaving_group", etc.
    };
    std::vector<ReactiveSite> reactive_sites;

    // ── Donor / acceptor map ──────────────────────────────────────────────
    struct DonorAcceptor {
        uint32_t atom_index;
        bool is_donor;              // H-bond donor (OH, NH)
        bool is_acceptor;           // H-bond acceptor (lone pair on O, N, S)
        double strength;            // 0-1 estimated strength
    };
    std::vector<DonorAcceptor> donor_acceptor_map;

    // ── Local polarity map ────────────────────────────────────────────────
    // Per-atom partial charge and polarity contribution
    struct PolaritySite {
        uint32_t atom_index;
        double partial_charge;      // e units
        double polarity_contribution;
    };
    std::vector<PolaritySite> polarity_map;

    // ── Torsional profile (rotatable bonds) ───────────────────────────────
    struct TorsionProfile {
        uint32_t bond_atom_a;
        uint32_t bond_atom_b;
        double barrier_kcal_mol;    // Rotational barrier height
        int symmetry_fold;          // 2-fold, 3-fold, etc.
    };
    std::vector<TorsionProfile> torsion_profiles;
};

// ============================================================================
// Organic classifier: derives descriptors from atomistic State
// ============================================================================

/**
 * OrganicClassifier analyses an atomistic::State and produces an
 * OrganicCandidate with all descriptors populated.
 *
 * It does NOT invent data.  If information is unavailable (e.g. no
 * Fukui function computed), the corresponding field remains at its
 * default (zero / empty).
 */
class OrganicClassifier {
public:
    OrganicClassifier() = default;

    /**
     * Classify an atomistic state as an organic candidate.
     * Returns an OrganicCandidate with all derivable fields populated.
     *
     * @param state  The atomistic state (must have connectivity)
     * @return       Populated OrganicCandidate
     */
    OrganicCandidate classify(const State& state) const;

    /**
     * Determine primary and secondary organic families from topology.
     * @param state  The atomistic state
     * @return       Vector of applicable OrganicFamily tags
     */
    std::vector<OrganicFamily>
    identify_families(const State& state) const;

    /**
     * Detect functional groups from connectivity and element types.
     * @param state  The atomistic state
     * @return       Human-readable functional group labels
     */
    std::vector<std::string>
    detect_functional_groups(const State& state) const;

    /**
     * Count rings (total, aromatic, strained).
     * Uses SSSR (smallest set of smallest rings) algorithm.
     */
    struct RingInfo {
        int total         = 0;
        int aromatic      = 0;
        int strained      = 0;   // 3- or 4-membered
        int heteroaromatic = 0;
    };
    RingInfo count_rings(const State& state) const;

    /**
     * Compute rotatable bond count.
     * A bond is rotatable if: single bond, not in ring, not terminal,
     * and rotation changes at least one torsion angle.
     */
    int count_rotatable_bonds(const State& state) const;

    /**
     * Compute steric score [0,1] from local coordination and atom radii.
     */
    double compute_steric(const State& state) const;

    /**
     * Compute polarity score [0,1] from partial charges.
     */
    double compute_polarity(const State& state) const;

    /**
     * Compute conjugation score [0,1] from alternating single/double bonds.
     */
    double compute_conjugation(const State& state) const;

    /**
     * Compute strain score [0,1] from ring sizes and angle deviations.
     */
    double compute_strain(const State& state) const;

    /**
     * Build the donor/acceptor map for hydrogen bonding.
     */
    std::vector<OrganicCandidate::DonorAcceptor>
    map_donors_acceptors(const State& state) const;

    /**
     * Build the reactive site map (Fukui-based if available, else heuristic).
     */
    std::vector<OrganicCandidate::ReactiveSite>
    map_reactive_sites(const State& state) const;

    /**
     * Estimate decomposition risk [0,1] from bond energies and strain.
     */
    double estimate_decomposition_risk(const State& state) const;
};

} // namespace classify
} // namespace atomistic
