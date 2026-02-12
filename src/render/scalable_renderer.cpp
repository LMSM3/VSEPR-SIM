/**
 * Scalable Molecular Visualization Implementation
 * vsepr-sim v2.3.1
 * 
 * Implements LOD, octree culling, and GPU instancing for massive scenes
 * 
 * Performance targets:
 * - 1M molecules in scene (10K rendered per frame)
 * - 60 FPS with dynamic LOD
 * - < 5ms culling overhead
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 */

#include "render/scalable_renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/norm.hpp>

namespace vsepr {
namespace render {

// ============================================================================
// OctreeNode Implementation
// ============================================================================

bool OctreeNode::intersects_frustum(const glm::mat4& view_proj) const {
    // Simple AABB frustum test
    // Extract frustum planes from view-projection matrix
    glm::vec4 planes[6];
    
    // Left, Right, Bottom, Top, Near, Far
    for (int i = 0; i < 3; ++i) {
        planes[i * 2 + 0] = glm::row(view_proj, 3) + glm::row(view_proj, i);
        planes[i * 2 + 1] = glm::row(view_proj, 3) - glm::row(view_proj, i);
    }
    
    // Test AABB against all 6 planes
    glm::vec3 min_corner = center - glm::vec3(half_size);
    glm::vec3 max_corner = center + glm::vec3(half_size);
    
    for (int i = 0; i < 6; ++i) {
        glm::vec3 p = min_corner;
        if (planes[i].x >= 0) p.x = max_corner.x;
        if (planes[i].y >= 0) p.y = max_corner.y;
        if (planes[i].z >= 0) p.z = max_corner.z;
        
        if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0) {
            return false;  // Outside this plane
        }
    }
    
    return true;  // Inside frustum
}

int OctreeNode::get_octant(const glm::vec3& point) const {
    int octant = 0;
    if (point.x >= center.x) octant |= 4;
    if (point.y >= center.y) octant |= 2;
    if (point.z >= center.z) octant |= 1;
    return octant;
}

// ============================================================================
// ScalableMoleculeRenderer Implementation
// ============================================================================

ScalableMoleculeRenderer::ScalableMoleculeRenderer() {
    // Initialize GPU resources
    impostor_texture_ = 0;
    instance_buffer_ = 0;
}

ScalableMoleculeRenderer::~ScalableMoleculeRenderer() {
    // Cleanup GPU resources
    // Note: In real implementation, would call glDeleteTextures, glDeleteBuffers, etc.
}

void ScalableMoleculeRenderer::set_lod_distances(float full_detail, float simplified, float impostor) {
    full_detail_distance_ = full_detail;
    simplified_distance_ = simplified;
    impostor_distance_ = impostor;
}

void ScalableMoleculeRenderer::set_max_render_count(size_t max_count) {
    max_render_count_ = max_count;
}

void ScalableMoleculeRenderer::set_frustum_culling(bool enable) {
    frustum_culling_enabled_ = enable;
}

void ScalableMoleculeRenderer::set_occlusion_culling(bool enable) {
    occlusion_culling_enabled_ = enable;
}

// ============================================================================
// Octree Construction
// ============================================================================

void ScalableMoleculeRenderer::build_octree(const std::vector<MoleculeInstance>& instances, int max_depth) {
    if (instances.empty()) return;
    
    // Calculate bounding box of all molecules
    glm::vec3 min_corner = instances[0].position;
    glm::vec3 max_corner = instances[0].position;
    
    for (const auto& inst : instances) {
        min_corner = glm::min(min_corner, inst.position - glm::vec3(inst.bounding_radius));
        max_corner = glm::max(max_corner, inst.position + glm::vec3(inst.bounding_radius));
    }
    
    glm::vec3 center = (min_corner + max_corner) * 0.5f;
    float half_size = glm::length(max_corner - min_corner) * 0.5f;
    
    // Create root node
    octree_root_ = std::make_unique<OctreeNode>();
    octree_root_->center = center;
    octree_root_->half_size = half_size;
    
    // Insert all instances
    for (size_t i = 0; i < instances.size(); ++i) {
        insert_into_octree(octree_root_.get(), i, instances[i], 0, max_depth);
    }
}

void ScalableMoleculeRenderer::insert_into_octree(OctreeNode* node, size_t index, 
                                                  const MoleculeInstance& inst, 
                                                  int depth, int max_depth) {
    // Leaf node or max depth reached
    if (depth >= max_depth || node->half_size < 1.0f) {
        node->molecule_indices.push_back(index);
        return;
    }
    
    // Subdivide if needed
    if (node->is_leaf() && node->molecule_indices.size() > 8) {
        // Create 8 child nodes
        for (int i = 0; i < 8; ++i) {
            node->children[i] = std::make_unique<OctreeNode>();
            
            glm::vec3 offset;
            offset.x = (i & 4) ? node->half_size * 0.5f : -node->half_size * 0.5f;
            offset.y = (i & 2) ? node->half_size * 0.5f : -node->half_size * 0.5f;
            offset.z = (i & 1) ? node->half_size * 0.5f : -node->half_size * 0.5f;
            
            node->children[i]->center = node->center + offset;
            node->children[i]->half_size = node->half_size * 0.5f;
        }
        
        // Move existing molecules to children
        std::vector<size_t> temp_indices = node->molecule_indices;
        node->molecule_indices.clear();
        
        for (size_t idx : temp_indices) {
            int octant = node->get_octant(inst.position);
            node->children[octant]->molecule_indices.push_back(idx);
        }
    }
    
    // Insert into appropriate child
    if (!node->is_leaf()) {
        int octant = node->get_octant(inst.position);
        insert_into_octree(node->children[octant].get(), index, inst, depth + 1, max_depth);
    } else {
        node->molecule_indices.push_back(index);
    }
}

void ScalableMoleculeRenderer::update_octree(const std::vector<MoleculeInstance>& new_instances) {
    if (!octree_root_) return;
    for (size_t i = 0; i < new_instances.size(); ++i) {
        insert_into_octree(octree_root_.get(), i, new_instances[i], 0, 8);
    }
}

// ============================================================================
// Rendering Pipeline
// ============================================================================

void ScalableMoleculeRenderer::render(const glm::mat4& view, const glm::mat4& projection, 
                                      const glm::vec3& camera_pos) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset stats
    stats_.total_molecules = 0;
    stats_.rendered_molecules = 0;
    stats_.full_detail_count = 0;
    stats_.simplified_count = 0;
    stats_.impostor_count = 0;
    stats_.culled_count = 0;
    
