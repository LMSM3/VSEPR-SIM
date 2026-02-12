#pragma once

#include "core/types.hpp"
#include "core/math_vec3.hpp"
#include "core/geom_ops.hpp"
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace vsepr {

// VSEPR parameters
struct VSEPRParams {
    double w_LP_LP = 2.0;   // Lone pair - lone pair weight
    double w_LP_BP = 1.5;   // Lone pair - bond pair weight  
    double w_BP_BP = 1.0;   // Bond pair - bond pair weight
    double p = 1.5;         // Stiffness exponent
    double epsilon = 0.01;  // Regularization to prevent singularity
    double k_vsepr = 50.0;  // Overall strength (kcal/mol)
    double r0_lp = 0.5;     // Virtual site distance from center (Å)
};

// Domain descriptor: bond or lone pair on a central atom
struct Domain {
    uint32_t central_atom;  // Central atom index
    bool is_lone_pair;      // True for LP, false for BP
    uint32_t partner_atom;  // For BP: bonded atom. For LP: LP direction index
};

// VSEPR energy using explicit electron domain repulsion
// Lone pairs represented as virtual sites: r_lp = r_center + r0 * u
// where u is a unit vector optimized in extended coordinate space
class VSEPREnergy {
public:
    VSEPREnergy(const std::vector<Atom>& atoms,
                const std::vector<Bond>& bonds,
                const VSEPRParams& params = {})
        : atoms_(atoms), bonds_(bonds), params_(params)
    {
        build_domains();
    }
    
    // Evaluate energy and gradient
    // coords: [atom coords (3*N), lone_pair unit vectors (3*N_LP)]
    // Lone pair positions computed as: r_lp = r_center + r0 * u
    double evaluate(const std::vector<double>& coords,
                   std::vector<double>& gradient) const
    {
        size_t N_atoms = atoms_.size();
        size_t N_LP_total = count_total_lone_pairs();
        
        if (coords.size() != 3 * N_atoms + 3 * N_LP_total) {
            throw std::runtime_error("VSEPR: coordinate size mismatch");
        }
        
        // Zero gradient
        std::fill(gradient.begin(), gradient.end(), 0.0);
        
        double E_total = 0.0;
        
        // For each central atom with domains
        for (const auto& [central, doms] : domains_by_atom_) {
            if (doms.size() < 2) continue;  // Need at least 2 domains
            
            Vec3 r_central = get_pos(coords, central);
            
            // Get domain vectors
            std::vector<Vec3> domain_vecs;
            std::vector<bool> is_LP;
            std::vector<uint32_t> indices;  // Partner atom or LP index
            
            for (const auto& dom : doms) {
                if (dom.is_lone_pair) {
                    // Lone pair: virtual site at r_center + r0 * u
                    size_t lp_idx = dom.partner_atom;
                    Vec3 u = get_lp_direction(coords, lp_idx);
                    Vec3 lp_vec = u.normalized();  // Should already be normalized
                    domain_vecs.push_back(lp_vec);
                    is_LP.push_back(true);
                    indices.push_back(lp_idx);
                } else {
                    // Bond: direction to partner atom
                    Vec3 r_partner = get_pos(coords, dom.partner_atom);
                    Vec3 bond_dir = (r_partner - r_central).normalized();
                    domain_vecs.push_back(bond_dir);
                    is_LP.push_back(false);
                    indices.push_back(dom.partner_atom);
                }
            }
            
            // Pairwise domain repulsion
            for (size_t a = 0; a < domain_vecs.size(); ++a) {
                for (size_t b = a + 1; b < domain_vecs.size(); ++b) {
                    Vec3 d_a = domain_vecs[a];
                    Vec3 d_b = domain_vecs[b];
                    
                    double cos_theta = d_a.dot(d_b);
                    cos_theta = std::clamp(cos_theta, -1.0, 1.0);
                    
                    // Select weight based on domain types
                    double w = params_.w_BP_BP;
                    if (is_LP[a] && is_LP[b]) {
                        w = params_.w_LP_LP;
                    } else if (is_LP[a] || is_LP[b]) {
                        w = params_.w_LP_BP;
                    }
                    
                    // Energy: w / [ε + (1 - cos θ)]^p
                    double denom = params_.epsilon + (1.0 - cos_theta);
                    double E_pair = params_.k_vsepr * w / std::pow(denom, params_.p);
                    E_total += E_pair;
                    
                    // Gradient: dE/d(cos θ) = w * p * (1 - cos θ)^(-p-1)
                    double dE_dcos = params_.k_vsepr * w * params_.p / std::pow(denom, params_.p + 1.0);
                    
                    // d(cos θ)/d(d_a) = d_b - d_a * (d_a · d_b)
                    // d(cos θ)/d(d_b) = d_a - d_b * (d_a · d_b)
                    Vec3 dcos_dda = d_b - d_a * cos_theta;
                    Vec3 dcos_ddb = d_a - d_b * cos_theta;
                    
                    Vec3 grad_a = dcos_dda * dE_dcos;
                    Vec3 grad_b = dcos_ddb * dE_dcos;
                    
                    // Accumulate gradients
                    if (is_LP[a]) {
                        // Gradient w.r.t. LP direction u
                        // r_lp = r_center + r0*u, normalized direction is just u
                        accumulate_lp_grad(gradient, indices[a], grad_a);
                        // No gradient on central atom for LP direction change
                    } else {
                        // Gradient w.r.t. bond (affects both atoms)
                        Vec3 r_partner = get_pos(coords, indices[a]);
                        double r_ab = (r_partner - r_central).norm();
                        if (r_ab > 1e-12) {
                            Vec3 g_partner = grad_a / r_ab;
                            Vec3 g_central = -g_partner;
                            accumulate_grad(gradient, indices[a], g_partner);
                            accumulate_grad(gradient, central, g_central);
                        }
                    }
                    
                    if (is_LP[b]) {
                        accumulate_lp_grad(gradient, indices[b], grad_b);
                    } else {
                        Vec3 r_partner = get_pos(coords, indices[b]);
                        double r_ab = (r_partner - r_central).norm();
                        if (r_ab > 1e-12) {
                            Vec3 g_partner = grad_b / r_ab;
                            Vec3 g_central = -g_partner;
                            accumulate_grad(gradient, indices[b], g_partner);
                            accumulate_grad(gradient, central, g_central);
                        }
                    }
                }
            }
        }
        
        return E_total;
    }
    
    // Normalize all lone pair direction vectors
    // Call after each optimizer step to enforce |u| = 1 constraint
    void normalize_lone_pairs(std::vector<double>& coords) const {
        size_t N_atoms = atoms_.size();
        size_t N_LP = count_total_lone_pairs();
        
        for (size_t i = 0; i < N_LP; ++i) {
            size_t offset = 3 * N_atoms + 3 * i;
            Vec3 u{coords[offset], coords[offset + 1], coords[offset + 2]};
            double norm = u.norm();
            
            if (norm > 1e-12) {
                u /= norm;
                coords[offset + 0] = u.x;
                coords[offset + 1] = u.y;
                coords[offset + 2] = u.z;
            } else {
                // Degenerate case: reinitialize to random direction
                double theta = (i + 0.5) * M_PI / (N_LP + 1);
                double phi = i * 2.0 * M_PI / (N_LP + 1);
                coords[offset + 0] = std::sin(theta) * std::cos(phi);
                coords[offset + 1] = std::sin(theta) * std::sin(phi);
                coords[offset + 2] = std::cos(theta);
            }
        }
    }
    
    // Get total number of lone pairs across all atoms
    size_t count_total_lone_pairs() const {
        size_t count = 0;
        for (const auto& atom : atoms_) {
            count += atom.lone_pairs;
        }
        return count;
    }
    
    // Get lone pair direction vector u (unit vector)
    Vec3 get_lp_direction(const std::vector<double>& coords, size_t lp_idx) const {
        size_t N_atoms = atoms_.size();
        size_t offset = 3 * N_atoms + 3 * lp_idx;
        return {coords[offset], coords[offset + 1], coords[offset + 2]};
    }
    
    // Accumulate gradient into lone pair direction coordinates
    void accumulate_lp_grad(std::vector<double>& grad, size_t lp_idx, const Vec3& g) const {
        size_t N_atoms = atoms_.size();
        size_t offset = 3 * N_atoms + 3 * lp_idx;
        grad[offset + 0] += g.x;
        grad[offset + 1] += g.y;
        grad[offset + 2] += g.z;
    }
    
    // Initialize lone pair directions (random on sphere)
    void initialize_lone_pair_directions(std::vector<double>& coords) const {
        size_t N_atoms = atoms_.size();
        size_t N_LP = count_total_lone_pairs();
        
        coords.resize(3 * N_atoms + 3 * N_LP);
        
        // Initialize LP directions to random unit vectors
        for (size_t i = 0; i < N_LP; ++i) {
            // Simple initialization: spread around sphere
            double theta = (i + 0.5) * M_PI / (N_LP + 1);
            double phi = i * 2.0 * M_PI / (N_LP + 1);
            
            size_t idx = 3 * N_atoms + 3 * i;
            coords[idx + 0] = std::sin(theta) * std::cos(phi);
            coords[idx + 1] = std::sin(theta) * std::sin(phi);
            coords[idx + 2] = std::cos(theta);
        }
    }
    
private:
    const std::vector<Atom>& atoms_;
    const std::vector<Bond>& bonds_;
    VSEPRParams params_;
    
    // Domains grouped by central atom
    std::map<uint32_t, std::vector<Domain>> domains_by_atom_;
    
    // Mapping from atom to its lone pair coordinate indices
    std::map<uint32_t, std::vector<uint32_t>> atom_to_lp_indices_;
    
    void build_domains() {
        // Build neighbor lists
        std::vector<std::vector<uint32_t>> neighbors(atoms_.size());
        for (const auto& bond : bonds_) {
            neighbors[bond.i].push_back(bond.j);
            neighbors[bond.j].push_back(bond.i);
        }
        
        uint32_t lp_coord_idx = 0;
        
        // For each atom, create domains
        for (size_t i = 0; i < atoms_.size(); ++i) {
            std::vector<Domain> doms;
            
            // Bond pair domains
            for (uint32_t neighbor : neighbors[i]) {
                doms.push_back({static_cast<uint32_t>(i), false, neighbor});
            }
            
            // Lone pair domains
            for (uint8_t lp = 0; lp < atoms_[i].lone_pairs; ++lp) {
                doms.push_back({static_cast<uint32_t>(i), true, lp_coord_idx});
                atom_to_lp_indices_[i].push_back(lp_coord_idx);
                lp_coord_idx++;
            }
            
            if (doms.size() >= 2) {
                domains_by_atom_[i] = doms;
            }
        }
    }
    
    Vec3 get_pos(const std::vector<double>& coords, size_t atom_idx) const {
        return {coords[3 * atom_idx], coords[3 * atom_idx + 1], coords[3 * atom_idx + 2]};
    }
    
    void accumulate_grad(std::vector<double>& grad, size_t atom_idx, const Vec3& g) const {
        grad[3 * atom_idx + 0] += g.x;
        grad[3 * atom_idx + 1] += g.y;
        grad[3 * atom_idx + 2] += g.z;
    }
};

} // namespace vsepr
