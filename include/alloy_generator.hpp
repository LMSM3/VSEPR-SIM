#pragma once
/**
 * alloy_generator.hpp — Stochastic Metallic Alloy Generator
 *
 * Generates random copper-base alloy compositions with a configurable
 * chaos factor that controls:
 *
 *   - Substitutional disorder amplitude (site occupation probability spread)
 *   - Secondary phase precipitation probability
 *   - Elemental epsilon perturbation (thermal / defect noise on ε_i)
 *   - Grain boundary segregation fraction
 *
 * The chaos factor χ ∈ (0, ∞):
 *   χ < 1.0   — near-ideal binary/ternary, low disorder
 *   χ = 1.0   — balanced stochastic composition
 *   χ > 1.0   — high compositional disorder, secondary phases, defect beads
 *   χ = 1.25  — the intended operating point: metallic chaos, rich bead variety
 *
 * Every alloy bead generated carries full L2 identity (Z, A, Q, ε) and the
 * correct StructuralRole for a metallic species.  The chaos factor perturbs
 * ε within physics-valid bounds but never changes Z or A (propagation
 * invariant).
 *
 * Copper-base alloy element pool (Z values):
 *   Cu  (29) — base metal
 *   Zn  (30) — brass former
 *   Sn  (50) — bronze former
 *   Ni  (28) — cupro-nickel, Monel
 *   Al  (13) — aluminium bronze
 *   Mn  (25) — manganese bronze
 *   Si  (14) — silicon bronze
 *   Be  ( 4) — beryllium copper (high-strength)
 *   P   (15) — phosphor bronze
 *   Fe  (26) — iron-bearing copper
 *
 * Anti-black-box: every bead's ε perturbation, its site-swap probability,
 * and its secondary-phase flag are recorded in AlloyBeadRecord.
 *
 * Deterministic: provide the same seed → same alloy every time.
 *
 * Reference: include/layer_stack.hpp (L2, L4 layers)
 *            docs/section_layer_stack.tex §3–§4
 */

#include "include/layer_stack.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include "src/pot/lj_epsilon_params.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace alloy {

// ============================================================================
// LJ sigma table (Å) for the alloy element pool — metallic atomic radii
// ============================================================================

struct ElementRecord {
    uint8_t     Z;
    uint8_t     A;           // Most-abundant isotope
    double      mass;        // amu
    double      sigma;       // Å  — metallic radius × 2 (LJ σ)
    double      epsilon_ref; // kcal/mol — base value from LJ table
    const char* symbol;
    const char* role_hint;   // "solvent", "solute-substitutional", "interstitial"
};

// Pool of alloying elements for Cu-base systems
// sigma ≈ 2 × metallic radius (standard LJ convention for metals)
static constexpr std::array<ElementRecord, 10> CU_ALLOY_POOL = {{
    {  4,  9,   9.012, 2.23, 0.085, "Be", "solute-substitutional" },
    { 13, 27,  26.982, 2.86, 0.155, "Al", "solute-substitutional" },
    { 14, 28,  28.086, 2.35, 0.202, "Si", "solute-substitutional" },
    { 15, 31,  30.974, 2.18, 0.305, "P",  "interstitial"           },
    { 25, 55,  54.938, 2.74, 0.250, "Mn", "solute-substitutional" },
    { 26, 56,  55.845, 2.60, 0.250, "Fe", "solute-substitutional" },
    { 28, 58,  58.693, 2.50, 0.250, "Ni", "solute-substitutional" },
    { 29, 63,  63.546, 2.56, 0.250, "Cu", "solvent"               },
    { 30, 64,  65.380, 2.74, 0.250, "Zn", "solute-substitutional" },
    { 50,120, 118.710, 2.90, 0.300, "Sn", "solute-substitutional" },
}};

// Cu is always index 7 in the pool
static constexpr uint32_t CU_IDX = 7;

// ============================================================================
// Chaos Factor definition
// ============================================================================

/**
 * ChaosFactor — named chaos parameter with derived sub-factors.
 *
 * χ = 1.25 produces:
 *   disorder_amp  = 0.31   (31% relative spread in site occupation)
 *   precip_prob   = 0.19   (19% chance a bead is in a secondary phase)
 *   epsilon_noise = 0.156  (ε ± 15.6% Gaussian noise)
 *   gb_fraction   = 0.188  (18.8% of beads in grain boundary environment)
 */