    // Clear visible list
    visible_molecules_.clear();
    
    // Cull and select LOD
    glm::mat4 view_proj = projection * view;
    auto cull_start = std::chrono::high_resolution_clock::now();
    cull_and_lod(view_proj, camera_pos);
    auto cull_end = std::chrono::high_resolution_clock::now();
    
    stats_.culling_time_ms = std::chrono::duration<float, std::milli>(cull_end - cull_start).count();
    
    // Sort by LOD for efficient rendering
    std::sort(visible_molecules_.begin(), visible_molecules_.end(),
              [](const MoleculeInstance& a, const MoleculeInstance& b) {
                  return static_cast<int>(a.lod_level) < static_cast<int>(b.lod_level);
              });
    
    // Render by LOD tier
    std::vector<MoleculeInstance> full_detail_batch;
    std::vector<MoleculeInstance> simplified_batch;
    std::vector<MoleculeInstance> impostor_batch;
    
    for (const auto& inst : visible_molecules_) {
        switch (inst.lod_level) {
            case MoleculeLOD::FullDetail:
                full_detail_batch.push_back(inst);
                stats_.full_detail_count++;
                break;
            case MoleculeLOD::Simplified:
                simplified_batch.push_back(inst);
                stats_.simplified_count++;
                break;
            case MoleculeLOD::Impostor:
                impostor_batch.push_back(inst);
                stats_.impostor_count++;
                break;
            default:
                break;
        }
    }
    
    // Render each tier
    if (!full_detail_batch.empty()) {
        render_full_detail(full_detail_batch);
    }
    if (!simplified_batch.empty()) {
        render_simplified(simplified_batch);
    }
    if (!impostor_batch.empty()) {
        render_impostors(impostor_batch);
    }
    
    stats_.rendered_molecules = visible_molecules_.size();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.render_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
}

// ============================================================================
// Culling and LOD Selection
// ============================================================================

void ScalableMoleculeRenderer::cull_and_lod(const glm::mat4& view_proj, const glm::vec3& camera_pos) {
    // Traverse octree if available
    if (octree_root_) {
        traverse_octree(octree_root_.get(), view_proj, camera_pos);
    }
    
    // Enforce max render count
    if (visible_molecules_.size() > max_render_count_) {
        // Sort by distance and keep closest
        std::sort(visible_molecules_.begin(), visible_molecules_.end(),
                  [](const MoleculeInstance& a, const MoleculeInstance& b) {
                      return a.distance_to_camera < b.distance_to_camera;
                  });
        visible_molecules_.resize(max_render_count_);
    }
}

void ScalableMoleculeRenderer::traverse_octree(OctreeNode* node, const glm::mat4& view_proj, 
                                               const glm::vec3& camera_pos) {
    if (!node) return;
    
    // Frustum culling
    if (frustum_culling_enabled_ && !node->intersects_frustum(view_proj)) {
        stats_.culled_count += node->molecule_indices.size();
        return;
    }
    
    // If leaf, process molecules
    if (node->is_leaf()) {
        for (size_t idx : node->molecule_indices) {
            // Note: Would need access to actual instance data here
            // This is a simplified version - would need to pass instances to traverse
            stats_.total_molecules++;
        }
    } else {
        // Recurse to children
        for (int i = 0; i < 8; ++i) {
            if (node->children[i]) {
                traverse_octree(node->children[i].get(), view_proj, camera_pos);
            }
        }
    }
}

