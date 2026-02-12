/**
 * Scalable Molecular Visualization System
 * vsepr-sim v2.3.1
 * 
 * PROBLEM: Can't render millions of molecules at full detail
 * SOLUTION: Local sampling + Level of Detail (LOD) + Spatial culling
 * 
 * Rendering Tiers:
 * 1. High Detail (nearby) - Full atoms + bonds (< 10 units from camera)
 * 2. Medium Detail (mid) - Simplified spheres (10-50 units)
 * 3. Low Detail (far) - Billboards/impostors (50-200 units)
 * 4. Culled (very far) - Not rendered (> 200 units)
 * 
 * Features:
 * - Spatial octree for fast culling
 * - GPU instancing for identical molecules
 * - Impostor rendering for distant molecules
 * - Dynamic LOD based on camera distance
 * - Frustum culling (only render what's visible)
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 * License: MIT
 */

#pragma once

#include "sim/molecule.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/norm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace vsepr {
namespace render {

// ============================================================================
// Level of Detail (LOD) Definitions
// ============================================================================

enum class MoleculeLOD {
    FullDetail,      // All atoms as spheres, all bonds as cylinders
    Simplified,      // Single sphere per molecule (center of mass)
    Impostor,        // Billboard sprite (pre-rendered image)
    Culled           // Not rendered (too far or outside frustum)
};

// ============================================================================
// Spatial Octree for Culling
// ============================================================================

struct OctreeNode {
    glm::vec3 center;
    float half_size;
    
    // Child nodes (8 octants)
    std::unique_ptr<OctreeNode> children[8];
    
    // Molecules in this node (if leaf)
    std::vector<size_t> molecule_indices;
    
    bool is_leaf() const {
        return children[0] == nullptr;
    }
    
    // Check if AABB intersects frustum
    bool intersects_frustum(const glm::mat4& view_proj) const;
    
    // Get octant index for a point
    int get_octant(const glm::vec3& point) const;
};

// ============================================================================
// Renderable Molecule Instance
// ============================================================================

struct MoleculeInstance {
    size_t molecule_id;       // Index in continuous generation buffer
    glm::vec3 position;       // World position
    glm::mat4 transform;      // Full transform matrix
    float distance_to_camera; // For LOD selection
    MoleculeLOD lod_level;    // Current LOD tier
    
    // Bounding sphere (for culling)
    glm::vec3 bounding_center;
    float bounding_radius;
    
    // Cached rendering data
    uint32_t vertex_buffer_offset;  // Offset in GPU buffer
    uint32_t instance_id;            // For GPU instancing
};

// ============================================================================
// Scalable Molecular Renderer
// ============================================================================

class ScalableMoleculeRenderer {
public:
    ScalableMoleculeRenderer();
    ~ScalableMoleculeRenderer();
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * Set LOD distance thresholds
     * @param full_detail_distance Max distance for full detail (default: 10)
     * @param simplified_distance Max distance for simplified (default: 50)
     * @param impostor_distance Max distance for impostors (default: 200)
     */
    void set_lod_distances(float full_detail, float simplified, float impostor);
    
    /**
     * Set maximum molecules to render per frame
     * @param max_count Hard limit (default: 10,000)
     */
    void set_max_render_count(size_t max_count);
    
    /**
     * Enable/disable frustum culling
     */
    void set_frustum_culling(bool enable);
    
    /**
     * Enable/disable occlusion culling
     */
    void set_occlusion_culling(bool enable);
    
    // ========================================================================
    // Spatial Structure Management
    // ========================================================================
    
    /**
     * Build octree from molecule positions
     * @param instances All molecule instances
     * @param max_depth Maximum octree depth (default: 8)
     */
    void build_octree(const std::vector<MoleculeInstance>& instances, int max_depth = 8);
    
    /**
     * Update octree incrementally (for streaming generation)
     * @param new_instances Newly generated molecules
     */
    void update_octree(const std::vector<MoleculeInstance>& new_instances);
    
    // ========================================================================
    // Rendering Pipeline
    // ========================================================================
    
    /**
     * Render molecules with LOD and culling
     * @param view View matrix (camera)
     * @param projection Projection matrix
     * @param camera_pos Camera position in world space
     */
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos);
    
    /**
     * Render debug visualization (octree, bounding spheres, LOD colors)
     */
    void render_debug(const glm::mat4& view_proj);
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    struct RenderStats {
        size_t total_molecules;        // Total in scene
        size_t rendered_molecules;     // Actually rendered this frame
        size_t full_detail_count;      // High LOD
        size_t simplified_count;       // Mid LOD
        size_t impostor_count;         // Low LOD
        size_t culled_count;           // Not rendered
        float render_time_ms;          // Frame time
        float culling_time_ms;         // Culling overhead
    };
    
