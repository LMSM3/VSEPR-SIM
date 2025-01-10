#pragma once
/**
 * unit_cell.hpp
 * -------------
 * Crystal unit cell definition and preset library.
 *
 * A UnitCell stores:
 *   - Lattice (triclinic-capable lattice vectors)
 *   - Basis atoms in fractional coordinates
 *   - Element types, charges, masses
 *   - Space group metadata (informational)
 *
 * Presets emit fully configured UnitCell objects for common structures.
 */

#include "lattice.hpp"
#include <vector>
#include <string>

namespace atomistic {
namespace crystal {

// ============================================================================
// Basis Atom: one atom in the unit cell, fractional coordinates
// ============================================================================

struct BasisAtom {
    Vec3     frac;     // Fractional coordinates ∈ [0, 1)
    uint32_t type;     // Atomic number (Z)
    double   charge;   // Formal charge (e)
    double   mass;     // Atomic mass (amu)
};

// ============================================================================
// Unit Cell
// ============================================================================

struct UnitCell {
    std::string name;
    Lattice     lattice;
    std::vector<BasisAtom> basis;

    // Space group metadata (informational, not used for generation)
    int         space_group_number = 0;
    std::string space_group_symbol;

    UnitCell(const std::string& n, const Lattice& lat)
        : name(n), lattice(lat) {}

    // Add a basis atom
    void add_atom(const Vec3& frac, uint32_t Z, double q = 0.0, double m = 0.0);

    // Number of atoms in the basis
    size_t num_atoms() const { return basis.size(); }

    // Convert to atomistic::State (single unit cell, PBC enabled)
    State to_state() const;
};

// ============================================================================
// Preset Crystal Structures
// ============================================================================

namespace presets {

// --- Metals ---

/** FCC aluminum: Fm-3m, a = 4.05 Å */
UnitCell aluminum_fcc();

/** BCC iron: Im-3m, a = 2.87 Å */
UnitCell iron_bcc();

/** FCC copper: Fm-3m, a = 3.61 Å */
UnitCell copper_fcc();

/** FCC gold: Fm-3m, a = 4.08 Å */
UnitCell gold_fcc();

// --- Ionic ---

/** NaCl rocksalt: Fm-3m, a = 5.64 Å */
UnitCell sodium_chloride();

/** MgO rocksalt: Fm-3m, a = 4.21 Å */
UnitCell magnesium_oxide();

/** CsCl: Pm-3m, a = 4.12 Å */
UnitCell cesium_chloride();

// --- Covalent ---

/** Diamond Si: Fd-3m, a = 5.43 Å */
UnitCell silicon_diamond();

/** Diamond C: Fd-3m, a = 3.57 Å */
UnitCell carbon_diamond();

// --- Tetragonal ---

/** Rutile TiO₂: P4₂/mnm, a = 4.59 Å, c = 2.96 Å */
UnitCell rutile_tio2();

// ============================================================================
// Benchmark Crystal Library (SS10b validation targets)
// ============================================================================

// --- Fluorite-type (AO₂) ---

/** ThO₂: Fm-3m, a = 5.597 Å, Z=4 */
UnitCell tho2_fluorite();

/** PuO₂: Fm-3m, a = 5.396 Å, Z=4 */
UnitCell puo2_fluorite();

/** CeO₂: Fm-3m, a = 5.411 Å, Z=4 */
UnitCell ceo2_fluorite();

/** ZrO₂ cubic: Fm-3m, a = 5.07 Å (high-T stabilised) */
UnitCell zro2_cubic();

/** ZrO₂ tetragonal: P4₂/nmc, a = 3.64, c = 5.27 Å */
UnitCell zro2_tetragonal();

/** ZrO₂ monoclinic: P2₁/c, a=5.15, b=5.21, c=5.31, β=99.2° */
UnitCell zro2_monoclinic();

/** HfO₂ cubic: Fm-3m, a = 5.08 Å */
UnitCell hfo2_cubic();

/** HfO₂ tetragonal: P4₂/nmc, a = 3.58, c = 5.20 Å */
UnitCell hfo2_tetragonal();

/** HfO₂ monoclinic: P2₁/c, a=5.12, b=5.17, c=5.30, β=99.2° */
UnitCell hfo2_monoclinic();

// --- Spinel-type (AB₂O₄) ---

/** MgAl₂O₄: Fd-3m, a = 8.083 Å, Z=8 */
UnitCell mgal2o4_spinel();

/** Fe₃O₄ (magnetite): Fd-3m, a = 8.396 Å, Z=8 */
UnitCell fe3o4_spinel();

/** Co₃O₄: Fd-3m, a = 8.084 Å, Z=8 */
UnitCell co3o4_spinel();

// --- Perovskite-type (ABO₃) ---

/** SrTiO₃: Pm-3m, a = 3.905 Å, Z=1 */
UnitCell srtio3_perovskite();

/** BaTiO₃ cubic: Pm-3m, a = 4.009 Å */
UnitCell batio3_cubic();

/** BaTiO₃ tetragonal: P4mm, a = 3.994, c = 4.038 Å */
UnitCell batio3_tetragonal();

/** CaTiO₃ orthorhombic: Pbnm, a=5.38, b=5.44, c=7.64 Å */
UnitCell catio3_orthorhombic();

/** LaAlO₃ rhombohedral: R-3c, a=5.357, α=60.1° (pseudo-cubic ~3.79) */
UnitCell laalo3_rhombohedral();

// --- Garnet-type (A₃B₅O₁₂) ---

/** Y₃Al₅O₁₂ (YAG): Ia-3d, a = 12.01 Å, Z=8 */
UnitCell y3al5o12_garnet();

/** Gd₃Ga₅O₁₂ (GGG): Ia-3d, a = 12.383 Å, Z=8 */
UnitCell gd3ga5o12_garnet();

// --- Apatite ---

/** Ca₅(PO₄)₃F (fluorapatite): P6₃/m, a=9.367, c=6.884 Å */
UnitCell ca5po4_3f_apatite();

// --- Monazite ---

/** LaPO₄: P2₁/n, a=6.83, b=7.07, c=6.50, β=103.3° */
UnitCell lapo4_monazite();

// --- Pyrochlore-type (A₂B₂O₇) ---

/** Gd₂Ti₂O₇: Fd-3m, a = 10.185 Å, Z=8 */
UnitCell gd2ti2o7_pyrochlore();

/** La₂Zr₂O₇: Fd-3m, a = 10.786 Å, Z=8 */
UnitCell la2zr2o7_pyrochlore();

/** Bi₂Ti₂O₇: Fd-3m, a = 10.354 Å, Z=8 */
UnitCell bi2ti2o7_pyrochlore();

// --- Actinide Oxides ---

/** U₃O₈ orthorhombic: C2mm, a=6.72, b=11.96, c=4.15 Å */
UnitCell u3o8_orthorhombic();

/** UO₃ gamma: Fddd, a=9.81, b=19.90, c=9.71 Å */
UnitCell uo3_gamma();

} // namespace presets

} // namespace crystal
} // namespace atomistic
