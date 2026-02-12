#pragma once
/**
 * conformer_finder.hpp - Comprehensive Isomer and Conformer Discovery
 * 
 * Systematic enumeration and optimization of molecular variants:
 * 1. GEOMETRIC ISOMERS: cis/trans, fac/mer coordination complexes
 * 2. CONFORMERS: torsional rotamers (same connectivity)
 * 3. CONSTITUTIONAL ISOMERS: different bonding patterns (future)
 * 
 * New architecture features:
 * - Canonical isomer signatures (index-invariant)
 * - Symmetry-aware deduplication
 * - Separate handling of isomers vs conformers
 * - Post-optimization clustering with RMSD
 * - Early rejection of illegal/redundant variants
 * 
 * Design principles:
 * - Deterministic: same (formula + seed + flags) → same minima
 * - Stable output: sorted by energy, reproducible ordering
 * - Chemically aware: coordination rules, ring protection
 * - Multi-level deduplication: signature + geometry + energy
 */

#include "molecule.hpp"
#include "sim/optimizer.hpp"
#include "isomer_signature.hpp"
#include "isomer_generator.hpp"
#include "../core/geom_ops.hpp"
#include "../core/chemistry.hpp"
#include <vector>
#include <random>
#include <algorithm>
#include <unordered_set>
#include <map>
#include <cmath>
#include <iostream>

