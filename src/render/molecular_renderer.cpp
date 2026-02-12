/**
 * molecular_renderer.cpp
 * ======================
 * Implementation of OpenGL molecular rendering
 */

#include "render/molecular_renderer.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vsepr {
namespace render {

// ============================================================================
// Sphere Renderer
// ============================================================================

SphereRenderer::SphereRenderer(int slices, int stacks) 
    : slices_(slices), stacks_(stacks) {
    // Pre-generate sphere geometry
    for (int i = 0; i <= stacks; ++i) {
        float phi = M_PI * float(i) / float(stacks);
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * M_PI * float(j) / float(slices);
            
            float x = std::sin(phi) * std::cos(theta);
            float y = std::sin(phi) * std::sin(theta);
            float z = std::cos(phi);
            
            vertices_.push_back(x);
            vertices_.push_back(y);
            vertices_.push_back(z);
        }
    }
}

void SphereRenderer::render(float x, float y, float z, float radius) const {
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(radius, radius, radius);
    
    for (int i = 0; i < stacks_; ++i) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= slices_; ++j) {
            int idx1 = i * (slices_ + 1) + j;
            int idx2 = (i + 1) * (slices_ + 1) + j;
            
            float nx1 = vertices_[idx1 * 3 + 0];
            float ny1 = vertices_[idx1 * 3 + 1];
            float nz1 = vertices_[idx1 * 3 + 2];
            
            float nx2 = vertices_[idx2 * 3 + 0];
            float ny2 = vertices_[idx2 * 3 + 1];
            float nz2 = vertices_[idx2 * 3 + 2];
            
            glNormal3f(nx1, ny1, nz1);
            glVertex3f(nx1, ny1, nz1);
            
            glNormal3f(nx2, ny2, nz2);
            glVertex3f(nx2, ny2, nz2);
        }
        glEnd();
    }
    
    glPopMatrix();
}

void SphereRenderer::render_colored(float x, float y, float z, float radius,
                                    float r, float g, float b) const {
    glColor3f(r, g, b);
    render(x, y, z, radius);
}

// ============================================================================
// Cylinder Renderer
// ============================================================================

void CylinderRenderer::render(float x1, float y1, float z1,
                               float x2, float y2, float z2,
                               float radius) const {
    // Vector from start to end
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    float length = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    if (length < 1e-6f) return;
    
    glPushMatrix();
    
    // Translate to start point
    glTranslatef(x1, y1, z1);
    
    // Rotate to align with bond direction
    float vx = dx / length;
    float vy = dy / length;
    float vz = dz / length;
    
    // Angle between z-axis and bond vector
    float angle = std::acos(vz) * 180.0f / M_PI;
    
    // Rotation axis (cross product of z-axis and bond vector)
    float ax = -vy;
    float ay = vx;
    float az = 0.0f;
    float len_a = std::sqrt(ax*ax + ay*ay);
    
    if (len_a > 1e-6f) {
        ax /= len_a;
        ay /= len_a;
        glRotatef(angle, ax, ay, az);
    }
    
    // Draw cylinder as stacked discs
    int slices = 12;
    int stacks = 2;
    
    for (int i = 0; i < stacks; ++i) {
        float z0 = length * float(i) / float(stacks);
        float z1 = length * float(i + 1) / float(stacks);
        
        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * M_PI * float(j) / float(slices);
            float x = radius * std::cos(theta);
            float y = radius * std::sin(theta);
            
            glNormal3f(x / radius, y / radius, 0.0f);
            glVertex3f(x, y, z0);
            glVertex3f(x, y, z1);
        }
        glEnd();
    }
    
    glPopMatrix();
}

void CylinderRenderer::render_colored(float x1, float y1, float z1,
                                       float x2, float y2, float z2,
                                       float radius,
                                       float r, float g, float b) const {
    glColor3f(r, g, b);
    render(x1, y1, z1, x2, y2, z2, radius);
}

// ============================================================================
// Molecular Renderer
// ============================================================================

MolecularRenderer::MolecularRenderer() 
    : sphere_(16, 16) {
    camera_.zoom = 10.0f;
}

void MolecularRenderer::setup_viewport(int width, int height) {
    glViewport(0, 0, width, height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    float aspect = float(width) / float(height > 0 ? height : 1);
    float fov = 45.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;
    
    // Simple perspective projection
    float f = 1.0f / std::tan(fov * M_PI / 360.0f);
    float proj[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (far_plane + near_plane) / (near_plane - far_plane), -1,
        0, 0, (2 * far_plane * near_plane) / (near_plane - far_plane), 0
    };
    glLoadMatrixf(proj);
    
    glMatrixMode(GL_MODELVIEW);
}

void MolecularRenderer::setup_lighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    
    // Light position
    GLfloat light_pos[] = { 5.0f, 5.0f, 10.0f, 1.0f };
    GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat light_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    
    // Material properties
    GLfloat mat_specular[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    GLfloat mat_shininess[] = { 50.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
}

void MolecularRenderer::render_axes() {
    if (!options_.show_axes) return;
    
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    
    // X axis (red)
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(2.0f, 0.0f, 0.0f);
    
    // Y axis (green)
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 2.0f, 0.0f);
    
    // Z axis (blue)
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 2.0f);
    
    glEnd();
    glEnable(GL_LIGHTING);
}

void MolecularRenderer::render_atoms(const Molecule& mol) {
    if (!options_.show_atoms) return;
    
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        render_atom(mol, i);
    }
}

