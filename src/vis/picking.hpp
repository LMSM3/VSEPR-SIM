#pragma once

#include "renderer_base.hpp"
#include "core/math_vec3.hpp"
#include <vector>
#include <optional>

namespace vsepr {
namespace render {

/**
 * Picking result for atoms
 */
struct AtomPick {
    int atom_index;       // Index in geometry
    float distance;       // Distance from camera
    Vec3 position;        // World position
    int atomic_number;    // Z value
};

/**
 * Picking result for bonds
 */
struct BondPick {
    int bond_index;       // Index in bond list
    int atom1, atom2;     // Bonded atom indices
    float distance;       // Distance from camera
    Vec3 midpoint;        // Bond midpoint
    float length;         // Bond length (Å)
};

/**
 * Mouse picking for molecular visualization
 * 
 * Ray-casting to detect atoms (spheres) and bonds (cylinders) under cursor.
 * Used for hover tooltips and interactive selection.
 */
class MoleculePicker {
public:
    MoleculePicker();
    
    /**
     * Pick atom under mouse cursor
     * 
     * @param geom Molecular geometry
     * @param mouse_x Mouse X in screen coordinates
     * @param mouse_y Mouse Y in screen coordinates
     * @param screen_width Viewport width
     * @param screen_height Viewport height
     * @param view_matrix Camera view matrix (4x4, column-major)
     * @param proj_matrix Camera projection matrix (4x4, column-major)
     * 
     * @return Atom pick result (nullopt if no hit)
     */
    std::optional<AtomPick> pick_atom(
        const AtomicGeometry& geom,
        float mouse_x, float mouse_y,
        int screen_width, int screen_height,
        const float* view_matrix,
        const float* proj_matrix) const;
    
    /**
     * Pick bond under mouse cursor
     * 
     * @return Bond pick result (nullopt if no hit)
     */
    std::optional<BondPick> pick_bond(
        const AtomicGeometry& geom,
        float mouse_x, float mouse_y,
        int screen_width, int screen_height,
        const float* view_matrix,
        const float* proj_matrix) const;
    
    /**
     * Pick any object (atom or bond) - returns closest
     * 
     * @param atom_pick Output atom pick (if closest)
     * @param bond_pick Output bond pick (if closest)
     * @return True if atom is closest, false if bond is closest
     */
    bool pick_closest(
        const AtomicGeometry& geom,
        float mouse_x, float mouse_y,
        int screen_width, int screen_height,
        const float* view_matrix,
        const float* proj_matrix,
        std::optional<AtomPick>& atom_pick,
        std::optional<BondPick>& bond_pick) const;
    
    /**
     * Set atom scale (for picking sphere size)
     */
    void set_atom_scale(float scale) { atom_scale_ = scale; }
    
    /**
     * Set bond radius (for picking cylinder size)
     */
    void set_bond_radius(float radius) { bond_radius_ = radius; }
    
private:
    float atom_scale_ = 0.3f;
    float bond_radius_ = 0.15f;
    
    // Ray-sphere intersection
    struct Ray {
        Vec3 origin;
        Vec3 direction;  // Normalized
    };
    
    Ray compute_picking_ray(
        float mouse_x, float mouse_y,
        int screen_width, int screen_height,
        const float* view_matrix,
        const float* proj_matrix) const;
    
    bool ray_sphere_intersect(
        const Ray& ray,
        const Vec3& sphere_center,
        float sphere_radius,
        float& t) const;
    
    bool ray_cylinder_intersect(
        const Ray& ray,
        const Vec3& cylinder_start,
        const Vec3& cylinder_end,
        float cylinder_radius,
        float& t) const;
    
    // Matrix utilities
    void invert_matrix_4x4(const float* m, float* out) const;
    Vec3 transform_point(const float* matrix, const Vec3& point) const;
    Vec3 transform_direction(const float* matrix, const Vec3& dir) const;
};

} // namespace render
} // namespace vsepr