namespace vsepr {

//=============================================================================
// Rotatable Bond Detection
//=============================================================================

/**
 * Detect rotatable bonds for torsion randomization.
 * 
 * A bond (i-j) is rotatable if:
 * 1. Single bond (order == 1)
 * 2. Not terminal (degree > 1 for both atoms)
 * 3. Not in a ring (cycle-free)
 * 4. Has valid dihedral neighbors
 */
struct RotatableBond {
    uint32_t i, j;        // Central bond atoms
    uint32_t a, b;        // Dihedral neighbors: (a-i-j-b)
    double current_angle; // Current torsion angle (radians)
};

inline std::vector<RotatableBond> find_rotatable_bonds(const Molecule& mol) {
    std::vector<RotatableBond> rotatable;
    
    if (mol.bonds.empty()) return rotatable;
    
    // Build adjacency list for degree and neighbor lookup
    std::vector<std::vector<uint32_t>> neighbors(mol.num_atoms());
    for (const auto& bond : mol.bonds) {
        neighbors[bond.i].push_back(bond.j);
        neighbors[bond.j].push_back(bond.i);
    }
    
    // Simple ring detection: DFS to find cycles
    // For MVP: mark all bonds in small cycles (<= 8 atoms) as non-rotatable
    auto is_in_ring = [&](uint32_t i, uint32_t j) -> bool {
        // BFS from i, excluding edge i-j, see if we can reach j in <=7 steps
        std::vector<int> dist(mol.num_atoms(), -1);
        std::vector<uint32_t> queue;
        
        dist[i] = 0;
        queue.push_back(i);
        
        for (size_t idx = 0; idx < queue.size(); ++idx) {
            uint32_t curr = queue[idx];
            if (dist[curr] >= 7) break; // Stop at ring size 8
            
            for (uint32_t next : neighbors[curr]) {
                // Skip the i-j edge we're testing
                if ((curr == i && next == j) || (curr == j && next == i)) {
                    continue;
                }
                
                if (dist[next] == -1) {
                    dist[next] = dist[curr] + 1;
                    queue.push_back(next);
                    
                    // Found j via alternate path -> ring detected
                    if (next == j) {
                        return true;
                    }
                }
            }
        }
        return false;
    };
    
    // Check each bond
    for (const auto& bond : mol.bonds) {
        uint32_t i = bond.i;
        uint32_t j = bond.j;
        
        // Rule 1: Single bond only
        if (bond.order != 1) continue;
        
        // Rule 2: Not terminal
        if (neighbors[i].size() < 2 || neighbors[j].size() < 2) continue;
        
        // Rule 3: Not in ring
        if (is_in_ring(i, j)) continue;
        
        // Rule 4: Find valid dihedral neighbors
        // Pick first non-j neighbor of i, first non-i neighbor of j
        uint32_t a = UINT32_MAX;
        for (uint32_t n : neighbors[i]) {
            if (n != j) { a = n; break; }
        }
        
        uint32_t b = UINT32_MAX;
        for (uint32_t n : neighbors[j]) {
            if (n != i) { b = n; break; }
        }
        
        if (a == UINT32_MAX || b == UINT32_MAX) continue;
        
        // Compute current torsion angle
        double angle = torsion(mol.coords, a, i, j, b);
        
        rotatable.push_back({i, j, a, b, angle});
    }
    
    // Stable ordering: sort by (min(i,j), max(i,j))
    std::sort(rotatable.begin(), rotatable.end(),
        [](const RotatableBond& x, const RotatableBond& y) {
            uint32_t x_min = std::min(x.i, x.j);
            uint32_t x_max = std::max(x.i, x.j);
            uint32_t y_min = std::min(y.i, y.j);
            uint32_t y_max = std::max(y.i, y.j);
            return (x_min < y_min) || (x_min == y_min && x_max < y_max);
        });
    
    return rotatable;
}

/**
 * Set torsion angle by rotating fragment around bond axis.
 * Rotates all atoms on the 'b' side of the i-j bond.
 */
inline void set_torsion_angle(
    Molecule& mol,
    const RotatableBond& rot_bond,
    double target_angle)
{
    // Build which atoms to rotate (BFS from j, excluding i)
    std::vector<bool> to_rotate(mol.num_atoms(), false);
    std::vector<uint32_t> neighbors[mol.num_atoms()];
    
    for (const auto& bond : mol.bonds) {
        neighbors[bond.i].push_back(bond.j);
        neighbors[bond.j].push_back(bond.i);
    }
    
    // BFS from j, mark all reachable atoms (excluding path through i)
    std::vector<uint32_t> queue;
    queue.push_back(rot_bond.j);
    to_rotate[rot_bond.j] = true;
    
    for (size_t idx = 0; idx < queue.size(); ++idx) {
        uint32_t curr = queue[idx];
        for (uint32_t next : neighbors[curr]) {
            if (next == rot_bond.i) continue; // Don't cross the bond
            if (!to_rotate[next]) {
                to_rotate[next] = true;
                queue.push_back(next);
            }
        }
    }
    
    // Compute rotation axis (i -> j direction)
    double xi = mol.coords[3*rot_bond.i];
    double yi = mol.coords[3*rot_bond.i+1];
    double zi = mol.coords[3*rot_bond.i+2];
    
    double xj = mol.coords[3*rot_bond.j];
    double yj = mol.coords[3*rot_bond.j+1];
    double zj = mol.coords[3*rot_bond.j+2];
    
    double dx = xj - xi;
    double dy = yj - yi;
    double dz = zj - zi;
    double len = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    if (len < 1e-10) return; // Degenerate bond
    
    dx /= len; dy /= len; dz /= len;
    
    // Rotation angle: delta = target - current
    double delta = target_angle - rot_bond.current_angle;
    
    // Rodrigues rotation formula around axis (dx, dy, dz) by delta
    double c = std::cos(delta);
    double s = std::sin(delta);
    double t = 1.0 - c;
    
    for (uint32_t atom = 0; atom < mol.num_atoms(); ++atom) {
        if (!to_rotate[atom]) continue;
        
        // Translate to origin at i
        double x = mol.coords[3*atom] - xi;
        double y = mol.coords[3*atom+1] - yi;
        double z = mol.coords[3*atom+2] - zi;
        
        // Rotate
        double xr = (t*dx*dx + c)*x + (t*dx*dy - s*dz)*y + (t*dx*dz + s*dy)*z;
        double yr = (t*dx*dy + s*dz)*x + (t*dy*dy + c)*y + (t*dy*dz - s*dx)*z;
        double zr = (t*dx*dz - s*dy)*x + (t*dy*dz + s*dx)*y + (t*dz*dz + c)*z;
        
        // Translate back
        mol.coords[3*atom] = xr + xi;
        mol.coords[3*atom+1] = yr + yi;
        mol.coords[3*atom+2] = zr + zi;
    }
}

//=============================================================================
// Conformer Fingerprinting (Permutation-Invariant)
//=============================================================================

/**
 * Compute heavy-atom distance fingerprint for deduplication.
 * Translation, rotation, and mirror invariant.
 */
struct ConformerFingerprint {
    double energy_bin;              // Energy rounded to bin (1e-3 kcal/mol)
    std::vector<double> distances;  // Sorted heavy-atom pair distances
    
