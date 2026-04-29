#pragma once
/*
atomic_masses.hpp
-----------------
IUPAC standard atomic weights for all elements.

Data source: IUPAC Commission on Isotopic Abundances and Atomic Weights
Values represent the standard atomic weight (conventional) in unified
atomic mass units (Da / amu).

Reference:
IUPAC Technical Report (2021). "Standard Atomic Weights of the Elements
2021". Pure and Applied Chemistry.

For elements without stable isotopes (Tc, Pm, and Z >= 84), the value
listed is the mass number of the longest-lived or most commonly
encountered isotope, enclosed in brackets by convention.  Here we store
the numeric value directly for computational use.
*/

#include <cstdint>
#include <array>

namespace vsepr {

// Standard atomic weights in Da (amu) for elements Z=1-118
// Index by atomic number Z (index 0 is unused, Z=1 starts at index 1)
constexpr std::array<double, 119> ATOMIC_MASSES = {
    0.000,      // Z=0 (unused)
    1.008,      // H   (1)
    4.0026,     // He  (2)
    6.941,      // Li  (3)
    9.0122,     // Be  (4)
    10.81,      // B   (5)
    12.011,     // C   (6)
    14.007,     // N   (7)
    15.999,     // O   (8)
    18.998,     // F   (9)
    20.180,     // Ne  (10)
    22.990,     // Na  (11)
    24.305,     // Mg  (12)
    26.982,     // Al  (13)
    28.085,     // Si  (14)
    30.974,     // P   (15)
    32.06,      // S   (16)
    35.45,      // Cl  (17)
    39.948,     // Ar  (18)
    39.098,     // K   (19)
    40.078,     // Ca  (20)
    44.956,     // Sc  (21)
    47.867,     // Ti  (22)
    50.942,     // V   (23)
    51.996,     // Cr  (24)
    54.938,     // Mn  (25)
    55.845,     // Fe  (26)
    58.933,     // Co  (27)
    58.693,     // Ni  (28)
    63.546,     // Cu  (29)
    65.38,      // Zn  (30)
    69.723,     // Ga  (31)
    72.630,     // Ge  (32)
    74.922,     // As  (33)
    78.971,     // Se  (34)
    79.904,     // Br  (35)
    83.798,     // Kr  (36)
    85.468,     // Rb  (37)
    87.62,      // Sr  (38)
    88.906,     // Y   (39)
    91.224,     // Zr  (40)
    92.906,     // Nb  (41)
    95.95,      // Mo  (42)
    97.0,       // Tc  (43)  [longest-lived isotope]
    101.07,     // Ru  (44)
    102.91,     // Rh  (45)
    106.42,     // Pd  (46)
    107.87,     // Ag  (47)
    112.41,     // Cd  (48)
    114.82,     // In  (49)
    118.71,     // Sn  (50)
    121.76,     // Sb  (51)
    127.60,     // Te  (52)
    126.90,     // I   (53)
    131.29,     // Xe  (54)
    132.91,     // Cs  (55)
    137.33,     // Ba  (56)
    138.91,     // La  (57)
    140.12,     // Ce  (58)
    140.91,     // Pr  (59)
    144.24,     // Nd  (60)
    145.0,      // Pm  (61)  [longest-lived isotope]
    150.36,     // Sm  (62)
    151.96,     // Eu  (63)
    157.25,     // Gd  (64)
    158.93,     // Tb  (65)
    162.50,     // Dy  (66)
    164.93,     // Ho  (67)
    167.26,     // Er  (68)
    168.93,     // Tm  (69)
    173.05,     // Yb  (70)
    174.97,     // Lu  (71)
    178.49,     // Hf  (72)
    180.95,     // Ta  (73)
    183.84,     // W   (74)
    186.21,     // Re  (75)
    190.23,     // Os  (76)
    192.22,     // Ir  (77)
    195.08,     // Pt  (78)
    196.97,     // Au  (79)
    200.59,     // Hg  (80)
    204.38,     // Tl  (81)
    207.2,      // Pb  (82)
    208.98,     // Bi  (83)
    209.0,      // Po  (84)  [longest-lived isotope]
    210.0,      // At  (85)  [longest-lived isotope]
    222.0,      // Rn  (86)  [longest-lived isotope]
    223.0,      // Fr  (87)  [longest-lived isotope]
    226.0,      // Ra  (88)  [longest-lived isotope]
    227.0,      // Ac  (89)  [longest-lived isotope]
    232.04,     // Th  (90)
    231.04,     // Pa  (91)
    238.03,     // U   (92)
    237.0,      // Np  (93)  [longest-lived isotope]
    244.0,      // Pu  (94)  [longest-lived isotope]
    243.0,      // Am  (95)  [longest-lived isotope]
    247.0,      // Cm  (96)  [longest-lived isotope]
    247.0,      // Bk  (97)  [longest-lived isotope]
    251.0,      // Cf  (98)  [longest-lived isotope]
    252.0,      // Es  (99)  [longest-lived isotope]
    257.0,      // Fm  (100) [longest-lived isotope]
    258.0,      // Md  (101) [longest-lived isotope]
    259.0,      // No  (102) [longest-lived isotope]
    266.0,      // Lr  (103) [longest-lived isotope]
    267.0,      // Rf  (104) [longest-lived isotope]
    268.0,      // Db  (105) [longest-lived isotope]
    269.0,      // Sg  (106) [longest-lived isotope]
    270.0,      // Bh  (107) [longest-lived isotope]
    277.0,      // Hs  (108) [longest-lived isotope]
    278.0,      // Mt  (109) [longest-lived isotope]
    281.0,      // Ds  (110) [longest-lived isotope]
    282.0,      // Rg  (111) [longest-lived isotope]
    285.0,      // Cn  (112) [longest-lived isotope]
    286.0,      // Nh  (113) [longest-lived isotope]
    289.0,      // Fl  (114) [longest-lived isotope]
    290.0,      // Mc  (115) [longest-lived isotope]
    293.0,      // Lv  (116) [longest-lived isotope]
    294.0,      // Ts  (117) [longest-lived isotope]
    294.0       // Og  (118) [longest-lived isotope]
};

// Get standard atomic weight for atomic number Z
// Returns 0.0 for invalid Z
inline double get_atomic_mass(uint8_t Z) {
    if (Z == 0 || Z >= ATOMIC_MASSES.size()) return 0.0;
    return ATOMIC_MASSES[Z];
}

} // namespace vsepr