struct ChaosFactor {
    double chi{1.0};

    // Derived amplitudes — documented, not magic
    double disorder_amplitude()  const { return 0.25 * chi; }           // site-swap spread
    double precipitation_prob()  const { return 0.15 * chi; }           // secondary phase
    double epsilon_noise_frac()  const { return 0.125 * chi; }          // ε noise fraction
    double grain_boundary_frac() const { return 0.15 * chi; }           // GB bead fraction
    double solute_max_fraction() const { return std::min(0.45, 0.36 * chi); }  // max alloying
};

// ============================================================================
// Per-bead alloy record
// ============================================================================

/**
 * AlloyBeadRecord — the anti-black-box provenance record for one alloy bead.
 *
 * Every decision made by the generator is recorded here so that the
 * alloy can be inspected, reproduced, or audited.
 */
struct AlloyBeadRecord {
    uint32_t bead_index{};

    // Identity (L2 propagation contract: Z, A immutable post-generation)
    uint8_t  Z{};
    uint8_t  A{};
    double   Q{};          // Effective charge (metallic: near 0, GB: small nonzero)
    double   epsilon{};    // Perturbed ε (kcal/mol)
    double   sigma{};      // Å
    double   mass{};       // amu
    std::string symbol;
    const char* role_hint{};

    // Chaos provenance
    double   epsilon_base{};      // Unperturbed ε from LJ table
    double   epsilon_perturbation{}; // Δε applied by chaos factor
    double   site_occupation_prob{}; // Probability this element occupied this site
    bool     is_secondary_phase{false}; // True if precipitation kicked in
    bool     is_grain_boundary{false};  // True if GB environment
    coarse_grain::StructuralRole structural_role{coarse_grain::StructuralRole::Metallic};
    coarse_grain::StabilityClass  stability_class{coarse_grain::StabilityClass::BulkLattice};
};

// ============================================================================
// Alloy composition record
// ============================================================================

struct AlloyComposition {
    std::string name;                  // e.g. "Cu-Zn-Sn-Ni (random, χ=1.25)"
    double chi{};                      // chaos factor used
    uint32_t n_beads{};
    std::vector<AlloyBeadRecord> beads;

    // Composition statistics (computed by generate())
    std::array<double, 10> mole_fraction{};  // Index matches CU_ALLOY_POOL
    double mean_epsilon{};
    double std_epsilon{};
    double gb_fraction_actual{};
    double secondary_phase_fraction{};
    uint32_t dominant_element_Z{29};  // almost always Cu
};

// ============================================================================
// Alloy Generator
// ============================================================================

/**
 * generate_copper_alloy — main entry point.
 *
 * @param n_beads    Number of CG beads to generate
 * @param chaos      ChaosFactor (χ = 1.25 for high metallic disorder)
 * @param seed       RNG seed (same seed → same alloy)
 * @param cu_base_fraction  Nominal Cu fraction (default 0.60 — 60 at% Cu)
 *
 * Algorithm:
 *  1. Draw site occupation probability vector from Dirichlet-like distribution
 *     perturbed by disorder_amplitude().
 *  2. For each bead, sample element from the occupation distribution.
 *  3. Apply Gaussian ε perturbation scaled by epsilon_noise_frac().
 *  4. Mark grain-boundary beads by uniform probability grain_boundary_frac().
 *  5. Apply precipitation rule: if precip_prob() fires, reclassify bead
 *     stability to Metastable and add a small charge offset.
 *  6. Build AlloyBeadRecord with full provenance.
 *  7. Assemble AlloyComposition statistics.
 */