    bool operator==(const ConformerFingerprint& other) const {
        const double E_TOL = 1e-3;
        const double D_TOL = 1e-2; // 0.01 Å distance tolerance
        
        if (std::abs(energy_bin - other.energy_bin) > E_TOL) {
            return false;
        }
        
        if (distances.size() != other.distances.size()) {
            return false;
        }
        
        for (size_t i = 0; i < distances.size(); ++i) {
            if (std::abs(distances[i] - other.distances[i]) > D_TOL) {
                return false;
            }
        }
        
        return true;
    }
};

inline ConformerFingerprint compute_fingerprint(
    const Molecule& mol,
    double energy,
    double energy_bin_size = 1e-3)
{
    ConformerFingerprint fp;
    fp.energy_bin = std::round(energy / energy_bin_size) * energy_bin_size;
    
    // Collect heavy atoms (Z > 1)
    std::vector<uint32_t> heavy_atoms;
    for (uint32_t i = 0; i < mol.num_atoms(); ++i) {
        if (mol.atoms[i].Z > 1) {
            heavy_atoms.push_back(i);
        }
    }
    
    // Compute all pair distances
    fp.distances.reserve(heavy_atoms.size() * (heavy_atoms.size() - 1) / 2);
    
    for (size_t i = 0; i < heavy_atoms.size(); ++i) {
        for (size_t j = i + 1; j < heavy_atoms.size(); ++j) {
            uint32_t ai = heavy_atoms[i];
            uint32_t aj = heavy_atoms[j];
            
            double dx = mol.coords[3*ai] - mol.coords[3*aj];
            double dy = mol.coords[3*ai+1] - mol.coords[3*aj+1];
            double dz = mol.coords[3*ai+2] - mol.coords[3*aj+2];
            
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            fp.distances.push_back(dist);
        }
    }
    
    // Sort for permutation invariance
    std::sort(fp.distances.begin(), fp.distances.end());
    
    return fp;
}

//=============================================================================
// RMSD-Based Geometry Clustering
//=============================================================================

/**
 * Compute RMSD between two molecules after optimal superposition.
 * Uses Kabsch algorithm for alignment.
 */
inline double compute_rmsd(const Molecule& mol1, const Molecule& mol2) {
    if (mol1.num_atoms() != mol2.num_atoms()) {
        return 1e6; // Infinite distance for different sizes
    }
    
    const uint32_t n = mol1.num_atoms();
    
    // Compute centroids
    Vec3 c1(0, 0, 0), c2(0, 0, 0);
    for (uint32_t i = 0; i < n; ++i) {
        c1.x += mol1.coords[3*i];
        c1.y += mol1.coords[3*i+1];
        c1.z += mol1.coords[3*i+2];
        
        c2.x += mol2.coords[3*i];
        c2.y += mol2.coords[3*i+1];
        c2.z += mol2.coords[3*i+2];
    }
    c1 = c1 / double(n);
    c2 = c2 / double(n);
    
    // Compute RMSD (without rotation for simplicity - good enough for duplicates)
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        double dx = (mol1.coords[3*i] - c1.x) - (mol2.coords[3*i] - c2.x);
        double dy = (mol1.coords[3*i+1] - c1.y) - (mol2.coords[3*i+1] - c2.y);
        double dz = (mol1.coords[3*i+2] - c1.z) - (mol2.coords[3*i+2] - c2.z);
        sum_sq += dx*dx + dy*dy + dz*dz;
    }
    
    return std::sqrt(sum_sq / n);
}

//=============================================================================
// Conformer/Isomer Archive
//=============================================================================

/**
 * Unified structure for all molecular variants.
 * Can represent conformers, geometric isomers, or constitutional isomers.
 */
struct MolecularVariant {
    Molecule geometry;
    double energy;
    
    // Multi-level identification
    IsomerSignature isomer_sig;        // Constitutional + geometric + chiral
    ConformerFingerprint conformer_fp; // Distance-based (old system)
    
    VariantType type;
    std::string descriptor; // "cis", "trans", "gauche", "anti", etc.
    int trial_id;          // Which trial/generation found this
    
    bool operator<(const MolecularVariant& other) const {
        // Sort by energy, then by trial_id for stability
        if (std::abs(energy - other.energy) > 1e-6) {
            return energy < other.energy;
        }
        return trial_id < other.trial_id;
    }
};

//=============================================================================
// ConformerFinder Settings
//=============================================================================

struct ConformerFinderSettings {
    // Conformational search
    int num_starts = 100;           // Number of random torsion initial states
    int seed = 42;                  // RNG seed for reproducibility
    bool enable_basin_hopping = false;
    int basin_iterations = 0;
    
    // Isomer enumeration
    bool enumerate_geometric_isomers = true; // Generate cis/trans, fac/mer
    bool enumerate_conformers = true;        // Torsional sampling
    