void MolecularRenderer::render_bonds(const Molecule& mol) {
    if (!options_.show_bonds) return;
    
    for (size_t i = 0; i < mol.num_bonds(); ++i) {
        render_bond(mol, i);
    }
}

void MolecularRenderer::render_atom(const Molecule& mol, size_t idx) {
    const auto& atom = mol.atoms[idx];
    double x, y, z;
    mol.get_position(idx, x, y, z);
    
    float r, g, b;
    if (options_.use_cpk_colors) {
        get_element_color(atom.Z, r, g, b);
    } else {
        r = g = b = 0.8f;  // Gray
    }
    
    float radius = get_atom_radius(atom.Z) * options_.atom_scale;
    sphere_.render_colored(x, y, z, radius, r, g, b);
}

void MolecularRenderer::render_bond(const Molecule& mol, size_t idx) {
    const auto& bond = mol.bonds[idx];
    
    double x1, y1, z1, x2, y2, z2;
    mol.get_position(bond.i, x1, y1, z1);
    mol.get_position(bond.j, x2, y2, z2);
    
    // Midpoint
    float mx = (x1 + x2) * 0.5f;
    float my = (y1 + y2) * 0.5f;
    float mz = (z1 + z2) * 0.5f;
    
    // Color by atom type (two-tone bonds)
    float r1, g1, b1, r2, g2, b2;
    get_element_color(mol.atoms[bond.i].Z, r1, g1, b1);
    get_element_color(mol.atoms[bond.j].Z, r2, g2, b2);
    
    // Draw bond in two halves (different colors)
    cylinder_.render_colored(x1, y1, z1, mx, my, mz, 
                            options_.bond_radius, r1, g1, b1);
    cylinder_.render_colored(mx, my, mz, x2, y2, z2,
                            options_.bond_radius, r2, g2, b2);
    
    // For double/triple bonds, draw additional cylinders
    if (bond.order >= 2) {
        // Offset perpendicular to bond
        float dx = x2 - x1;
        float dy = y2 - y1;
        float dz = z2 - z1;
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (len > 1e-6f) {
            float offset = 0.15f;
            float ox = -dy / len * offset;
            float oy = dx / len * offset;
            float oz = 0.0f;
            
            cylinder_.render_colored(x1 + ox, y1 + oy, z1 + oz,
                                    mx + ox, my + oy, mz + oz,
                                    options_.bond_radius * 0.7f, r1, g1, b1);
            cylinder_.render_colored(mx + ox, my + oy, mz + oz,
                                    x2 + ox, y2 + oy, z2 + oz,
                                    options_.bond_radius * 0.7f, r2, g2, b2);
        }
    }
    
    if (bond.order >= 3) {
        // Third bond offset
        float dx = x2 - x1;
        float dy = y2 - y1;
        float dz = z2 - z1;
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (len > 1e-6f) {
            float offset = 0.15f;
            float ox = dy / len * offset;
            float oy = -dx / len * offset;
            float oz = 0.0f;
            
            cylinder_.render_colored(x1 + ox, y1 + oy, z1 + oz,
                                    mx + ox, my + oy, mz + oz,
                                    options_.bond_radius * 0.7f, r1, g1, b1);
            cylinder_.render_colored(mx + ox, my + oy, mz + oz,
                                    x2 + ox, y2 + oy, z2 + oz,
                                    options_.bond_radius * 0.7f, r2, g2, b2);
        }
    }
}

void MolecularRenderer::render(const Molecule& mol, int width, int height) {
    setup_viewport(width, height);
    setup_lighting();
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    camera_.apply();
    
    render_axes();
    render_bonds(mol);  // Draw bonds first (behind atoms)
    render_atoms(mol);
}

// ============================================================================
// Interaction Handler
// ============================================================================

void InteractionHandler::on_mouse_button(int button, int action, double x, double y) {
    if (button == 0) {  // Left button
        dragging_ = (action == 1);  // 1 = press, 0 = release
        last_x_ = x;
        last_y_ = y;
    }
}

void InteractionHandler::on_mouse_move(double x, double y, Camera& camera) {
    if (dragging_) {
        double dx = x - last_x_;
        double dy = y - last_y_;
        
        camera.rotation_y += dx * 0.5f;
        camera.rotation_x += dy * 0.5f;
        
        // Clamp rotation
        camera.rotation_x = std::clamp(camera.rotation_x, -89.0f, 89.0f);
        
        last_x_ = x;
        last_y_ = y;
    }
}

void InteractionHandler::on_scroll(double offset, Camera& camera) {
    camera.zoom -= offset * 0.5f;
    camera.zoom = std::clamp(camera.zoom, 1.0f, 50.0f);
}

void InteractionHandler::on_key(int key, Camera& camera) {
    float step = 0.5f;
    
    // WASD for panning
    if (key == 'W' || key == 'w') camera.pan_y += step;
    if (key == 'S' || key == 's') camera.pan_y -= step;
    if (key == 'A' || key == 'a') camera.pan_x -= step;
    if (key == 'D' || key == 'd') camera.pan_x += step;
    
    // Reset view
    if (key == 'R' || key == 'r') {
        camera.zoom = 10.0f;
        camera.rotation_x = 0.0f;
        camera.rotation_y = 0.0f;
        camera.pan_x = 0.0f;
        camera.pan_y = 0.0f;
    }
}

} // namespace render
} // namespace vsepr