inline AlloyComposition generate_copper_alloy(
    uint32_t    n_beads,
    ChaosFactor chaos,
    uint64_t    seed              = 42,
    double      cu_base_fraction  = 0.60)
{
    std::mt19937_64 rng(seed);

    // ── 1. Build nominal composition fractions ──────────────────────────────
    // Cu gets cu_base_fraction.  Remaining (1 - cu_base_fraction) is split
    // across the 9 alloying elements with disorder perturbation.

    constexpr int N_ELEM = 10;
    std::array<double, N_ELEM> base_frac{};

    // Start: Cu gets the lion's share; distribute remainder with disorder
    double remainder = 1.0 - cu_base_fraction;
    std::uniform_real_distribution<double> disorder(-chaos.disorder_amplitude(),
                                                     chaos.disorder_amplitude());

    // Assign base fractions to non-Cu elements
    double non_cu_sum = 0.0;
    for (int i = 0; i < N_ELEM; ++i) {
        if (i == static_cast<int>(CU_IDX)) continue;
        // Base weight: heavier alloying elements tend to be minor
        double base = (remainder / 9.0);
        double perturbed = base + disorder(rng) * base;
        base_frac[i] = std::max(0.0, perturbed);
        non_cu_sum += base_frac[i];
    }
    // Re-normalise non-Cu fractions to sum to remainder
    if (non_cu_sum > 1e-12)
        for (int i = 0; i < N_ELEM; ++i)
            if (i != static_cast<int>(CU_IDX))
                base_frac[i] *= remainder / non_cu_sum;
    // Cu gets what's left
    base_frac[CU_IDX] = 1.0 - remainder;

    // Build CDF for sampling
    std::array<double, N_ELEM> cdf{};
    cdf[0] = base_frac[0];
    for (int i = 1; i < N_ELEM; ++i) cdf[i] = cdf[i-1] + base_frac[i];

    // ── 2. Set up per-bead noise ────────────────────────────────────────────
    std::normal_distribution<double>  eps_noise(0.0, 1.0);
    std::uniform_real_distribution<double> uniform01(0.0, 1.0);
    std::normal_distribution<double>  charge_noise(0.0, 0.05 * chaos.chi);

    // ── 3. Generate beads ───────────────────────────────────────────────────
    AlloyComposition alloy;
    alloy.chi     = chaos.chi;
    alloy.n_beads = n_beads;
    alloy.beads.reserve(n_beads);

    std::array<double, N_ELEM> mf{};
    mf.fill(0.0);
    double sum_eps = 0.0, sum_eps2 = 0.0;
    uint32_t n_gb = 0, n_sec = 0;

    for (uint32_t bi = 0; bi < n_beads; ++bi) {
        // Sample element
        double u = uniform01(rng);
        int elem_idx = N_ELEM - 1;
        for (int i = 0; i < N_ELEM; ++i) {
            if (u <= cdf[i]) { elem_idx = i; break; }
        }
        const auto& er = CU_ALLOY_POOL[static_cast<size_t>(elem_idx)];
        mf[static_cast<size_t>(elem_idx)] += 1.0;

        // ε perturbation
        double noise_sigma = er.epsilon_ref * chaos.epsilon_noise_frac();
        double delta_eps   = eps_noise(rng) * noise_sigma;
        double eps_final   = std::max(1e-4, er.epsilon_ref + delta_eps);

        // Grain boundary?
        bool is_gb = (uniform01(rng) < chaos.grain_boundary_frac());
        if (is_gb) n_gb++;

        // Secondary phase (precipitation)?
        bool is_sec = (uniform01(rng) < chaos.precipitation_prob());
        if (is_sec) n_sec++;

        // Charge: metallic beads are near-neutral; GB/precipitate get small Q
        double Q = 0.0;
        if (is_gb)  Q += charge_noise(rng);
        if (is_sec) Q += charge_noise(rng) * 1.5;

        // Stability: precipitates are metastable; GB is also metastable
        auto stab = (is_sec || is_gb)
                    ? coarse_grain::StabilityClass::Metastable
                    : coarse_grain::StabilityClass::BulkLattice;

        AlloyBeadRecord rec;
        rec.bead_index          = bi;
        rec.Z                   = er.Z;
        rec.A                   = er.A;
        rec.Q                   = Q;
        rec.epsilon             = eps_final;
        rec.sigma               = er.sigma;
        rec.mass                = er.mass;
        rec.symbol              = er.symbol;
        rec.role_hint           = er.role_hint;
        rec.epsilon_base        = er.epsilon_ref;
        rec.epsilon_perturbation = delta_eps;
        rec.site_occupation_prob = base_frac[static_cast<size_t>(elem_idx)];
        rec.is_secondary_phase  = is_sec;
        rec.is_grain_boundary   = is_gb;
        rec.structural_role     = coarse_grain::StructuralRole::Metallic;
        rec.stability_class     = stab;

        sum_eps  += eps_final;
        sum_eps2 += eps_final * eps_final;

        alloy.beads.push_back(rec);
    }

    // ── 4. Statistics ───────────────────────────────────────────────────────
    for (int i = 0; i < N_ELEM; ++i)
        alloy.mole_fraction[static_cast<size_t>(i)] = mf[static_cast<size_t>(i)]
                                                      / static_cast<double>(n_beads);
    alloy.mean_epsilon = sum_eps / n_beads;
    double var = sum_eps2 / n_beads - alloy.mean_epsilon * alloy.mean_epsilon;
    alloy.std_epsilon  = (var > 0) ? std::sqrt(var) : 0.0;
    alloy.gb_fraction_actual         = static_cast<double>(n_gb) / n_beads;
    alloy.secondary_phase_fraction   = static_cast<double>(n_sec) / n_beads;
    alloy.dominant_element_Z         = 29;  // Cu base

    // Build name string
    alloy.name = "Cu-base random alloy (chi=" + std::to_string(chaos.chi).substr(0, 4)
               + ", N=" + std::to_string(n_beads)
               + ", seed=" + std::to_string(seed) + ")";

    return alloy;
}

