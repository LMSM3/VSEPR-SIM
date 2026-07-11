// valence_tables.hpp — Deterministic valence and electronegativity lookup tables
// Data-driven (no hardcoded Z arrays), constexpr where possible
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace vsepr::chem {

struct ValenceEntry {
    std::uint8_t Z;
    std::string_view symbol;
    std::uint8_t max_valence;
    std::uint8_t common_valences[4]; // up to 4 common valence states, 0-terminated
    double electronegativity;        // Pauling scale
    double covalent_radius_pm;
    double vdw_radius_pm;
    double mass_u;
};

// Organic-relevant elements (H, C, N, O, S, P, F, Cl, Br, I, Se, B, Si)
inline constexpr std::array<ValenceEntry, 14> ORGANIC_VALENCE_TABLE {{
    { 1, "H",   1, {1,0,0,0},   2.20,   31.0,  120.0,   1.008},
    { 5, "B",   3, {3,0,0,0},   2.04,   84.0,  192.0,  10.81 },
    { 6, "C",   4, {4,2,0,0},   2.55,   76.0,  170.0,  12.011},
    { 7, "N",   4, {3,2,1,0},   3.04,   71.0,  155.0,  14.007},
    { 8, "O",   2, {2,1,0,0},   3.44,   66.0,  152.0,  15.999},
    { 9, "F",   1, {1,0,0,0},   3.98,   57.0,  147.0,  18.998},
    {14, "Si",  4, {4,2,0,0},   1.90,  111.0,  210.0,  28.086},
    {15, "P",   5, {5,3,0,0},   2.19,  107.0,  180.0,  30.974},
    {16, "S",   6, {6,4,2,0},   2.58,  105.0,  180.0,  32.06 },
    {17, "Cl",  7, {1,3,5,7},   3.16,  102.0,  175.0,  35.45 },
    {34, "Se",  6, {6,4,2,0},   2.55,  120.0,  190.0,  78.96 },
    {35, "Br",  7, {1,3,5,0},   2.96,  120.0,  185.0,  79.90 },
    {53, "I",   7, {1,3,5,7},   2.66,  139.0,  198.0, 126.90 },
    {26, "Fe",  6, {2,3,0,0},   1.83,  132.0,  204.0,  55.845},
}};

inline constexpr const ValenceEntry* lookup_valence(std::uint8_t Z) noexcept {
    for (const auto& e : ORGANIC_VALENCE_TABLE) {
        if (e.Z == Z) return &e;
    }
    return nullptr;
}

inline constexpr bool is_valid_valence(std::uint8_t Z, std::uint8_t observed_bonds) noexcept {
    const auto* entry = lookup_valence(Z);
    if (!entry) return false;
    if (observed_bonds > entry->max_valence) return false;
    for (auto cv : entry->common_valences) {
        if (cv == 0) break;
        if (cv == observed_bonds) return true;
    }
    return false;
}

// Standard peptide bond geometry (IUPAC reference values)
inline constexpr double PEPTIDE_BOND_LENGTH_A     = 1.33;   // C-N amide bond (Angstrom)
inline constexpr double PEPTIDE_BOND_ANGLE_CAN    = 121.7;  // C(=O)-N-CA angle (degrees)
inline constexpr double CA_C_BOND_LENGTH_A        = 1.52;   // CA-C bond
inline constexpr double C_O_BOND_LENGTH_A         = 1.23;   // C=O carbonyl
inline constexpr double N_CA_BOND_LENGTH_A        = 1.47;   // N-CA bond
inline constexpr double OMEGA_TRANS_DEG           = 180.0;  // trans peptide bond
inline constexpr double OMEGA_CIS_DEG             = 0.0;    // cis (rare, Pro)
inline constexpr double PLANARITY_TOLERANCE_DEG   = 10.0;   // amide planarity threshold

} // namespace vsepr::chem
