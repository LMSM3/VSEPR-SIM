// Crystal.hpp - Immutable provenance + mutable caches
// Represents a molecular/crystalline structure with full lineage

#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <unordered_map>
#include <cstdint>

namespace vsepr::data {

// ════════════════════════════════════════════════════════════════════════════
// File format types (xyzZ, xyzA, xyzC)
// ════════════════════════════════════════════════════════════════════════════

enum class XYZFormat {
    Z,  // Raw input (standard XYZ)
    A,  // Annotated (bonds, IDs, metadata)
    C   // Constructed (derived: supercells, relaxed, CG)
};

struct Vec3 {
    float x, y, z;
};

struct Atom {
    std::string element;
    Vec3 position;
    
    // xyzA extensions
    std::string id;          // "a1", "a2", ...
    std::string group;       // grouping (e.g., "ring", "chain")
    float charge;            // partial charge (e)
    float mass;              // atomic mass (amu)
    std::string tag;         // user tag
    uint32_t flags;          // bitfield
};

struct Bond {
    std::string atom_a;
    std::string atom_b;
    int order;               // 1=single, 2=double, 3=triple
    std::string type;        // "single", "aromatic", etc.
};

struct LatticeVectors {
    Vec3 a, b, c;            // Cell vectors (Angstroms)
};

// ════════════════════════════════════════════════════════════════════════════
// Construction provenance (for xyzC)
// ════════════════════════════════════════════════════════════════════════════

struct ConstructionStep {
    std::string name;        // "supercell", "relax", "cg"
    std::unordered_map<std::string, std::string> params;
};

struct ConstructionRecipe {
    std::string pipeline_id;
    std::vector<ConstructionStep> steps;
    std::string hash;        // SHA256 of source + steps
};

// ════════════════════════════════════════════════════════════════════════════
// Reserved slots for bulk/CG properties (xyzC)
// ════════════════════════════════════════════════════════════════════════════

struct BulkProperties {
    std::optional<float> density;          // g/cm³
    std::optional<float> elastic_modulus;  // GPa
    std::optional<std::string> rdf_ref;    // path to RDF data
};

struct CoarseGrainedProperties {
    std::optional<int> bead_count;
    std::optional<std::string> bead_types;
    std::optional<std::string> bead_bonds;
    std::optional<std::string> pmf_ref;    // path to PMF data
};

struct ConstructionResults {
    std::optional<float> energy;           // eV
    std::optional<bool> converged;
    std::string notes;
};

// ════════════════════════════════════════════════════════════════════════════
// Crystal: The Special Object
// ════════════════════════════════════════════════════════════════════════════

class Crystal {
public:
    // ─── Immutable source references ───
    std::string xyz_path;        // foo.xyz  (raw input)
    std::string xyzA_path;       // foo.xyzA (annotated)
    std::string xyzC_path;       // foo.xyzC (constructed)
    
    // ─── Constructive state (if xyzC) ───
    std::optional<LatticeVectors> lattice;
    std::optional<std::array<int, 3>> replication; // nx, ny, nz
    std::optional<ConstructionRecipe> recipe;
    
    // ─── Bulk/CG slots ───
    BulkProperties bulk;
    CoarseGrainedProperties cg;
    ConstructionResults results;
    
    // ─── Runtime caches (throwaway) ───
    mutable std::vector<Bond> inferred_bonds;
    mutable bool bonds_computed = false;
    
    // ─── Core data ───
    std::vector<Atom> atoms;
    std::string title;
    std::string units = "angstrom";
    
    // ─── Metadata ───
    XYZFormat source_format;
    std::string created_utc;
    
    // ─── Methods ───
    static Crystal load_xyz(const std::string& path);
    static Crystal load_xyzA(const std::string& path);
    static Crystal load_xyzC(const std::string& path);
    
    void save_xyz(const std::string& path) const;
    void save_xyzA(const std::string& path) const;
    void save_xyzC(const std::string& path) const;
    
    // Regenerate derived assets (if source changed)
    bool needs_rebuild() const;
    void rebuild();
    
    // Bond inference (lazy)
    const std::vector<Bond>& get_bonds() const;
    void invalidate_bonds() const { bonds_computed = false; }
    
    // Provenance query
    std::string get_hash() const;
    bool matches_hash(const std::string& h) const;
};

// ════════════════════════════════════════════════════════════════════════════
// File I/O utilities
// ════════════════════════════════════════════════════════════════════════════

class XYZParser {
public:
    static Crystal parse(const std::string& path, XYZFormat fmt);
    static void write(const std::string& path, const Crystal& cryst, XYZFormat fmt);
    
private:
    static Crystal parse_xyzZ(const std::string& content);
    static Crystal parse_xyzA(const std::string& content);
    static Crystal parse_xyzC(const std::string& content);
    
    static std::string write_xyzZ(const Crystal& c);
    static std::string write_xyzA(const Crystal& c);
    static std::string write_xyzC(const Crystal& c);
};

// ════════════════════════════════════════════════════════════════════════════
// Watch system (for --watch mode)
// ════════════════════════════════════════════════════════════════════════════

class CrystalWatcher {
public:
    void watch(const std::string& xyz_path);
    void stop();
    
    // Callback when source changes
    std::function<void(const Crystal&)> on_changed;
    
private:
    std::string watched_path_;
    std::string last_hash_;
    bool running_ = false;
};

} // namespace vsepr::data
