#pragma once
/**
 * uff_params.hpp - Universal Force Field Parameter Database
 * 
 * Purpose: Centralized LJ parameter storage for all nonbonded potentials
 * 
 * Source: Rappe et al. (1992) "UFF, a full periodic table force field for 
 *         molecular mechanics and molecular dynamics simulations"
 *         J. Am. Chem. Soc. 114(25), 10024-10035
 * 
 * Used by:
 * - atomistic/models/lj_coulomb.cpp (MD mode: full LJ + Coulomb)
 * - src/pot/energy_nonbonded.hpp (VSEPR mode: WCA repulsion-only)
 * 
 * Note: Name is "UFF" for historical reasons, but this is a generic parameter DB.
 *       Add other force field parameters here as needed (OPLS, AMBER, etc.)
 */

#include <map>
#include <optional>
#include <cstdint>
#include <cmath>
#include <stdexcept>

namespace vsepr {

/**
 * Per-element LJ parameters
 */
struct LJParams {
    double sigma;    // Å (collision diameter)
    double epsilon;  // kcal/mol (well depth)
};

/**
 * UFF parameters indexed by atomic number (Z)
 * 
 * These are r* and D* from UFF paper (Table II).
 * - sigma = r* (zero-crossing distance)
 * - epsilon = D* (well depth)
 * 
 * For pair interactions, use Lorentz-Berthelot mixing rules.
 */
static const std::map<int, LJParams> UFF_PARAMS_BY_Z = {
    // Period 1
    {1,  {2.886, 0.044}},   // H
    {2,  {2.362, 0.056}},   // He
    // Period 2
    {3,  {2.451, 0.025}},   // Li
    {4,  {2.745, 0.085}},   // Be
    {5,  {4.083, 0.180}},   // B
    {6,  {3.851, 0.105}},   // C
    {7,  {3.660, 0.069}},   // N
    {8,  {3.500, 0.060}},   // O
    {9,  {3.364, 0.050}},   // F
    {10, {3.243, 0.042}},   // Ne
    // Period 3
    {11, {3.328, 0.030}},   // Na
    {12, {3.021, 0.111}},   // Mg
    {13, {4.499, 0.505}},   // Al
    {14, {4.295, 0.402}},   // Si
    {15, {4.147, 0.305}},   // P
    {16, {4.035, 0.274}},   // S
    {17, {3.947, 0.227}},   // Cl
    {18, {3.400, 0.238}},   // Ar
    // Period 4
    {19, {3.812, 0.035}},   // K
    {20, {3.399, 0.238}},   // Ca
    {21, {2.936, 0.019}},   // Sc
    {22, {2.828, 0.017}},   // Ti
    {23, {2.800, 0.016}},   // V
    {24, {2.693, 0.015}},   // Cr
    {25, {2.638, 0.013}},   // Mn
    {26, {2.912, 0.013}},   // Fe
    {27, {2.872, 0.014}},   // Co
    {28, {2.834, 0.015}},   // Ni
    {29, {3.495, 0.005}},   // Cu
    {30, {2.763, 0.124}},   // Zn
    {31, {4.383, 0.415}},   // Ga
    {32, {4.280, 0.379}},   // Ge
    {33, {4.230, 0.309}},   // As
    {34, {4.205, 0.291}},   // Se
    {35, {4.189, 0.251}},   // Br
    {36, {3.900, 0.220}},   // Kr
    // Period 5 (selected)
    {37, {4.114, 0.040}},   // Rb
    {38, {3.641, 0.235}},   // Sr
    {39, {2.980, 0.072}},   // Y
    {40, {2.783, 0.069}},   // Zr
    {41, {2.820, 0.059}},   // Nb
    {42, {2.719, 0.056}},   // Mo
    {43, {2.670, 0.048}},   // Tc
    {44, {2.683, 0.056}},   // Ru
    {45, {2.634, 0.053}},   // Rh
    {46, {2.582, 0.048}},   // Pd
    {47, {2.804, 0.036}},   // Ag
    {48, {2.537, 0.228}},   // Cd
    {49, {3.976, 0.599}},   // In
    {50, {3.912, 0.567}},   // Sn
    {51, {3.937, 0.449}},   // Sb
    {52, {3.982, 0.398}},   // Te
    {53, {4.009, 0.339}},   // I
    {54, {4.404, 0.332}},   // Xe
    {55, {4.517, 0.045}},   // Cs
    // Period 6 (selected)
    {56, {3.703, 0.364}},   // Ba
    {72, {2.798, 0.072}},   // Hf
    {73, {2.824, 0.081}},   // Ta
    {74, {2.734, 0.067}},   // W
    {75, {2.631, 0.066}},   // Re
    {76, {2.779, 0.037}},   // Os
    {77, {2.530, 0.073}},   // Ir
    {78, {2.754, 0.080}},   // Pt
    {79, {3.293, 0.039}},   // Au
    {80, {2.705, 0.385}},   // Hg
    {81, {3.873, 0.680}},   // Tl
    {82, {3.828, 0.663}},   // Pb
    {83, {3.893, 0.518}},   // Bi
    {84, {4.195, 0.325}},   // Po
};

/**
 * Lookup LJ parameters for atomic number Z
 * 
 * @param Z Atomic number (1-118)
 * @return LJParams if defined, std::nullopt otherwise
 */
inline std::optional<LJParams> get_lj_params(int Z) {
    auto it = UFF_PARAMS_BY_Z.find(Z);
    if (it != UFF_PARAMS_BY_Z.end()) {
        return it->second;
    }
    return std::nullopt;
}

/**
 * Lookup LJ parameters with fallback to carbon
 * 
 * @param Z Atomic number (1-118)
 * @return LJParams (uses carbon if Z not defined)
 */
inline LJParams get_lj_params_or_carbon(int Z) {
    auto params = get_lj_params(Z);
    if (params) {
        return *params;
    }
    // Fallback to carbon-like parameters
    return {3.851, 0.105};
}

/**
 * Lorentz-Berthelot mixing rules for pair interactions
 * 
 * Given parameters for atoms i and j, compute mixed parameters:
 * - sigma_ij = (sigma_i + sigma_j) / 2    [arithmetic mean]
 * - epsilon_ij = sqrt(epsilon_i * epsilon_j)  [geometric mean]
 * 
 * @param params_i LJ parameters for atom i
 * @param params_j LJ parameters for atom j
 * @return Mixed LJ parameters for pair i-j
 */
inline LJParams lorentz_berthelot_mix(const LJParams& params_i, const LJParams& params_j) {
    return {
        (params_i.sigma + params_j.sigma) / 2.0,           // Arithmetic mean
        std::sqrt(params_i.epsilon * params_j.epsilon)     // Geometric mean
    };
}

/**
 * Check if atomic number Z has defined parameters
 */
inline bool has_lj_params(int Z) {
    return UFF_PARAMS_BY_Z.find(Z) != UFF_PARAMS_BY_Z.end();
}

} // namespace vsepr
