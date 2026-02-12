/**
 * crystal_grid.hpp
 * ----------------
 * Crystallographic grid visualization with coordination polyhedra
 * 
 * Mathematical approach:
 * 1. Define lattice vectors A (3x3 matrix)
 * 2. Use fractional coordinates f
 * 3. Compute atomic positions: r = A·f
 * 4. Apply space-group symmetry
 * 5. Render coordination polyhedra
 * 
 * Color rule (information-dense):
 * - Base atom color: element-specific
 * - Polyhedron color: inverted mean RGB of constituent atoms
 *   Formula: RGB_poly = (255, 255, 255) - mean(RGB_atoms)
 * 
 * Math does the aesthetics. Humans optional.
 */

#pragma once

#include "core/math_vec3.hpp"
#include <vector>
#include <array>
#include <string>  // Added for std::string
#include <cstdint>

namespace vsepr {
namespace render {

// ============================================================================
// Lattice Vectors and Fractional Coordinates
// ============================================================================

/**
 * 3x3 lattice matrix: A = [a, b, c] where a, b, c are column vectors
 * 
 * Atomic position in real space: r = A·f
 * where f = fractional coordinates (0 ≤ f_i < 1)
 */
struct LatticeVectors {
    // Column vectors: a, b, c
    Vec3 a;  // First lattice vector
    Vec3 b;  // Second lattice vector
    Vec3 c;  // Third lattice vector
    
    /**
     * Convert fractional coordinates to Cartesian
     * r = A·f = f_x*a + f_y*b + f_z*c
     */
    Vec3 to_cartesian(const Vec3& fractional) const {
        return a * fractional.x + b * fractional.y + c * fractional.z;
    }
    
    /**
     * Convert Cartesian to fractional coordinates
     * f = A^{-1}·r
     */
    Vec3 to_fractional(const Vec3& cartesian) const;
    
    /**
     * Get lattice parameters (a, b, c, α, β, γ)
     */
    void get_parameters(
        double& a_len, double& b_len, double& c_len,
        double& alpha, double& beta, double& gamma
    ) const;
    
    /**
     * Volume of unit cell: V = a·(b × c)
     */
    double volume() const;
    
    // Convenience constructors for common crystal systems
    
    /**
     * Cubic: a = b = c, α = β = γ = 90°
     */
    static LatticeVectors cubic(double a);
    
    /**
     * FCC (face-centered cubic)
     * Conventional cell: a = b = c
     * Primitive cell vectors:
     *   a = (a/2)[0, 1, 1]
     *   b = (a/2)[1, 0, 1]
     *   c = (a/2)[1, 1, 0]
     */
    static LatticeVectors fcc(double a);
    
    /**
     * BCC (body-centered cubic)
     * Conventional cell: a = b = c
     * Primitive cell vectors:
     *   a = (a/2)[-1, 1, 1]
     *   b = (a/2)[1, -1, 1]
     *   c = (a/2)[1, 1, -1]
     */
    static LatticeVectors bcc(double a);
};

// ============================================================================
// Crystal Atom (with fractional coordinates)
// ============================================================================

struct CrystalAtom {
    uint8_t atomic_number;    // Element (Z)
    Vec3 fractional;          // Fractional coordinates (0 ≤ f < 1)
    Vec3 cartesian;           // Real-space position (computed from A·f)
    
    // Visualization properties
    std::array<uint8_t, 3> color_rgb;  // Base color
    float radius;                       // Atomic radius
};

// ============================================================================
// Coordination Polyhedron
// ============================================================================

struct CoordinationPolyhedron {
    size_t central_atom_idx;           // Central atom
    std::vector<size_t> neighbor_indices;  // Neighbor atoms
    
    // Polyhedron geometry
    std::vector<std::array<size_t, 3>> faces;  // Triangular faces (indices into neighbors)
    
    // Color (inverted mean RGB of constituent atoms)
    std::array<uint8_t, 3> color_rgb;
    