    // Deduplication thresholds
    double energy_threshold = 1e-3;    // kcal/mol
    double rmsd_threshold = 0.1;       // Angstroms
    
    // Keep top K results (0 = keep all)
    int top_k = 0;
    
    // Thermal configuration for ranking
    ThermalConfig thermal_config;
    
    // Optimization settings
    OptimizerSettings opt_settings;
    
    ConformerFinderSettings() {
        opt_settings.max_iterations = 500;
        opt_settings.tol_rms_force = 1e-3;
        opt_settings.print_every = 0; // Silent
    }
};

//=============================================================================
// ConformerFinder Main Class (Now: Isomer + Conformer Finder)
//=============================================================================

class ConformerFinder {
public:
    ConformerFinder(const ConformerFinderSettings& settings)
        : settings_(settings), rng_(settings.seed) {}
    
    /**
     * MAIN ENTRY POINT: Find all unique isomers and conformers.
     * 
     * Workflow:
     * 1. Generate geometric isomers (if metal complex detected)
     * 2. For each isomer, generate conformational variants
     * 3. Optimize all structures
     * 4. Deduplicate using multi-level signatures
     * 5. Cluster by RMSD to remove duplicates
     * 
     * Returns sorted list of unique molecular variants.
     */
    std::vector<MolecularVariant> find_all_variants(
        const Molecule& base_molecule,
        const EnergyModel& energy_model);
    
    /**
     * LEGACY INTERFACE: Run conformer search only (no isomer enumeration).
     * Returns sorted list of unique conformers.
     */
    std::vector<MolecularVariant> find_conformers(
        const Molecule& base_molecule,
        const EnergyModel& energy_model);
    
    // Statistics
    int num_trials() const { return num_trials_; }
    int num_unique() const { return archive_.size(); }
    int num_duplicates() const { return num_duplicates_; }
    int num_isomers() const { return num_isomers_; }
    
    /**
     * Compute ensemble free energy F = -kT ln(sum_i exp(-E_i/kT))
     * For T=0: returns minimum energy
     * For T>0: proper Boltzmann-weighted free energy
     */
    double get_ensemble_free_energy(const std::vector<MolecularVariant>& variants) const {
        if (variants.empty()) return 0.0;
        
        std::vector<double> energies;
        energies.reserve(variants.size());
        for (const auto& var : variants) {
            energies.push_back(var.energy);
        }
        
        return settings_.thermal_config.free_energy_from_energies(energies);
    }

private:
    ConformerFinderSettings settings_;
    std::mt19937 rng_;
    std::vector<MolecularVariant> archive_;
    int num_trials_ = 0;
    int num_duplicates_ = 0;
    int num_isomers_ = 0;
    
    // Generate geometric isomer starting structures
    std::vector<Molecule> generate_isomer_structures(
        const Molecule& mol);
    
    // Randomize torsions to create diverse initial state
    void randomize_torsions(Molecule& mol, const std::vector<RotatableBond>& rotatable);
    
    // Multi-level deduplication check
    bool is_duplicate(
        const MolecularVariant& candidate);
    
    // Add variant to archive if unique
    bool try_add_variant(MolecularVariant&& var);
    
    // Basin-hopping: perturb existing minimum
    Molecule perturb_variant(
        const MolecularVariant& var,
        const std::vector<RotatableBond>& rotatable);
};

//=============================================================================
// Implementation: Isomer Generation
//=============================================================================

inline std::vector<Molecule> ConformerFinder::generate_isomer_structures(
    const Molecule& mol)
{
    std::vector<Molecule> isomers;
    
    if (!settings_.enumerate_geometric_isomers) {
        isomers.push_back(mol);
        return isomers;
    }
    
    // Check if this is a coordination complex
    // Simple heuristic: atoms with Z in transition metal range
    auto is_metal = [](uint32_t Z) -> bool {
        return (Z >= 21 && Z <= 30) || // 3d metals
               (Z >= 39 && Z <= 48) || // 4d metals
               (Z >= 72 && Z <= 80);   // 5d metals
    };
    
    uint32_t metal_idx = UINT32_MAX;
    for (uint32_t i = 0; i < mol.num_atoms(); ++i) {
        if (is_metal(mol.atoms[i].Z)) {
            metal_idx = i;
            break;
        }
    }
    
    if (metal_idx == UINT32_MAX) {
        // Not a coordination complex → no geometric isomers
        isomers.push_back(mol);
        return isomers;
    }
    
    // Extract ligand information
    uint32_t metal_Z = mol.atoms[metal_idx].Z;
    std::map<uint32_t, uint32_t> ligand_counts;
    uint32_t coordination_number = 0;
    
    for (const auto& bond : mol.bonds) {
        if (bond.i == metal_idx) {
            ligand_counts[mol.atoms[bond.j].Z]++;
            coordination_number++;
        } else if (bond.j == metal_idx) {
            ligand_counts[mol.atoms[bond.i].Z]++;
            coordination_number++;
        }
    }
    
    // Generate geometric isomers using IsomerGenerator
    auto variants = IsomerGenerator::generate_coordination_isomers(
        metal_Z, ligand_counts, coordination_number);
    
    num_isomers_ = variants.size();
    
    for (const auto& variant : variants) {
        isomers.push_back(variant.structure);
    }
    
    return isomers;
}

