#pragma once
/**
 * gl_mesh.hpp
 * Mesh and vertex buffer management
 */

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace vsepr {
namespace vis {

// ============================================================================
// Vertex Attributes
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 texcoord;
    glm::vec4 color;
};

// ============================================================================
// Mesh (VAO/VBO/EBO management)
// ============================================================================

class Mesh {
public:
    Mesh(const std::string& name = "Mesh");
    ~Mesh();
    
    /**
     * Set vertex data
     */
    void set_vertices(const std::vector<Vertex>& vertices);
    void set_vertices(const std::vector<glm::vec3>& positions,
                     const std::vector<glm::vec3>& normals,
                     const std::vector<glm::vec4>& colors = {});
    
    /**
     * Set index data
     */
    void set_indices(const std::vector<uint32_t>& indices);
    
    /**
     * Upload to GPU
     */
    void upload();
    
    /**
     * Bind mesh for rendering
     */
    void bind() const;
    
    /**
     * Unbind mesh
     */
    static void unbind();
    
    /**
     * Render mesh
     */
    void render() const;
    void render(GLuint first, GLuint count) const;
    
    // ========== Getters ==========
    
    GLuint get_vao() const { return vao_; }
    GLuint get_vbo() const { return vbo_; }
    GLuint get_ebo() const { return ebo_; }
    
    uint32_t get_vertex_count() const { return vertices_.size(); }
    uint32_t get_index_count() const { return indices_.size(); }
    
    const std::string& get_name() const { return name_; }
    
    /**
     * Get bounds
     */
    void get_bounds(glm::vec3& min, glm::vec3& max) const;
    
private:
    std::string name_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    
    bool uploaded_ = false;
};

// ============================================================================
// Mesh Builders (Primitives)
// ============================================================================

class MeshBuilder {
public:
    /**
     * Create cube mesh
     */
    static std::shared_ptr<Mesh> create_cube(float size = 1.0f);
    
    /**
     * Create sphere mesh
     */
    static std::shared_ptr<Mesh> create_sphere(float radius = 1.0f, 
                                              int segments = 32, 
                                              int rings = 16);
    
    /**
     * Create cylinder mesh
     */
    static std::shared_ptr<Mesh> create_cylinder(float radius = 1.0f, 
                                                 float height = 2.0f,
                                                 int segments = 32);
    
    /**
     * Create icosphere (better for sphere)
     */
    static std::shared_ptr<Mesh> create_icosphere(float radius = 1.0f, 
                                                  int subdivisions = 2);
    
    /**
     * Create plane
     */
    static std::shared_ptr<Mesh> create_plane(float width = 1.0f, 
                                              float height = 1.0f,
                                              int x_segments = 1, 
                                              int y_segments = 1);
    
    /**
     * Create grid
     */
    static std::shared_ptr<Mesh> create_grid(float width = 10.0f, 
                                             float height = 10.0f,
                                             int x_count = 10, 
                                             int y_count = 10);
};

} // namespace vis
} // namespace vsepr
