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
 * - meso/models/lj_coulomb.cpp (MD mode: full LJ + Coulomb)
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
    {1,  {2.886, 0.044}},   // H
    {6,  {3.851, 0.105}},   // C
    {7,  {3.660, 0.069}},   // N
    {8,  {3.500, 0.060}},   // O
    {9,  {3.364, 0.050}},   // F
    {11, {3.328, 0.030}},   // Na
    {12, {3.021, 0.111}},   // Mg
    {13, {4.499, 0.505}},   // Al
    {14, {4.295, 0.402}},   // Si
    {15, {4.147, 0.305}},   // P
    {16, {4.035, 0.274}},   // S
    {17, {3.947, 0.227}},   // Cl
    {18, {3.400, 0.238}},   // Ar (needed for fundamental tests!)
    {20, {3.399, 0.238}},   // Ca
    {26, {2.912, 0.013}},   // Fe
    {29, {3.495, 0.005}},   // Cu
    {30, {2.763, 0.124}},   // Zn
    {54, {4.404, 0.332}},   // Xe
    {55, {4.517, 0.045}},   // Cs
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
