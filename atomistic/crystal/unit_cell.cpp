#include "unit_cell.hpp"
#include <cmath>

namespace atomistic {
namespace crystal {

// ============================================================================
// UnitCell
// ============================================================================

void UnitCell::add_atom(const Vec3& frac, uint32_t Z, double q, double m) {
    basis.push_back({Lattice::wrap_frac(frac), Z, q, m});
}

State UnitCell::to_state() const {
    State s;
    s.N = static_cast<uint32_t>(basis.size());
    s.X.resize(s.N);
    s.V.resize(s.N, {0, 0, 0});
    s.F.resize(s.N, {0, 0, 0});
    s.M.resize(s.N);
    s.Q.resize(s.N);
    s.type.resize(s.N);
    s.T.resize(s.N, 0.0);

    for (size_t i = 0; i < basis.size(); ++i) {
        s.X[i] = lattice.to_cartesian(basis[i].frac);
        s.type[i] = basis[i].type;
        s.Q[i] = basis[i].charge;
        s.M[i] = basis[i].mass > 0 ? basis[i].mass : 1.0;
    }

    // Set orthogonal PBC box from lattice
    s.box = lattice.to_box_pbc();

    return s;
}

// ============================================================================
// Preset Crystal Structures
// ============================================================================

namespace presets {

// Approximate atomic masses (amu)
namespace mass {
    constexpr double H  = 1.008;
    constexpr double C  = 12.011;
    constexpr double N  = 14.007;
    constexpr double O  = 15.999;
    constexpr double F  = 18.998;
    constexpr double Na = 22.990;
    constexpr double Mg = 24.305;
    constexpr double Al = 26.982;
    constexpr double Si = 28.086;
    constexpr double P  = 30.974;
    constexpr double Cl = 35.453;
    constexpr double Ca = 40.078;
    constexpr double Ti = 47.867;
    constexpr double Fe = 55.845;
    constexpr double Co = 58.933;
    constexpr double Cu = 63.546;
    constexpr double Zn = 65.380;
    constexpr double Ga = 69.723;
    constexpr double Sr = 87.620;
    constexpr double Y  = 88.906;
    constexpr double Zr = 91.224;
    constexpr double Ba = 137.327;
    constexpr double La = 138.905;
    constexpr double Ce = 140.116;
    constexpr double Gd = 157.250;
    constexpr double Hf = 178.490;
    constexpr double Au = 196.967;
    constexpr double Bi = 208.980;
    constexpr double Th = 232.038;
    constexpr double U  = 238.029;
    constexpr double Pu = 244.064;
    constexpr double Cs = 132.905;
}

UnitCell aluminum_fcc() {
    UnitCell uc("Al FCC", Lattice::cubic(4.05));
    uc.space_group_number = 225;
    uc.space_group_symbol = "Fm-3m";
    // 4 atoms per conventional cell
    uc.add_atom({0.0, 0.0, 0.0}, 13, 0.0, mass::Al);
    uc.add_atom({0.5, 0.5, 0.0}, 13, 0.0, mass::Al);
    uc.add_atom({0.5, 0.0, 0.5}, 13, 0.0, mass::Al);
    uc.add_atom({0.0, 0.5, 0.5}, 13, 0.0, mass::Al);
    return uc;
}

UnitCell iron_bcc() {
    UnitCell uc("Fe BCC", Lattice::cubic(2.87));
    uc.space_group_number = 229;
    uc.space_group_symbol = "Im-3m";
    uc.add_atom({0.0, 0.0, 0.0}, 26, 0.0, mass::Fe);
    uc.add_atom({0.5, 0.5, 0.5}, 26, 0.0, mass::Fe);
    return uc;
}

UnitCell copper_fcc() {
    UnitCell uc("Cu FCC", Lattice::cubic(3.61));
    uc.space_group_number = 225;
    uc.space_group_symbol = "Fm-3m";
    uc.add_atom({0.0, 0.0, 0.0}, 29, 0.0, mass::Cu);
    uc.add_atom({0.5, 0.5, 0.0}, 29, 0.0, mass::Cu);
    uc.add_atom({0.5, 0.0, 0.5}, 29, 0.0, mass::Cu);
    uc.add_atom({0.0, 0.5, 0.5}, 29, 0.0, mass::Cu);
    return uc;
}

UnitCell gold_fcc() {
    UnitCell uc("Au FCC", Lattice::cubic(4.08));
    uc.space_group_number = 225;
    uc.space_group_symbol = "Fm-3m";
    uc.add_atom({0.0, 0.0, 0.0}, 79, 0.0, mass::Au);
    uc.add_atom({0.5, 0.5, 0.0}, 79, 0.0, mass::Au);
    uc.add_atom({0.5, 0.0, 0.5}, 79, 0.0, mass::Au);
    uc.add_atom({0.0, 0.5, 0.5}, 79, 0.0, mass::Au);
    return uc;
}

UnitCell sodium_chloride() {
    UnitCell uc("NaCl", Lattice::cubic(5.64));
    uc.space_group_number = 225;
    uc.space_group_symbol = "Fm-3m";
    // Na sublattice (FCC)
    uc.add_atom({0.0, 0.0, 0.0}, 11, +1.0, mass::Na);
    uc.add_atom({0.5, 0.5, 0.0}, 11, +1.0, mass::Na);
    uc.add_atom({0.5, 0.0, 0.5}, 11, +1.0, mass::Na);
    uc.add_atom({0.0, 0.5, 0.5}, 11, +1.0, mass::Na);
    // Cl sublattice (offset FCC)
    uc.add_atom({0.5, 0.0, 0.0}, 17, -1.0, mass::Cl);
    uc.add_atom({0.0, 0.5, 0.0}, 17, -1.0, mass::Cl);
    uc.add_atom({0.0, 0.0, 0.5}, 17, -1.0, mass::Cl);
    uc.add_atom({0.5, 0.5, 0.5}, 17, -1.0, mass::Cl);
    return uc;
}

UnitCell magnesium_oxide() {
    UnitCell uc("MgO", Lattice::cubic(4.21));
    uc.space_group_number = 225;
    uc.space_group_symbol = "Fm-3m";
    uc.add_atom({0.0, 0.0, 0.0}, 12, +2.0, mass::Mg);
    uc.add_atom({0.5, 0.5, 0.0}, 12, +2.0, mass::Mg);
    uc.add_atom({0.5, 0.0, 0.5}, 12, +2.0, mass::Mg);
    uc.add_atom({0.0, 0.5, 0.5}, 12, +2.0, mass::Mg);
    uc.add_atom({0.5, 0.0, 0.0},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.5, 0.0},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.0, 0.5},  8, -2.0, mass::O);
    uc.add_atom({0.5, 0.5, 0.5},  8, -2.0, mass::O);
    return uc;
}

UnitCell cesium_chloride() {
    UnitCell uc("CsCl", Lattice::cubic(4.12));
    uc.space_group_number = 221;
    uc.space_group_symbol = "Pm-3m";
    uc.add_atom({0.0, 0.0, 0.0}, 55, +1.0, mass::Cs);
    uc.add_atom({0.5, 0.5, 0.5}, 17, -1.0, mass::Cl);
    return uc;
}

UnitCell silicon_diamond() {
    UnitCell uc("Si diamond", Lattice::cubic(5.43));
    uc.space_group_number = 227;
    uc.space_group_symbol = "Fd-3m";
    // FCC sublattice
    uc.add_atom({0.0,  0.0,  0.0 }, 14, 0.0, mass::Si);
    uc.add_atom({0.5,  0.5,  0.0 }, 14, 0.0, mass::Si);
    uc.add_atom({0.5,  0.0,  0.5 }, 14, 0.0, mass::Si);
    uc.add_atom({0.0,  0.5,  0.5 }, 14, 0.0, mass::Si);
    // Offset sublattice (diamond)
    uc.add_atom({0.25, 0.25, 0.25}, 14, 0.0, mass::Si);
    uc.add_atom({0.75, 0.75, 0.25}, 14, 0.0, mass::Si);
    uc.add_atom({0.75, 0.25, 0.75}, 14, 0.0, mass::Si);
    uc.add_atom({0.25, 0.75, 0.75}, 14, 0.0, mass::Si);
    return uc;
}

UnitCell carbon_diamond() {
    UnitCell uc("C diamond", Lattice::cubic(3.57));
    uc.space_group_number = 227;
    uc.space_group_symbol = "Fd-3m";
    uc.add_atom({0.0,  0.0,  0.0 }, 6, 0.0, mass::C);
    uc.add_atom({0.5,  0.5,  0.0 }, 6, 0.0, mass::C);
    uc.add_atom({0.5,  0.0,  0.5 }, 6, 0.0, mass::C);
    uc.add_atom({0.0,  0.5,  0.5 }, 6, 0.0, mass::C);
    uc.add_atom({0.25, 0.25, 0.25}, 6, 0.0, mass::C);
    uc.add_atom({0.75, 0.75, 0.25}, 6, 0.0, mass::C);
    uc.add_atom({0.75, 0.25, 0.75}, 6, 0.0, mass::C);
    uc.add_atom({0.25, 0.75, 0.75}, 6, 0.0, mass::C);
    return uc;
}

UnitCell rutile_tio2() {
    // Tetragonal: a = 4.59, c = 2.96
    UnitCell uc("TiO2 rutile", Lattice::tetragonal(4.59, 2.96));
    uc.space_group_number = 136;
    uc.space_group_symbol = "P4_2/mnm";
    // 2 Ti + 4 O per unit cell
    constexpr double u = 0.305;  // Oxygen parameter
    uc.add_atom({0.0, 0.0, 0.0}, 22, +4.0, mass::Ti);
    uc.add_atom({0.5, 0.5, 0.5}, 22, +4.0, mass::Ti);
    uc.add_atom({  u,   u, 0.0},  8, -2.0, mass::O);
    uc.add_atom({1-u, 1-u, 0.0},  8, -2.0, mass::O);
    uc.add_atom({0.5+u, 0.5-u, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.5-u, 0.5+u, 0.5}, 8, -2.0, mass::O);
    return uc;
}

// ============================================================================
// CENTERING HELPERS
// For space groups with F/I centering, apply translations to populate
// the full conventional cell from a primitive basis.
// ============================================================================

// F-centering (Fm-3m, Fd-3m): (0,0,0) + (0,½,½) + (½,0,½) + (½,½,0)
// Multiplies atom count by 4.
static void apply_fcc_centering(UnitCell& uc) {
    auto original = uc.basis;
    static const double shifts[3][3] = {
        {0.0, 0.5, 0.5}, {0.5, 0.0, 0.5}, {0.5, 0.5, 0.0}
    };
    for (const auto& atom : original) {
        for (int s = 0; s < 3; ++s) {
            uc.add_atom({atom.frac.x + shifts[s][0],
                         atom.frac.y + shifts[s][1],
                         atom.frac.z + shifts[s][2]},
                        atom.type, atom.charge, atom.mass);
        }
    }
}

// I-centering (Im-3m, Ia-3d): (0,0,0) + (½,½,½)
// Multiplies atom count by 2.
static void apply_bcc_centering(UnitCell& uc) {
    auto original = uc.basis;
    for (const auto& atom : original) {
        uc.add_atom({atom.frac.x + 0.5, atom.frac.y + 0.5, atom.frac.z + 0.5},
                    atom.type, atom.charge, atom.mass);
    }
}

// ============================================================================
// FLUORITE-TYPE (AO₂) — Fm-3m (#225)
// Cation 4a: (0,0,0) + FCC translations
// Anion  8c: (1/4,1/4,1/4) + FCC translations
// ============================================================================

// Helper: populate fluorite basis (4 cations + 8 anions = 12 atoms)
static void add_fluorite_basis(UnitCell& uc, uint32_t Z_cat, double q_cat, double m_cat,
                                uint32_t Z_an, double q_an, double m_an) {
    // Cation FCC sublattice (4a)
    uc.add_atom({0.0, 0.0, 0.0}, Z_cat, q_cat, m_cat);
    uc.add_atom({0.5, 0.5, 0.0}, Z_cat, q_cat, m_cat);
    uc.add_atom({0.5, 0.0, 0.5}, Z_cat, q_cat, m_cat);
    uc.add_atom({0.0, 0.5, 0.5}, Z_cat, q_cat, m_cat);
    // Anion tetrahedral holes (8c)
    uc.add_atom({0.25, 0.25, 0.25}, Z_an, q_an, m_an);
    uc.add_atom({0.75, 0.75, 0.25}, Z_an, q_an, m_an);
    uc.add_atom({0.75, 0.25, 0.75}, Z_an, q_an, m_an);
    uc.add_atom({0.25, 0.75, 0.75}, Z_an, q_an, m_an);
    uc.add_atom({0.25, 0.25, 0.75}, Z_an, q_an, m_an);
    uc.add_atom({0.75, 0.75, 0.75}, Z_an, q_an, m_an);
    uc.add_atom({0.75, 0.25, 0.25}, Z_an, q_an, m_an);
    uc.add_atom({0.25, 0.75, 0.25}, Z_an, q_an, m_an);
}

UnitCell tho2_fluorite() {
    UnitCell uc("ThO2 fluorite", Lattice::cubic(5.597));
    uc.space_group_number = 225; uc.space_group_symbol = "Fm-3m";
    add_fluorite_basis(uc, 90, +4.0, mass::Th, 8, -2.0, mass::O);
    return uc;
}

UnitCell puo2_fluorite() {
    UnitCell uc("PuO2 fluorite", Lattice::cubic(5.396));
    uc.space_group_number = 225; uc.space_group_symbol = "Fm-3m";
    add_fluorite_basis(uc, 94, +4.0, mass::Pu, 8, -2.0, mass::O);
    return uc;
}

UnitCell ceo2_fluorite() {
    UnitCell uc("CeO2 fluorite", Lattice::cubic(5.411));
    uc.space_group_number = 225; uc.space_group_symbol = "Fm-3m";
    add_fluorite_basis(uc, 58, +4.0, mass::Ce, 8, -2.0, mass::O);
    return uc;
}

UnitCell zro2_cubic() {
    UnitCell uc("ZrO2 cubic", Lattice::cubic(5.07));
    uc.space_group_number = 225; uc.space_group_symbol = "Fm-3m";
    add_fluorite_basis(uc, 40, +4.0, mass::Zr, 8, -2.0, mass::O);
    return uc;
}

UnitCell hfo2_cubic() {
    UnitCell uc("HfO2 cubic", Lattice::cubic(5.08));
    uc.space_group_number = 225; uc.space_group_symbol = "Fm-3m";
    add_fluorite_basis(uc, 72, +4.0, mass::Hf, 8, -2.0, mass::O);
    return uc;
}

// ============================================================================
// ZrO₂ / HfO₂ tetragonal — P4₂/nmc (#137), Z=2
// Cation 2a: (0,0,0), (0.5,0.5,0.5)
// Anion  4d: (0,0.5,z), (0.5,0,z+0.5), (0,0.5,-z), (0.5,0,-z+0.5)
// ============================================================================

UnitCell zro2_tetragonal() {
    UnitCell uc("ZrO2 tetragonal", Lattice::tetragonal(3.64, 5.27));
    uc.space_group_number = 137; uc.space_group_symbol = "P4_2/nmc";
    constexpr double z_O = 0.206;
    uc.add_atom({0.0, 0.0, 0.0}, 40, +4.0, mass::Zr);
    uc.add_atom({0.5, 0.5, 0.5}, 40, +4.0, mass::Zr);
    uc.add_atom({0.0,  0.5,    z_O}, 8, -2.0, mass::O);
    uc.add_atom({0.5,  0.0, 0.5+z_O}, 8, -2.0, mass::O);
    uc.add_atom({0.0,  0.5,   -z_O}, 8, -2.0, mass::O);
    uc.add_atom({0.5,  0.0, 0.5-z_O}, 8, -2.0, mass::O);
    return uc;
}

UnitCell hfo2_tetragonal() {
    UnitCell uc("HfO2 tetragonal", Lattice::tetragonal(3.58, 5.20));
    uc.space_group_number = 137; uc.space_group_symbol = "P4_2/nmc";
    constexpr double z_O = 0.206;
    uc.add_atom({0.0, 0.0, 0.0}, 72, +4.0, mass::Hf);
    uc.add_atom({0.5, 0.5, 0.5}, 72, +4.0, mass::Hf);
    uc.add_atom({0.0,  0.5,    z_O}, 8, -2.0, mass::O);
    uc.add_atom({0.5,  0.0, 0.5+z_O}, 8, -2.0, mass::O);
    uc.add_atom({0.0,  0.5,   -z_O}, 8, -2.0, mass::O);
    uc.add_atom({0.5,  0.0, 0.5-z_O}, 8, -2.0, mass::O);
    return uc;
}

// ============================================================================
// ZrO₂ / HfO₂ monoclinic — P2₁/c (#14), Z=4
// Simplified: 4 cations + 8 anions in monoclinic cell
// ============================================================================

UnitCell zro2_monoclinic() {
    UnitCell uc("ZrO2 monoclinic",
        Lattice::from_parameters(5.1505, 5.2116, 5.3173, 90.0, 99.23, 90.0));
    uc.space_group_number = 14; uc.space_group_symbol = "P2_1/c";
    // Zr at Wyckoff 4e: (x, y, z) and symmetry equivalents
    constexpr double xZ=0.2758, yZ=0.0411, zZ=0.2082;
    uc.add_atom({   xZ,    yZ,    zZ}, 40, +4.0, mass::Zr);
    uc.add_atom({  -xZ, 0.5+yZ, 0.5-zZ}, 40, +4.0, mass::Zr);
    uc.add_atom({  -xZ,   -yZ,   -zZ}, 40, +4.0, mass::Zr);
    uc.add_atom({   xZ, 0.5-yZ, 0.5+zZ}, 40, +4.0, mass::Zr);
    // O1 at 4e
    constexpr double xO1=0.0703, yO1=0.3359, zO1=0.3406;
    uc.add_atom({  xO1,   yO1,   zO1}, 8, -2.0, mass::O);
    uc.add_atom({ -xO1, 0.5+yO1, 0.5-zO1}, 8, -2.0, mass::O);
    uc.add_atom({ -xO1,  -yO1,  -zO1}, 8, -2.0, mass::O);
    uc.add_atom({  xO1, 0.5-yO1, 0.5+zO1}, 8, -2.0, mass::O);
    // O2 at 4e
    constexpr double xO2=0.4496, yO2=0.7569, zO2=0.4792;
    uc.add_atom({  xO2,   yO2,   zO2}, 8, -2.0, mass::O);
    uc.add_atom({ -xO2, 0.5+yO2, 0.5-zO2}, 8, -2.0, mass::O);
    uc.add_atom({ -xO2,  -yO2,  -zO2}, 8, -2.0, mass::O);
    uc.add_atom({  xO2, 0.5-yO2, 0.5+zO2}, 8, -2.0, mass::O);
    return uc;
}

UnitCell hfo2_monoclinic() {
    UnitCell uc("HfO2 monoclinic",
        Lattice::from_parameters(5.1156, 5.1722, 5.2948, 90.0, 99.18, 90.0));
    uc.space_group_number = 14; uc.space_group_symbol = "P2_1/c";
    constexpr double xZ=0.2754, yZ=0.0395, zZ=0.2083;
    uc.add_atom({   xZ,    yZ,    zZ}, 72, +4.0, mass::Hf);
    uc.add_atom({  -xZ, 0.5+yZ, 0.5-zZ}, 72, +4.0, mass::Hf);
    uc.add_atom({  -xZ,   -yZ,   -zZ}, 72, +4.0, mass::Hf);
    uc.add_atom({   xZ, 0.5-yZ, 0.5+zZ}, 72, +4.0, mass::Hf);
    constexpr double xO1=0.0700, yO1=0.3317, zO1=0.3447;
    uc.add_atom({  xO1,   yO1,   zO1}, 8, -2.0, mass::O);
    uc.add_atom({ -xO1, 0.5+yO1, 0.5-zO1}, 8, -2.0, mass::O);
    uc.add_atom({ -xO1,  -yO1,  -zO1}, 8, -2.0, mass::O);
    uc.add_atom({  xO1, 0.5-yO1, 0.5+zO1}, 8, -2.0, mass::O);
    constexpr double xO2=0.4476, yO2=0.7564, zO2=0.4789;
    uc.add_atom({  xO2,   yO2,   zO2}, 8, -2.0, mass::O);
    uc.add_atom({ -xO2, 0.5+yO2, 0.5-zO2}, 8, -2.0, mass::O);
    uc.add_atom({ -xO2,  -yO2,  -zO2}, 8, -2.0, mass::O);
    uc.add_atom({  xO2, 0.5-yO2, 0.5+zO2}, 8, -2.0, mass::O);
    return uc;
}

// ============================================================================
// SPINEL-TYPE (AB₂O₄) — Fd-3m (#227), Z=8, 56 atoms/conventional cell
// Origin choice 2:
// A (tet) 8a: primitive positions then F-centered
// B (oct) 16d: primitive positions then F-centered
// O 32e: (u,u,u) then F-centered
// Primitive (1/4 cell): 2A + 4B + 8O = 14, × 4 FCC = 56
// ============================================================================

UnitCell mgal2o4_spinel() {
    UnitCell uc("MgAl2O4 spinel", Lattice::cubic(8.083));
    uc.space_group_number = 227; uc.space_group_symbol = "Fd-3m";
    constexpr double u = 0.3863;
    // 8a tetrahedral (primitive: 2 positions)
    uc.add_atom({0.125, 0.125, 0.125}, 12, +2.0, mass::Mg);
    uc.add_atom({0.875, 0.875, 0.875}, 12, +2.0, mass::Mg);
    // 16d octahedral (primitive: 4 positions)
    uc.add_atom({0.5, 0.5, 0.5},   13, +3.0, mass::Al);
    uc.add_atom({0.5, 0.25, 0.25}, 13, +3.0, mass::Al);
    uc.add_atom({0.25, 0.5, 0.25}, 13, +3.0, mass::Al);
    uc.add_atom({0.25, 0.25, 0.5}, 13, +3.0, mass::Al);
    // 32e oxygen (primitive: 8 positions)
    uc.add_atom({u, u, u}, 8, -2.0, mass::O);
    uc.add_atom({u, 0.25-u, 0.25-u}, 8, -2.0, mass::O);
    uc.add_atom({0.25-u, u, 0.25-u}, 8, -2.0, mass::O);
    uc.add_atom({0.25-u, 0.25-u, u}, 8, -2.0, mass::O);
    uc.add_atom({-u, -u, -u}, 8, -2.0, mass::O);
    uc.add_atom({-u, 0.25+u, 0.25+u}, 8, -2.0, mass::O);
    uc.add_atom({0.25+u, -u, 0.25+u}, 8, -2.0, mass::O);
    uc.add_atom({0.25+u, 0.25+u, -u}, 8, -2.0, mass::O);
    apply_fcc_centering(uc);  // 14 × 4 = 56 atoms
    return uc;
}

UnitCell fe3o4_spinel() {
    // Inverse spinel: Fe³⁺ on 8a (tet), Fe²⁺+Fe³⁺ on 16d (oct)
    UnitCell uc("Fe3O4 magnetite", Lattice::cubic(8.396));
    uc.space_group_number = 227; uc.space_group_symbol = "Fd-3m";
    constexpr double u = 0.3799;
    uc.add_atom({0.125, 0.125, 0.125}, 26, +3.0, mass::Fe);
    uc.add_atom({0.875, 0.875, 0.875}, 26, +3.0, mass::Fe);
    uc.add_atom({0.5, 0.5, 0.5},   26, +2.5, mass::Fe);
    uc.add_atom({0.5, 0.25, 0.25}, 26, +2.5, mass::Fe);
    uc.add_atom({0.25, 0.5, 0.25}, 26, +2.5, mass::Fe);
    uc.add_atom({0.25, 0.25, 0.5}, 26, +2.5, mass::Fe);
    uc.add_atom({u, u, u}, 8, -2.0, mass::O);
    uc.add_atom({u, 0.25-u, 0.25-u}, 8, -2.0, mass::O);
    uc.add_atom({0.25-u, u, 0.25-u}, 8, -2.0, mass::O);
    uc.add_atom({0.25-u, 0.25-u, u}, 8, -2.0, mass::O);
    uc.add_atom({-u, -u, -u}, 8, -2.0, mass::O);
    uc.add_atom({-u, 0.25+u, 0.25+u}, 8, -2.0, mass::O);
    uc.add_atom({0.25+u, -u, 0.25+u}, 8, -2.0, mass::O);
    uc.add_atom({0.25+u, 0.25+u, -u}, 8, -2.0, mass::O);
    apply_fcc_centering(uc);  // 56 atoms
    return uc;
}

UnitCell co3o4_spinel() {
    // Normal spinel: Co²⁺ tet, Co³⁺ oct
    UnitCell uc("Co3O4 spinel", Lattice::cubic(8.084));
    uc.space_group_number = 227; uc.space_group_symbol = "Fd-3m";
    constexpr double u = 0.3894;
    uc.add_atom({0.125, 0.125, 0.125}, 27, +2.0, mass::Co);
    uc.add_atom({0.875, 0.875, 0.875}, 27, +2.0, mass::Co);
    uc.add_atom({0.5, 0.5, 0.5},   27, +3.0, mass::Co);
    uc.add_atom({0.5, 0.25, 0.25}, 27, +3.0, mass::Co);
    uc.add_atom({0.25, 0.5, 0.25}, 27, +3.0, mass::Co);
    uc.add_atom({0.25, 0.25, 0.5}, 27, +3.0, mass::Co);
    uc.add_atom({u, u, u}, 8, -2.0, mass::O);
    uc.add_atom({u, 0.25-u, 0.25-u}, 8, -2.0, mass::O);
    uc.add_atom({0.25-u, u, 0.25-u}, 8, -2.0, mass::O);
    uc.add_atom({0.25-u, 0.25-u, u}, 8, -2.0, mass::O);
    uc.add_atom({-u, -u, -u}, 8, -2.0, mass::O);
    uc.add_atom({-u, 0.25+u, 0.25+u}, 8, -2.0, mass::O);
    uc.add_atom({0.25+u, -u, 0.25+u}, 8, -2.0, mass::O);
    uc.add_atom({0.25+u, 0.25+u, -u}, 8, -2.0, mass::O);
    apply_fcc_centering(uc);  // 56 atoms
    return uc;
}

// ============================================================================
// PEROVSKITE-TYPE (ABO₃) — Pm-3m (#221) for cubic, Z=1
// A at 1b: (0.5,0.5,0.5) — cuboctahedral
// B at 1a: (0,0,0) — octahedral
// O at 3d: (0.5,0,0), (0,0.5,0), (0,0,0.5) — bridging
// ============================================================================

UnitCell srtio3_perovskite() {
    UnitCell uc("SrTiO3 perovskite", Lattice::cubic(3.905));
    uc.space_group_number = 221; uc.space_group_symbol = "Pm-3m";
    uc.add_atom({0.5, 0.5, 0.5}, 38, +2.0, mass::Sr);  // A-site
    uc.add_atom({0.0, 0.0, 0.0}, 22, +4.0, mass::Ti);   // B-site
    uc.add_atom({0.5, 0.0, 0.0},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.5, 0.0},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.0, 0.5},  8, -2.0, mass::O);
    return uc;
}

