// Forces.hpp - Force vector storage and analysis
// Extends Crystal with computed force fields

#pragma once
#include "data/Crystal.hpp"
#include <vector>
#include <array>
#include <optional>
#include <unordered_map>

namespace vsepr::data {

// ════════════════════════════════════════════════════════════════════════════
// Force vector data structures
// ════════════════════════════════════════════════════════════════════════════

struct ForceVector {
    Vec3 direction;      // Unit vector (direction)
    float magnitude;     // Force magnitude (kcal/mol/Å or eV/Å)
    std::string source;  // Source atom ID (who exerts this force)
    
    // Decomposition (optional)
    std::optional<Vec3> bonded;      // Bonded contribution
    std::optional<Vec3> nonbonded;   // Non-bonded (LJ + Coulomb)
    std::optional<Vec3> lj;          // Lennard-Jones only
    std::optional<Vec3> coulomb;     // Coulomb only
};

struct AtomForces {
    std::string atom_id;             // "a1", "a2", ...
    Vec3 net_force;                  // Total force on this atom
    
    // Contributions from all neighbors
    std::vector<ForceVector> contributions;
    
    // Primary interaction (largest magnitude)
    std::optional<ForceVector> primary;
    
    // Statistics
    float max_magnitude;             // Largest single contribution
    float total_magnitude;           // |F_net|
    int num_contributors;            // Number of neighbors contributing
};

// ════════════════════════════════════════════════════════════════════════════
// Forces: The force field wrapper
// ════════════════════════════════════════════════════════════════════════════

class Forces {
public:
    // ─── Core data ───
    std::string xyz_path;            // Source geometry (foo.xyz)
    std::string xyzF_path;           // Force field file (foo.xyzF)
    
    std::vector<AtomForces> atom_forces;
    
    // ─── Metadata ───
    std::string units;               // "kcal_mol_A" or "eV_A"
    std::string model;               // "LJ", "LJ+Coulomb", etc.
    std::optional<float> temperature;// Temperature (K) if MD
    std::optional<int> frame;        // Frame number if trajectory
    
    // ─── Provenance ───
    struct Computation {
        std::string method;          // "pairwise_lj", "bonded_mm", etc.
        std::unordered_map<std::string, std::string> params;
        std::string timestamp;
        std::string hash;            // SHA256 of geometry + params
    } computation;
    
    // ─── Statistics ───
    float max_force;                 // Global max |F|
    float mean_force;                // Mean |F| over all atoms
    float rms_force;                 // RMS force
    
    // ─── Methods ───
    static Forces compute_from_crystal(const Crystal& cryst,
                                       const std::string& model = "LJ+Coulomb");
    
    static Forces load_xyzF(const std::string& path);
    void save_xyzF(const std::string& path) const;
    
    // Analysis
    std::vector<std::pair<std::string, std::string>> find_strong_interactions(float threshold) const;
    AtomForces* get_atom_forces(const std::string& atom_id);
    
    // Visualization helpers
    std::vector<std::tuple<Vec3, Vec3, float>> get_force_arrows() const;  // (origin, direction, magnitude)
    std::vector<std::tuple<Vec3, Vec3, float>> get_primary_arrows() const; // Only primary interactions
};

// ════════════════════════════════════════════════════════════════════════════
// Force computation engine
// ════════════════════════════════════════════════════════════════════════════

class ForceComputer {
public:
    explicit ForceComputer(const Crystal& cryst) : cryst_(cryst) {}
    
    // Compute forces using specified model
    Forces compute(const std::string& model = "LJ+Coulomb");
    
    // Pairwise force between two atoms
    ForceVector compute_pairwise(const Atom& a, const Atom& b,
                                 const std::string& model) const;
    
    // Decompose into bonded/nonbonded
    void decompose_forces(Forces& forces) const;
    
private:
    Vec3 compute_lj_force(const Atom& a, const Atom& b) const;
    Vec3 compute_coulomb_force(const Atom& a, const Atom& b) const;
    Vec3 compute_bonded_force(const Atom& a, const Atom& b) const;
    
    const Crystal& cryst_;
};

// ════════════════════════════════════════════════════════════════════════════
// xyzF file I/O
// ════════════════════════════════════════════════════════════════════════════

class XYZFParser {
public:
    static Forces parse(const std::string& path);
    static void write(const std::string& path, const Forces& forces);
    
private:
    static std::string format_xyzF(const Forces& f);
    static Forces parse_xyzF(const std::string& content);
};

} // namespace vsepr::data
