#include "renderer_classic.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

namespace vsepr {
namespace render {

ClassicRenderer::ClassicRenderer() = default;

ClassicRenderer::~ClassicRenderer() {
    cleanup_buffers();
    
    if (sphere_shader_program_) {
        glDeleteProgram(sphere_shader_program_);
    }
    if (cylinder_shader_program_) {
        glDeleteProgram(cylinder_shader_program_);
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool ClassicRenderer::initialize() {
    // Initialize GLEW (if not already done)
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "[ClassicRenderer] GLEW init failed: " 
                  << glewGetErrorString(err) << std::endl;
        return false;
    }
    
    // Load shaders
    if (!load_shaders()) {
        std::cerr << "[ClassicRenderer] Shader loading failed" << std::endl;
        return false;
    }
    
    // Generate geometry
    sphere_geom_ = SphereGeometry::generate(get_sphere_lod());
    cylinder_geom_ = CylinderGeometry::generate(get_cylinder_segments());
    
    // Initialize OpenGL buffers
    initialize_sphere_buffers();
    initialize_cylinder_buffers();
    
    buffers_initialized_ = true;
    
    std::cout << "[ClassicRenderer] Initialized successfully" << std::endl;
    std::cout << "  - Sphere: " << sphere_geom_.triangle_count() << " triangles" << std::endl;
    std::cout << "  - Cylinder: " << cylinder_geom_.triangle_count() << " triangles" << std::endl;
    
    return true;
}

int ClassicRenderer::get_sphere_lod() const {
    switch (quality_) {
        case RenderQuality::ULTRA:   return 5;  // 20,480 triangles
        case RenderQuality::HIGH:    return 4;  // 5,120 triangles
        case RenderQuality::MEDIUM:  return 3;  // 1,280 triangles
        case RenderQuality::LOW:     return 2;  // 320 triangles
        case RenderQuality::MINIMAL: return 0;  // 20 triangles (wireframe)
        default: return 3;
    }
}

int ClassicRenderer::get_cylinder_segments() const {
    switch (quality_) {
        case RenderQuality::ULTRA:
        case RenderQuality::HIGH:    return 32;
        case RenderQuality::MEDIUM:  return 16;
        case RenderQuality::LOW:     return 8;
        case RenderQuality::MINIMAL: return 4;
        default: return 16;
    }
}

// ============================================================================
// Shader Loading
// ============================================================================

std::string ClassicRenderer::read_shader_file(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ClassicRenderer] Failed to open shader: " << path << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint ClassicRenderer::compile_shader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    // Check compilation status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        std::cerr << "[ClassicRenderer] Shader compilation failed:\n" << info_log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint ClassicRenderer::link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    // Check linking status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, nullptr, info_log);
        std::cerr << "[ClassicRenderer] Shader linking failed:\n" << info_log << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    
    return program;
}

bool ClassicRenderer::load_shaders() {
    // Load sphere shaders
    std::string sphere_vert_src = read_shader_file("src/vis/shaders/classic/sphere.vert");
    std::string sphere_frag_src = read_shader_file("src/vis/shaders/classic/sphere.frag");
    
    if (sphere_vert_src.empty() || sphere_frag_src.empty()) {
        return false;
    }
    
    GLuint sphere_vert = compile_shader(sphere_vert_src.c_str(), GL_VERTEX_SHADER);
    GLuint sphere_frag = compile_shader(sphere_frag_src.c_str(), GL_FRAGMENT_SHADER);
    
    if (!sphere_vert || !sphere_frag) {
        return false;
    }
    
    sphere_shader_program_ = link_program(sphere_vert, sphere_frag);
    glDeleteShader(sphere_vert);
    glDeleteShader(sphere_frag);
    
    if (!sphere_shader_program_) {
        return false;
    }
    
    // Get uniform locations (sphere)
    sphere_u_view_projection_ = glGetUniformLocation(sphere_shader_program_, "u_ViewProjection");
    sphere_u_light_dir_ = glGetUniformLocation(sphere_shader_program_, "u_LightDir");
    sphere_u_view_pos_ = glGetUniformLocation(sphere_shader_program_, "u_ViewPos");
    sphere_u_ambient_color_ = glGetUniformLocation(sphere_shader_program_, "u_AmbientColor");
    sphere_u_light_color_ = glGetUniformLocation(sphere_shader_program_, "u_LightColor");
    sphere_u_shininess_ = glGetUniformLocation(sphere_shader_program_, "u_Shininess");
    
    // Load cylinder shaders
    std::string cylinder_vert_src = read_shader_file("src/vis/shaders/classic/cylinder.vert");
    std::string cylinder_frag_src = read_shader_file("src/vis/shaders/classic/cylinder.frag");
    
    if (cylinder_vert_src.empty() || cylinder_frag_src.empty()) {
        return false;
    }
    
    GLuint cylinder_vert = compile_shader(cylinder_vert_src.c_str(), GL_VERTEX_SHADER);
    GLuint cylinder_frag = compile_shader(cylinder_frag_src.c_str(), GL_FRAGMENT_SHADER);
    
    if (!cylinder_vert || !cylinder_frag) {
        return false;
    }
    
    cylinder_shader_program_ = link_program(cylinder_vert, cylinder_frag);
    glDeleteShader(cylinder_vert);
    glDeleteShader(cylinder_frag);
    
    if (!cylinder_shader_program_) {
        return false;
    }
    
    // Get uniform locations (cylinder)
    cylinder_u_view_projection_ = glGetUniformLocation(cylinder_shader_program_, "u_ViewProjection");
    cylinder_u_light_dir_ = glGetUniformLocation(cylinder_shader_program_, "u_LightDir");
    cylinder_u_view_pos_ = glGetUniformLocation(cylinder_shader_program_, "u_ViewPos");
    cylinder_u_ambient_color_ = glGetUniformLocation(cylinder_shader_program_, "u_AmbientColor");
    cylinder_u_light_color_ = glGetUniformLocation(cylinder_shader_program_, "u_LightColor");
    cylinder_u_shininess_ = glGetUniformLocation(cylinder_shader_program_, "u_Shininess");
    
    return true;
}

// ============================================================================
// Buffer Initialization
// ============================================================================

void ClassicRenderer::initialize_sphere_buffers() {
    glGenVertexArrays(1, &sphere_vao_);
    glBindVertexArray(sphere_vao_);
    
    // Vertex buffer (positions + normals)
    glGenBuffers(1, &sphere_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    glBufferData(GL_ARRAY_BUFFER, 
                 sphere_geom_.vertices.size() * sizeof(float),
                 sphere_geom_.vertices.data(),
                 GL_STATIC_DRAW);
    
    // Vertex attributes: position (location = 0), normal (location = 1)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    // Index buffer
    glGenBuffers(1, &sphere_ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sphere_geom_.indices.size() * sizeof(unsigned int),
                 sphere_geom_.indices.data(),
                 GL_STATIC_DRAW);
    
    // Instance buffer (will be updated per frame)
    glGenBuffers(1, &sphere_instance_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_instance_vbo_);
    
    // Instance attributes: position (location = 2), radius (location = 3), color (location = 4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glVertexAttribDivisor(2, 1);  // Per-instance
    
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
    
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    
    glBindVertexArray(0);
}

void ClassicRenderer::initialize_cylinder_buffers() {
    glGenVertexArrays(1, &cylinder_vao_);
    glBindVertexArray(cylinder_vao_);
    
    // Vertex buffer
    glGenBuffers(1, &cylinder_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, cylinder_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 cylinder_geom_.vertices.size() * sizeof(float),
                 cylinder_geom_.vertices.data(),
                 GL_STATIC_DRAW);
    
    // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    // Index buffer
    glGenBuffers(1, &cylinder_ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cylinder_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 cylinder_geom_.indices.size() * sizeof(unsigned int),
                 cylinder_geom_.indices.data(),
                 GL_STATIC_DRAW);
    
    // Instance buffer: start_pos (3), end_pos (3), radius (1), color (3) = 10 floats
    glGenBuffers(1, &cylinder_instance_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, cylinder_instance_vbo_);
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
    glVertexAttribDivisor(2, 1);
    
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
    
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(6 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(7 * sizeof(float)));
    glVertexAttribDivisor(5, 1);
    
    glBindVertexArray(0);
}

void ClassicRenderer::cleanup_buffers() {
    if (sphere_vao_) glDeleteVertexArrays(1, &sphere_vao_);
    if (sphere_vbo_) glDeleteBuffers(1, &sphere_vbo_);
    if (sphere_ebo_) glDeleteBuffers(1, &sphere_ebo_);
    if (sphere_instance_vbo_) glDeleteBuffers(1, &sphere_instance_vbo_);
    
    if (cylinder_vao_) glDeleteVertexArrays(1, &cylinder_vao_);
    if (cylinder_vbo_) glDeleteBuffers(1, &cylinder_vbo_);
    if (cylinder_ebo_) glDeleteBuffers(1, &cylinder_ebo_);
    if (cylinder_instance_vbo_) glDeleteBuffers(1, &cylinder_instance_vbo_);
    
    buffers_initialized_ = false;
}

// ============================================================================
// Rendering
// ============================================================================

void ClassicRenderer::render(const AtomicGeometry& geom,
const vis::Camera& camera,
int width, int height) {
    if (!buffers_initialized_) {
        std::cerr << "[ClassicRenderer] Not initialized!" << std::endl;
        return;
    }
    
    // Set viewport
    glViewport(0, 0, width, height);
    
    // Clear background
    glClearColor(background_[0], background_[1], background_[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Render atoms (spheres)
    render_atoms(geom, camera);
    
    // Render bonds (cylinders)
    if (show_bonds_) {
        render_bonds(geom, camera);
    }
}

void ClassicRenderer::render_atoms(const AtomicGeometry& geom, const vis::Camera& camera) {
    if (geom.atomic_numbers.empty() || geom.positions.empty()) {
        return;
    }
    
    // Build instance data
    InstancedSphereData instance_data;
    
    for (size_t i = 0; i < geom.atomic_numbers.size(); ++i) {
        int Z = geom.atomic_numbers[i];
        Vec3 pos = geom.positions[i];
        
        // Get atom properties
        float vdw_radius = get_vdw_radius(Z);
        float radius = vdw_radius * atom_scale_;  // Scale down for ball-and-stick
        
        float color[3];
        get_cpk_color(Z, color);
        
        instance_data.add_instance(pos, radius, color[0], color[1], color[2]);
    }
    
    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, sphere_instance_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 instance_data.instance_count() * 7 * sizeof(float),
                 instance_data.positions.data(),  // Interleaved: pos(3), radius(1), color(3)
                 GL_DYNAMIC_DRAW);
    
    // Note: We need to properly interleave the data
    std::vector<float> interleaved;
    interleaved.reserve(instance_data.instance_count() * 7);
    for (int i = 0; i < instance_data.instance_count(); ++i) {
        interleaved.push_back(instance_data.positions[i*3 + 0]);
        interleaved.push_back(instance_data.positions[i*3 + 1]);
        interleaved.push_back(instance_data.positions[i*3 + 2]);
        interleaved.push_back(instance_data.radii[i]);
        interleaved.push_back(instance_data.colors[i*3 + 0]);
        interleaved.push_back(instance_data.colors[i*3 + 1]);
        interleaved.push_back(instance_data.colors[i*3 + 2]);
    }
    
    glBufferData(GL_ARRAY_BUFFER,
                 interleaved.size() * sizeof(float),
                 interleaved.data(),
                 GL_DYNAMIC_DRAW);
    
    // Use sphere shader
    glUseProgram(sphere_shader_program_);
    
    // Set uniforms
    // TODO: Get view-projection matrix from camera
    // For now, use placeholder identity matrix
    float view_proj[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    glUniformMatrix4fv(sphere_u_view_projection_, 1, GL_FALSE, view_proj);
    
    // Lighting
    float light_dir[3] = {0.577f, 0.577f, 0.577f};  // Diagonal
    glUniform3fv(sphere_u_light_dir_, 1, light_dir);
    
    float view_pos[3] = {0.0f, 0.0f, 10.0f};
    glUniform3fv(sphere_u_view_pos_, 1, view_pos);
    
    float ambient[3] = {0.3f, 0.3f, 0.3f};
    glUniform3fv(sphere_u_ambient_color_, 1, ambient);
    
    float light_color[3] = {1.0f, 1.0f, 1.0f};
    glUniform3fv(sphere_u_light_color_, 1, light_color);
    
    glUniform1f(sphere_u_shininess_, 32.0f);
    
    // Draw instanced
    glBindVertexArray(sphere_vao_);
    glDrawElementsInstanced(GL_TRIANGLES,
                           static_cast<GLsizei>(sphere_geom_.indices.size()),
                           GL_UNSIGNED_INT,
                           0,
                           instance_data.instance_count());
    
    glBindVertexArray(0);
}

void ClassicRenderer::render_bonds(const AtomicGeometry& geom, const vis::Camera& camera) {
    // Get bonds (either from geom or auto-detect)
    std::vector<std::pair<int,int>> bonds = geom.bonds;
    if (bonds.empty() && auto_bond_) {
        bonds = detect_bonds(geom);
    }
    
    if (bonds.empty()) {
        return;
    }
    
    // Build instance data
    InstancedCylinderData instance_data;
    
    for (const auto& bond : bonds) {
        int i = bond.first;
        int j = bond.second;
        
        if (i >= (int)geom.positions.size() || j >= (int)geom.positions.size()) {
            continue;
        }
        
        Vec3 pos_i = geom.positions[i];
        Vec3 pos_j = geom.positions[j];
        
        // Average color (or use bond-specific color)
        float color[3];
        get_cpk_color(geom.atomic_numbers[i], color);  // Use first atom's color
        
        instance_data.add_instance(pos_i, pos_j, bond_radius_, 
                                  color[0], color[1], color[2]);
    }
    
    // Interleave instance data
    std::vector<float> interleaved;
    interleaved.reserve(instance_data.instance_count() * 10);
    for (int i = 0; i < instance_data.instance_count(); ++i) {
        interleaved.push_back(instance_data.start_positions[i*3 + 0]);
        interleaved.push_back(instance_data.start_positions[i*3 + 1]);
        interleaved.push_back(instance_data.start_positions[i*3 + 2]);
        interleaved.push_back(instance_data.end_positions[i*3 + 0]);
        interleaved.push_back(instance_data.end_positions[i*3 + 1]);
        interleaved.push_back(instance_data.end_positions[i*3 + 2]);
        interleaved.push_back(instance_data.radii[i]);
        interleaved.push_back(instance_data.colors[i*3 + 0]);
        interleaved.push_back(instance_data.colors[i*3 + 1]);
        interleaved.push_back(instance_data.colors[i*3 + 2]);
    }
    
    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, cylinder_instance_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 interleaved.size() * sizeof(float),
                 interleaved.data(),
                 GL_DYNAMIC_DRAW);
    
    // Use cylinder shader
    glUseProgram(cylinder_shader_program_);
    
    // Set uniforms (same as spheres)
    float view_proj[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    glUniformMatrix4fv(cylinder_u_view_projection_, 1, GL_FALSE, view_proj);
    
    float light_dir[3] = {0.577f, 0.577f, 0.577f};
    glUniform3fv(cylinder_u_light_dir_, 1, light_dir);
    
    float view_pos[3] = {0.0f, 0.0f, 10.0f};
    glUniform3fv(cylinder_u_view_pos_, 1, view_pos);
    
    float ambient[3] = {0.3f, 0.3f, 0.3f};
    glUniform3fv(cylinder_u_ambient_color_, 1, ambient);
    
    float light_color[3] = {1.0f, 1.0f, 1.0f};
    glUniform3fv(cylinder_u_light_color_, 1, light_color);
    
    glUniform1f(cylinder_u_shininess_, 32.0f);
    
    // Draw instanced
    glBindVertexArray(cylinder_vao_);
    glDrawElementsInstanced(GL_TRIANGLES,
                           static_cast<GLsizei>(cylinder_geom_.indices.size()),
                           GL_UNSIGNED_INT,
                           0,
                           instance_data.instance_count());
    
    glBindVertexArray(0);
}

// ============================================================================
// Bond Detection
// ============================================================================

std::vector<std::pair<int,int>> ClassicRenderer::detect_bonds(const AtomicGeometry& geom) {
    std::vector<std::pair<int,int>> bonds;
    
    int n = static_cast<int>(geom.atomic_numbers.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i+1; j < n; ++j) {
            Vec3 pos_i = geom.positions[i];
            Vec3 pos_j = geom.positions[j];
            
            float dx = pos_j.x - pos_i.x;
            float dy = pos_j.y - pos_i.y;
            float dz = pos_j.z - pos_i.z;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            float r_i = get_covalent_radius(geom.atomic_numbers[i]);
            float r_j = get_covalent_radius(geom.atomic_numbers[j]);
            float threshold = bond_tolerance_ * (r_i + r_j);
            
            if (dist < threshold) {
                bonds.push_back({i, j});
            }
        }
    }
    
    return bonds;
}

} // namespace render
} // namespace vsepr
