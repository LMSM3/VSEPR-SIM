#pragma once
/*
covalent_radii.hpp
------------------
Covalent radii lookup for bond length estimation.

Data source: Pyykkö & Atsumi (2009) single-bond covalent radii
Values in Ångströms (Å)

Reference:
Pyykkö, P.; Atsumi, M. (2009). "Molecular Single-Bond Covalent Radii 
for Elements 1–118". Chemistry: A European Journal. 15 (1): 186–197.
*/

#include <cstdint>
#include <array>

namespace vsepr {

// Covalent radii in Ångströms for elements Z=1-118
// Index by atomic number Z (index 0 is unused, Z=1 starts at index 1)
constexpr std::array<double, 119> COVALENT_RADII = {
    0.00,  // Z=0 (unused)
    0.32,  // H  (1)
    0.46,  // He (2)
    1.33,  // Li (3)
    1.02,  // Be (4)
    0.85,  // B  (5)
    0.75,  // C  (6)
    0.71,  // N  (7)
    0.63,  // O  (8)
    0.64,  // F  (9)
    0.67,  // Ne (10)
    1.55,  // Na (11)
    1.39,  // Mg (12)
    1.26,  // Al (13)
    1.16,  // Si (14)
    1.11,  // P  (15)
    1.03,  // S  (16)
    0.99,  // Cl (17)
    0.96,  // Ar (18)
    1.96,  // K  (19)
    1.71,  // Ca (20)
    1.48,  // Sc (21)
    1.36,  // Ti (22)
    1.34,  // V  (23)
    1.22,  // Cr (24)
    1.19,  // Mn (25)
    1.16,  // Fe (26)
    1.11,  // Co (27)
    1.10,  // Ni (28)
    1.12,  // Cu (29)
    1.18,  // Zn (30)
    1.24,  // Ga (31)
    1.21,  // Ge (32)
    1.21,  // As (33)
    1.16,  // Se (34)
    1.14,  // Br (35)
    1.17,  // Kr (36)
    2.10,  // Rb (37)
    1.85,  // Sr (38)
    1.63,  // Y  (39)
    1.54,  // Zr (40)
    1.47,  // Nb (41)
    1.38,  // Mo (42)
    1.28,  // Tc (43)
    1.25,  // Ru (44)
    1.25,  // Rh (45)
    1.20,  // Pd (46)
    1.28,  // Ag (47)
    1.36,  // Cd (48)
    1.42,  // In (49)
    1.40,  // Sn (50)
    1.40,  // Sb (51)
    1.36,  // Te (52)
    1.33,  // I  (53)
    1.31,  // Xe (54)
    2.32,  // Cs (55)
    1.96,  // Ba (56)
    1.80,  // La (57)
    1.63,  // Ce (58)
    1.76,  // Pr (59)
    1.74,  // Nd (60)
    1.73,  // Pm (61)
    1.72,  // Sm (62)
    1.68,  // Eu (63)
    1.69,  // Gd (64)
    1.68,  // Tb (65)
    1.67,  // Dy (66)
    1.66,  // Ho (67)
    1.65,  // Er (68)
    1.64,  // Tm (69)
    1.70,  // Yb (70)
    1.62,  // Lu (71)
    1.52,  // Hf (72)
    1.46,  // Ta (73)
    1.37,  // W  (74)
    1.31,  // Re (75)
    1.29,  // Os (76)
    1.22,  // Ir (77)
    1.23,  // Pt (78)
    1.24,  // Au (79)
    1.33,  // Hg (80)
    1.44,  // Tl (81)
    1.44,  // Pb (82)
    1.51,  // Bi (83)
    1.45,  // Po (84)
    1.47,  // At (85)
    1.42,  // Rn (86)
    2.23,  // Fr (87)
    2.01,  // Ra (88)
    1.86,  // Ac (89)
    1.75,  // Th (90)
    1.69,  // Pa (91)
    1.70,  // U  (92)
    1.71,  // Np (93)
    1.72,  // Pu (94)
    1.66,  // Am (95)
    1.66,  // Cm (96)
    1.68,  // Bk (97)
    1.68,  // Cf (98)
    1.65,  // Es (99)
    1.67,  // Fm (100)
    1.73,  // Md (101)
    1.76,  // No (102)
    1.61,  // Lr (103)
    1.57,  // Rf (104)
    1.49,  // Db (105)
    1.43,  // Sg (106)
    1.41,  // Bh (107)
    1.34,  // Hs (108)
    1.29,  // Mt (109)
    1.28,  // Ds (110)
    1.21,  // Rg (111)
    1.22,  // Cn (112)
    1.36,  // Nh (113)
    1.43,  // Fl (114)
    1.62,  // Mc (115)
    1.75,  // Lv (116)
    1.65,  // Ts (117)
    1.57   // Og (118)
};

// Get covalent radius for atomic number Z
// Returns 0.0 for invalid Z
inline double get_covalent_radius(uint8_t Z) {
    if (Z == 0 || Z >= COVALENT_RADII.size()) return 0.0;
    return COVALENT_RADII[Z];
}

// Estimate equilibrium bond length between atoms with atomic numbers Z1 and Z2
// Uses sum of covalent radii
inline double estimate_bond_length(uint8_t Z1, uint8_t Z2) {
    return get_covalent_radius(Z1) + get_covalent_radius(Z2);
}

// Scale factor for multiple bonds (empirical)
// Single bond: 1.0, double: ~0.87, triple: ~0.78
inline double bond_order_scale(uint8_t order) {
    switch(order) {
        case 1: return 1.00;   // single
        case 2: return 0.87;   // double
        case 3: return 0.78;   // triple
        default: return 1.00;
    }
}

} // namespace vsepr