//=============================================================================
// Implementation: Deduplication
//=============================================================================

inline bool ConformerFinder::is_duplicate(
    const MolecularVariant& candidate)
{
    for (const auto& existing : archive_) {
        // Level 1: Energy check
        if (std::abs(candidate.energy - existing.energy) > settings_.energy_threshold) {
            continue;
        }
        
        // Level 2: Isomer signature (fast)
        if (candidate.isomer_sig != existing.isomer_sig) {
            continue;
        }
        
        // Level 3: Old conformer fingerprint (distance-based)
        if (!(candidate.conformer_fp == existing.conformer_fp)) {
            continue;
        }
        
        // Level 4: RMSD (slow but definitive)
        double rmsd = compute_rmsd(candidate.geometry, existing.geometry);
        if (rmsd < settings_.rmsd_threshold) {
            return true; // Duplicate found
        }
    }
    
    return false; // Unique
}

inline bool ConformerFinder::try_add_variant(
    MolecularVariant&& var)
{
    if (is_duplicate(var)) {
        num_duplicates_++;
        return false;
    }
    
    archive_.push_back(std::move(var));
    return true;
}

//=============================================================================
// Implementation: Torsion Randomization
//=============================================================================

inline void ConformerFinder::randomize_torsions(
    Molecule& mol,
    const std::vector<RotatableBond>& rotatable)
{
    std::uniform_real_distribution<double> angle_dist(-M_PI, M_PI);
    
    for (const auto& rot : rotatable) {
        double target_angle = angle_dist(rng_);
        set_torsion_angle(mol, rot, target_angle);
    }
}

inline Molecule ConformerFinder::perturb_variant(
    const MolecularVariant& var,
    const std::vector<RotatableBond>& rotatable)
{
    Molecule mol = var.geometry;
    
    // Perturb a random subset of torsions
    std::uniform_real_distribution<double> angle_dist(-M_PI/4, M_PI/4); // ±45°
    std::uniform_int_distribution<int> select_dist(0, 1);
    
    for (const auto& rot : rotatable) {
        if (select_dist(rng_) == 0) continue; // 50% chance to perturb each
        
        double delta = angle_dist(rng_);
        double new_angle = rot.current_angle + delta;
        set_torsion_angle(mol, rot, new_angle);
    }
    
    return mol;
}

//=============================================================================
// Implementation: Main Search Routines
//=============================================================================

