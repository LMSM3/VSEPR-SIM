#include "sphere.hpp"
#include <cmath>
#include <map>

namespace vsepr {
namespace render {

Vec3 SphereGeometry::normalize(const Vec3& v) {
    float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-8f) return Vec3{0, 0, 1};  // Avoid division by zero
    return Vec3{v.x/len, v.y/len, v.z/len};
}

void SphereGeometry::create_icosahedron(std::vector<Vec3>& vertices,
                                       std::vector<unsigned int>& indices) {
    // Golden ratio for icosahedron vertices
    const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;
    const float a = 1.0f;
    const float b = 1.0f / phi;
    
    // 12 vertices of icosahedron (normalized to unit sphere)
    vertices = {
        normalize(Vec3{ 0,  b, -a}), normalize(Vec3{ b,  a,  0}),
        normalize(Vec3{-b,  a,  0}), normalize(Vec3{ 0,  b,  a}),
        normalize(Vec3{ 0, -b,  a}), normalize(Vec3{-a,  0,  b}),
        normalize(Vec3{ 0, -b, -a}), normalize(Vec3{ a,  0, -b}),
        normalize(Vec3{ a,  0,  b}), normalize(Vec3{-a,  0, -b}),
        normalize(Vec3{ b, -a,  0}), normalize(Vec3{-b, -a,  0})
    };
    
    // 20 triangular faces
    indices = {
        2, 1, 0,   1, 2, 3,   5, 4, 3,   4, 8, 3,
        7, 6, 0,   6, 9, 0,   11, 10, 4, 10, 11, 6,
        9, 5, 2,   5, 9, 11,  8, 7, 1,   7, 8, 10,
        2, 5, 3,   8, 1, 3,   9, 2, 0,   1, 7, 0,
        11, 9, 6,  7, 10, 6,  5, 11, 4,  10, 8, 4
    };
}

int SphereGeometry::add_midpoint(std::vector<Vec3>& vertices,
                                int i1, int i2,
                                std::vector<std::pair<int,int>>& cache) {
    // Check cache (avoid duplicate vertices)
    for (size_t i = 0; i < cache.size(); ++i) {
        auto& pair = cache[i];
        if ((pair.first == i1 && pair.second == i2) ||
            (pair.first == i2 && pair.second == i1)) {
            return static_cast<int>(vertices.size() - cache.size() + i);
        }
    }
    
    // Compute midpoint and normalize to sphere surface
    Vec3 v1 = vertices[i1];
    Vec3 v2 = vertices[i2];
    Vec3 mid = Vec3{(v1.x + v2.x)/2, (v1.y + v2.y)/2, (v1.z + v2.z)/2};
    mid = normalize(mid);
    
    int new_idx = static_cast<int>(vertices.size());
    vertices.push_back(mid);
    cache.push_back({i1, i2});
    
    return new_idx;
}

void SphereGeometry::subdivide(std::vector<Vec3>& vertices,
                              std::vector<unsigned int>& indices) {
    std::vector<unsigned int> new_indices;
    std::vector<std::pair<int,int>> midpoint_cache;
    
    // Subdivide each triangle into 4 smaller triangles
    for (size_t i = 0; i < indices.size(); i += 3) {
        int v1 = indices[i];
        int v2 = indices[i+1];
        int v3 = indices[i+2];
        
        // Get midpoints of edges
        int m12 = add_midpoint(vertices, v1, v2, midpoint_cache);
        int m23 = add_midpoint(vertices, v2, v3, midpoint_cache);
        int m31 = add_midpoint(vertices, v3, v1, midpoint_cache);
        
        // Create 4 new triangles
        new_indices.push_back(v1);  new_indices.push_back(m12); new_indices.push_back(m31);
        new_indices.push_back(v2);  new_indices.push_back(m23); new_indices.push_back(m12);
        new_indices.push_back(v3);  new_indices.push_back(m31); new_indices.push_back(m23);
        new_indices.push_back(m12); new_indices.push_back(m23); new_indices.push_back(m31);
    }
    
    indices = new_indices;
}

SphereGeometry SphereGeometry::generate(int lod) {
    if (lod < 0) lod = 0;
    if (lod > 5) lod = 5;  // Cap at 20,480 triangles
    
    std::vector<Vec3> verts;
    std::vector<unsigned int> inds;
    
    // Start with icosahedron
    create_icosahedron(verts, inds);
    
    // Subdivide recursively
    for (int i = 0; i < lod; ++i) {
        subdivide(verts, inds);
    }
    
    // Interleave vertex data: [x, y, z, nx, ny, nz, ...]
    // For unit sphere, normal = position
    SphereGeometry result;
    result.vertices.reserve(verts.size() * 6);
    for (const auto& v : verts) {
        result.vertices.push_back(v.x);
        result.vertices.push_back(v.y);
        result.vertices.push_back(v.z);
        result.vertices.push_back(v.x);  // Normal = position (unit sphere)
        result.vertices.push_back(v.y);
        result.vertices.push_back(v.z);
    }
    
    result.indices = inds;
    
    return result;
}

} // namespace render
} // namespace vsepr