UnitCell batio3_cubic() {
    UnitCell uc("BaTiO3 cubic", Lattice::cubic(4.009));
    uc.space_group_number = 221; uc.space_group_symbol = "Pm-3m";
    uc.add_atom({0.5, 0.5, 0.5}, 56, +2.0, mass::Ba);
    uc.add_atom({0.0, 0.0, 0.0}, 22, +4.0, mass::Ti);
    uc.add_atom({0.5, 0.0, 0.0},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.5, 0.0},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.0, 0.5},  8, -2.0, mass::O);
    return uc;
}

UnitCell batio3_tetragonal() {
    // Ferroelectric phase: Ti displaced along c
    UnitCell uc("BaTiO3 tetragonal", Lattice::tetragonal(3.994, 4.038));
    uc.space_group_number = 99; uc.space_group_symbol = "P4mm";
    uc.add_atom({0.5, 0.5, 0.513}, 56, +2.0, mass::Ba);
    uc.add_atom({0.0, 0.0, 0.0},   22, +4.0, mass::Ti);
    uc.add_atom({0.5, 0.0, 0.023},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.5, 0.023},  8, -2.0, mass::O);
    uc.add_atom({0.0, 0.0, 0.486},  8, -2.0, mass::O);
    return uc;
}

UnitCell catio3_orthorhombic() {
    // GdFeO₃-type tilted perovskite: Pbnm (#62), Z=4
    UnitCell uc("CaTiO3 orthorhombic",
        Lattice::orthorhombic(5.381, 5.443, 7.645));
    uc.space_group_number = 62; uc.space_group_symbol = "Pbnm";
    // Ca 4c: (x,y,1/4)
    uc.add_atom({0.993, 0.036, 0.25}, 20, +2.0, mass::Ca);
    uc.add_atom({0.507, 0.464, 0.25}, 20, +2.0, mass::Ca);
    uc.add_atom({0.007, 0.964, 0.75}, 20, +2.0, mass::Ca);
    uc.add_atom({0.493, 0.536, 0.75}, 20, +2.0, mass::Ca);
    // Ti 4b: (0,1/2,0)
    uc.add_atom({0.0, 0.5, 0.0}, 22, +4.0, mass::Ti);
    uc.add_atom({0.5, 0.0, 0.0}, 22, +4.0, mass::Ti);
    uc.add_atom({0.0, 0.5, 0.5}, 22, +4.0, mass::Ti);
    uc.add_atom({0.5, 0.0, 0.5}, 22, +4.0, mass::Ti);
    // O1 4c: (x,y,1/4)
    uc.add_atom({0.070, 0.484, 0.25},  8, -2.0, mass::O);
    uc.add_atom({0.430, 0.016, 0.25},  8, -2.0, mass::O);
    uc.add_atom({0.930, 0.516, 0.75},  8, -2.0, mass::O);
    uc.add_atom({0.570, 0.984, 0.75},  8, -2.0, mass::O);
    // O2 8d: (x,y,z) general position
    uc.add_atom({0.289, 0.289, 0.037},  8, -2.0, mass::O);
    uc.add_atom({0.211, 0.211, 0.537},  8, -2.0, mass::O);
    uc.add_atom({0.711, 0.789, 0.037},  8, -2.0, mass::O);
    uc.add_atom({0.789, 0.711, 0.537},  8, -2.0, mass::O);
    uc.add_atom({0.711, 0.711, 0.963},  8, -2.0, mass::O);
    uc.add_atom({0.789, 0.789, 0.463},  8, -2.0, mass::O);
    uc.add_atom({0.289, 0.211, 0.963},  8, -2.0, mass::O);
    uc.add_atom({0.211, 0.289, 0.463},  8, -2.0, mass::O);
    return uc;
}

