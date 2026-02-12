#pragma once

#include "renderer_base.hpp"
#include "core/math_vec3.hpp"
#include <vector>

namespace vsepr {
namespace render {

/**
 * Periodic boundary condition visualization
 * 
 * Renders infinite repeating unit cells for crystal/solid structures.
 * 
 * Features:
 * - Replicate unit cell in 3D grid (nx × ny × nz)
 * - Render PBC box edges
 * - Ghost atoms (translucent) in neighboring cells
 * - Configurable replication factor
 */
class PBCVisualizer {
public:
    PBCVisualizer();
    
    /**
     * Enable/disable PBC visualization
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }
    
    /**
     * Set replication factor (how many unit cells to show in each direction)
     * 
     * @param nx Number of cells along a-vector
     * @param ny Number of cells along b-vector
     * @param nz Number of cells along c-vector
     * 
     * Total cells = (2*nx+1) × (2*ny+1) × (2*nz+1)
     * 
     * Example:
     *   (1, 1, 1) → 3×3×3 = 27 cells
     *   (2, 2, 2) → 5×5×5 = 125 cells
     */
    void set_replication(int nx, int ny, int nz);
    
    /**
     * Get replication factors
     */
    void get_replication(int& nx, int& ny, int& nz) const {
        nx = replicate_x_;
        ny = replicate_y_;
        nz = replicate_z_;
    }
    
    /**
     * Generate replicated geometry
     * 
     * Takes base unit cell and replicates it according to PBC box.
     * Returns expanded geometry with all replicas.
     * 
     * @param base_geom Unit cell geometry
     * @return Replicated geometry (may be large!)
     */
    AtomicGeometry generate_replicas(const AtomicGeometry& base_geom) const;
    
    /**
     * Enable ghost atoms (translucent atoms in non-central cells)
     */
    void set_ghost_atoms(bool enable) { ghost_atoms_ = enable; }
    bool has_ghost_atoms() const { return ghost_atoms_; }
    
    /**
     * Set ghost atom opacity (0.0 = invisible, 1.0 = fully opaque)
     */
    void set_ghost_opacity(float opacity) { ghost_opacity_ = opacity; }
    float get_ghost_opacity() const { return ghost_opacity_; }
    
    /**
     * Show/hide unit cell box edges
     */
    void set_show_box(bool show) { show_box_ = show; }
    bool is_showing_box() const { return show_box_; }
    
    /**
     * Set box edge color
     */
    void set_box_color(float r, float g, float b) {
        box_color_[0] = r;
        box_color_[1] = g;
        box_color_[2] = b;
    }
    
    /**
     * Get box edge rendering data (for renderer)
     * 
     * Returns 12 line segments (edges of parallelepiped)
     */
    struct BoxEdge {
        Vec3 start;
        Vec3 end;
    };
    std::vector<BoxEdge> get_box_edges(const AtomicGeometry::PBCBox& box) const;
    
private:
    bool enabled_ = false;
    int replicate_x_ = 1;  // ±1 cell in x
    int replicate_y_ = 1;  // ±1 cell in y
    int replicate_z_ = 1;  // ±1 cell in z
    
    bool ghost_atoms_ = true;
    float ghost_opacity_ = 0.3f;
    
    bool show_box_ = true;
    float box_color_[3] = {0.5f, 0.5f, 0.5f};  // Gray
    
    // Compute linear combination of lattice vectors
    Vec3 compute_translation(const AtomicGeometry::PBCBox& box,
                            int ix, int iy, int iz) const;
};

/**
 * Extended renderer settings for PBC
 */
class PBCRendererExtension {
public:
    /**
     * Add PBC visualization to existing geometry
     * 
     * Modifies geom in-place to include replicas.
     * Marks ghost atoms via occupancy field.
     */
    static void apply_pbc(AtomicGeometry& geom,
                         const PBCVisualizer& pbc_vis);
};

} // namespace render
} // namespace vsepr