    RenderStats get_stats() const { return stats_; }
    
private:
    // LOD distance thresholds
    float full_detail_distance_ = 10.0f;
    float simplified_distance_ = 50.0f;
    float impostor_distance_ = 200.0f;
    
    // Culling settings
    bool frustum_culling_enabled_ = true;
    bool occlusion_culling_enabled_ = false;
    size_t max_render_count_ = 10000;
    
    // Spatial structure
    std::unique_ptr<OctreeNode> octree_root_;
    
    // Rendering state
    RenderStats stats_;
    std::vector<MoleculeInstance> visible_molecules_;
    
    // GPU resources
    uint32_t impostor_texture_;  // Atlas of pre-rendered molecule sprites
    uint32_t instance_buffer_;   // GPU buffer for instanced rendering
    
    // Helper functions
    void cull_and_lod(const glm::mat4& view_proj, const glm::vec3& camera_pos);
    void render_full_detail(const std::vector<MoleculeInstance>& instances);
    void render_simplified(const std::vector<MoleculeInstance>& instances);
    void render_impostors(const std::vector<MoleculeInstance>& instances);
    
    MoleculeLOD select_lod(float distance_to_camera) const;
    void traverse_octree(OctreeNode* node, const glm::mat4& view_proj, const glm::vec3& camera_pos);
    void insert_into_octree(OctreeNode* node, size_t idx, const MoleculeInstance& instance, int depth, int max_depth);
};

// ============================================================================
// GPU Instancing Helper
// ============================================================================

/**
 * Batch identical molecules for GPU instancing
 * Instead of drawing each molecule separately, draw all identical molecules in one call
 */
class MoleculeInstanceBatcher {
public:
    /**
     * Add molecule to batch
     * @param molecule_hash Hash of molecule structure (same structure = same batch)
     * @param transform Transform matrix for this instance
     */
    void add_instance(uint64_t molecule_hash, const glm::mat4& transform);
    
    /**
     * Render all batches
     */
    void render_all();
    
    /**
     * Clear batches (call each frame)
     */
    void clear();
    
private:
    struct InstanceBatch {
        uint64_t molecule_hash;
        std::vector<glm::mat4> transforms;
        uint32_t vao;  // Vertex Array Object
        uint32_t vbo;  // Vertex Buffer Object
        uint32_t instance_vbo;  // Instance data buffer
    };
    
    std::unordered_map<uint64_t, InstanceBatch> batches_;
};

// ============================================================================
// Impostor System (Billboard Rendering)
// ============================================================================

/**
 * Pre-render molecules as sprites for distant LOD
 * Much faster than rendering geometry when far away
 */
class ImpostorSystem {
public:
    /**
     * Generate impostor texture atlas
     * @param molecules List of unique molecules to pre-render
     * @param resolution Resolution per impostor (e.g., 128x128)
     */
    void generate_impostor_atlas(const std::vector<Molecule>& molecules, int resolution = 128);
    
    /**
     * Render molecule as billboard sprite
     * @param molecule_hash Hash identifying which impostor to use
     * @param position World position
     * @param view View matrix
     * @param projection Projection matrix
     */
    void render_impostor(uint64_t molecule_hash, const glm::vec3& position,
                        const glm::mat4& view, const glm::mat4& projection);
    
    /**
     * Render multiple impostors in one batch
     */
    void render_impostor_batch(const std::vector<MoleculeInstance>& instances,
                              const glm::mat4& view, const glm::mat4& projection);
    
private:
    uint32_t impostor_texture_atlas_;  // Texture array
    std::unordered_map<uint64_t, int> molecule_to_atlas_index_;
    int atlas_resolution_;
    int atlas_size_;  // Number of impostors in atlas
};

// ============================================================================
// Continuous Generation Integration
// ============================================================================

/**
 * Bridge between continuous generation and scalable rendering
 * Manages spatial distribution of generated molecules
 */
class StreamingMoleculeManager {
public:
    /**
     * Add newly generated molecule to scene
     * @param mol Molecule data
     * @param position World position (can be random, grid, etc.)
     */
    void add_molecule(const Molecule& mol, const glm::vec3& position);
    
    /**
     * Remove molecules outside render distance
     * (For infinite generation, need to cull old molecules)
     */
    void remove_distant_molecules(const glm::vec3& camera_pos, float max_distance);
    
    /**
     * Get all instances in scene
     */
    const std::vector<MoleculeInstance>& get_instances() const { return instances_; }
    
    /**
     * Get instances in local region (for rendering)
     * @param center Region center
     * @param radius Region radius
     */
    std::vector<MoleculeInstance> get_local_instances(const glm::vec3& center, float radius) const;
    
private:
    std::vector<MoleculeInstance> instances_;
    std::unordered_map<size_t, Molecule> molecules_;  // Full molecule data
};

} // namespace render
} // namespace vsepr
