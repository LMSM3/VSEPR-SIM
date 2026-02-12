#pragma once
/**
 * clash_relaxation.hpp
 * --------------------
 * Pre-optimization clash detection and resolution
 * 
 * Purpose: Fix terrible initial geometries BEFORE handing to optimizer
 * 
 * Problem: Star molecule builders can place atoms on top of each other
 * - Fibonacci sphere is "pretty" but doesn't account for varying radii
 * - Hypervalent molecules (PF5, BrF5) spawn with F-F overlaps
 * - Optimizer wastes 1000s of iterations fighting singularities
 * 
 * Solution: Quick geometric fix-up pass
 * - Detect overlaps: d_ij < 0.7 * (r_i + r_j)
 * - Push apart along separation vector
 * - 10-50 cheap iterations (no FIRE, just move apart)
 * - NOT physics, just "don't spawn atoms inside atoms"
 */

#include "core/types.hpp"
#include "pot/covalent_radii.hpp"
#include "pot/vdw_radii.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace vsepr {

// ============================================================================
// Clash Detection Parameters
// ============================================================================
struct ClashParams {
    double overlap_threshold = 0.7;   // Clash if d < threshold * (r_i + r_j)
    double push_strength = 0.3;       // Push by 30% of overlap per iteration
    int max_iterations = 50;          // Cheap iterations
    double convergence_tol = 0.01;    // Stop when max overlap < 1% of radii sum
    bool use_vdw_radii = true;        // Use VDW radii (true) or covalent (false)
    bool verbose = false;
};

// ============================================================================
// Clash Relaxation Engine
// ============================================================================
class ClashRelaxer {
public:
    ClashRelaxer(const ClashParams& params = ClashParams())
        : params_(params) {}
    
    // Main interface: fix overlaps in coordinates
    // Returns: number of iterations used
    int relax(std::vector<double>& coords, const std::vector<Atom>& atoms,
              const std::vector<Bond>& bonds) {
        
        const size_t N = atoms.size();
        if (coords.size() != 3 * N) {
            throw std::runtime_error("Coordinate size mismatch in clash relaxation");
        }
        
        // Build exclusion set (bonded pairs don't clash)
        std::vector<std::pair<uint32_t, uint32_t>> bonded_pairs;
        for (const auto& bond : bonds) {
            bonded_pairs.push_back({bond.i, bond.j});
        }
        
        int iter = 0;
        double max_overlap = 0.0;
        
        for (iter = 0; iter < params_.max_iterations; ++iter) {
            max_overlap = 0.0;
            std::vector<double> displacements(3 * N, 0.0);
            
            // Detect all overlaps
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = i + 1; j < N; ++j) {
                    // Skip bonded pairs
                    if (is_bonded(i, j, bonded_pairs)) continue;
                    
                    // Get atom radii
                    double r_i = params_.use_vdw_radii ? 
                        get_vdw_radius(atoms[i].Z) : get_covalent_radius(atoms[i].Z);
                    double r_j = params_.use_vdw_radii ? 
                        get_vdw_radius(atoms[j].Z) : get_covalent_radius(atoms[j].Z);
                    
                    // Compute distance
                    double dx = coords[3*j]   - coords[3*i];
                    double dy = coords[3*j+1] - coords[3*i+1];
                    double dz = coords[3*j+2] - coords[3*i+2];
                    double d = std::sqrt(dx*dx + dy*dy + dz*dz);
                    
                    // Check for clash
                    double r_sum = r_i + r_j;
                    double threshold_dist = params_.overlap_threshold * r_sum;
                    
                    if (d < threshold_dist && d > 1e-10) {
                        // Overlap detected!
                        double overlap = threshold_dist - d;
                        max_overlap = std::max(max_overlap, overlap / r_sum);
                        
                        // Push apart along separation vector
                        double push = params_.push_strength * overlap;
                        double norm = push / d;
                        
                        // Apply symmetric displacement (Newton's 3rd law style)
                        displacements[3*i]   -= dx * norm;
                        displacements[3*i+1] -= dy * norm;
                        displacements[3*i+2] -= dz * norm;
                        
                        displacements[3*j]   += dx * norm;
                        displacements[3*j+1] += dy * norm;
                        displacements[3*j+2] += dz * norm;
                    }
                }
            }
            
            // Apply displacements
            for (size_t i = 0; i < 3 * N; ++i) {
                coords[i] += displacements[i];
            }
            