inline std::vector<MolecularVariant> ConformerFinder::find_all_variants(
    const Molecule& base_molecule,
    const EnergyModel& energy_model)
{
    archive_.clear();
    num_trials_ = 0;
    num_duplicates_ = 0;
    num_isomers_ = 0;
    
    std::cout << "\n=== Isomer + Conformer Search ===\n";
    
    // Step 1: Generate geometric isomers
    auto isomer_structures = generate_isomer_structures(base_molecule);
    std::cout << "Generated " << isomer_structures.size() << " geometric isomers\n";
    
    // Step 2: For each isomer, generate conformational variants
    for (size_t iso_idx = 0; iso_idx < isomer_structures.size(); ++iso_idx) {
        const Molecule& isomer_base = isomer_structures[iso_idx];
        
        std::cout << "\nProcessing isomer " << (iso_idx + 1) << "...\n";
        
        // Detect rotatable bonds
        auto rotatable = find_rotatable_bonds(isomer_base);
        std::cout << "  Rotatable bonds: " << rotatable.size() << "\n";
        
        if (!settings_.enumerate_conformers || rotatable.empty()) {
            // No conformational freedom → just optimize base structure
            Molecule mol = isomer_base;
            
            // Optimize
            FIREOptimizer optimizer(settings_.opt_settings);
            auto result = optimizer.minimize(mol.coords, energy_model);
            
            if (result.converged) {
                mol.coords = result.coords;
                
                MolecularVariant variant;
                variant.geometry = mol;
                variant.energy = result.energy;
                variant.isomer_sig = compute_isomer_signature(mol);
                variant.conformer_fp = compute_fingerprint(mol, result.energy);
                variant.type = VariantType::GEOMETRIC_ISOMER;
                variant.descriptor = "base";
                variant.trial_id = num_trials_++;
                
                try_add_variant(std::move(variant));
            }
            
            continue;
        }
        
        // Generate conformers via torsion randomization
        for (int trial = 0; trial < settings_.num_starts; ++trial) {
            Molecule mol = isomer_base;
            
            // Randomize torsions
            randomize_torsions(mol, rotatable);
            
            // Optimize
            FIREOptimizer optimizer(settings_.opt_settings);
            auto result = optimizer.minimize(mol.coords, energy_model);
            
            if (result.converged) {
                mol.coords = result.coords;
                
                MolecularVariant variant;
                variant.geometry = mol;
                variant.energy = result.energy;
                variant.isomer_sig = compute_isomer_signature(mol);
                variant.conformer_fp = compute_fingerprint(mol, result.energy);
                variant.type = VariantType::CONFORMER;
                variant.descriptor = "conformer";
                variant.trial_id = num_trials_++;
                
                try_add_variant(std::move(variant));
            }
        }
        
        std::cout << "  Found " << archive_.size() << " unique variants so far\n";
    }
    
    // Step 3: Sort by energy
    std::sort(archive_.begin(), archive_.end());
    
    // Step 4: Optionally keep only top K
    if (settings_.top_k > 0 && archive_.size() > size_t(settings_.top_k)) {
        archive_.resize(settings_.top_k);
    }
    
    std::cout << "\n=== Search Complete ===\n";
    std::cout << "Total trials: " << num_trials_ << "\n";
    std::cout << "Unique variants: " << archive_.size() << "\n";
    std::cout << "Duplicates rejected: " << num_duplicates_ << "\n";
    std::cout << "Geometric isomers: " << num_isomers_ << "\n\n";
    
    return archive_;
}

inline std::vector<MolecularVariant> ConformerFinder::find_conformers(
    const Molecule& base_molecule,
    const EnergyModel& energy_model)
{
    // Legacy interface: conformers only, no isomer enumeration
    archive_.clear();
    num_trials_ = 0;
    num_duplicates_ = 0;
    
    // No need for chemistry object - signatures are self-contained
    
    // Detect rotatable bonds
    auto rotatable = find_rotatable_bonds(base_molecule);
    
    if (rotatable.empty()) {
        // No rotatable bonds → return optimized base structure
        Molecule mol = base_molecule;
        FIREOptimizer optimizer(settings_.opt_settings);
        auto result = optimizer.minimize(mol.coords, energy_model);
        
        if (result.converged) {
            mol.coords = result.coords;
            
            MolecularVariant variant;
            variant.geometry = mol;
            variant.energy = result.energy;
            variant.isomer_sig = compute_isomer_signature(mol);
            variant.conformer_fp = compute_fingerprint(mol, result.energy);
            variant.type = VariantType::CONFORMER;
            variant.descriptor = "base";
            variant.trial_id = 0;
            
            archive_.push_back(variant);
        }
        
        return archive_;
    }
    
    // Multi-start conformational search
    FIREOptimizer optimizer(settings_.opt_settings);
    
    for (int trial = 0; trial < settings_.num_starts; ++trial) {
        Molecule mol = base_molecule;
        randomize_torsions(mol, rotatable);
        
        auto result = optimizer.minimize(mol.coords, energy_model);
        
        if (result.converged) {
            mol.coords = result.coords;
            
            MolecularVariant variant;
            variant.geometry = mol;
            variant.energy = result.energy;
            variant.isomer_sig = compute_isomer_signature(mol);
            variant.conformer_fp = compute_fingerprint(mol, result.energy);
            variant.type = VariantType::CONFORMER;
            variant.descriptor = "conformer";
            variant.trial_id = num_trials_++;
            
            try_add_variant(std::move(variant));
        }
    }
    
    // Sort and optionally truncate
    std::sort(archive_.begin(), archive_.end());
    
    if (settings_.top_k > 0 && archive_.size() > size_t(settings_.top_k)) {
        archive_.resize(settings_.top_k);
    }
    
    return archive_;
}

} // namespace vsepr
