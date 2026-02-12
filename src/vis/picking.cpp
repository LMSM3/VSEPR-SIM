#include "picking.hpp"
#include "renderer_base.hpp"
#include <cmath>
#include <limits>
#include <algorithm>

namespace vsepr {
namespace render {

MoleculePicker::MoleculePicker() = default;

// ============================================================================
// Ray Computation
// ============================================================================

MoleculePicker::Ray MoleculePicker::compute_picking_ray(
    float mouse_x, float mouse_y,
    int screen_width, int screen_height,
    const float* view_matrix,
    const float* proj_matrix) const {
    
    // Convert mouse to NDC (Normalized Device Coordinates)
    float ndc_x = (2.0f * mouse_x) / screen_width - 1.0f;
    float ndc_y = 1.0f - (2.0f * mouse_y) / screen_height;  // Flip Y
    
    // Clip space (near plane)
    Vec3 clip_near = {ndc_x, ndc_y, -1.0f};
    Vec3 clip_far = {ndc_x, ndc_y, 1.0f};
    
    // Invert projection matrix
    float inv_proj[16];
    invert_matrix_4x4(proj_matrix, inv_proj);
    
    // Invert view matrix
    float inv_view[16];
    invert_matrix_4x4(view_matrix, inv_view);
    
    // Transform to view space
    Vec3 view_near = transform_point(inv_proj, clip_near);
    Vec3 view_far = transform_point(inv_proj, clip_far);
    
    // Transform to world space
    Vec3 world_near = transform_point(inv_view, view_near);
    Vec3 world_far = transform_point(inv_view, view_far);
    
    // Construct ray
    Ray ray;
    ray.origin = world_near;
    
    Vec3 dir = {
        world_far.x - world_near.x,
        world_far.y - world_near.y,
        world_far.z - world_near.z
    };
    
    // Normalize direction
    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (len > 1e-6f) {
        ray.direction.x = dir.x / len;
        ray.direction.y = dir.y / len;
        ray.direction.z = dir.z / len;
    } else {
        ray.direction = {0, 0, -1};  // Default
    }
    
    return ray;
}

// ============================================================================
// Ray-Sphere Intersection
// ============================================================================

bool MoleculePicker::ray_sphere_intersect(
    const Ray& ray,
    const Vec3& sphere_center,
    float sphere_radius,
    float& t) const {
    
    // Ray: P(t) = origin + t * direction
    // Sphere: |P - C|² = r²
    // Solve: |origin + t*dir - C|² = r²
    
    Vec3 oc = {
        ray.origin.x - sphere_center.x,
        ray.origin.y - sphere_center.y,
        ray.origin.z - sphere_center.z
    };
    
    float a = ray.direction.x * ray.direction.x +
              ray.direction.y * ray.direction.y +
              ray.direction.z * ray.direction.z;
    
    float b = 2.0f * (oc.x * ray.direction.x +
                      oc.y * ray.direction.y +
                      oc.z * ray.direction.z);
    
    float c = oc.x*oc.x + oc.y*oc.y + oc.z*oc.z - sphere_radius*sphere_radius;
    
    float discriminant = b*b - 4*a*c;
    
    if (discriminant < 0) {
        return false;  // No intersection
    }
    
    // Quadratic formula
    float sqrt_disc = std::sqrt(discriminant);
    float t1 = (-b - sqrt_disc) / (2.0f * a);
    float t2 = (-b + sqrt_disc) / (2.0f * a);
    
    // Pick closest positive t
    if (t1 > 0) {
        t = t1;
        return true;
    } else if (t2 > 0) {
        t = t2;
        return true;
    }
    
    return false;
}

// ============================================================================
// Ray-Cylinder Intersection
// ============================================================================

bool MoleculePicker::ray_cylinder_intersect(
    const Ray& ray,
    const Vec3& cylinder_start,
    const Vec3& cylinder_end,
    float cylinder_radius,
    float& t) const {
    
    // Simplified: cylinder as infinite line + capped length
    // More accurate: use proper finite cylinder intersection
    
    Vec3 axis = {
        cylinder_end.x - cylinder_start.x,
        cylinder_end.y - cylinder_start.y,
        cylinder_end.z - cylinder_start.z
    };
    
    float axis_len = std::sqrt(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
    if (axis_len < 1e-6f) {
        return false;  // Degenerate cylinder
    }
    
    // Normalize axis
    axis.x /= axis_len;
    axis.y /= axis_len;
    axis.z /= axis_len;
    
    // Vector from cylinder start to ray origin
    Vec3 delta = {
        ray.origin.x - cylinder_start.x,
        ray.origin.y - cylinder_start.y,
        ray.origin.z - cylinder_start.z
    };
    
    // Project ray direction and delta onto plane perpendicular to axis
    float dot_dir_axis = ray.direction.x * axis.x +
                         ray.direction.y * axis.y +
                         ray.direction.z * axis.z;
    
    float dot_delta_axis = delta.x * axis.x +
                           delta.y * axis.y +
                           delta.z * axis.z;
    
    Vec3 dir_perp = {
        ray.direction.x - dot_dir_axis * axis.x,
        ray.direction.y - dot_dir_axis * axis.y,
        ray.direction.z - dot_dir_axis * axis.z
    };
    
    Vec3 delta_perp = {
        delta.x - dot_delta_axis * axis.x,
        delta.y - dot_delta_axis * axis.y,
        delta.z - dot_delta_axis * axis.z
    };
    
    // Quadratic equation for infinite cylinder
    float a = dir_perp.x*dir_perp.x + dir_perp.y*dir_perp.y + dir_perp.z*dir_perp.z;
    float b = 2.0f * (delta_perp.x*dir_perp.x + delta_perp.y*dir_perp.y + delta_perp.z*dir_perp.z);
    float c = delta_perp.x*delta_perp.x + delta_perp.y*delta_perp.y + delta_perp.z*delta_perp.z
              - cylinder_radius*cylinder_radius;
    
    float disc = b*b - 4*a*c;
    if (disc < 0) {
        return false;
    }
    
    float sqrt_disc = std::sqrt(disc);
    float t1 = (-b - sqrt_disc) / (2.0f * a);
    float t2 = (-b + sqrt_disc) / (2.0f * a);
    
    // Check if intersection is within cylinder length
    auto check_length = [&](float test_t) -> bool {
        Vec3 point = {
            ray.origin.x + test_t * ray.direction.x,
            ray.origin.y + test_t * ray.direction.y,
            ray.origin.z + test_t * ray.direction.z
        };
        
        Vec3 to_point = {
            point.x - cylinder_start.x,
            point.y - cylinder_start.y,
            point.z - cylinder_start.z
        };
        
        float projection = to_point.x * axis.x + to_point.y * axis.y + to_point.z * axis.z;
        return projection >= 0 && projection <= axis_len;
    };
    
    if (t1 > 0 && check_length(t1)) {
        t = t1;
        return true;
    } else if (t2 > 0 && check_length(t2)) {
        t = t2;
        return true;
    }
    
    return false;
}

// ============================================================================
// Picking Functions
// ============================================================================

std::optional<AtomPick> MoleculePicker::pick_atom(
    const AtomicGeometry& geom,
    float mouse_x, float mouse_y,
    int screen_width, int screen_height,
    const float* view_matrix,
    const float* proj_matrix) const {
    
    Ray ray = compute_picking_ray(mouse_x, mouse_y, screen_width, screen_height,
                                  view_matrix, proj_matrix);
    
    float closest_t = std::numeric_limits<float>::max();
    int closest_atom = -1;
    
    // Test all atoms
    for (size_t i = 0; i < geom.atomic_numbers.size(); ++i) {
        int Z = geom.atomic_numbers[i];
        Vec3 pos = geom.positions[i];
        float vdw_radius = MoleculeRendererBase::get_vdw_radius(Z);
        float render_radius = vdw_radius * atom_scale_;
        
        float t;
        if (ray_sphere_intersect(ray, pos, render_radius, t)) {
            if (t < closest_t) {
                closest_t = t;
                closest_atom = static_cast<int>(i);
            }
        }
    }
    
    if (closest_atom >= 0) {
        AtomPick pick;
        pick.atom_index = closest_atom;
        pick.distance = closest_t;
        pick.position = geom.positions[closest_atom];
        pick.atomic_number = geom.atomic_numbers[closest_atom];
        return pick;
    }
    
    return std::nullopt;
}

std::optional<BondPick> MoleculePicker::pick_bond(
    const AtomicGeometry& geom,
    float mouse_x, float mouse_y,
    int screen_width, int screen_height,
    const float* view_matrix,
    const float* proj_matrix) const {
    
    Ray ray = compute_picking_ray(mouse_x, mouse_y, screen_width, screen_height,
                                  view_matrix, proj_matrix);
    
    float closest_t = std::numeric_limits<float>::max();
    int closest_bond = -1;
    
    // Test all bonds
    for (size_t i = 0; i < geom.bonds.size(); ++i) {
        const auto& bond = geom.bonds[i];
        int atom1 = bond.first;
        int atom2 = bond.second;
        
        if (atom1 >= (int)geom.positions.size() || atom2 >= (int)geom.positions.size()) {
            continue;
        }
        
        Vec3 pos1 = geom.positions[atom1];
        Vec3 pos2 = geom.positions[atom2];
        
        float t;
        if (ray_cylinder_intersect(ray, pos1, pos2, bond_radius_, t)) {
            if (t < closest_t) {
                closest_t = t;
                closest_bond = static_cast<int>(i);
            }
        }
    }
    
    if (closest_bond >= 0) {
        const auto& bond = geom.bonds[closest_bond];
        Vec3 pos1 = geom.positions[bond.first];
        Vec3 pos2 = geom.positions[bond.second];
        
        Vec3 midpoint = {
            (pos1.x + pos2.x) / 2.0f,
            (pos1.y + pos2.y) / 2.0f,
            (pos1.z + pos2.z) / 2.0f
        };
        
        float dx = pos2.x - pos1.x;
        float dy = pos2.y - pos1.y;
        float dz = pos2.z - pos1.z;
        float length = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        BondPick pick;
        pick.bond_index = closest_bond;
        pick.atom1 = bond.first;
        pick.atom2 = bond.second;
        pick.distance = closest_t;
        pick.midpoint = midpoint;
        pick.length = length;
        return pick;
    }
    
    return std::nullopt;
}

bool MoleculePicker::pick_closest(
    const AtomicGeometry& geom,
    float mouse_x, float mouse_y,
    int screen_width, int screen_height,
    const float* view_matrix,
    const float* proj_matrix,
    std::optional<AtomPick>& atom_pick,
    std::optional<BondPick>& bond_pick) const {
    
    atom_pick = pick_atom(geom, mouse_x, mouse_y, screen_width, screen_height,
                         view_matrix, proj_matrix);
    
    bond_pick = pick_bond(geom, mouse_x, mouse_y, screen_width, screen_height,
                         view_matrix, proj_matrix);
    
    // Return true if atom is closer
    if (atom_pick && bond_pick) {
        return atom_pick->distance < bond_pick->distance;
    } else if (atom_pick) {
        return true;
    } else if (bond_pick) {
        return false;
    }
    
    return false;  // No hit
}

// ============================================================================
// Matrix Utilities
// ============================================================================

void MoleculePicker::invert_matrix_4x4(const float* m, float* out) const {
    // Simplified 4x4 matrix inversion (for view/projection matrices)
    // This is a full implementation - would need proper inverse computation
    // For now, use identity as placeholder
    for (int i = 0; i < 16; ++i) {
        out[i] = (i % 5 == 0) ? 1.0f : 0.0f;  // Identity matrix
    }
    // TODO: Implement proper matrix inversion
}

Vec3 MoleculePicker::transform_point(const float* matrix, const Vec3& point) const {
    // 4x4 matrix * homogeneous point (column-major)
    float x = matrix[0]*point.x + matrix[4]*point.y + matrix[8]*point.z + matrix[12];
    float y = matrix[1]*point.x + matrix[5]*point.y + matrix[9]*point.z + matrix[13];
    float z = matrix[2]*point.x + matrix[6]*point.y + matrix[10]*point.z + matrix[14];
    float w = matrix[3]*point.x + matrix[7]*point.y + matrix[11]*point.z + matrix[15];
    
    if (std::abs(w) > 1e-6f) {
        return {x/w, y/w, z/w};
    }
    return {x, y, z};
}

Vec3 MoleculePicker::transform_direction(const float* matrix, const Vec3& dir) const {
    // Transform direction (w=0, so translation doesn't apply)
    float x = matrix[0]*dir.x + matrix[4]*dir.y + matrix[8]*dir.z;
    float y = matrix[1]*dir.x + matrix[5]*dir.y + matrix[9]*dir.z;
    float z = matrix[2]*dir.x + matrix[6]*dir.y + matrix[10]*dir.z;
    return {x, y, z};
}

} // namespace render
} // namespace vsepr
