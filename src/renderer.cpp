#include "vis/renderer.hpp"
#include "pot/covalent_radii.hpp"
#include <GL/glew.h>
#include <cmath>
#include <vector>
#include <iostream>

namespace vsepr {

// ============================================================================
// Camera Implementation
// ============================================================================

Camera::Camera() 
    : target_(0, 0, 0)
    , distance_(10.0)
    , theta_(0.0)
    , phi_(M_PI / 4.0)  // 45 degrees
    , fov_(45.0)
    , near_clip_(0.1)
    , far_clip_(100.0)
{}

void Camera::orbit(double dx, double dy) {
    const double sensitivity = 0.005;
    theta_ += dx * sensitivity;
    phi_ += dy * sensitivity;
    
    // Clamp phi to avoid gimbal lock
    const double epsilon = 0.01;
    if (phi_ < epsilon) phi_ = epsilon;
    if (phi_ > M_PI - epsilon) phi_ = M_PI - epsilon;
}

void Camera::pan(double dx, double dy) {
    const double sensitivity = 0.01;
    Vec3 right(std::sin(theta_ - M_PI/2), 0, std::cos(theta_ - M_PI/2));
    Vec3 up(0, 1, 0);
    target_ = target_ + right * (-dx * sensitivity * distance_);
    target_ = target_ + up * (dy * sensitivity * distance_);
}

void Camera::zoom(double delta) {
    distance_ *= std::exp(-delta * 0.1);
    if (distance_ < 1.0) distance_ = 1.0;
    if (distance_ > 50.0) distance_ = 50.0;
}

void Camera::reset() {
    distance_ = 10.0;
    theta_ = 0.0;
    phi_ = M_PI / 4.0;
    target_ = Vec3(0, 0, 0);
}

void Camera::get_view_matrix(float* matrix) const {
    // Calculate eye position in spherical coordinates
    double x = target_.x + distance_ * std::sin(phi_) * std::sin(theta_);
    double y = target_.y + distance_ * std::cos(phi_);
    double z = target_.z + distance_ * std::sin(phi_) * std::cos(theta_);
    
    Vec3 eye(x, y, z);
    Vec3 forward = (target_ - eye).normalized();
    Vec3 world_up(0, 1, 0);
    Vec3 right = cross(forward, world_up).normalized();
    Vec3 up = cross(right, forward);
    
    // Column-major order for OpenGL
    matrix[0] = static_cast<float>(right.x);
    matrix[1] = static_cast<float>(up.x);
    matrix[2] = static_cast<float>(-forward.x);
    matrix[3] = 0.0f;
    
    matrix[4] = static_cast<float>(right.y);
    matrix[5] = static_cast<float>(up.y);
    matrix[6] = static_cast<float>(-forward.y);
    matrix[7] = 0.0f;
    
    matrix[8] = static_cast<float>(right.z);
    matrix[9] = static_cast<float>(up.z);
    matrix[10] = static_cast<float>(-forward.z);
    matrix[11] = 0.0f;
    
    matrix[12] = static_cast<float>(-dot(right, eye));
    matrix[13] = static_cast<float>(-dot(up, eye));
    matrix[14] = static_cast<float>(dot(forward, eye));
    matrix[15] = 1.0f;
}

void Camera::get_projection_matrix(float* matrix, float aspect) const {
    float f = 1.0f / std::tan(fov_ * M_PI / 360.0f);
    float nf = 1.0f / (near_clip_ - far_clip_);
    
    matrix[0] = f / aspect;
    matrix[1] = 0.0f;
    matrix[2] = 0.0f;
    matrix[3] = 0.0f;
    
    matrix[4] = 0.0f;
    matrix[5] = f;
    matrix[6] = 0.0f;
    matrix[7] = 0.0f;
    
    matrix[8] = 0.0f;
    matrix[9] = 0.0f;
    matrix[10] = static_cast<float>((far_clip_ + near_clip_) * nf);
    matrix[11] = -1.0f;
    
    matrix[12] = 0.0f;
    matrix[13] = 0.0f;
    matrix[14] = static_cast<float>(2.0f * far_clip_ * near_clip_ * nf);
    matrix[15] = 0.0f;
}

// ============================================================================
// Renderer Implementation
// ============================================================================

// Simple vertex and fragment shaders
const char* vertex_shader_source = R"(
#version 150 core
in vec3 aPos;
in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
)";

