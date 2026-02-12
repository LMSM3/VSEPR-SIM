#pragma once
/**
 * gl_material.hpp
 * Material system with PBR and traditional lighting
 */

#include <glm/glm.hpp>
#include <string>
#include <memory>

namespace vsepr {
namespace vis {

// ============================================================================
// Material Properties
// ============================================================================

struct MaterialProperties {
    // PBR properties
    glm::vec3 albedo = {0.9f, 0.9f, 0.9f};           // Base color
    float metallic = 0.0f;                            // 0=dielectric, 1=metal
    float roughness = 0.5f;                           // Surface roughness
    
    // Traditional properties
    glm::vec3 ambient = {0.1f, 0.1f, 0.1f};
    glm::vec3 specular = {1.0f, 1.0f, 1.0f};
    float shininess = 32.0f;
    
    // Emission
    glm::vec3 emission = {0.0f, 0.0f, 0.0f};
    float emission_strength = 0.0f;
    
    // Transparency
    float alpha = 1.0f;
    bool transparent = false;
    
    // Flags
    bool use_pbr = false;                             // Use PBR or traditional
    bool double_sided = false;
};

// ============================================================================
// Material Library
// ============================================================================

class Material {
public:
    Material(const std::string& name = "Default");
    
    /**
     * Set PBR properties
     */
    void set_pbr(const glm::vec3& albedo, float metallic, float roughness);
    
    /**
     * Set traditional material properties
     */
    void set_traditional(const glm::vec3& ambient, const glm::vec3& diffuse,
                        const glm::vec3& specular, float shininess);
    
    /**
     * Set emission
     */
    void set_emission(const glm::vec3& color, float strength = 1.0f);
    
    /**
     * Set transparency
     */
    void set_transparency(float alpha);
    
    /**
     * Get properties
     */
    const MaterialProperties& get_properties() const { return properties_; }
    MaterialProperties& get_properties() { return properties_; }
    
    /**
     * Get name
     */
    const std::string& get_name() const { return name_; }
    
private:
    std::string name_;
    MaterialProperties properties_;
};

// ============================================================================
// Material Library / Presets
// ============================================================================

class MaterialLibrary {
public:
    /**
     * Create and register standard materials
     */
    static void init();
    
    /**
     * Get material by name
     */
    static std::shared_ptr<Material> get(const std::string& name);
    
    /**
     * Register custom material
     */
    static void register_material(const std::string& name, 
                                 std::shared_ptr<Material> material);
    
    /**
     * Clear all materials
     */
    static void clear();
    
private:
    static std::map<std::string, std::shared_ptr<Material>> materials_;
    
    /**
     * Create built-in materials
     */
    static void create_element_materials();
    static void create_standard_materials();
};

// ============================================================================
// Lighting
// ============================================================================

enum class LightType {
    DIRECTIONAL,
    POINT,
    SPOT
};

struct Light {
    LightType type;
    glm::vec3 position;          // For point/spot lights
    glm::vec3 direction;         // For directional/spot lights
    glm::vec3 color;
    float intensity;
    
    // Point/Spot specific
    float range = 100.0f;
    
    // Spot specific
    float inner_cone = 0.7f;
    float outer_cone = 0.5f;
    
    // Shadows
    bool cast_shadow = true;
    float shadow_bias = 0.005f;
};

// ============================================================================
// Lighting System
// ============================================================================

class LightingSystem {
public:
    /**
     * Add light to scene
     */
    void add_light(const Light& light);
    
    /**
     * Remove light by index
     */
    void remove_light(int index);
    
    /**
     * Get light
     */
    Light& get_light(int index) { return lights_[index]; }
    const Light& get_light(int index) const { return lights_[index]; }
    
    /**
     * Get light count
     */
    int get_light_count() const { return lights_.size(); }
    
    /**
     * Clear all lights
     */
    void clear();
    
    /**
     * Set ambient light
     */
    void set_ambient(const glm::vec3& color) { ambient_ = color; }
    glm::vec3 get_ambient() const { return ambient_; }
    
private:
    std::vector<Light> lights_;
    glm::vec3 ambient_ = {0.1f, 0.1f, 0.1f};
};

} // namespace vis
} // namespace vsepr
