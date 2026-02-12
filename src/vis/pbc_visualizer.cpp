#include "pbc_visualizer.hpp"
#include <cmath>

namespace vsepr {
namespace render {

PBCVisualizer::PBCVisualizer() = default;

void PBCVisualizer::set_replication(int nx, int ny, int nz) {
    replicate_x_ = std::max(0, nx);
    replicate_y_ = std::max(0, ny);
    replicate_z_ = std::max(0, nz);
}

Vec3 PBCVisualizer::compute_translation(const AtomicGeometry::PBCBox& box,
                                       int ix, int iy, int iz) const {
    // Translation = ix*a + iy*b + iz*c
    Vec3 trans;
    trans.x = ix * box.a.x + iy * box.b.x + iz * box.c.x;
    trans.y = ix * box.a.y + iy * box.b.y + iz * box.c.y;
    trans.z = ix * box.a.z + iy * box.b.z + iz * box.c.z;
    return trans;
}

AtomicGeometry PBCVisualizer::generate_replicas(const AtomicGeometry& base_geom) const {
    if (!enabled_ || !base_geom.box) {
        return base_geom;  // No PBC, return original
    }
    
    AtomicGeometry result;
    result.box = base_geom.box;  // Preserve box
    
    int n_atoms_per_cell = static_cast<int>(base_geom.atomic_numbers.size());
    int total_cells = (2*replicate_x_ + 1) * (2*replicate_y_ + 1) * (2*replicate_z_ + 1);
    
    // Pre-allocate
    result.atomic_numbers.reserve(n_atoms_per_cell * total_cells);
    result.positions.reserve(n_atoms_per_cell * total_cells);
    if (ghost_atoms_) {
        result.occupancies.reserve(n_atoms_per_cell * total_cells);
    }
    
    // Generate all replicas
    for (int ix = -replicate_x_; ix <= replicate_x_; ++ix) {
        for (int iy = -replicate_y_; iy <= replicate_y_; ++iy) {
            for (int iz = -replicate_z_; iz <= replicate_z_; ++iz) {
                Vec3 trans = compute_translation(*base_geom.box, ix, iy, iz);
                
                // Is this the central cell?
                bool is_central = (ix == 0 && iy == 0 && iz == 0);
                float opacity = is_central ? 1.0f : ghost_opacity_;
                
                // Add all atoms from base geometry with translation
                for (int i = 0; i < n_atoms_per_cell; ++i) {
                    result.atomic_numbers.push_back(base_geom.atomic_numbers[i]);
                    
                    Vec3 pos = base_geom.positions[i];
                    pos.x += trans.x;
                    pos.y += trans.y;
                    pos.z += trans.z;
                    result.positions.push_back(pos);
                    
                    if (ghost_atoms_) {
                        result.occupancies.push_back(opacity);
                    }
                }
            }
        }
    }
    
    // TODO: Replicate bonds (need to handle cross-cell bonds)
    // For now, only replicate atoms
    
    return result;
}

std::vector<PBCVisualizer::BoxEdge> PBCVisualizer::get_box_edges(
    const AtomicGeometry::PBCBox& box) const {
    
    std::vector<BoxEdge> edges;
    if (!show_box_) {
        return edges;
    }
    
    // Box vertices (parallelepiped)
    // Origin at (0, 0, 0)
    Vec3 origin = {0, 0, 0};
    Vec3 a = box.a;
    Vec3 b = box.b;
    Vec3 c = box.c;
    Vec3 ab = {a.x + b.x, a.y + b.y, a.z + b.z};
    Vec3 ac = {a.x + c.x, a.y + c.y, a.z + c.z};
    Vec3 bc = {b.x + c.x, b.y + c.y, b.z + c.z};
    Vec3 abc = {a.x + b.x + c.x, a.y + b.y + c.y, a.z + b.z + c.z};
    
    // 12 edges of parallelepiped
    edges.push_back({origin, a});
    edges.push_back({origin, b});
    edges.push_back({origin, c});
    edges.push_back({a, ab});
    edges.push_back({a, ac});
    edges.push_back({b, ab});
    edges.push_back({b, bc});
    edges.push_back({c, ac});
    edges.push_back({c, bc});
    edges.push_back({ab, abc});
    edges.push_back({ac, abc});
    edges.push_back({bc, abc});
    
    return edges;
}

// ============================================================================
// PBCRendererExtension
// ============================================================================

void PBCRendererExtension::apply_pbc(AtomicGeometry& geom,
                                    const PBCVisualizer& pbc_vis) {
    if (!pbc_vis.is_enabled()) {
        return;
    }
    
    // Generate replicas
    AtomicGeometry replicated = pbc_vis.generate_replicas(geom);
    
    // Replace original with replicated
    geom = replicated;
}

} // namespace render
} // namespace vsepr