// ============================================================================
// Alloy → BeadSystem converter
// ============================================================================

/**
 * alloy_to_bead_system — place alloy beads on a 3D FCC-like lattice with
 * chaos-driven positional jitter, producing a BeadSystem ready for
 * the layer stack.
 *
 * The lattice constant is derived from the mean σ of the alloy.
 * Jitter amplitude scales with chi so χ=1.25 produces visibly disordered
 * but physically plausible positions.
 */
inline coarse_grain::BeadSystem alloy_to_bead_system(
    const AlloyComposition& alloy,
    uint64_t position_seed = 99)
{
    std::mt19937_64 rng(position_seed);

    // Mean sigma → lattice constant (FCC: a ≈ 2^(1/3) * sigma for close-pack)
    double mean_sigma = 0.0;
    for (auto& br : alloy.beads) mean_sigma += br.sigma;
    mean_sigma /= alloy.beads.size();
    double a0 = std::pow(2.0, 1.0/3.0) * mean_sigma;  // Å

    // Build FCC basis: 4 atoms per unit cell, positions (fraction of a0)
    // 000, 0½½, ½0½, ½½0
    static const std::array<std::array<double,3>, 4> FCC_BASIS = {{
        {0.0, 0.0, 0.0},
        {0.0, 0.5, 0.5},
        {0.5, 0.0, 0.5},
        {0.5, 0.5, 0.0}
    }};

    // Determine supercell size to accommodate n_beads
    uint32_t N = static_cast<uint32_t>(alloy.beads.size());
    uint32_t nc = 1;
    while (nc * nc * nc * 4 < N) nc++;

    std::normal_distribution<double> jitter_dist(0.0, alloy.chi * 0.08 * a0);

    coarse_grain::BeadSystem sys;
    sys.beads.reserve(N);
    sys.source_atom_count = N;

    uint32_t placed = 0;
    for (uint32_t ix = 0; ix < nc && placed < N; ++ix)
    for (uint32_t iy = 0; iy < nc && placed < N; ++iy)
    for (uint32_t iz = 0; iz < nc && placed < N; ++iz)
    for (int b = 0; b < 4   && placed < N; ++b) {
        const auto& rec = alloy.beads[placed];
        const auto& fb  = FCC_BASIS[static_cast<size_t>(b)];

        coarse_grain::Bead bead;
        bead.position = {
            (static_cast<double>(ix) + fb[0]) * a0 + jitter_dist(rng),
            (static_cast<double>(iy) + fb[1]) * a0 + jitter_dist(rng),
            (static_cast<double>(iz) + fb[2]) * a0 + jitter_dist(rng)
        };
        bead.mass            = rec.mass;
        bead.charge          = rec.Q;
        bead.type_id         = static_cast<uint32_t>(rec.Z);
        bead.structural_role = rec.structural_role;
        bead.stability_class = rec.stability_class;
        bead.parent_atom_indices = {placed};
        bead.com_position    = bead.position;
        bead.cog_position    = bead.position;

        sys.beads.push_back(bead);
        ++placed;
    }

    return sys;
}

} // namespace alloy
