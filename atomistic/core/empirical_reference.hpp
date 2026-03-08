#pragma once
/**
 * empirical_reference.hpp -- Expanded curated empirical database
 *
 * Sources:
 *   NIST_CCCBDB  NIST Computational Chemistry Comparison and Benchmark DB r22
 *   NIST_WB      NIST Chemistry WebBook
 *   CRC_104      CRC Handbook of Chemistry and Physics, 104th ed.
 *   WYCKOFF      Wyckoff, Crystal Structures, 2nd ed.
 *   RAPPE_1992   Rappe et al. J. Am. Chem. Soc. 114, 10024 (1992) UFF
 */

#include <optional>
#include <cmath>
#include <string>

namespace empirical {

struct BondRef    { const char* formula, *bond;   double r_eq,  tol;            const char* source; };
struct AngleRef   { const char* formula, *angle;  double theta_eq, tol;         const char* source; };
struct CrystalRef { const char* name, *structure; double a, tol_a; int Z_per_cell; double nn; int cn; const char* source; };
struct LJRef      { int Z; const char* symbol;    double sigma, epsilon, r_min; };
struct SolventRef { const char* name;             double dielectric, source_T;  const char* source; };
struct NobleGasRef{ const char* symbol; int Z;    double well_depth_K, sigma_A; const char* source; };
struct IonRef     { const char* symbol; int Z;    double formal_charge, radius_A; const char* source; };
struct DiatomicRef{ const char* formula, *bond;   double r_eq, D_e, omega_e;   const char* source; };

// ---- Bond lengths (Angstrom) ------------------------------------------------
inline const BondRef BOND_REFS[] = {
    {"H2",    "H-H",   0.741, 0.005, "NIST_CCCBDB"},
    {"F2",    "F-F",   1.412, 0.005, "NIST_CCCBDB"},
    {"Cl2",   "Cl-Cl", 1.988, 0.010, "NIST_CCCBDB"},
    {"Br2",   "Br-Br", 2.281, 0.010, "NIST_CCCBDB"},
    {"I2",    "I-I",   2.666, 0.010, "NIST_CCCBDB"},
    {"N2",    "N=N",   1.098, 0.005, "NIST_CCCBDB"},
    {"O2",    "O=O",   1.208, 0.005, "NIST_CCCBDB"},
    {"CO",    "C=O",   1.128, 0.005, "NIST_CCCBDB"},
    {"NO",    "N=O",   1.151, 0.005, "NIST_CCCBDB"},
    {"HF",    "H-F",   0.917, 0.005, "NIST_CCCBDB"},
    {"HCl",   "H-Cl",  1.275, 0.005, "NIST_CCCBDB"},
    {"HBr",   "H-Br",  1.414, 0.010, "NIST_CCCBDB"},
    {"HI",    "H-I",   1.609, 0.010, "NIST_CCCBDB"},
    {"NaCl",  "Na-Cl", 2.361, 0.010, "NIST_CCCBDB gas"},
    {"KCl",   "K-Cl",  2.667, 0.010, "NIST_CCCBDB gas"},
    {"LiF",   "Li-F",  1.564, 0.010, "NIST_CCCBDB"},
    {"LiH",   "Li-H",  1.596, 0.010, "NIST_CCCBDB"},
    {"H2O",   "O-H",   0.958, 0.005, "NIST_CCCBDB"},
    {"H2S",   "S-H",   1.336, 0.010, "NIST_CCCBDB"},
    {"H2Se",  "Se-H",  1.460, 0.010, "NIST_CCCBDB"},
    {"H2Te",  "Te-H",  1.658, 0.015, "NIST_CCCBDB"},
    {"NH3",   "N-H",   1.012, 0.005, "NIST_CCCBDB"},
    {"PH3",   "P-H",   1.421, 0.010, "NIST_CCCBDB"},
    {"AsH3",  "As-H",  1.511, 0.010, "NIST_CCCBDB"},
    {"CH4",   "C-H",   1.087, 0.005, "NIST_CCCBDB"},
    {"SiH4",  "Si-H",  1.480, 0.010, "NIST_CCCBDB"},
    {"GeH4",  "Ge-H",  1.514, 0.010, "NIST_CCCBDB"},
    {"C2H6",  "C-C",   1.536, 0.010, "NIST_CCCBDB"},
    {"C2H6",  "C-H",   1.091, 0.010, "NIST_CCCBDB"},
    {"C2H4",  "C=C",   1.339, 0.010, "NIST_CCCBDB"},
    {"C2H4",  "C-H",   1.086, 0.010, "NIST_CCCBDB"},
    {"C2H2",  "C#C",   1.203, 0.010, "NIST_CCCBDB"},
    {"C2H2",  "C-H",   1.063, 0.010, "NIST_CCCBDB"},
    {"C6H6",  "C-C",   1.397, 0.010, "NIST_CCCBDB aromatic"},
    {"C6H6",  "C-H",   1.084, 0.010, "NIST_CCCBDB"},
    {"CH3OH", "C-O",   1.427, 0.010, "NIST_CCCBDB"},
    {"CH3OH", "O-H",   0.945, 0.010, "NIST_CCCBDB"},
    {"CH3OH", "C-H",   1.093, 0.010, "NIST_CCCBDB"},
    {"HCOOH", "C=O",   1.202, 0.010, "NIST_CCCBDB"},
    {"HCOOH", "C-O",   1.343, 0.010, "NIST_CCCBDB"},
    {"CH3NH2","C-N",   1.471, 0.010, "NIST_CCCBDB"},
    {"HCN",   "C#N",   1.156, 0.005, "NIST_CCCBDB"},
    {"HCN",   "C-H",   1.065, 0.005, "NIST_CCCBDB"},
    {"CO2",   "C=O",   1.160, 0.005, "NIST_CCCBDB"},
    {"CS2",   "C=S",   1.554, 0.010, "NIST_CCCBDB"},
    {"SO2",   "S=O",   1.432, 0.010, "NIST_CCCBDB"},
    {"NO2",   "N=O",   1.197, 0.010, "NIST_CCCBDB"},
    {"N2O",   "N=N",   1.128, 0.010, "NIST_CCCBDB"},
    {"N2O",   "N=O",   1.184, 0.010, "NIST_CCCBDB"},
    {"OF2",   "O-F",   1.405, 0.010, "NIST_CCCBDB"},
    {"ClF",   "Cl-F",  1.628, 0.010, "NIST_CCCBDB"},
    {"CH3F",  "C-F",   1.382, 0.010, "NIST_CCCBDB"},
    {"CH3Cl", "C-Cl",  1.781, 0.010, "NIST_CCCBDB"},
    {"CH3Br", "C-Br",  1.934, 0.015, "NIST_CCCBDB"},
    {"CH3I",  "C-I",   2.136, 0.015, "NIST_CCCBDB"},
    {"CH3SH", "C-S",   1.819, 0.015, "NIST_CCCBDB"},
    {"CH3SH", "S-H",   1.335, 0.010, "NIST_CCCBDB"},
};
inline constexpr int N_BOND_REFS = (int)(sizeof(BOND_REFS)/sizeof(BOND_REFS[0]));

// ---- Bond angles (degrees) --------------------------------------------------
inline const AngleRef ANGLE_REFS[] = {
    {"H2O",   "H-O-H",  104.45, 1.0, "NIST_CCCBDB"},
    {"H2S",   "H-S-H",   92.12, 1.5, "NIST_CCCBDB"},
    {"H2Se",  "H-Se-H",  91.0,  2.0, "NIST_CCCBDB"},
    {"H2Te",  "H-Te-H",  89.5,  2.0, "NIST_CCCBDB"},
    {"SO2",   "O-S-O",  119.5,  1.0, "NIST_CCCBDB"},
    {"NO2",   "O-N-O",  134.1,  1.5, "NIST_CCCBDB"},
    {"OF2",   "F-O-F",  103.1,  1.5, "NIST_CCCBDB"},
    {"CO2",   "O-C-O",  180.0,  0.5, "NIST_CCCBDB linear"},
    {"CS2",   "S-C-S",  180.0,  0.5, "NIST_CCCBDB linear"},
    {"HCN",   "H-C-N",  180.0,  0.5, "NIST_CCCBDB linear"},
    {"N2O",   "N-N-O",  180.0,  0.5, "NIST_CCCBDB linear"},
    {"NH3",   "H-N-H",  106.67, 1.0, "NIST_CCCBDB"},
    {"PH3",   "H-P-H",   93.35, 1.5, "NIST_CCCBDB"},
    {"AsH3",  "H-As-H",  91.8,  2.0, "NIST_CCCBDB"},
    {"CH4",   "H-C-H",  109.47, 0.5, "tetrahedral exact"},
    {"SiH4",  "H-Si-H", 109.47, 0.5, "tetrahedral exact"},
    {"GeH4",  "H-Ge-H", 109.47, 0.5, "tetrahedral exact"},
    {"C2H4",  "H-C-H",  117.4,  2.0, "NIST_CCCBDB"},
    {"C2H4",  "H-C-C",  121.3,  2.0, "NIST_CCCBDB"},
    {"C6H6",  "C-C-C",  120.0,  1.0, "NIST_CCCBDB D6h"},
    {"C6H6",  "H-C-C",  120.0,  1.0, "NIST_CCCBDB"},
    {"CH3OH", "H-O-C",  108.9,  2.0, "NIST_CCCBDB"},
    {"CH3OH", "H-C-O",  107.2,  2.0, "NIST_CCCBDB"},
    {"CH3NH2","H-N-H",  106.0,  2.0, "NIST_CCCBDB"},
    {"CH3NH2","H-C-N",  110.3,  2.0, "NIST_CCCBDB"},
};
inline constexpr int N_ANGLE_REFS = (int)(sizeof(ANGLE_REFS)/sizeof(ANGLE_REFS[0]));

// ---- Crystal structures (Wyckoff) ------------------------------------------
inline const CrystalRef CRYSTAL_REFS[] = {
    {"Al",  "FCC",       4.050,0.02, 4, 2.863,12,"WYCKOFF"},
    {"Cu",  "FCC",       3.615,0.02, 4, 2.556,12,"WYCKOFF"},
    {"Au",  "FCC",       4.078,0.02, 4, 2.884,12,"WYCKOFF"},
    {"Ag",  "FCC",       4.086,0.02, 4, 2.889,12,"WYCKOFF"},
    {"Ni",  "FCC",       3.524,0.02, 4, 2.492,12,"WYCKOFF"},
    {"Pb",  "FCC",       4.951,0.05, 4, 3.501,12,"WYCKOFF"},
    {"Pt",  "FCC",       3.924,0.02, 4, 2.775,12,"WYCKOFF"},
    {"Pd",  "FCC",       3.890,0.02, 4, 2.750,12,"WYCKOFF"},
    {"Ca",  "FCC",       5.585,0.05, 4, 3.949,12,"WYCKOFF"},
    {"Fe",  "BCC",       2.870,0.02, 2, 2.485, 8,"WYCKOFF"},
    {"W",   "BCC",       3.165,0.02, 2, 2.741, 8,"WYCKOFF"},
    {"Mo",  "BCC",       3.147,0.02, 2, 2.726, 8,"WYCKOFF"},
    {"Cr",  "BCC",       2.884,0.02, 2, 2.498, 8,"WYCKOFF"},
    {"V",   "BCC",       3.030,0.02, 2, 2.625, 8,"WYCKOFF"},
    {"Na",  "BCC",       4.225,0.05, 2, 3.659, 8,"WYCKOFF"},
    {"K",   "BCC",       5.332,0.05, 2, 4.619, 8,"WYCKOFF"},
    {"NaCl","rocksalt",  5.640,0.02, 8, 2.820, 6,"WYCKOFF"},
    {"KCl", "rocksalt",  6.293,0.02, 8, 3.147, 6,"WYCKOFF"},
    {"MgO", "rocksalt",  4.212,0.02, 8, 2.106, 6,"WYCKOFF"},
    {"LiF", "rocksalt",  4.017,0.02, 8, 2.009, 6,"WYCKOFF"},
    {"NaF", "rocksalt",  4.634,0.02, 8, 2.317, 6,"WYCKOFF"},
    {"CaO", "rocksalt",  4.810,0.02, 8, 2.405, 6,"WYCKOFF"},
    {"Si",  "diamond",   5.431,0.02, 8, 2.352, 4,"WYCKOFF"},
    {"Ge",  "diamond",   5.658,0.02, 8, 2.450, 4,"WYCKOFF"},
    {"C",   "diamond",   3.567,0.02, 8, 1.545, 4,"WYCKOFF"},
    {"CsCl","CsCl-type", 4.123,0.05, 2, 3.571, 8,"WYCKOFF"},
    {"CsBr","CsCl-type", 4.286,0.05, 2, 3.714, 8,"WYCKOFF"},
    {"Mg",  "HCP",       3.209,0.05, 2, 3.209,12,"WYCKOFF a"},
    {"Ti",  "HCP",       2.951,0.05, 2, 2.951,12,"WYCKOFF"},
    {"Zn",  "HCP",       2.665,0.05, 2, 2.665,12,"WYCKOFF"},
};
inline constexpr int N_CRYSTAL_REFS = (int)(sizeof(CRYSTAL_REFS)/sizeof(CRYSTAL_REFS[0]));

// ---- LJ parameters -- UFF full periodic table (Rappe 1992 Table I) ---------
inline const LJRef LJ_REFS[] = {
    { 1,"H",  2.886,0.044, 2.886*1.122462}, { 2,"He",2.362,0.056, 2.362*1.122462},
    { 3,"Li", 2.451,0.025, 2.451*1.122462}, { 4,"Be",2.745,0.085, 2.745*1.122462},
    { 5,"B",  4.083,0.180, 4.083*1.122462}, { 6,"C", 3.851,0.105, 3.851*1.122462},
    { 7,"N",  3.660,0.069, 3.660*1.122462}, { 8,"O", 3.500,0.060, 3.500*1.122462},
    { 9,"F",  3.364,0.050, 3.364*1.122462}, {10,"Ne",3.243,0.042, 3.243*1.122462},
    {11,"Na", 3.328,0.030, 3.328*1.122462}, {12,"Mg",3.021,0.111, 3.021*1.122462},
    {13,"Al", 4.499,0.505, 4.499*1.122462}, {14,"Si",4.295,0.402, 4.295*1.122462},
    {15,"P",  4.147,0.305, 4.147*1.122462}, {16,"S", 4.035,0.274, 4.035*1.122462},
    {17,"Cl", 3.947,0.227, 3.947*1.122462}, {18,"Ar",3.400,0.238, 3.400*1.122462},
    {19,"K",  3.812,0.035, 3.812*1.122462}, {20,"Ca",3.399,0.238, 3.399*1.122462},
    {22,"Ti", 2.828,0.017, 2.828*1.122462}, {23,"V", 2.800,0.016, 2.800*1.122462},
    {24,"Cr", 2.693,0.015, 2.693*1.122462}, {25,"Mn",2.638,0.013, 2.638*1.122462},
    {26,"Fe", 2.912,0.013, 2.912*1.122462}, {27,"Co",2.872,0.014, 2.872*1.122462},
    {28,"Ni", 2.834,0.015, 2.834*1.122462}, {29,"Cu",3.495,0.005, 3.495*1.122462},
    {30,"Zn", 2.763,0.124, 2.763*1.122462}, {35,"Br",4.189,0.251, 4.189*1.122462},
    {36,"Kr", 3.900,0.220, 3.900*1.122462}, {42,"Mo",2.719,0.056, 2.719*1.122462},
    {46,"Pd", 2.582,0.048, 2.582*1.122462}, {47,"Ag",2.804,0.036, 2.804*1.122462},
    {50,"Sn", 3.912,0.567, 3.912*1.122462}, {53,"I", 4.009,0.339, 4.009*1.122462},
    {54,"Xe", 4.404,0.332, 4.404*1.122462}, {55,"Cs",4.517,0.045, 4.517*1.122462},
    {74,"W",  2.734,0.067, 2.734*1.122462}, {78,"Pt",2.754,0.080, 2.754*1.122462},
    {79,"Au", 3.293,0.039, 3.293*1.122462}, {82,"Pb",3.828,0.663, 3.828*1.122462},
};
inline constexpr int N_LJ_REFS = (int)(sizeof(LJ_REFS)/sizeof(LJ_REFS[0]));

// ---- Noble gas dimers (gas-phase experimental) ------------------------------
inline const NobleGasRef NOBLE_GAS_REFS[] = {
    {"He", 2,  10.8, 2.569,"CRC_104 Aziz1995"},
    {"Ne",10,  35.8, 2.782,"CRC_104 Aziz1993"},
    {"Ar",18, 143.2, 3.405,"CRC_104 Aziz1986"},
    {"Kr",36, 200.3, 3.600,"CRC_104 Dham1989"},
    {"Xe",54, 281.0, 3.960,"CRC_104 Aziz1986"},
};
inline constexpr int N_NOBLE_GAS_REFS = (int)(sizeof(NOBLE_GAS_REFS)/sizeof(NOBLE_GAS_REFS[0]));

// ---- Diatomic spectroscopic constants (NIST WebBook) -----------------------
inline const DiatomicRef DIATOMIC_REFS[] = {
    {"H2",  "H-H",  0.7414,103.26,4401.2,"NIST_WB"},
    {"N2",  "N=N",  1.0977,225.1, 2358.6,"NIST_WB"},
    {"O2",  "O=O",  1.2075,117.96,1580.2,"NIST_WB"},
    {"F2",  "F-F",  1.4119, 37.0,  916.6,"NIST_WB"},
    {"Cl2", "Cl-Cl",1.9878, 57.1,  559.7,"NIST_WB"},
    {"HF",  "H-F",  0.9169,135.1, 4138.3,"NIST_WB"},
    {"HCl", "H-Cl", 1.2746,102.2, 2990.9,"NIST_WB"},
    {"CO",  "C=O",  1.1283,255.8, 2169.8,"NIST_WB"},
    {"NO",  "N=O",  1.1508,150.1, 1876.4,"NIST_WB"},
    {"NaCl","Na-Cl",2.3609, 98.0,  366.0,"NIST_WB"},
};
inline constexpr int N_DIATOMIC_REFS = (int)(sizeof(DIATOMIC_REFS)/sizeof(DIATOMIC_REFS[0]));

// ---- Solvents (CRC 104, 25 C) -----------------------------------------------
inline const SolventRef SOLVENT_REFS[] = {
    {"vacuum",          1.000,   0.0,"exact"},
    {"water",          78.400,298.15,"CRC_104"},
    {"methanol",       32.660,298.15,"CRC_104"},
    {"ethanol",        24.850,298.15,"CRC_104"},
    {"acetone",        20.700,298.15,"CRC_104"},
    {"DMSO",           46.700,298.15,"CRC_104"},
    {"DMF",            36.700,298.15,"CRC_104"},
    {"acetonitrile",   35.690,298.15,"CRC_104"},
    {"chloroform",      4.810,298.15,"CRC_104"},
    {"dichloromethane", 8.930,298.15,"CRC_104"},
    {"THF",             7.580,298.15,"CRC_104"},
    {"toluene",         2.379,298.15,"CRC_104"},
    {"hexane",          1.882,298.15,"CRC_104"},
    {"cyclohexane",     2.028,298.15,"CRC_104"},
    {"benzene",         2.274,298.15,"CRC_104"},
    {"diethyl_ether",   4.266,298.15,"CRC_104"},
    {"formic_acid",    51.100,298.15,"CRC_104"},
    {"glycerol",       46.530,298.15,"CRC_104"},
    {"ammonia",        16.900,240.0, "CRC_104 liq240K"},
};
inline constexpr int N_SOLVENT_REFS = (int)(sizeof(SOLVENT_REFS)/sizeof(SOLVENT_REFS[0]));

// ---- Ions (Shannon radii, CRC 104, CN=6) ------------------------------------
inline const IonRef ION_REFS[] = {
    {"Li+", 3,+1.0,0.760,"CRC_104 CN6"}, {"Na+",11,+1.0,1.020,"CRC_104 CN6"},
    {"K+", 19,+1.0,1.380,"CRC_104 CN6"}, {"Rb+",37,+1.0,1.520,"CRC_104 CN6"},
    {"Cs+",55,+1.0,1.670,"CRC_104 CN6"}, {"Mg2+",12,+2.0,0.720,"CRC_104 CN6"},
    {"Ca2+",20,+2.0,1.000,"CRC_104 CN6"},{"Ba2+",56,+2.0,1.350,"CRC_104 CN6"},
    {"F-",  9,-1.0,1.330,"CRC_104 CN6"}, {"Cl-",17,-1.0,1.810,"CRC_104 CN6"},
    {"Br-",35,-1.0,1.960,"CRC_104 CN6"}, {"I-", 53,-1.0,2.200,"CRC_104 CN6"},
    {"O2-", 8,-2.0,1.400,"CRC_104 CN6"}, {"S2-",16,-2.0,1.840,"CRC_104 CN6"},
    {"N3-", 7,-3.0,1.460,"CRC_104 CN6"},
};
inline constexpr int N_ION_REFS = (int)(sizeof(ION_REFS)/sizeof(ION_REFS[0]));

// ---- Lookup helpers ---------------------------------------------------------
inline std::optional<CrystalRef> find_crystal(const char* n) {
    for(int i=0;i<N_CRYSTAL_REFS;++i) if(std::string(CRYSTAL_REFS[i].name)==n) return CRYSTAL_REFS[i];
    return std::nullopt; }
inline std::optional<LJRef> find_lj(int Z) {
    for(int i=0;i<N_LJ_REFS;++i) if(LJ_REFS[i].Z==Z) return LJ_REFS[i];
    return std::nullopt; }
inline std::optional<LJRef> find_lj(const char* s) {
    for(int i=0;i<N_LJ_REFS;++i) if(std::string(LJ_REFS[i].symbol)==s) return LJ_REFS[i];
    return std::nullopt; }
inline std::optional<SolventRef> find_solvent(const char* n) {
    for(int i=0;i<N_SOLVENT_REFS;++i) if(std::string(SOLVENT_REFS[i].name)==n) return SOLVENT_REFS[i];
    return std::nullopt; }

} // namespace empirical
