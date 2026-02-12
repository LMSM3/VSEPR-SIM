#pragma once
/*
vdw_radii.hpp
-------------
Van der Waals radii for all elements.

Data source: Bondi (1964) and Alvarez (2013) compilations
Values in Ångströms (Å)

These radii define the "soft sphere" size for nonbonded interactions.
Used for Lennard-Jones σ parameter: σ_ij = r_vdw_i + r_vdw_j

References:
- Bondi, A. (1964). J. Phys. Chem. 68, 441-451.
- Alvarez, S. (2013). Dalton Trans. 42, 8617-8636.
*/

#include <cstdint>
#include <array>

namespace vsepr {

// Van der Waals radii in Ångströms for elements Z=1-118
constexpr std::array<double, 119> VDW_RADII = {
    0.00,  // Z=0 (unused)
    1.20,  // H  (1)
    1.40,  // He (2)
    1.82,  // Li (3)
    1.53,  // Be (4)
    1.92,  // B  (5)
    1.70,  // C  (6)
    1.55,  // N  (7)
    1.52,  // O  (8)
    1.47,  // F  (9)
    1.54,  // Ne (10)
    2.27,  // Na (11)
    1.73,  // Mg (12)
    1.84,  // Al (13)
    2.10,  // Si (14)
    1.80,  // P  (15)
    1.80,  // S  (16)
    1.75,  // Cl (17)
    1.88,  // Ar (18)
    2.75,  // K  (19)
    2.31,  // Ca (20)
    2.11,  // Sc (21)
    1.87,  // Ti (22)
    1.79,  // V  (23)
    1.89,  // Cr (24)
    1.97,  // Mn (25)
    1.94,  // Fe (26)
    1.92,  // Co (27)
    1.84,  // Ni (28)
    1.86,  // Cu (29)
    2.10,  // Zn (30)
    1.87,  // Ga (31)
    2.11,  // Ge (32)
    1.85,  // As (33)
    1.90,  // Se (34)
    1.85,  // Br (35)
    2.02,  // Kr (36)
    3.03,  // Rb (37)
    2.49,  // Sr (38)
    2.19,  // Y  (39)
    2.00,  // Zr (40)
    1.98,  // Nb (41)
    1.96,  // Mo (42)
    2.01,  // Tc (43)
    1.97,  // Ru (44)
    1.95,  // Rh (45)
    2.02,  // Pd (46)
    2.03,  // Ag (47)
    2.30,  // Cd (48)
    1.93,  // In (49)
    2.17,  // Sn (50)
    2.06,  // Sb (51)
    2.06,  // Te (52)
    1.98,  // I  (53)
    2.16,  // Xe (54)
    3.43,  // Cs (55)
    2.68,  // Ba (56)
    2.43,  // La (57)
    2.42,  // Ce (58)
    2.40,  // Pr (59)
    2.39,  // Nd (60)
    2.38,  // Pm (61)
    2.36,  // Sm (62)
    2.35,  // Eu (63)
    2.34,  // Gd (64)
    2.33,  // Tb (65)
    2.31,  // Dy (66)
    2.30,  // Ho (67)
    2.29,  // Er (68)
    2.27,  // Tm (69)
    2.26,  // Yb (70)
    2.24,  // Lu (71)
    2.23,  // Hf (72)
    2.22,  // Ta (73)
    2.18,  // W  (74)
    2.16,  // Re (75)
    2.16,  // Os (76)
    2.13,  // Ir (77)
    2.13,  // Pt (78)
    2.14,  // Au (79)
    2.23,  // Hg (80)
    1.96,  // Tl (81)
    2.02,  // Pb (82)
    2.07,  // Bi (83)
    1.97,  // Po (84)
    2.02,  // At (85)
    2.20,  // Rn (86)
    3.48,  // Fr (87)
    2.83,  // Ra (88)
    2.47,  // Ac (89)
    2.45,  // Th (90)
    2.43,  // Pa (91)
    2.41,  // U  (92)
    2.39,  // Np (93)
    2.43,  // Pu (94)
    2.44,  // Am (95)
    2.45,  // Cm (96)
    2.44,  // Bk (97)
    2.45,  // Cf (98)
    2.45,  // Es (99)
    2.45,  // Fm (100)
    2.46,  // Md (101)
    2.46,  // No (102)
    2.46,  // Lr (103)
    2.30,  // Rf (104)
    2.30,  // Db (105)
    2.30,  // Sg (106)
    2.30,  // Bh (107)
    2.30,  // Hs (108)
    2.30,  // Mt (109)
    2.30,  // Ds (110)
    2.30,  // Rg (111)
    2.30,  // Cn (112)
    2.30,  // Nh (113)
    2.30,  // Fl (114)
    2.30,  // Mc (115)
    2.30,  // Lv (116)
    2.30,  // Ts (117)
    2.30   // Og (118)
};

// Get VDW radius for atomic number Z
inline double get_vdw_radius(uint8_t Z) {
    if (Z == 0 || Z >= VDW_RADII.size()) return 2.0;  // Default fallback
    return VDW_RADII[Z];
}

} // namespace vsepr