const char* fragment_shader_source = R"(
#version 150 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 uColor;
uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main() {
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * uColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
)";

Renderer::Renderer()
    : background_{0.1f, 0.1f, 0.15f}
    , atom_scale_(1.0f)
    , bond_radius_(0.15f)
    , show_bonds_(true)
    , sphere_vao_(0)
    , sphere_vbo_(0)
    , sphere_ebo_(0)
    , shader_program_(0)
    , sphere_index_count_(0)
{}

Renderer::~Renderer() {
    if (sphere_vao_) glDeleteVertexArrays(1, &sphere_vao_);
    if (sphere_vbo_) glDeleteBuffers(1, &sphere_vbo_);
    if (sphere_ebo_) glDeleteBuffers(1, &sphere_ebo_);
    if (shader_program_) glDeleteProgram(shader_program_);
}

bool Renderer::initialize() {
    std::cout << "Initializing renderer...\n";
    
    // Print OpenGL context info
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL Vendor: " << (vendor ? (const char*)vendor : "unknown") << "\n";
    std::cout << "OpenGL Renderer: " << (renderer ? (const char*)renderer : "unknown") << "\n";
    std::cout << "OpenGL Version: " << (version ? (const char*)version : "unknown") << "\n";
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "GLEW init returned: " << err << " - " << glewGetErrorString(err) << "\n";
        // For Mesa/llvmpipe, GLEW sometimes reports errors but still works
        // Check if we can get basic GL functions
        if (!glGenVertexArrays || !glGenBuffers) {
            std::cerr << "Critical: GL functions not available\n";
            return false;
        }
        std::cerr << "Warning: GLEW reported error but GL functions seem available, continuing...\n";
    } else {
        std::cout << "GLEW initialized successfully\n";
    }
    
    // Clear any GL errors from GLEW init
    while (glGetError() != GL_NO_ERROR);
    
    // Compile shaders
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);
    
    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
        std::cerr << "Vertex shader compilation failed:\n" << info_log << "\n";
        return false;
    }
    
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
        std::cerr << "Fragment shader compilation failed:\n" << info_log << "\n";
        return false;
    }
    
    shader_program_ = glCreateProgram();
    glAttachShader(shader_program_, vertex_shader);
    glAttachShader(shader_program_, fragment_shader);
    glLinkProgram(shader_program_);
    
    glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(shader_program_, 512, nullptr, info_log);
        std::cerr << "Shader linking failed:\n" << info_log << "\n";
        return false;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    // Create sphere geometry
    const int lat_bands = 20;
    const int lon_bands = 20;
    const float radius = 1.0f;
    
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    for (int lat = 0; lat <= lat_bands; ++lat) {
        float theta = lat * M_PI / lat_bands;
        float sin_theta = std::sin(theta);
        float cos_theta = std::cos(theta);
        
        for (int lon = 0; lon <= lon_bands; ++lon) {
            float phi = lon * 2 * M_PI / lon_bands;
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);
            
            float x = cos_phi * sin_theta;
            float y = cos_theta;
            float z = sin_phi * sin_theta;
            
            vertices.push_back(x * radius);
            vertices.push_back(y * radius);
            vertices.push_back(z * radius);
            vertices.push_back(x);  // Normal
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }
    
    for (int lat = 0; lat < lat_bands; ++lat) {
        for (int lon = 0; lon < lon_bands; ++lon) {
            int first = lat * (lon_bands + 1) + lon;
            int second = first + lon_bands + 1;
            
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    
    sphere_index_count_ = indices.size();
    
    glGenVertexArrays(1, &sphere_vao_);
    glGenBuffers(1, &sphere_vbo_);
    glGenBuffers(1, &sphere_ebo_);
    
    glBindVertexArray(sphere_vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    // Enable depth testing and face culling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    return true;
}

void Renderer::render(const FrameSnapshot& frame, int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(background_[0], background_[1], background_[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (!frame.is_valid()) {
        return;
    }
    
    render_atoms(frame);
    
    if (show_bonds_ && !frame.bonds.empty()) {
        render_bonds(frame);
    }
}

void Renderer::render_atoms(const FrameSnapshot& frame) {
    glUseProgram(shader_program_);
    
    float view[16], projection[16];
    camera_.get_view_matrix(view);
    camera_.get_projection_matrix(projection, 1280.0f / 720.0f);  // TODO: Use actual aspect
    
    GLint view_loc = glGetUniformLocation(shader_program_, "uView");
    GLint proj_loc = glGetUniformLocation(shader_program_, "uProjection");
    GLint model_loc = glGetUniformLocation(shader_program_, "uModel");
    GLint color_loc = glGetUniformLocation(shader_program_, "uColor");
    GLint light_pos_loc = glGetUniformLocation(shader_program_, "uLightPos");
    GLint view_pos_loc = glGetUniformLocation(shader_program_, "uViewPos");
    
    glUniformMatrix4fv(view_loc, 1, GL_FALSE, view);
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);
    
    // Set light position (from camera)
    glUniform3f(light_pos_loc, 10.0f, 10.0f, 10.0f);
    glUniform3f(view_pos_loc, 10.0f, 10.0f, 10.0f);
    
    glBindVertexArray(sphere_vao_);
    
    for (size_t i = 0; i < frame.positions.size(); ++i) {
        const Vec3& pos = frame.positions[i];
        int Z = frame.atomic_numbers[i];
        
        float radius = get_atom_radius(Z) * atom_scale_;
        
        // Model matrix: translate and scale
        float model[16] = {
            radius, 0, 0, 0,
            0, radius, 0, 0,
            0, 0, radius, 0,
            static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z), 1
        };
        
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, model);
        
        float color[3];
        get_atom_color(Z, color);
        glUniform3fv(color_loc, 1, color);
        
        glDrawElements(GL_TRIANGLES, sphere_index_count_, GL_UNSIGNED_INT, 0);
    }
    
    glBindVertexArray(0);
}

void Renderer::render_bonds(const FrameSnapshot& frame) {
    // TODO: Implement cylinder rendering for bonds
    // For now, use simple lines
    glUseProgram(0);
    glDisable(GL_LIGHTING);
    glColor3f(0.5f, 0.5f, 0.5f);
    glLineWidth(2.0f);
    
    glBegin(GL_LINES);
    for (const auto& bond : frame.bonds) {
        if (static_cast<size_t>(bond.first) < frame.positions.size() && 
            static_cast<size_t>(bond.second) < frame.positions.size()) {
            const Vec3& p1 = frame.positions[bond.first];
            const Vec3& p2 = frame.positions[bond.second];
            glVertex3d(p1.x, p1.y, p1.z);
            glVertex3d(p2.x, p2.y, p2.z);
        }
    }
    glEnd();
}

void Renderer::set_background_color(float r, float g, float b) {
    background_[0] = r;
    background_[1] = g;
    background_[2] = b;
}

void Renderer::get_atom_color(int Z, float* rgb) const {
    // CPK coloring scheme
    switch (Z) {
        case 1:  rgb[0] = 1.0f; rgb[1] = 1.0f; rgb[2] = 1.0f; break; // H - white
        case 6:  rgb[0] = 0.5f; rgb[1] = 0.5f; rgb[2] = 0.5f; break; // C - gray
        case 7:  rgb[0] = 0.2f; rgb[1] = 0.2f; rgb[2] = 1.0f; break; // N - blue
        case 8:  rgb[0] = 1.0f; rgb[1] = 0.0f; rgb[2] = 0.0f; break; // O - red
        case 9:  rgb[0] = 0.0f; rgb[1] = 1.0f; rgb[2] = 0.0f; break; // F - green
        case 15: rgb[0] = 1.0f; rgb[1] = 0.5f; rgb[2] = 0.0f; break; // P - orange
        case 16: rgb[0] = 1.0f; rgb[1] = 1.0f; rgb[2] = 0.0f; break; // S - yellow
        case 17: rgb[0] = 0.0f; rgb[1] = 1.0f; rgb[2] = 0.0f; break; // Cl - green
        default: rgb[0] = 1.0f; rgb[1] = 0.0f; rgb[2] = 1.0f; break; // Unknown - magenta
    }
}

float Renderer::get_atom_radius(int Z) const {
    // Use covalent radii if available, otherwise default
    double radius = get_covalent_radius(static_cast<uint8_t>(Z));
    return radius > 0.0 ? static_cast<float>(radius) : 1.0f;
}

} // namespace vsepr
