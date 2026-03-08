#pragma once
/**
 * reference_data.hpp
 * ------------------
 * Empirical reference data for crystal verification.
 * Sources: ICSD, Wyckoff Crystal Structures, CRC Handbook.
 *
 * Each entry provides: formula, lattice params, space group, Z, density,
 * expected CN per site, bond length ranges, and XRD peak positions.
 */

#include "atomistic/crystal/crystal_metrics.hpp"

namespace atomistic {
namespace crystal {
namespace reference {

// ============================================================================
// Reference Data Factory Functions
// ============================================================================

// --- Metals ---

inline ReferenceData aluminum_fcc_ref() {
    ReferenceData r;
    r.formula = "Al";
    r.a = 4.05; r.b = 4.05; r.c = 4.05;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;  // Fm-3m
    r.Z = 4;
    r.density_gcc = 2.70;
    r.expected_CN[13] = 12;  // FCC: CN=12
    r.expected_bonds[{13,13}] = {2.80, 2.90};  // Al-Al NN
    r.expected_peaks = {38.5, 44.7, 65.1, 78.2};  // Cu Kα
    return r;
}

inline ReferenceData iron_bcc_ref() {
    ReferenceData r;
    r.formula = "Fe";
    r.a = 2.87; r.b = 2.87; r.c = 2.87;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 229;  // Im-3m
    r.Z = 2;
    r.density_gcc = 7.87;
    r.expected_CN[26] = 8;   // BCC: CN=8
    r.expected_bonds[{26,26}] = {2.45, 2.55};
    r.expected_peaks = {44.7, 65.0, 82.3};
    return r;
}

inline ReferenceData copper_fcc_ref() {
    ReferenceData r;
    r.formula = "Cu";
    r.a = 3.61; r.b = 3.61; r.c = 3.61;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;
    r.Z = 4;
    r.density_gcc = 8.96;
    r.expected_CN[29] = 12;
    r.expected_bonds[{29,29}] = {2.50, 2.60};
    r.expected_peaks = {43.3, 50.4, 74.1};
    return r;
}

inline ReferenceData gold_fcc_ref() {
    ReferenceData r;
    r.formula = "Au";
    r.a = 4.08; r.b = 4.08; r.c = 4.08;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;
    r.Z = 4;
    r.density_gcc = 19.30;
    r.expected_CN[79] = 12;
    r.expected_bonds[{79,79}] = {2.84, 2.92};
    r.expected_peaks = {38.2, 44.4, 64.6, 77.5};
    return r;
}

// --- Ionic ---

inline ReferenceData nacl_ref() {
    ReferenceData r;
    r.formula = "NaCl";
    r.a = 5.64; r.b = 5.64; r.c = 5.64;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;  // Fm-3m
    r.Z = 4;
    r.density_gcc = 2.16;
    r.expected_CN[11] = 6;   // Na: octahedral
    r.expected_CN[17] = 6;   // Cl: octahedral
    r.expected_bonds[{11,17}] = {2.78, 2.86};  // Na-Cl
    r.expected_peaks = {27.4, 31.7, 45.5, 53.9, 56.5};
    return r;
}

inline ReferenceData mgo_ref() {
    ReferenceData r;
    r.formula = "MgO";
    r.a = 4.21; r.b = 4.21; r.c = 4.21;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;
    r.Z = 4;
    r.density_gcc = 3.58;
    r.expected_CN[12] = 6;  // Mg: octahedral
    r.expected_CN[8] = 6;   // O: octahedral
    r.expected_bonds[{8,12}] = {2.08, 2.14};
    r.expected_peaks = {36.9, 42.9, 62.3, 74.7, 78.6};
    return r;
}

inline ReferenceData cscl_ref() {
    ReferenceData r;
    r.formula = "ClCs";
    r.a = 4.12; r.b = 4.12; r.c = 4.12;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 221;  // Pm-3m
    r.Z = 1;
    r.density_gcc = 3.99;
    r.expected_CN[55] = 8;  // Cs: cubic coordination
    r.expected_CN[17] = 8;  // Cl: cubic coordination
    r.expected_bonds[{17,55}] = {3.50, 3.60};
    r.expected_peaks = {21.5, 30.5, 37.6, 43.6};
    return r;
}

// --- Covalent ---

inline ReferenceData silicon_ref() {
    ReferenceData r;
    r.formula = "Si";
    r.a = 5.43; r.b = 5.43; r.c = 5.43;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 227;  // Fd-3m
    r.Z = 8;
    r.density_gcc = 2.33;
    r.expected_CN[14] = 4;   // Diamond: tetrahedral
    r.expected_bonds[{14,14}] = {2.32, 2.40};
    r.expected_peaks = {28.4, 47.3, 56.1, 69.1, 76.4};
    return r;
}

inline ReferenceData diamond_ref() {
    ReferenceData r;
    r.formula = "C";
    r.a = 3.57; r.b = 3.57; r.c = 3.57;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 227;
    r.Z = 8;
    r.density_gcc = 3.51;
    r.expected_CN[6] = 4;
    r.expected_bonds[{6,6}] = {1.52, 1.58};
    r.expected_peaks = {43.9, 75.3};
    return r;
}

// --- Oxides ---

inline ReferenceData rutile_tio2_ref() {
    ReferenceData r;
    r.formula = "O2Ti";
    r.a = 4.59; r.b = 4.59; r.c = 2.96;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 136;  // P4₂/mnm
    r.Z = 2;
    r.density_gcc = 4.25;
    r.expected_CN[22] = 6;   // Ti: octahedral
    r.expected_CN[8] = 3;    // O: trigonal planar
    r.expected_bonds[{8,22}] = {1.90, 2.00};
    r.expected_peaks = {27.4, 36.1, 39.2, 41.2, 44.0, 54.3};
    return r;
}

// ============================================================================
// Future benchmark targets (data stubs for the run list)
// ============================================================================

inline ReferenceData fluorite_ceo2_ref() {
    ReferenceData r;
    r.formula = "CeO2";
    r.a = 5.411; r.b = 5.411; r.c = 5.411;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;  // Fm-3m
    r.Z = 4;
    r.density_gcc = 7.22;
    r.expected_CN[58] = 8;   // Ce: cubic coordination
    r.expected_CN[8] = 4;    // O: tetrahedral
    r.expected_bonds[{8,58}] = {2.30, 2.40};
    r.expected_peaks = {28.5, 33.1, 47.5, 56.3};
    return r;
}

inline ReferenceData fluorite_tho2_ref() {
    ReferenceData r;
    r.formula = "O2Th";
    r.a = 5.597; r.b = 5.597; r.c = 5.597;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;
    r.Z = 4;
    r.density_gcc = 10.00;
    r.expected_CN[90] = 8;
    r.expected_CN[8] = 4;
    r.expected_bonds[{8,90}] = {2.40, 2.48};
    return r;
}

inline ReferenceData fluorite_puo2_ref() {
    ReferenceData r;
    r.formula = "O2Pu";
    r.a = 5.396; r.b = 5.396; r.c = 5.396;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;
    r.Z = 4;
    r.density_gcc = 11.46;
    r.expected_CN[94] = 8;
    r.expected_CN[8] = 4;
    r.expected_bonds[{8,94}] = {2.32, 2.40};
    return r;
}

inline ReferenceData zro2_cubic_ref() {
    ReferenceData r;
    r.formula = "O2Zr";
    r.a = 5.07; r.b = 5.07; r.c = 5.07;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 225;
    r.Z = 4;
    r.density_gcc = 6.27;
    r.expected_CN[40] = 8;
    r.expected_CN[8] = 4;
    return r;
}

inline ReferenceData zro2_tetragonal_ref() {
    ReferenceData r;
    r.formula = "O2Zr";
    r.a = 3.64; r.b = 3.64; r.c = 5.27;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 137;
    r.Z = 2;
    r.density_gcc = 6.10;
    r.expected_CN[40] = 8;
    return r;
}

inline ReferenceData zro2_monoclinic_ref() {
    ReferenceData r;
    r.formula = "O2Zr";
    r.a = 5.1505; r.b = 5.2116; r.c = 5.3173;
    r.alpha = 90; r.beta = 99.23; r.gamma = 90;
    r.space_group = 14;
    r.Z = 4;
    r.density_gcc = 5.68;
    r.expected_CN[40] = 7;
    return r;
}

inline ReferenceData hfo2_monoclinic_ref() {
    ReferenceData r;
    r.formula = "HfO2";
    r.a = 5.1156; r.b = 5.1722; r.c = 5.2948;
    r.alpha = 90; r.beta = 99.18; r.gamma = 90;
    r.space_group = 14;
    r.Z = 4;
    r.density_gcc = 9.68;
    return r;
}

inline ReferenceData perovskite_srtio3_ref() {
    ReferenceData r;
    r.formula = "O3SrTi";
    r.a = 3.905; r.b = 3.905; r.c = 3.905;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 221;
    r.Z = 1;
    r.density_gcc = 5.12;
    r.expected_CN[38] = 12;
    r.expected_CN[22] = 6;
    r.expected_bonds[{8,22}] = {1.93, 1.97};
    return r;
}

inline ReferenceData perovskite_batio3_cubic_ref() {
    ReferenceData r;
    r.formula = "BaO3Ti";
    r.a = 4.009; r.b = 4.009; r.c = 4.009;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 221;
    r.Z = 1;
    r.density_gcc = 6.02;
    r.expected_CN[22] = 6;
    r.expected_bonds[{8,22}] = {1.99, 2.03};
    return r;
}

inline ReferenceData spinel_mgal2o4_ref() {
    ReferenceData r;
    r.formula = "Al2MgO4";
    r.a = 8.083; r.b = 8.083; r.c = 8.083;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 227;
    r.Z = 8;
    r.density_gcc = 3.58;
    r.expected_CN[12] = 4;
    r.expected_CN[13] = 6;
    return r;
}

inline ReferenceData spinel_fe3o4_ref() {
    ReferenceData r;
    r.formula = "Fe3O4";
    r.a = 8.396; r.b = 8.396; r.c = 8.396;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 227;
    r.Z = 8;
    r.density_gcc = 5.17;
    return r;
}

inline ReferenceData pyrochlore_gd2ti2o7_ref() {
    ReferenceData r;
    r.formula = "Gd2O7Ti2";
    r.a = 10.185; r.b = 10.185; r.c = 10.185;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 227;
    r.Z = 8;
    r.density_gcc = 6.56;
    return r;
}

inline ReferenceData pyrochlore_la2zr2o7_ref() {
    ReferenceData r;
    r.formula = "La2O7Zr2";
    r.a = 10.786; r.b = 10.786; r.c = 10.786;
    r.alpha = 90; r.beta = 90; r.gamma = 90;
    r.space_group = 227;
    r.Z = 8;
    r.density_gcc = 5.90;
    return r;
}

} // namespace reference
} // namespace crystal
} // namespace atomistic
