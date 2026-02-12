#pragma once

#include "core/math_vec3.hpp"
#include <vector>

namespace vsepr {
namespace render {

/**
 * Cylinder geometry for bond rendering
 * 
 * Generates smooth cylinders with proper normals for lighting.
 * Cylinder aligned along +Z axis, centered at origin.
 * Use model matrix to position/orient for each bond.
 */
struct CylinderGeometry {
    std::vector<float> vertices;   // Interleaved: x,y,z, nx,ny,nz (6 floats per vertex)
    std::vector<unsigned int> indices;  // Triangle indices
    
    int vertex_count() const { return static_cast<int>(vertices.size() / 6); }
    int triangle_count() const { return static_cast<int>(indices.size() / 3); }
    
    /**
     * Generate cylinder geometry
     * 
     * @param segments Number of radial segments (8-32 typical)
     *   8  = octagonal (low quality, 16 triangles)
     *   16 = default (good balance, 32 triangles)
     *   32 = smooth (high quality, 64 triangles)
     * 
     * Cylinder properties:
     *   - Height: 1.0 (scale via uniform)
     *   - Radius: 1.0 (scale via uniform)
     *   - Aligned along +Z axis
     *   - Centered at origin: z ∈ [-0.5, 0.5]
     *   - No end caps (bonds visible inside spheres)
     */
    static CylinderGeometry generate(int segments = 16);
};

/**
 * Instanced cylinder rendering data
 * 
 * For rendering N bonds as instanced cylinders (one draw call).
 * Each instance has: start point, end point, radius, color.
 */
struct InstancedCylinderData {
    std::vector<float> start_positions;  // xyz (3 floats per bond)
    std::vector<float> end_positions;    // xyz (3 floats per bond)
    std::vector<float> radii;            // cylinder radius (1 float per bond)
    std::vector<float> colors;           // rgb (3 floats per bond)
    
    int instance_count() const { return static_cast<int>(start_positions.size() / 3); }
    
    /**
     * Add bond instance
     */
    void add_instance(const Vec3& start, const Vec3& end, 
                     float radius, float r, float g, float b) {
        start_positions.push_back(start.x);
        start_positions.push_back(start.y);
        start_positions.push_back(start.z);
        
        end_positions.push_back(end.x);
        end_positions.push_back(end.y);
        end_positions.push_back(end.z);
        
        radii.push_back(radius);
        
        colors.push_back(r);
        colors.push_back(g);
        colors.push_back(b);
    }
    
    void clear() {
        start_positions.clear();
        end_positions.clear();
        radii.clear();
        colors.clear();
    }
};

} // namespace render
} // namespace vsepr
