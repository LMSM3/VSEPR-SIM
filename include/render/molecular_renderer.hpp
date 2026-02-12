/**
 * molecular_renderer.hpp
 * ======================
 * OpenGL 3D rendering for molecules (atoms as spheres, bonds as cylinders)
 */

#pragma once

#include "sim/molecule.hpp"
#include <GL/gl.h>
#include <cmath>
#include <vector>

namespace vsepr {
namespace render {

// ============================================================================
// Camera
// ============================================================================

struct Camera {
    float zoom = 5.0f;
    float rotation_x = 0.0f;
    float rotation_y = 0.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    
    void apply() const {
        glLoadIdentity();
        glTranslatef(pan_x, pan_y, -zoom);
        glRotatef(rotation_x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotation_y, 0.0f, 1.0f, 0.0f);
    }
};

// ============================================================================
// Sphere Rendering (Atoms)
// ============================================================================

class SphereRenderer {
    std::vector<float> vertices_;
    std::vector<unsigned int> indices_;
    int slices_ = 16;
    int stacks_ = 16;
    
public:
    SphereRenderer(int slices = 16, int stacks = 16);
    
    void render(float x, float y, float z, float radius) const;
    void render_colored(float x, float y, float z, float radius,
                       float r, float g, float b) const;
};

// ============================================================================
// Cylinder Rendering (Bonds)
// ============================================================================

class CylinderRenderer {
public:
    void render(float x1, float y1, float z1,
                float x2, float y2, float z2,
                float radius) const;
    
    void render_colored(float x1, float y1, float z1,
                       float x2, float y2, float z2,
                       float radius,
                       float r, float g, float b) const;
};

// ============================================================================
// Molecular Renderer (Main)
// ============================================================================

struct RenderOptions {
    bool show_atoms = true;
    bool show_bonds = true;
    bool show_lone_pairs = false;
    bool show_axes = true;
    bool show_labels = false;
    float atom_scale = 0.3f;
    float bond_radius = 0.1f;
    bool use_cpk_colors = true;  // CPK coloring scheme
};

class MolecularRenderer {
    SphereRenderer sphere_;
    CylinderRenderer cylinder_;
    Camera camera_;
    RenderOptions options_;
    
public:
    MolecularRenderer();
    
    // Render full molecule
    void render(const Molecule& mol, int width, int height);
    
    // Camera control
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }
    
    // Options
    RenderOptions& options() { return options_; }
    const RenderOptions& options() const { return options_; }
    
    // Element colors (CPK)
    static void get_element_color(uint8_t Z, float& r, float& g, float& b);
    
    // Atom size (van der Waals radii)
    static float get_atom_radius(uint8_t Z);
    
private:
    void setup_viewport(int width, int height);
    void setup_lighting();
    void render_axes();
    void render_atoms(const Molecule& mol);
    void render_bonds(const Molecule& mol);
    void render_atom(const Molecule& mol, size_t idx);
    void render_bond(const Molecule& mol, size_t idx);
};

// ============================================================================
// CPK Color Scheme
// ============================================================================

inline void MolecularRenderer::get_element_color(uint8_t Z, float& r, float& g, float& b) {
    switch (Z) {
        case 1:  r = 1.0f; g = 1.0f; b = 1.0f; break;  // H: white
        case 6:  r = 0.5f; g = 0.5f; b = 0.5f; break;  // C: gray
        case 7:  r = 0.2f; g = 0.2f; b = 1.0f; break;  // N: blue
        case 8:  r = 1.0f; g = 0.0f; b = 0.0f; break;  // O: red
        case 9:  r = 0.0f; g = 1.0f; b = 0.0f; break;  // F: green
        case 15: r = 1.0f; g = 0.5f; b = 0.0f; break;  // P: orange
        case 16: r = 1.0f; g = 1.0f; b = 0.0f; break;  // S: yellow
        case 17: r = 0.0f; g = 1.0f; b = 0.0f; break;  // Cl: green
        case 35: r = 0.6f; g = 0.2f; b = 0.2f; break;  // Br: brown
        case 53: r = 0.5f; g = 0.0f; b = 0.5f; break;  // I: purple
        default: r = 1.0f; g = 0.0f; b = 1.0f; break;  // Unknown: magenta
    }
}

inline float MolecularRenderer::get_atom_radius(uint8_t Z) {
    switch (Z) {
        case 1:  return 0.25f;  // H
        case 6:  return 0.40f;  // C
        case 7:  return 0.35f;  // N
        case 8:  return 0.35f;  // O
        case 9:  return 0.30f;  // F
        case 15: return 0.45f;  // P
        case 16: return 0.45f;  // S
        case 17: return 0.40f;  // Cl
        case 35: return 0.50f;  // Br
        case 53: return 0.55f;  // I
        default: return 0.40f;
    }
}

// ============================================================================
// Mouse/Keyboard Interaction
// ============================================================================

class InteractionHandler {
    bool dragging_ = false;
    double last_x_ = 0.0;
    double last_y_ = 0.0;
    
public:
    void on_mouse_button(int button, int action, double x, double y);
    void on_mouse_move(double x, double y, Camera& camera);
    void on_scroll(double offset, Camera& camera);
    void on_key(int key, Camera& camera);
};

} // namespace render
} // namespace vsepr