UnitCell laalo3_rhombohedral() {
    // Rhombohedral perovskite: R-3c (#167), pseudo-cubic ~3.79 Å
    // Using hexagonal setting: a=5.357, c=13.11
    UnitCell uc("LaAlO3 rhombohedral",
        Lattice::hexagonal(5.357, 13.11));
    uc.space_group_number = 167; uc.space_group_symbol = "R-3c";
    // La at 6a: (0,0,1/4) + R centering
    uc.add_atom({0.0, 0.0, 0.25}, 57, +3.0, mass::La);
    uc.add_atom({0.0, 0.0, 0.75}, 57, +3.0, mass::La);
    uc.add_atom({1.0/3.0, 2.0/3.0, 2.0/3.0+0.25}, 57, +3.0, mass::La);
    uc.add_atom({1.0/3.0, 2.0/3.0, 2.0/3.0+0.75}, 57, +3.0, mass::La);
    uc.add_atom({2.0/3.0, 1.0/3.0, 1.0/3.0+0.25}, 57, +3.0, mass::La);
    uc.add_atom({2.0/3.0, 1.0/3.0, 1.0/3.0+0.75}, 57, +3.0, mass::La);
    // Al at 6b: (0,0,0) + R
    uc.add_atom({0.0, 0.0, 0.0}, 13, +3.0, mass::Al);
    uc.add_atom({0.0, 0.0, 0.5}, 13, +3.0, mass::Al);
    uc.add_atom({1.0/3.0, 2.0/3.0, 2.0/3.0}, 13, +3.0, mass::Al);
    uc.add_atom({1.0/3.0, 2.0/3.0, 2.0/3.0+0.5}, 13, +3.0, mass::Al);
    uc.add_atom({2.0/3.0, 1.0/3.0, 1.0/3.0}, 13, +3.0, mass::Al);
    uc.add_atom({2.0/3.0, 1.0/3.0, 1.0/3.0+0.5}, 13, +3.0, mass::Al);
    // O at 18e: (x,0,1/4) + R, x≈0.526
    constexpr double xO = 0.526;
    uc.add_atom({xO, 0.0, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({0.0, xO, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({1.0-xO, 1.0-xO, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({1.0-xO, 0.0, 0.75}, 8, -2.0, mass::O);
    uc.add_atom({0.0, 1.0-xO, 0.75}, 8, -2.0, mass::O);
    uc.add_atom({xO, xO, 0.75}, 8, -2.0, mass::O);
    // +R centering for O (1/3,2/3,2/3) shift
    uc.add_atom({xO+1.0/3.0, 2.0/3.0, 2.0/3.0+0.25}, 8, -2.0, mass::O);
    uc.add_atom({1.0/3.0, xO+2.0/3.0, 2.0/3.0+0.25}, 8, -2.0, mass::O);
    uc.add_atom({1.0/3.0-xO, 2.0/3.0-xO, 2.0/3.0+0.25}, 8, -2.0, mass::O);
    uc.add_atom({1.0/3.0-xO, 2.0/3.0, 2.0/3.0+0.75}, 8, -2.0, mass::O);
    uc.add_atom({1.0/3.0, 2.0/3.0-xO, 2.0/3.0+0.75}, 8, -2.0, mass::O);
    uc.add_atom({1.0/3.0+xO, 2.0/3.0+xO, 2.0/3.0+0.75}, 8, -2.0, mass::O);
    // +R centering (2/3,1/3,1/3) shift
    uc.add_atom({xO+2.0/3.0, 1.0/3.0, 1.0/3.0+0.25}, 8, -2.0, mass::O);
    uc.add_atom({2.0/3.0, xO+1.0/3.0, 1.0/3.0+0.25}, 8, -2.0, mass::O);
    uc.add_atom({2.0/3.0-xO, 1.0/3.0-xO, 1.0/3.0+0.25}, 8, -2.0, mass::O);
    uc.add_atom({2.0/3.0-xO, 1.0/3.0, 1.0/3.0+0.75}, 8, -2.0, mass::O);
    uc.add_atom({2.0/3.0, 1.0/3.0-xO, 1.0/3.0+0.75}, 8, -2.0, mass::O);
    uc.add_atom({2.0/3.0+xO, 1.0/3.0+xO, 1.0/3.0+0.75}, 8, -2.0, mass::O);
    return uc;
}

// ============================================================================
// GARNET-TYPE (A₃B₅O₁₂) — Ia-3d (#230), Z=8, 160 atoms/conventional cell
// I-centering doubles primitive to full cell.
// Primitive (half cell, 80 atoms):
//   12 A (24c/2), 8 B_oct (16a/2), 12 B_tet (24d/2), 48 O (96h/2)
// ============================================================================

UnitCell y3al5o12_garnet() {
    UnitCell uc("Y3Al5O12 YAG", Lattice::cubic(12.01));
    uc.space_group_number = 230; uc.space_group_symbol = "Ia-3d";

    // Y at 24c: (1/8,0,1/4) and permutations + sign changes → 12 per half-cell
    const double yc[][3] = {
        {0.125,0.0,0.25}, {0.875,0.0,0.75}, {0.375,0.0,0.75}, {0.625,0.0,0.25},
        {0.25,0.125,0.0}, {0.75,0.875,0.0}, {0.75,0.375,0.0}, {0.25,0.625,0.0},
        {0.0,0.25,0.125}, {0.0,0.75,0.875}, {0.0,0.75,0.375}, {0.0,0.25,0.625}
    };
    for (auto& p : yc) uc.add_atom({p[0],p[1],p[2]}, 39, +3.0, mass::Y);

    // Al at 16a: (0,0,0) subset → 8 per half-cell
    const double oa[][3] = {
        {0.0,0.0,0.0}, {0.5,0.0,0.5}, {0.0,0.5,0.5}, {0.5,0.5,0.0},
        {0.25,0.25,0.25}, {0.75,0.25,0.75}, {0.25,0.75,0.75}, {0.75,0.75,0.25}
    };
    for (auto& p : oa) uc.add_atom({p[0],p[1],p[2]}, 13, +3.0, mass::Al);

    // Al at 24d: (3/8,0,1/4) and permutations → 12 per half-cell
    const double td[][3] = {
        {0.375,0.0,0.25}, {0.625,0.0,0.75}, {0.875,0.0,0.25}, {0.125,0.0,0.75},
        {0.25,0.375,0.0}, {0.75,0.625,0.0}, {0.25,0.875,0.0}, {0.75,0.125,0.0},
        {0.0,0.25,0.375}, {0.0,0.75,0.625}, {0.0,0.25,0.875}, {0.0,0.75,0.125}
    };
    for (auto& p : td) uc.add_atom({p[0],p[1],p[2]}, 13, +3.0, mass::Al);

    // O at 96h: (x,y,z) with x≈-0.0306, y≈0.0512, z≈0.1494 → 48 per half-cell
    constexpr double ox=-0.0306, oy=0.0512, oz=0.1494;
    // Generate 48 positions from (x,y,z) using Ia-3d half-cell operations
    const double signs[4][3] = {{1,1,1},{-1,-1,1},{-1,1,-1},{1,-1,-1}};
    for (auto& s : signs) {
        double sx = ox*s[0], sy = oy*s[1], sz = oz*s[2];
        // (x,y,z), (z,x,y), (y,z,x) cyclic permutations
        uc.add_atom({sx, sy, sz}, 8, -2.0, mass::O);
        uc.add_atom({sz, sx, sy}, 8, -2.0, mass::O);
        uc.add_atom({sy, sz, sx}, 8, -2.0, mass::O);
        // Glide: (1/4+x, 1/4-z, 1/4+y)
        uc.add_atom({0.25+sx, 0.25-sz, 0.25+sy}, 8, -2.0, mass::O);
        uc.add_atom({0.25+sz, 0.25-sy, 0.25+sx}, 8, -2.0, mass::O);
        uc.add_atom({0.25+sy, 0.25-sx, 0.25+sz}, 8, -2.0, mass::O);
        // Screw: (1/4-x, 1/4+z, 3/4+y) and related
        uc.add_atom({0.25-sx, 0.25+sz, 0.75+sy}, 8, -2.0, mass::O);
        uc.add_atom({0.25-sz, 0.25+sy, 0.75+sx}, 8, -2.0, mass::O);
        uc.add_atom({0.25-sy, 0.25+sx, 0.75+sz}, 8, -2.0, mass::O);
        // Mirror+screw
        uc.add_atom({0.5+sx, 0.5-sy, 0.5+sz}, 8, -2.0, mass::O);
        uc.add_atom({0.5+sz, 0.5-sx, 0.5+sy}, 8, -2.0, mass::O);
        uc.add_atom({0.5+sy, 0.5-sz, 0.5+sx}, 8, -2.0, mass::O);
    }
    // 12 + 8 + 12 + 48 = 80 atoms (half cell)
    apply_bcc_centering(uc);  // → 160 atoms
    return uc;
}

UnitCell gd3ga5o12_garnet() {
    UnitCell uc("Gd3Ga5O12 GGG", Lattice::cubic(12.383));
    uc.space_group_number = 230; uc.space_group_symbol = "Ia-3d";

    const double yc[][3] = {
        {0.125,0.0,0.25}, {0.875,0.0,0.75}, {0.375,0.0,0.75}, {0.625,0.0,0.25},
        {0.25,0.125,0.0}, {0.75,0.875,0.0}, {0.75,0.375,0.0}, {0.25,0.625,0.0},
        {0.0,0.25,0.125}, {0.0,0.75,0.875}, {0.0,0.75,0.375}, {0.0,0.25,0.625}
    };
    for (auto& p : yc) uc.add_atom({p[0],p[1],p[2]}, 64, +3.0, mass::Gd);

    const double oa[][3] = {
        {0.0,0.0,0.0}, {0.5,0.0,0.5}, {0.0,0.5,0.5}, {0.5,0.5,0.0},
        {0.25,0.25,0.25}, {0.75,0.25,0.75}, {0.25,0.75,0.75}, {0.75,0.75,0.25}
    };
    for (auto& p : oa) uc.add_atom({p[0],p[1],p[2]}, 31, +3.0, mass::Ga);

    const double td[][3] = {
        {0.375,0.0,0.25}, {0.625,0.0,0.75}, {0.875,0.0,0.25}, {0.125,0.0,0.75},
        {0.25,0.375,0.0}, {0.75,0.625,0.0}, {0.25,0.875,0.0}, {0.75,0.125,0.0},
        {0.0,0.25,0.375}, {0.0,0.75,0.625}, {0.0,0.25,0.875}, {0.0,0.75,0.125}
    };
    for (auto& p : td) uc.add_atom({p[0],p[1],p[2]}, 31, +3.0, mass::Ga);

    constexpr double ox=-0.0279, oy=0.0564, oz=0.1526;
    const double signs[4][3] = {{1,1,1},{-1,-1,1},{-1,1,-1},{1,-1,-1}};
    for (auto& s : signs) {
        double sx = ox*s[0], sy = oy*s[1], sz = oz*s[2];
        uc.add_atom({sx, sy, sz}, 8, -2.0, mass::O);
        uc.add_atom({sz, sx, sy}, 8, -2.0, mass::O);
        uc.add_atom({sy, sz, sx}, 8, -2.0, mass::O);
        uc.add_atom({0.25+sx, 0.25-sz, 0.25+sy}, 8, -2.0, mass::O);
        uc.add_atom({0.25+sz, 0.25-sy, 0.25+sx}, 8, -2.0, mass::O);
        uc.add_atom({0.25+sy, 0.25-sx, 0.25+sz}, 8, -2.0, mass::O);
        uc.add_atom({0.25-sx, 0.25+sz, 0.75+sy}, 8, -2.0, mass::O);
        uc.add_atom({0.25-sz, 0.25+sy, 0.75+sx}, 8, -2.0, mass::O);
        uc.add_atom({0.25-sy, 0.25+sx, 0.75+sz}, 8, -2.0, mass::O);
        uc.add_atom({0.5+sx, 0.5-sy, 0.5+sz}, 8, -2.0, mass::O);
        uc.add_atom({0.5+sz, 0.5-sx, 0.5+sy}, 8, -2.0, mass::O);
        uc.add_atom({0.5+sy, 0.5-sz, 0.5+sx}, 8, -2.0, mass::O);
    }
    apply_bcc_centering(uc);  // → 160 atoms
    return uc;
}

// ============================================================================
// APATITE — Ca₅(PO₄)₃F — P6₃/m (#176), Z=2
// ============================================================================

UnitCell ca5po4_3f_apatite() {
    UnitCell uc("Ca5(PO4)3F apatite", Lattice::hexagonal(9.367, 6.884));
    uc.space_group_number = 176; uc.space_group_symbol = "P6_3/m";
    // Ca1 at 4f: (1/3, 2/3, z), z≈0.0015
    uc.add_atom({1.0/3.0, 2.0/3.0, 0.0015},  20, +2.0, mass::Ca);
    uc.add_atom({2.0/3.0, 1.0/3.0, 0.4985},  20, +2.0, mass::Ca);
    uc.add_atom({1.0/3.0, 2.0/3.0, 0.5015},  20, +2.0, mass::Ca);
    uc.add_atom({2.0/3.0, 1.0/3.0, 0.9985},  20, +2.0, mass::Ca);
    // Ca2 at 6h: (x,y,1/4), x≈0.2465, y≈0.9913
    uc.add_atom({0.2465, 0.9913, 0.25}, 20, +2.0, mass::Ca);
    uc.add_atom({0.0087, 0.2552, 0.25}, 20, +2.0, mass::Ca);
    uc.add_atom({0.7448, 0.7535, 0.25}, 20, +2.0, mass::Ca);
    uc.add_atom({0.7535, 0.0087, 0.75}, 20, +2.0, mass::Ca);
    uc.add_atom({0.9913, 0.7448, 0.75}, 20, +2.0, mass::Ca);
    uc.add_atom({0.2552, 0.2465, 0.75}, 20, +2.0, mass::Ca);
    // P at 6h: (x,y,1/4), x≈0.3989, y≈0.3689
    uc.add_atom({0.3989, 0.3689, 0.25}, 15, +5.0, mass::P);
    uc.add_atom({0.6311, 0.0300, 0.25}, 15, +5.0, mass::P);
    uc.add_atom({0.9700, 0.6011, 0.25}, 15, +5.0, mass::P);
    uc.add_atom({0.6011, 0.6311, 0.75}, 15, +5.0, mass::P);
    uc.add_atom({0.3689, 0.9700, 0.75}, 15, +5.0, mass::P);
    uc.add_atom({0.0300, 0.3989, 0.75}, 15, +5.0, mass::P);
    // O1 at 6h
    uc.add_atom({0.3285, 0.4843, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({0.5157, 0.8442, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({0.1558, 0.6715, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({0.6715, 0.5157, 0.75}, 8, -2.0, mass::O);
    uc.add_atom({0.4843, 0.1558, 0.75}, 8, -2.0, mass::O);
    uc.add_atom({0.8442, 0.3285, 0.75}, 8, -2.0, mass::O);
    // O2 at 6h
    uc.add_atom({0.5872, 0.4652, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({0.5348, 0.1220, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({0.8780, 0.4128, 0.25}, 8, -2.0, mass::O);
    uc.add_atom({0.4128, 0.5348, 0.75}, 8, -2.0, mass::O);
    uc.add_atom({0.4652, 0.8780, 0.75}, 8, -2.0, mass::O);
    uc.add_atom({0.1220, 0.5872, 0.75}, 8, -2.0, mass::O);
    // O3 at 12i: (x,y,z) general, x≈0.3428, y≈0.2580, z≈0.0703
    uc.add_atom({0.3428, 0.2580, 0.0703}, 8, -2.0, mass::O);
    uc.add_atom({0.7420, 0.0848, 0.0703}, 8, -2.0, mass::O);
    uc.add_atom({0.9152, 0.6572, 0.0703}, 8, -2.0, mass::O);
    uc.add_atom({0.6572, 0.7420, 0.5703}, 8, -2.0, mass::O);
    uc.add_atom({0.2580, 0.9152, 0.5703}, 8, -2.0, mass::O);
    uc.add_atom({0.0848, 0.3428, 0.5703}, 8, -2.0, mass::O);
    uc.add_atom({0.6572, 0.7420, 0.9297}, 8, -2.0, mass::O);
    uc.add_atom({0.2580, 0.9152, 0.9297}, 8, -2.0, mass::O);
    uc.add_atom({0.0848, 0.3428, 0.9297}, 8, -2.0, mass::O);
    uc.add_atom({0.3428, 0.2580, 0.4297}, 8, -2.0, mass::O);
    uc.add_atom({0.7420, 0.0848, 0.4297}, 8, -2.0, mass::O);
    uc.add_atom({0.9152, 0.6572, 0.4297}, 8, -2.0, mass::O);
    // F at 2a: (0,0,1/4) and (0,0,3/4)
    uc.add_atom({0.0, 0.0, 0.25}, 9, -1.0, mass::F);
    uc.add_atom({0.0, 0.0, 0.75}, 9, -1.0, mass::F);
    return uc;
}

// ============================================================================
// MONAZITE — LaPO₄ — P2₁/n (#14), Z=4
// ============================================================================

UnitCell lapo4_monazite() {
    UnitCell uc("LaPO4 monazite",
        Lattice::from_parameters(6.831, 7.071, 6.504, 90.0, 103.27, 90.0));
    uc.space_group_number = 14; uc.space_group_symbol = "P2_1/n";
    // La at 4e: (x,y,z) — Wyckoff general position
    constexpr double xL=0.2818, yL=0.1590, zL=0.1005;
    uc.add_atom({   xL,    yL,    zL}, 57, +3.0, mass::La);
    uc.add_atom({  -xL, 0.5+yL, 0.5-zL}, 57, +3.0, mass::La);
    uc.add_atom({  -xL,   -yL,   -zL}, 57, +3.0, mass::La);
    uc.add_atom({   xL, 0.5-yL, 0.5+zL}, 57, +3.0, mass::La);
    // P at 4e
    constexpr double xP=0.3044, yP=0.1632, zP=0.6124;
    uc.add_atom({   xP,    yP,    zP}, 15, +5.0, mass::P);
    uc.add_atom({  -xP, 0.5+yP, 0.5-zP}, 15, +5.0, mass::P);
    uc.add_atom({  -xP,   -yP,   -zP}, 15, +5.0, mass::P);
    uc.add_atom({   xP, 0.5-yP, 0.5+zP}, 15, +5.0, mass::P);
    // O1
    constexpr double xO1=0.2503, yO1=0.0064, zO1=0.4348;
    uc.add_atom({  xO1,   yO1,   zO1}, 8, -2.0, mass::O);
    uc.add_atom({ -xO1, 0.5+yO1, 0.5-zO1}, 8, -2.0, mass::O);
    uc.add_atom({ -xO1,  -yO1,  -zO1}, 8, -2.0, mass::O);
    uc.add_atom({  xO1, 0.5-yO1, 0.5+zO1}, 8, -2.0, mass::O);
    // O2
    constexpr double xO2=0.3815, yO2=0.3305, zO2=0.4975;
    uc.add_atom({  xO2,   yO2,   zO2}, 8, -2.0, mass::O);
    uc.add_atom({ -xO2, 0.5+yO2, 0.5-zO2}, 8, -2.0, mass::O);
    uc.add_atom({ -xO2,  -yO2,  -zO2}, 8, -2.0, mass::O);
    uc.add_atom({  xO2, 0.5-yO2, 0.5+zO2}, 8, -2.0, mass::O);
    // O3
    constexpr double xO3=0.4726, yO3=0.1052, zO3=0.8054;
    uc.add_atom({  xO3,   yO3,   zO3}, 8, -2.0, mass::O);
    uc.add_atom({ -xO3, 0.5+yO3, 0.5-zO3}, 8, -2.0, mass::O);
    uc.add_atom({ -xO3,  -yO3,  -zO3}, 8, -2.0, mass::O);
    uc.add_atom({  xO3, 0.5-yO3, 0.5+zO3}, 8, -2.0, mass::O);
    // O4
    constexpr double xO4=0.1268, yO4=0.2175, zO4=0.7107;
    uc.add_atom({  xO4,   yO4,   zO4}, 8, -2.0, mass::O);
    uc.add_atom({ -xO4, 0.5+yO4, 0.5-zO4}, 8, -2.0, mass::O);
    uc.add_atom({ -xO4,  -yO4,  -zO4}, 8, -2.0, mass::O);
    uc.add_atom({  xO4, 0.5-yO4, 0.5+zO4}, 8, -2.0, mass::O);
    return uc;
}

// ============================================================================
// PYROCHLORE-TYPE (A₂B₂O₇) — Fd-3m (#227), Z=8, 88 atoms/conventional cell
// A at 16d, B at 16c, O1 at 48f, O2 at 8b
// Primitive: 4A + 4B + 12O1 + 2O2 = 22, × 4 FCC = 88
// ============================================================================

static void add_pyrochlore_basis(UnitCell& uc, uint32_t Z_A, double m_A,
                                  uint32_t Z_B, double m_B, double x48f) {
    // A-site 16d (primitive: 4)
    uc.add_atom({0.5, 0.5, 0.5},   Z_A, +3.0, m_A);
    uc.add_atom({0.5, 0.25, 0.25}, Z_A, +3.0, m_A);
    uc.add_atom({0.25, 0.5, 0.25}, Z_A, +3.0, m_A);
    uc.add_atom({0.25, 0.25, 0.5}, Z_A, +3.0, m_A);
    // B-site 16c (primitive: 4)
    uc.add_atom({0.0, 0.0, 0.0},     Z_B, +4.0, m_B);
    uc.add_atom({0.0, 0.25, 0.25},   Z_B, +4.0, m_B);
    uc.add_atom({0.25, 0.0, 0.25},   Z_B, +4.0, m_B);
    uc.add_atom({0.25, 0.25, 0.0},   Z_B, +4.0, m_B);
    // O1 48f at (x,1/8,1/8) — primitive: 12 positions
    constexpr double e = 0.125;
    uc.add_atom({x48f, e, e}, 8, -2.0, mass::O);
    uc.add_atom({e, x48f, e}, 8, -2.0, mass::O);
    uc.add_atom({e, e, x48f}, 8, -2.0, mass::O);
    uc.add_atom({0.25-x48f, e, e}, 8, -2.0, mass::O);
    uc.add_atom({e, 0.25-x48f, e}, 8, -2.0, mass::O);
    uc.add_atom({e, e, 0.25-x48f}, 8, -2.0, mass::O);
    uc.add_atom({-x48f, -e, -e}, 8, -2.0, mass::O);
    uc.add_atom({-e, -x48f, -e}, 8, -2.0, mass::O);
    uc.add_atom({-e, -e, -x48f}, 8, -2.0, mass::O);
    uc.add_atom({0.25+x48f, -e, -e}, 8, -2.0, mass::O);
    uc.add_atom({-e, 0.25+x48f, -e}, 8, -2.0, mass::O);
    uc.add_atom({-e, -e, 0.25+x48f}, 8, -2.0, mass::O);
    // O2 8b at (3/8,3/8,3/8) — primitive: 2 positions
    uc.add_atom({0.375, 0.375, 0.375}, 8, -2.0, mass::O);
    uc.add_atom({0.625, 0.625, 0.625}, 8, -2.0, mass::O);
    // 4+4+12+2 = 22 primitive atoms
    apply_fcc_centering(uc);  // → 88 atoms
}

UnitCell gd2ti2o7_pyrochlore() {
    UnitCell uc("Gd2Ti2O7 pyrochlore", Lattice::cubic(10.185));
    uc.space_group_number = 227; uc.space_group_symbol = "Fd-3m";
    add_pyrochlore_basis(uc, 64, mass::Gd, 22, mass::Ti, 0.3317);
    return uc;
}

UnitCell la2zr2o7_pyrochlore() {
    UnitCell uc("La2Zr2O7 pyrochlore", Lattice::cubic(10.786));
    uc.space_group_number = 227; uc.space_group_symbol = "Fd-3m";
    add_pyrochlore_basis(uc, 57, mass::La, 40, mass::Zr, 0.3317);
    return uc;
}

UnitCell bi2ti2o7_pyrochlore() {
    UnitCell uc("Bi2Ti2O7 pyrochlore", Lattice::cubic(10.354));
    uc.space_group_number = 227; uc.space_group_symbol = "Fd-3m";
    add_pyrochlore_basis(uc, 83, mass::Bi, 22, mass::Ti, 0.3317);
    return uc;
}

// ============================================================================
// ACTINIDE OXIDES
// ============================================================================

UnitCell u3o8_orthorhombic() {
    // α-U₃O₈: C2mm (#38), a=6.716, b=11.960, c=4.147, Z=2
    // Average U oxidation state = +16/3 for exact charge neutrality
    UnitCell uc("U3O8 orthorhombic", Lattice::orthorhombic(6.716, 11.960, 4.147));
    uc.space_group_number = 38; uc.space_group_symbol = "C2mm";
    constexpr double q_U = 16.0 / 3.0;  // 6×(16/3) = 32 = 16×2
    // U1 at (0, y, 0) pentagonal bipyramidal
    uc.add_atom({0.0, 0.328, 0.0}, 92, +q_U, mass::U);
    uc.add_atom({0.0, 0.672, 0.0}, 92, +q_U, mass::U);
    uc.add_atom({0.5, 0.828, 0.0}, 92, +q_U, mass::U);
    uc.add_atom({0.5, 0.172, 0.0}, 92, +q_U, mass::U);
    // U2 at octahedral sites
    uc.add_atom({0.0, 0.0, 0.5},   92, +q_U, mass::U);
    uc.add_atom({0.5, 0.5, 0.5},   92, +q_U, mass::U);
    // 16 O atoms
    uc.add_atom({0.0, 0.0, 0.0}, 8, -2.0, mass::O);
    uc.add_atom({0.5, 0.5, 0.0}, 8, -2.0, mass::O);
    uc.add_atom({0.0, 0.20, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.0, 0.80, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.5, 0.70, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.5, 0.30, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.25, 0.10, 0.0}, 8, -2.0, mass::O);
    uc.add_atom({0.75, 0.40, 0.0}, 8, -2.0, mass::O);
    uc.add_atom({0.25, 0.90, 0.0}, 8, -2.0, mass::O);
    uc.add_atom({0.75, 0.60, 0.0}, 8, -2.0, mass::O);
    uc.add_atom({0.25, 0.40, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.75, 0.10, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.25, 0.60, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.75, 0.90, 0.5}, 8, -2.0, mass::O);
    uc.add_atom({0.0, 0.45, 0.0}, 8, -2.0, mass::O);
    uc.add_atom({0.5, 0.95, 0.0}, 8, -2.0, mass::O);
    return uc;
}

UnitCell uo3_gamma() {
    // γ-UO₃: Fddd (#70), a=9.813, b=19.897, c=9.711, Z=32
    // Primitive: 8 U + 24 O = 32, × 4 (F-centering) = 128 atoms
    UnitCell uc("UO3 gamma", Lattice::orthorhombic(9.813, 19.897, 9.711));
    uc.space_group_number = 70; uc.space_group_symbol = "Fddd";
    // U 32h primitive subset (8 positions)
    uc.add_atom({0.125, 0.050, 0.125}, 92, +6.0, mass::U);
    uc.add_atom({0.375, 0.200, 0.125}, 92, +6.0, mass::U);
    uc.add_atom({0.125, 0.200, 0.375}, 92, +6.0, mass::U);
    uc.add_atom({0.375, 0.050, 0.375}, 92, +6.0, mass::U);
    uc.add_atom({0.125, 0.300, 0.125}, 92, +6.0, mass::U);
    uc.add_atom({0.375, 0.450, 0.125}, 92, +6.0, mass::U);
    uc.add_atom({0.125, 0.450, 0.375}, 92, +6.0, mass::U);
    uc.add_atom({0.375, 0.300, 0.375}, 92, +6.0, mass::U);
    // O: 3 per U = 24 in primitive
    constexpr double o_coords[][3] = {
        {0.0, 0.0, 0.0}, {0.25, 0.0, 0.0}, {0.0, 0.125, 0.25},
        {0.25, 0.125, 0.25}, {0.0, 0.0, 0.25}, {0.25, 0.0, 0.25},
        {0.0, 0.125, 0.0}, {0.25, 0.125, 0.0}, {0.125, 0.050, 0.0},
        {0.375, 0.050, 0.0}, {0.125, 0.200, 0.0}, {0.375, 0.200, 0.0},
        {0.125, 0.050, 0.25}, {0.375, 0.050, 0.25}, {0.125, 0.200, 0.25},
        {0.375, 0.200, 0.25}, {0.0, 0.050, 0.125}, {0.25, 0.050, 0.125},
        {0.0, 0.200, 0.125}, {0.25, 0.200, 0.125}, {0.0, 0.050, 0.375},
        {0.25, 0.050, 0.375}, {0.0, 0.200, 0.375}, {0.25, 0.200, 0.375}
    };
    for (auto& p : o_coords)
        uc.add_atom({p[0], p[1], p[2]}, 8, -2.0, mass::O);
    // 8U + 24O = 32 primitive, × 4 FCC = 128 atoms
    apply_fcc_centering(uc);
    return uc;
}

} // namespace presets
} // namespace crystal
} // namespace atomistic
