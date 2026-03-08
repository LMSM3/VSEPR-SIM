#pragma once
/**
 * nanoparticle_ref.hpp
 * --------------------
 * Experimental reference data for common nanoparticle systems in the
 * near-atomistic regime (4–6 nm diameter).
 *
 * Sources:
 *   - CRC Handbook of Chemistry and Physics (105th ed.)
 *   - Tyson & Miller, Surf. Sci. 62, 267 (1977) — surface energies
 *   - Buffat & Borel, Phys. Rev. A 13, 2287 (1976) — Au melting depression
 *   - Cleveland et al., Phys. Rev. Lett. 79, 1873 (1997) — Au cluster energies
 *   - Baletto & Ferrando, Rev. Mod. Phys. 77, 371 (2005) — nanoparticle motifs
 *
 * Every entry carries its bibliographic tag in `source` for traceability.
 * Energies converted to internal units (kcal/mol, Å) at definition site.
 */

#include "atomistic/validation/experiment_match.hpp"

namespace atomistic {
namespace validation {
namespace reference {

// ============================================================================
// Noble Metals (4–6 nm regime)
// ============================================================================

inline ExperimentalRecord gold_5nm_fcc() {
    ExperimentalRecord r;
    r.label    = "Au_5nm_fcc";
    r.source   = "Cleveland1997+Tyson1977";
    r.Z        = 79;   // Au
    r.diameter_ang = 50.0;  // 5.0 nm
    r.approx_N    = 3871;   // ~3871 atoms for 5nm Au FCC sphere

    // Cohesive energy: bulk Au = 3.81 eV/atom → convert to kcal/mol
    r.cohesive_energy     = -3.81 * KCAL_PER_EV;  // −87.86 kcal/mol
    // Surface energy: Au(111) ≈ 1.50 J/m² → kcal/mol/Å²
    r.surface_energy_per_area = 1.50 * J_PER_M2_TO_KCAL_PER_ANG2;
    r.edge_energy_correction  = 0.8 * KCAL_PER_EV;  // ~0.8 eV excess per edge atom

    r.bulk_CN        = 12.0;    // FCC
    r.surface_CN     = 8.5;     // average for mixed (111)/(100) facets
    r.lattice_constant = 4.078; // Å

    r.diffusion_coeff     = 0.0;    // not applicable for solid NP
    r.lindemann_threshold = 0.12;   // melting criterion
    r.melting_point_K     = 830.0;  // ~830 K for 5nm Au (Buffat & Borel)

    r.tol_energy_frac = 0.10;
    r.tol_CN          = 1.5;
    r.tol_D_frac      = 0.30;
    return r;
}

inline ExperimentalRecord silver_5nm_fcc() {
    ExperimentalRecord r;
    r.label    = "Ag_5nm_fcc";
    r.source   = "Tyson1977+Baletto2005";
    r.Z        = 47;   // Ag
    r.diameter_ang = 50.0;
    r.approx_N    = 3871;

    r.cohesive_energy     = -2.95 * KCAL_PER_EV;  // bulk Ag 2.95 eV/atom
    r.surface_energy_per_area = 1.25 * J_PER_M2_TO_KCAL_PER_ANG2;
    r.edge_energy_correction  = 0.6 * KCAL_PER_EV;

    r.bulk_CN        = 12.0;
    r.surface_CN     = 8.5;
    r.lattice_constant = 4.086;

    r.diffusion_coeff     = 0.0;
    r.lindemann_threshold = 0.12;
    r.melting_point_K     = 750.0;  // ~750 K for 5nm Ag

    return r;
}

inline ExperimentalRecord copper_4nm_fcc() {
    ExperimentalRecord r;
    r.label    = "Cu_4nm_fcc";
    r.source   = "Tyson1977+CRC105";
    r.Z        = 29;   // Cu
    r.diameter_ang = 40.0;  // 4.0 nm
    r.approx_N    = 1985;

    r.cohesive_energy     = -3.49 * KCAL_PER_EV;  // bulk Cu 3.49 eV/atom
    r.surface_energy_per_area = 1.79 * J_PER_M2_TO_KCAL_PER_ANG2;
    r.edge_energy_correction  = 0.7 * KCAL_PER_EV;

    r.bulk_CN        = 12.0;
    r.surface_CN     = 8.0;
    r.lattice_constant = 3.615;

    r.diffusion_coeff     = 0.0;
    r.lindemann_threshold = 0.12;
    r.melting_point_K     = 900.0;  // ~900 K for 4nm Cu

    return r;
}

// ============================================================================
// Noble Gases (liquid / soft nanoparticle clusters at 4–6 nm)
// ============================================================================

inline ExperimentalRecord argon_5nm_cluster() {
    ExperimentalRecord r;
    r.label    = "Ar_5nm_cluster";
    r.source   = "CRC105+Baletto2005";
    r.Z        = 18;   // Ar
    r.diameter_ang = 50.0;
    r.approx_N    = 5083;  // ~5083 Ar atoms in 5nm sphere (FCC packing)

    r.cohesive_energy     = -0.080 * KCAL_PER_EV;  // bulk Ar ~0.08 eV/atom
    r.surface_energy_per_area = 0.037 * J_PER_M2_TO_KCAL_PER_ANG2;  // liquid Ar
    r.edge_energy_correction  = 0.01 * KCAL_PER_EV;

    r.bulk_CN        = 12.0;    // FCC ideal
    r.surface_CN     = 8.0;
    r.lattice_constant = 5.26;  // Å (Ar FCC at ~0 K)

    // Ar cluster at ~40 K: significant atomic mobility
    r.diffusion_coeff     = 1.0e-5;   // Å²/fs (order of magnitude)
    r.lindemann_threshold = 0.10;
    r.melting_point_K     = 40.0;     // ~40 K for 5nm Ar cluster

    r.tol_energy_frac = 0.15;  // Ar is weakly bound, larger tolerance
    r.tol_CN          = 2.0;
    r.tol_D_frac      = 0.50;
    return r;
}

// ============================================================================
// Transition Metals (4–6 nm regime)
// ============================================================================

inline ExperimentalRecord platinum_5nm_fcc() {
    ExperimentalRecord r;
    r.label    = "Pt_5nm_fcc";
    r.source   = "Tyson1977+CRC105";
    r.Z        = 78;   // Pt
    r.diameter_ang = 50.0;
    r.approx_N    = 3871;

    r.cohesive_energy     = -5.84 * KCAL_PER_EV;  // bulk Pt 5.84 eV/atom
    r.surface_energy_per_area = 2.49 * J_PER_M2_TO_KCAL_PER_ANG2;
    r.edge_energy_correction  = 1.0 * KCAL_PER_EV;

    r.bulk_CN        = 12.0;
    r.surface_CN     = 8.5;
    r.lattice_constant = 3.924;

    r.diffusion_coeff     = 0.0;
    r.lindemann_threshold = 0.12;
    r.melting_point_K     = 1400.0;  // ~1400 K for 5nm Pt (high melting metal)

    return r;
}

inline ExperimentalRecord iron_5nm_bcc() {
    ExperimentalRecord r;
    r.label    = "Fe_5nm_bcc";
    r.source   = "Tyson1977+CRC105";
    r.Z        = 26;   // Fe
    r.diameter_ang = 50.0;
    r.approx_N    = 5575;  // BCC packing, smaller atoms

    r.cohesive_energy     = -4.28 * KCAL_PER_EV;  // bulk Fe 4.28 eV/atom
    r.surface_energy_per_area = 2.42 * J_PER_M2_TO_KCAL_PER_ANG2;
    r.edge_energy_correction  = 0.9 * KCAL_PER_EV;

    r.bulk_CN        = 8.0;     // BCC
    r.surface_CN     = 5.5;
    r.lattice_constant = 2.870;

    r.diffusion_coeff     = 0.0;
    r.lindemann_threshold = 0.12;
    r.melting_point_K     = 1200.0;

    return r;
}

// ============================================================================
// Convenience: all built-in references
// ============================================================================

inline std::vector<ExperimentalRecord> all_nanoparticle_refs() {
    return {
        gold_5nm_fcc(),
        silver_5nm_fcc(),
        copper_4nm_fcc(),
        argon_5nm_cluster(),
        platinum_5nm_fcc(),
        iron_5nm_bcc()
    };
}

} // namespace reference
} // namespace validation
} // namespace atomistic
