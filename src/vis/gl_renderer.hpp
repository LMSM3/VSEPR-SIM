#pragma once
/**
 * gl_renderer.hpp
 * Main rendering engine with scene management
 */

#include "gl_mesh.hpp"
#include "gl_material.hpp"
#include "gl_camera.hpp"
#include "gl_shader.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace vsepr {
namespace vis {

// ============================================================================
// Renderable Entity
// ============================================================================

class Entity {
public:
    Entity(const std::string& name = "Entity");
    virtual ~Entity() = default;
    
    /**
     * Get/set transform
     */
    void set_position(const glm::vec3& pos);
    void set_rotation(const glm::vec3& euler);
    void set_scale(const glm::vec3& scale);
    
    glm::vec3 get_position() const { return position_; }
    glm::vec3 get_rotation() const { return rotation_; }
    glm::vec3 get_scale() const { return scale_; }
    
    glm::mat4 get_transform() const;
    
    /**
     * Set material
     */
    void set_material(std::shared_ptr<Material> material);
    std::shared_ptr<Material> get_material() { return material_; }
    
    /**
     * Set mesh
     */
    void set_mesh(std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> get_mesh() { return mesh_; }
    
    /**
     * Enable/disable rendering
     */
    void set_visible(bool visible) { visible_ = visible; }
    bool is_visible() const { return visible_; }
    
    /**
     * Get name
     */
    const std::string& get_name() const { return name_; }
    
    /**
     * Render entity (called by renderer)
     */
    virtual void render(const glm::mat4& view, const glm::mat4& projection);
    
protected:
    std::string name_;
    glm::vec3 position_ = {0, 0, 0};
    glm::vec3 rotation_ = {0, 0, 0};
    glm::vec3 scale_ = {1, 1, 1};
    
    std::shared_ptr<Mesh> mesh_;
    std::shared_ptr<Material> material_;
    bool visible_ = true;
};

// ============================================================================
// Renderer
// ============================================================================

class Renderer {
public:
    Renderer();
    
    /**
     * Initialize renderer
     */
    bool initialize();
    
    /**
     * Shutdown renderer
     */
    void shutdown();
    
    /**
     * Clear frame
     */
    void clear(const glm::vec4& color = {0.1f, 0.1f, 0.1f, 1.0f});
    
    /**
     * Render all entities
     */
    void render(Camera& camera);
    
    /**
     * Add entity to scene
     */
    void add_entity(std::shared_ptr<Entity> entity);
    
    /**
     * Remove entity from scene
     */
    void remove_entity(std::shared_ptr<Entity> entity);
    
    /**
     * Get entity by name
     */
    std::shared_ptr<Entity> get_entity(const std::string& name);
    
    /**
     * Clear all entities
     */
    void clear_entities();
    
    /**
     * Get entity count
     */
    int get_entity_count() const { return entities_.size(); }
    
    /**
     * Set viewport
     */
    void set_viewport(int x, int y, int width, int height);
    
    /**
     * Set background color
     */
    void set_background_color(const glm::vec4& color);
    
    /**
     * Enable/disable features
     */
    void enable_depth_test(bool enabled);
    void enable_blend(bool enabled);
    void enable_wireframe(bool enabled);
    void enable_culling(bool enabled, bool front = false);
    
    /**
     * Get lighting system
     */
    LightingSystem& get_lighting() { return lighting_; }
    
    /**
     * Render statistics
     */
    struct Stats {
        int entity_count = 0;
        int vertex_count = 0;
        int triangle_count = 0;
    };
    
    Stats get_stats() const;
    
private:
    std::vector<std::shared_ptr<Entity>> entities_;
    LightingSystem lighting_;
    
    glm::vec4 background_color_;
    bool depth_test_enabled_ = true;
    bool blend_enabled_ = false;
    bool wireframe_enabled_ = false;
    bool culling_enabled_ = true;
    
    void setup_render_state();
};

// ============================================================================
// Scene
// ============================================================================

class Scene {
public:
    Scene(const std::string& name = "Scene");
    
    /**
     * Add entity to scene
     */
    void add_entity(std::shared_ptr<Entity> entity);
    
    /**
     * Render scene
     */
    void render(Renderer& renderer, Camera& camera);
    
    /**
     * Update scene (called each frame)
     */
    void update(float delta_time);
    
    /**
     * Get name
     */
    const std::string& get_name() const { return name_; }
    
private:
    std::string name_;
    std::vector<std::shared_ptr<Entity>> entities_;
};

} // namespace vis
} // namespace vsepr
