#pragma once

#include "core/math_vec3.hpp"
#include <vector>

namespace vsepr {
namespace render {

/**
 * Sphere geometry via icosahedron subdivision
 * 
 * Generates high-quality spheres using recursive subdivision of an icosahedron.
 * All vertices lie exactly on unit sphere surface (proper normals for lighting).
 * 
 * Triangle counts:
 *   LOD 0: 20 triangles (icosahedron - debugging only)
 *   LOD 1: 80 triangles (1 subdivision)
 *   LOD 2: 320 triangles (2 subdivisions) - LOW quality
 *   LOD 3: 1,280 triangles (3 subdivisions) - MEDIUM quality
 *   LOD 4: 5,120 triangles (4 subdivisions) - HIGH quality
 *   LOD 5: 20,480 triangles (5 subdivisions) - ULTRA quality
 */
struct SphereGeometry {
    std::vector<float> vertices;   // Interleaved: x,y,z, nx,ny,nz (6 floats per vertex)
    std::vector<unsigned int> indices;  // Triangle indices
    
    int vertex_count() const { return static_cast<int>(vertices.size() / 6); }
    int triangle_count() const { return static_cast<int>(indices.size() / 3); }
    
    /**
     * Generate sphere geometry
     * 
     * @param lod Level of detail (0-5)
     *   0 = 20 triangles (wireframe debugging)
     *   2 = 320 triangles (LOW quality, 60+ FPS for 10k atoms)
     *   3 = 1,280 triangles (MEDIUM quality, default)
     *   4 = 5,120 triangles (HIGH quality, proteins)
     *   5 = 20,480 triangles (ULTRA quality, publication figures)
     * 
     * Vertices are unit sphere (scale in shader via uniform)
     * Normals point outward (for Phong/PBR lighting)
     */
    static SphereGeometry generate(int lod = 3);
    
private:
    // Icosahedron base vertices (12 vertices on unit sphere)
    static void create_icosahedron(std::vector<Vec3>& vertices,
                                   std::vector<unsigned int>& indices);
    
    // Subdivide triangle: midpoint of each edge pushed to sphere surface
    static void subdivide(std::vector<Vec3>& vertices,
                         std::vector<unsigned int>& indices);
    
    // Normalize vector to unit length
    static Vec3 normalize(const Vec3& v);
    
    // Find or add midpoint vertex (avoid duplicates)
    static int add_midpoint(std::vector<Vec3>& vertices,
                           int i1, int i2,
                           std::vector<std::pair<int,int>>& cache);
};

/**
 * Instanced sphere rendering data
 * 
 * For rendering N atoms as instanced spheres (one draw call).
 * Each instance has: position, radius, color.
 */
struct InstancedSphereData {
    std::vector<float> positions;  // xyz (3 floats per atom)
    std::vector<float> radii;      // sphere radius (1 float per atom)
    std::vector<float> colors;     // rgb (3 floats per atom)
    
    int instance_count() const { return static_cast<int>(positions.size() / 3); }
    
    /**
     * Add atom instance
     */
    void add_instance(const Vec3& pos, float radius, float r, float g, float b) {
        positions.push_back(pos.x);
        positions.push_back(pos.y);
        positions.push_back(pos.z);
        
        radii.push_back(radius);
        
        colors.push_back(r);
        colors.push_back(g);
        colors.push_back(b);
    }
    
    void clear() {
        positions.clear();
        radii.clear();
        colors.clear();
    }
};

} // namespace render
} // namespace vsepr