            // Check convergence
            if (max_overlap < params_.convergence_tol) {
                if (params_.verbose) {
                    std::cout << "Clash relaxation converged in " << iter + 1 
                              << " iterations (max overlap: " << max_overlap << ")\n";
                }
                break;
            }
        }
        
        if (params_.verbose && max_overlap >= params_.convergence_tol) {
            std::cout << "Clash relaxation stopped at max iterations (" 
                      << params_.max_iterations << "), max overlap: " << max_overlap << "\n";
        }
        
        return iter + 1;
    }
    
private:
    ClashParams params_;
    
    bool is_bonded(size_t i, size_t j, 
                   const std::vector<std::pair<uint32_t, uint32_t>>& bonded_pairs) const {
        for (const auto& pair : bonded_pairs) {
            if ((pair.first == i && pair.second == j) ||
                (pair.first == j && pair.second == i)) {
                return true;
            }
        }
        return false;
    }
    
    double get_vdw_radius(uint8_t Z) const {
        // VDW radii in Angstroms (from vdw_radii.hpp)
        const double vdw_radii[] = {
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
            // Continue through periodic table...
        };
        
        if (Z > 0 && Z <= 20) {
            return vdw_radii[Z - 1];
        }
        return 2.0;  // Default fallback
    }
    
    double get_covalent_radius(uint8_t Z) const {
        // Covalent radii from vsepr_geometry.hpp
        const double covalent_radii[] = {
            0.0,   // None (0)
            0.31,  // H  (1)
            0.28,  // He (2)
            1.28,  // Li (3)
            0.96,  // Be (4)
            0.84,  // B  (5)
            0.76,  // C  (6)
            0.71,  // N  (7)
            0.66,  // O  (8)
            0.57,  // F  (9)
            0.58,  // Ne (10)
            1.66,  // Na (11)
            1.41,  // Mg (12)
            1.21,  // Al (13)
            1.11,  // Si (14)
            1.07,  // P  (15)
            1.05,  // S  (16)
            1.02,  // Cl (17)
            1.06,  // Ar (18)
            2.03,  // K  (19)
            1.76,  // Ca (20)
        };
        
        if (Z < sizeof(covalent_radii) / sizeof(covalent_radii[0])) {
            return covalent_radii[Z];
        }
        return 1.5;  // Default fallback
    }
};

// ============================================================================
// Geometry Seeding with Covalent Radii
// ============================================================================

/**
 * Compute proper bond length from covalent radii
 * 
 * r_0 = s * (r_cov(A) + r_cov(B))
 * 
 * where s ∈ [0.95, 1.10] depending on bond order and polarity
 */
inline double compute_bond_length(uint8_t Z_a, uint8_t Z_b, double scale = 1.0) {
    const double covalent_radii[] = {
        0.0,   0.31,  0.28,  1.28,  0.96,  0.84,  0.76,  0.71,  0.66,  0.57,  // 0-9
        0.58,  1.66,  1.41,  1.21,  1.11,  1.07,  1.05,  1.02,  1.06,  2.03,  // 10-19
        1.76,  1.70,  1.60,  1.53,  1.39,  1.39,  1.32,  1.26,  1.24,  1.32,  // 20-29
        1.22,  1.22,  1.20,  1.19,  1.20,  1.20,  1.16,  2.20,  1.95,  1.90,  // 30-39
        1.75,  1.64,  1.54,  1.47,  1.46,  1.42,  1.39,  1.45,  1.44,  1.42,  // 40-49
        1.39,  1.39,  1.38,  1.39,  1.40   // 50-54 (up to Xe)
    };
    
    constexpr size_t MAX_Z = sizeof(covalent_radii) / sizeof(covalent_radii[0]) - 1;
    
    double r_a = (Z_a <= MAX_Z) ? covalent_radii[Z_a] : 1.5;
    double r_b = (Z_b <= MAX_Z) ? covalent_radii[Z_b] : 1.5;
    
    return scale * (r_a + r_b);
}

/**
 * Place ligand atoms at correct distance along VSEPR direction
 * 
 * This is what the star builder should do INSTEAD of:
 *   pos = central + radius * direction  // Wrong! What is "radius"?
 * 
 * Should be:
 *   r_0 = compute_bond_length(Z_central, Z_ligand)
 *   pos = central + r_0 * direction
 */
inline std::vector<double> place_ligand_at_distance(
    const std::vector<double>& central_pos,
    const std::vector<double>& direction,  // Unit vector
    uint8_t Z_central,
    uint8_t Z_ligand,
    double scale = 1.0)
{
    double bond_length = compute_bond_length(Z_central, Z_ligand, scale);
    
    return {
        central_pos[0] + bond_length * direction[0],
        central_pos[1] + bond_length * direction[1],
        central_pos[2] + bond_length * direction[2]
    };
}

} // namespace vsepr
