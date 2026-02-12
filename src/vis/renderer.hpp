#pragma once

#include "core/frame_snapshot.hpp"
#include "core/math_vec3.hpp"
#include "box/pbc.hpp"
#include <string>

// Forward declarations to avoid including GLFW headers
struct GLFWwindow;

namespace vsepr {

/**
 * Simple camera for 3D molecular visualization.
 * Supports orbit controls (rotate around molecule center).
 */
class Camera {
public:
    Camera();
    
    // Set camera view based on mouse input
    void orbit(double dx, double dy);           // Rotate around target
    void pan(double dx, double dy);             // Pan in screen space
    void zoom(double delta);                     // Zoom in/out
    
    // Get view/projection matrices (as 4x4 in column-major order)
    void get_view_matrix(float* matrix) const;
    void get_projection_matrix(float* matrix, float aspect) const;
    
    // Set target point (molecule center)
    void set_target(const Vec3& target) { target_ = target; }
    void reset();
    
private:
    Vec3 target_;           // Point we're looking at
    double distance_;       // Distance from target
    double theta_;          // Azimuthal angle (radians)
    double phi_;            // Polar angle (radians)
    double fov_;            // Field of view (degrees)
    double near_clip_;
    double far_clip_;
};

/**
 * OpenGL-based molecular renderer.
 * Renders atoms as spheres and bonds as cylinders.
 */
class Renderer {
public:
    Renderer();
    ~Renderer();
    
    // Initialize OpenGL resources (must be called with active GL context)
    bool initialize();
    
    // Render a frame snapshot
    void render(const FrameSnapshot& frame, int width, int height);
    
    // Camera access
    Camera& camera() { return camera_; }
    
    // Settings
    void set_background_color(float r, float g, float b);
    void set_atom_scale(float scale) { atom_scale_ = scale; }
    void set_bond_radius(float radius) { bond_radius_ = radius; }
    void set_show_bonds(bool show) { show_bonds_ = show; }
    void set_show_box(bool show) { show_box_ = show; }
    
private:
    void render_atoms(const FrameSnapshot& frame);
    void render_bonds(const FrameSnapshot& frame);
    void render_box(const BoxOrtho& box);
    void render_ui_overlay(const FrameSnapshot& frame, int width, int height);
    
    // Get color for atomic number
    void get_atom_color(int Z, float* rgb) const;
    float get_atom_radius(int Z) const;
    
    Camera camera_;
    float background_[3];
    float atom_scale_;
    float bond_radius_;
    bool show_bonds_;
    bool show_box_;
    
    // OpenGL resources (opaque handles)
    unsigned int sphere_vao_;
    unsigned int sphere_vbo_;
    unsigned int sphere_ebo_;
    unsigned int shader_program_;
    int sphere_index_count_;
};

} // namespace vsepr
