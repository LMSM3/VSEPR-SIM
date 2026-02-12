#pragma once
/**
 * gl_shader.hpp
 * Modern GLSL shader management with compilation, linking, uniforms
 */

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <map>
#include <memory>
#include <iostream>

namespace vsepr {
namespace vis {

// ============================================================================
// Shader Program
// ============================================================================

class ShaderProgram {
public:
    /**
     * Create shader program from source strings
     */
    bool compile_and_link(const std::string& vert_src, const std::string& frag_src);
    
    /**
     * Compile geometry shader (optional)
     */
    bool add_geometry_shader(const std::string& geom_src);
    
    /**
     * Use this program for rendering
     */
    void use() const;
    
    /**
     * Stop using this program
     */
    static void unuse();
    
    /**
     * Get program handle
     */
    GLuint get_handle() const { return program_; }
    
    /**
     * Check if program is valid
     */
    bool is_valid() const { return program_ != 0; }
    
    // ========== Uniform Setters ==========
    
    void set_uniform(const std::string& name, int value);
    void set_uniform(const std::string& name, float value);
    void set_uniform(const std::string& name, const glm::vec2& value);
    void set_uniform(const std::string& name, const glm::vec3& value);
    void set_uniform(const std::string& name, const glm::vec4& value);
    void set_uniform(const std::string& name, const glm::mat3& value);
    void set_uniform(const std::string& name, const glm::mat4& value);
    void set_uniform(const std::string& name, bool value);
    void set_uniform_array(const std::string& name, const float* values, int count);
    void set_uniform_array(const std::string& name, const glm::vec3* values, int count);
    
    /**
     * Destructor
     */
    ~ShaderProgram();
    
private:
    GLuint program_ = 0;
    GLuint vertex_shader_ = 0;
    GLuint fragment_shader_ = 0;
    GLuint geometry_shader_ = 0;
    std::map<std::string, GLint> uniform_cache_;
    
    GLint get_uniform_location(const std::string& name);
    
    static GLuint compile_shader(GLenum type, const std::string& source);
    static std::string get_shader_error(GLuint shader);
    static std::string get_program_error(GLuint program);
};

// ============================================================================
// Shader Library
// ============================================================================

class ShaderLibrary {
public:
    /**
     * Get or create shader program
     */
    static std::shared_ptr<ShaderProgram> get(const std::string& name);
    
    /**
     * Register shader program
     */
    static void register_shader(const std::string& name, 
                                std::shared_ptr<ShaderProgram> shader);
    
    /**
     * Load common built-in shaders
     */
    static void load_builtin_shaders();
    
    /**
     * Clear shader cache
     */
    static void clear();
    
private:
    static std::map<std::string, std::shared_ptr<ShaderProgram>> shaders_;
};

// ============================================================================
// Built-in Shaders
// ============================================================================

namespace shaders {
    
// Standard PBR shader
extern const char* pbr_vertex;
extern const char* pbr_fragment;

// Simple color shader
extern const char* color_vertex;
extern const char* color_fragment;

// Wireframe shader
extern const char* wireframe_vertex;
extern const char* wireframe_fragment;

// Skybox shader
extern const char* skybox_vertex;
extern const char* skybox_fragment;

// Shadow map shader
extern const char* shadow_vertex;
extern const char* shadow_fragment;

} // namespace shaders

} // namespace vis
} // namespace vsepr
