#pragma once

#include "renderer_base.hpp"
#include "geometry/sphere.hpp"
#include "geometry/cylinder.hpp"
#include <GL/glew.h>

namespace vsepr {
namespace render {

/**
 * Classic ball-and-stick molecular renderer
 * 
 * Optimized for:
 *   - Small molecules (VSEPR geometries)
 *   - Main group elements (C, H, N, O, halogens, etc.)
 *   - Ball-and-stick representation
 * 
 * Features:
 *   - High-quality sphere tessellation (192-20,480 triangles)
 *   - Smooth cylinders for bonds (16-32 segments)
 *   - Phong/Blinn-Phong lighting
 *   - Instanced rendering (one draw call per geometry type)
 *   - CPK coloring by default
 * 
 * Performance:
 *   - 240+ FPS for <100 atoms (MEDIUM quality)
 *   - 120+ FPS for <1000 atoms (MEDIUM quality)
 *   - 60+ FPS for <10k atoms (LOW quality)
 */
class ClassicRenderer : public MoleculeRendererBase {
public:
    ClassicRenderer();
    ~ClassicRenderer() override;
    
    // ========================================================================
    // MoleculeRendererBase Interface
    // ========================================================================
    
    bool initialize() override;
    
    void render(const AtomicGeometry& geom,
               const vis::Camera& camera,
               int width, int height) override;
    
    ChemistryType get_chemistry_type() const override {
        return ChemistryType::CLASSIC;
    }
    
    std::string get_name() const override {
        return "Ballstick";  // One-word name as requested
    }
    
    // ========================================================================
    // ClassicRenderer-Specific Settings
    // ========================================================================
    
    /**
     * Enable/disable auto-bonding (detect bonds from distances)
     * 
     * If enabled and geom.bonds is empty, bonds are detected using:
     *   distance < 1.2 * (r_cov[i] + r_cov[j])
     */
    void set_auto_bond(bool enable) { auto_bond_ = enable; }
    
    /**
     * Set bond detection tolerance (default: 1.2)
     */
    void set_bond_tolerance(float tol) { bond_tolerance_ = tol; }
    
    /**
     * Enable depth cueing (fog effect for depth perception)
     */
    void set_depth_cueing(bool enable) { depth_cueing_ = enable; }
    bool has_depth_cueing() const { return depth_cueing_; }
    
    /**
     * Set depth cueing parameters
     * 
     * @param near Distance where fog starts (camera units)
     * @param far Distance where fog is maximum (camera units)
     */
    void set_depth_cue_range(float near, float far) {
        depth_cue_near_ = near;
        depth_cue_far_ = far;
    }
    
    /**
     * Enable silhouette edges (outline rendering)
     */
    void set_silhouette(bool enable) { silhouette_ = enable; }
    bool has_silhouette() const { return silhouette_; }
    
    /**
     * Enable glow effect (bloom)
     */
    void set_glow(bool enable) { glow_ = enable; }
    bool has_glow() const { return glow_; }
    
    /**
     * Set atom opacity (for transparency)
     * 
     * @param opacity 0.0 = invisible, 1.0 = fully opaque
     */
    void set_atom_opacity(float opacity) { atom_opacity_ = opacity; }
    float get_atom_opacity() const { return atom_opacity_; }
    
private:
    // ========================================================================
    // Shader Management
    // ========================================================================
    
    bool load_shaders();
    GLuint compile_shader(const char* source, GLenum type);
    GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);
    std::string read_shader_file(const char* path);
    
    GLuint sphere_shader_program_ = 0;
    GLuint cylinder_shader_program_ = 0;
    
    // Uniform locations (sphere shader)
    GLint sphere_u_view_projection_ = -1;
    GLint sphere_u_light_dir_ = -1;
    GLint sphere_u_view_pos_ = -1;
    GLint sphere_u_ambient_color_ = -1;
    GLint sphere_u_light_color_ = -1;
    GLint sphere_u_shininess_ = -1;
    
    // Uniform locations (cylinder shader)
    GLint cylinder_u_view_projection_ = -1;
    GLint cylinder_u_light_dir_ = -1;
    GLint cylinder_u_view_pos_ = -1;
    GLint cylinder_u_ambient_color_ = -1;
    GLint cylinder_u_light_color_ = -1;
    GLint cylinder_u_shininess_ = -1;
    
    // ========================================================================
    // Geometry Buffers
    // ========================================================================
    
    // Sphere geometry (base mesh)
    SphereGeometry sphere_geom_;
    GLuint sphere_vao_ = 0;
    GLuint sphere_vbo_ = 0;  // Vertex data
    GLuint sphere_ebo_ = 0;  // Index data
    GLuint sphere_instance_vbo_ = 0;  // Instance data (pos, radius, color)
    
    // Cylinder geometry (base mesh)
    CylinderGeometry cylinder_geom_;
    GLuint cylinder_vao_ = 0;
    GLuint cylinder_vbo_ = 0;
    GLuint cylinder_ebo_ = 0;
    GLuint cylinder_instance_vbo_ = 0;
    
    bool buffers_initialized_ = false;
    
    void initialize_sphere_buffers();
    void initialize_cylinder_buffers();
    void cleanup_buffers();
    
    // ========================================================================
    // Rendering Helpers
    // ========================================================================
    
    void render_atoms(const AtomicGeometry& geom, const vis::Camera& camera);
    void render_bonds(const AtomicGeometry& geom, const vis::Camera& camera);
    
    /**
     * Auto-detect bonds from atomic positions
     * 
     * Uses covalent radii: bond exists if distance < tolerance * (r_i + r_j)
     */
    std::vector<std::pair<int,int>> detect_bonds(const AtomicGeometry& geom);
    
    /**
     * Get sphere LOD based on quality setting
     */
    int get_sphere_lod() const;
    
    /**
     * Get cylinder segment count based on quality setting
     */
    int get_cylinder_segments() const;
    
    // ========================================================================
    // Settings
    // ========================================================================
    
    bool auto_bond_ = true;
    float bond_tolerance_ = 1.2f;  // 20% tolerance for bond detection
    
    // Visual effects
    bool depth_cueing_ = false;
    float depth_cue_near_ = 5.0f;
    float depth_cue_far_ = 20.0f;
    bool silhouette_ = false;
    bool glow_ = false;
    float atom_opacity_ = 1.0f;
};

} // namespace render
} // namespace vsepr
