#include "cylinder.hpp"
#include <cmath>

namespace vsepr {
namespace render {

CylinderGeometry CylinderGeometry::generate(int segments) {
    if (segments < 3) segments = 3;
    if (segments > 64) segments = 64;
    
    CylinderGeometry result;
    
    // Create vertices for top and bottom circles
    // Cylinder: z ∈ [-0.5, 0.5], radius = 1.0
    const float height_half = 0.5f;
    const float radius = 1.0f;
    
    // Generate circle vertices
    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * M_PI * i / segments;
        float cos_theta = std::cos(theta);
        float sin_theta = std::sin(theta);
        
        float x = radius * cos_theta;
        float y = radius * sin_theta;
        
        // Normal points radially outward (perpendicular to Z axis)
        float nx = cos_theta;
        float ny = sin_theta;
        float nz = 0.0f;
        
        // Bottom vertex (z = -0.5)
        result.vertices.push_back(x);
        result.vertices.push_back(y);
        result.vertices.push_back(-height_half);
        result.vertices.push_back(nx);
        result.vertices.push_back(ny);
        result.vertices.push_back(nz);
        
        // Top vertex (z = +0.5)
        result.vertices.push_back(x);
        result.vertices.push_back(y);
        result.vertices.push_back(height_half);
        result.vertices.push_back(nx);
        result.vertices.push_back(ny);
        result.vertices.push_back(nz);
    }
    
    // Generate triangle strip indices
    // Each quad (bottom-top pair) = 2 triangles
    for (int i = 0; i < segments; ++i) {
        int bottom1 = i * 2;
        int top1 = i * 2 + 1;
        int bottom2 = (i + 1) * 2;
        int top2 = (i + 1) * 2 + 1;
        
        // Triangle 1: bottom1, top1, bottom2
        result.indices.push_back(bottom1);
        result.indices.push_back(top1);
        result.indices.push_back(bottom2);
        
        // Triangle 2: bottom2, top1, top2
        result.indices.push_back(bottom2);
        result.indices.push_back(top1);
        result.indices.push_back(top2);
    }
    
    return result;
}

} // namespace render
} // namespace vsepr