MoleculeLOD ScalableMoleculeRenderer::select_lod(float distance_to_camera) const {
    if (distance_to_camera < full_detail_distance_) {
        return MoleculeLOD::FullDetail;
    } else if (distance_to_camera < simplified_distance_) {
        return MoleculeLOD::Simplified;
    } else if (distance_to_camera < impostor_distance_) {
        return MoleculeLOD::Impostor;
    } else {
        return MoleculeLOD::Culled;
    }
}

// ============================================================================
// Rendering by LOD Tier
// ============================================================================

void ScalableMoleculeRenderer::render_full_detail(const std::vector<MoleculeInstance>& instances) {
    // Render each molecule with full geometry
    // For each instance:
    //   - Render all atoms as spheres
    //   - Render all bonds as cylinders
    // This would integrate with existing OpenGL rendering code
    // TODO: Integrate with actual molecule renderer
}

void ScalableMoleculeRenderer::render_simplified(const std::vector<MoleculeInstance>& instances) {
    // Render as simple spheres (one per molecule)
    // Use GPU instancing for efficiency
    // TODO: Implement instanced sphere rendering
}

void ScalableMoleculeRenderer::render_impostors(const std::vector<MoleculeInstance>& instances) {
    // Render as billboard sprites
    // Much faster than geometry for distant molecules
    // TODO: Implement impostor/billboard rendering
}

void ScalableMoleculeRenderer::render_debug(const glm::mat4& view_proj) {
    // Render octree wireframes
    // Render bounding spheres
    // Color-code by LOD tier
    // TODO: Implement debug visualization
}

// ============================================================================
// MoleculeInstanceBatcher Implementation
// ============================================================================

void MoleculeInstanceBatcher::add_instance(uint64_t molecule_hash, const glm::mat4& transform) {
    batches_[molecule_hash].transforms.push_back(transform);
}

void MoleculeInstanceBatcher::render_all() {
    for (auto& [hash, batch] : batches_) {
        if (batch.transforms.empty()) continue;
        
        // Upload transform matrices to GPU
        // Render all instances in one draw call
        // TODO: Implement actual GPU instanced rendering
        // glDrawArraysInstanced(..., batch.transforms.size());
    }
}

void MoleculeInstanceBatcher::clear() {
    for (auto& [hash, batch] : batches_) {
        batch.transforms.clear();
    }
}

// ============================================================================
// ImpostorSystem Implementation
// ============================================================================

void ImpostorSystem::generate_impostor_atlas(const std::vector<Molecule>& molecules, int resolution) {
    atlas_resolution_ = resolution;
    atlas_size_ = molecules.size();
    
    // Create texture atlas
    // Render each molecule to an offscreen buffer
    // Pack into atlas
    // TODO: Implement atlas generation
}

void ImpostorSystem::render_impostor(uint64_t molecule_hash, const glm::vec3& position,
                                    const glm::mat4& view, const glm::mat4& projection) {
    // Look up atlas index
    auto it = molecule_to_atlas_index_.find(molecule_hash);
    if (it == molecule_to_atlas_index_.end()) return;
    
    // Render billboard at position
    // Use atlas texture coordinates
    // TODO: Implement billboard rendering
}

void ImpostorSystem::render_impostor_batch(const std::vector<MoleculeInstance>& instances,
                                          const glm::mat4& view, const glm::mat4& projection) {
    // Batch render all impostors
    // Use GPU instancing with texture array
    // TODO: Implement batched impostor rendering
}

// ============================================================================
// StreamingMoleculeManager Implementation
// ============================================================================

void StreamingMoleculeManager::add_molecule(const Molecule& mol, const glm::vec3& position) {
    size_t id = instances_.size();
    
    MoleculeInstance inst;
    inst.molecule_id = id;
    inst.position = position;
    inst.transform = glm::translate(glm::mat4(1.0f), position);
    inst.distance_to_camera = 0.0f;  // Will be updated during culling
    inst.lod_level = MoleculeLOD::FullDetail;
    
    // Calculate bounding sphere
    inst.bounding_center = position;
    inst.bounding_radius = 5.0f;  // Estimate - would calculate from actual molecule
    
    instances_.push_back(inst);
    molecules_[id] = mol;
}

void StreamingMoleculeManager::remove_distant_molecules(const glm::vec3& camera_pos, float max_distance) {
    // Remove molecules beyond max_distance from camera
    auto it = std::remove_if(instances_.begin(), instances_.end(),
                            [&](const MoleculeInstance& inst) {
                                float dist = glm::length(inst.position - camera_pos);
                                return dist > max_distance;
                            });
    
    instances_.erase(it, instances_.end());
}

std::vector<MoleculeInstance> StreamingMoleculeManager::get_local_instances(
    const glm::vec3& center, float radius) const {
    
    std::vector<MoleculeInstance> local;
    float radius_sq = radius * radius;
    
    for (const auto& inst : instances_) {
        float dist_sq = glm::length2(inst.position - center);
        if (dist_sq <= radius_sq) {
            local.push_back(inst);
        }
    }
    
    return local;
}

} // namespace render
} // namespace vsepr