    /**
     * Compute polyhedron color using inverted mean rule
     * RGB_poly = (255, 255, 255) - mean(RGB_atoms)
     */
    static std::array<uint8_t, 3> compute_color(
        const std::vector<std::array<uint8_t, 3>>& atom_colors
    );
};

// ============================================================================
// Crystal Structure
// ============================================================================

struct CrystalStructure {
    std::string name;              // e.g., "Al FCC", "NaCl"
    LatticeVectors lattice;        // Lattice vectors
    std::vector<CrystalAtom> atoms;  // Atoms in unit cell (fractional coords)
    
    // Space group info (optional, for symmetry generation)
    int space_group_number;
    std::string space_group_symbol;
    
    /**
     * Generate supercell (nx × ny × nz replicas)
     * Returns expanded structure with all atoms
     */
    CrystalStructure generate_supercell(int nx, int ny, int nz) const;
    
    /**
     * Find coordination polyhedra
     * Uses distance cutoff to identify nearest neighbors
     */
    std::vector<CoordinationPolyhedron> find_coordination_polyhedra(
        double cutoff_angstrom = 3.5
    ) const;
    
    /**
     * Wrap fractional coordinates to [0, 1)
     */
    static void wrap_fractional(Vec3& f);
};

// ============================================================================
// Crystal Grid Renderer
// ============================================================================

class CrystalGridRenderer {
public:
    CrystalGridRenderer();
    
    /**
     * Set crystal structure to visualize
     */
    void set_structure(const CrystalStructure& structure);
    
    /**
     * Set supercell replication (default: 3×3×3 like reference image)
     */
    void set_replication(int nx, int ny, int nz) {
        nx_ = nx;
        ny_ = ny;
        nz_ = nz;
    }
    
    /**
     * Enable/disable coordination polyhedra
     */
    void show_polyhedra(bool enable) { show_polyhedra_ = enable; }
    
    /**
     * Enable/disable unit cell wireframe
     */
    void show_cell_edges(bool enable) { show_cell_edges_ = enable; }
    
    /**
     * Set polyhedron transparency (0.0 = invisible, 1.0 = opaque)
     */
    void set_polyhedron_opacity(float opacity) { polyhedron_opacity_ = opacity; }
    
    /**
     * Set cutoff for coordination determination
     */
    void set_coordination_cutoff(double cutoff) { cutoff_ = cutoff; }
    
    /**
     * Render the crystal grid
     */
    void render();
    
    /**
     * Get current structure (with supercell expansion)
     */
    const CrystalStructure& get_expanded_structure() const {
        return expanded_structure_;
    }
    
private:
    CrystalStructure base_structure_;       // Unit cell
    CrystalStructure expanded_structure_;   // Supercell
    
    int nx_, ny_, nz_;  // Replication factors
    
    bool show_polyhedra_;
    bool show_cell_edges_;
    float polyhedron_opacity_;
    double cutoff_;
    
    std::vector<CoordinationPolyhedron> polyhedra_;
    
    // Render helpers
    void render_atoms();
    void render_bonds();
    void render_polyhedra();
    void render_cell_edges();
    
    // Geometry generation
    void generate_polyhedron_faces(CoordinationPolyhedron& poly);
};

// ============================================================================
// Predefined Crystal Structures (for testing)
// ============================================================================

namespace crystals {

/**
 * Al FCC (like reference image)
 * Space group: Fm3̄m (#225)
 * a = 4.05 Å
 */
CrystalStructure aluminum_fcc();

/**
 * Fe BCC
 * Space group: Im3̄m (#229)
 * a = 2.87 Å
 */
CrystalStructure iron_bcc();

/**
 * NaCl rocksalt
 * Space group: Fm3̄m (#225)
 * a = 5.64 Å
 */
CrystalStructure sodium_chloride();

/**
 * Si diamond
 * Space group: Fd3̄m (#227)
 * a = 5.43 Å
 */
CrystalStructure silicon_diamond();

} // namespace crystals

} // namespace render
} // namespace vsepr
