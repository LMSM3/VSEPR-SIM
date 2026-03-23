#pragma once
/**
 * bead_inspector_view.hpp — Multi-Scale Bead Inspection Viewer
 *
 * Post-computation analysis layer for detailed bead inspection.
 * Renders each bead as a structured object derived from the bead's
 * solved internal geometry (BeadVisualRecord), not from hardcoded
 * shape class defaults.
 *
 * Scale ladder (continuous descent):
 *   Scene Cloud → Cluster → Bead Detail → Bead Interior → Fragment Compare
 *
 * Visual layers per bead:
 *   1. Outer shell (effective radius, anisotropy, confidence opacity)
 *   2. Local frame triad (x/y/z axes, provenance-styled)
 *   3. Interior scaffold (solved direction vectors, ring disks, spokes)
 *   4. Descriptor surface patches (directional heatmap on shell)
 *   5. Residual indicators (mismatch halos, error bands)
 *   6. Source fragment miniature (comparison overlay)
 *
 * The renderer reads only from BeadVisualRecord. It never re-solves
 * bead geometry, classifies chemistry, or infers VSEPR structure.
 *
 * Anti-black-box: all internal structure is directly visible.
 * Deterministic: same record → same visual output.
 */

#include "coarse_grain/vis/bead_visual_record.hpp"

#ifdef BUILD_VISUALIZATION

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {
namespace vis {

// ============================================================================
// BeadInspectorView — Detailed Bead Inspection Renderer
// ============================================================================

class BeadInspectorView {
public:
    /**
     * Render the bead detail view for a single bead.
     *
     * Call this within an existing GL context (typically from
     * CGVizViewer when a bead is selected for deep inspection).
     *
     * @param rec         The visual record to render
     * @param view_mode   Current view mode
     * @param scale_level Current scale level
     * @param clip_alpha  Clipping plane position for cutaway (0=none, 1=full cut)
     */
    static void render_bead(const BeadVisualRecord& rec,
                             ViewMode view_mode,
                             float clip_alpha = 0.0f);

    /**
     * Draw the ImGui inspector panel for the selected bead.
     *
     * Structured sections:
     *   Identity → Geometry → Structural Solution → Descriptors →
     *   Layer 2 (Surface State) → Layer 3 (Internal Ref) →
     *   Residuals → Connectivity
     */
    static void draw_inspector_panel(const BeadVisualRecord& rec,
                                      ViewMode& view_mode,
                                      float& clip_alpha);

private:
    struct Color3 { float r, g, b; };
    struct Color4 { float r, g, b, a; };

    // ---- Primitive rendering ----
    static void draw_sphere(float cx, float cy, float cz,
                             float radius, Color4 col, int segments = 20);
    static void draw_line(float x0, float y0, float z0,
                           float x1, float y1, float z1,
                           Color3 col, float width = 2.0f);
    static void draw_disk(float cx, float cy, float cz,
                           float nx, float ny, float nz,
                           float radius, Color4 col, int segments = 24);
    static void draw_cone(float bx, float by, float bz,
                           float tx, float ty, float tz,
                           float radius, Color3 col, int segments = 8);

    // ---- Composite rendering ----
    static void render_shell(const BeadVisualRecord& rec, float alpha);
    static void render_frame_triad(const BeadVisualRecord& rec);
    static void render_scaffold(const BeadVisualRecord& rec);
    static void render_descriptor_patches(const BeadVisualRecord& rec);
    static void render_residual_halo(const BeadVisualRecord& rec);
    static void render_source_fragment(const BeadVisualRecord& rec);
    static void render_clipping_plane(const BeadVisualRecord& rec, float clip_alpha);

    // ---- Layer 2: Surface-State heatmap ----
    static void render_surface_state_layer(const BeadVisualRecord& rec);

    // ---- Layer 3: Internal structural reference ----
    static void render_internal_reference(const BeadVisualRecord& rec);

    // ---- Element colors ----
    static Color3 element_color(int atomic_number) {
        switch (atomic_number) {
            case 1:  return {0.9f, 0.9f, 0.9f};  // H: white
            case 6:  return {0.3f, 0.3f, 0.3f};  // C: dark gray
            case 7:  return {0.2f, 0.2f, 0.9f};  // N: blue
            case 8:  return {0.9f, 0.2f, 0.2f};  // O: red
            case 9:  return {0.2f, 0.9f, 0.2f};  // F: green
            case 15: return {0.9f, 0.5f, 0.1f};  // P: orange
            case 16: return {0.9f, 0.8f, 0.2f};  // S: yellow
            case 17: return {0.1f, 0.9f, 0.1f};  // Cl: green
            case 26: return {0.5f, 0.3f, 0.1f};  // Fe: brown
            default: return {0.6f, 0.4f, 0.7f};  // Default: purple
        }
    }

    static Color3 scalar_to_color(double value, double vmin, double vmax) {
        if (vmax <= vmin) return {0.5f, 0.5f, 0.5f};
        float t = static_cast<float>((value - vmin) / (vmax - vmin));
        t = std::max(0.0f, std::min(1.0f, t));
        float r = std::min(1.0f, 2.0f * t);
        float g = 1.0f - 2.0f * std::abs(t - 0.5f);
        float b = std::min(1.0f, 2.0f * (1.0f - t));
        return {r, g, b};
    }
};

// ============================================================================
// Implementation — Primitive Rendering
// ============================================================================

inline void BeadInspectorView::draw_sphere(float cx, float cy, float cz,
                                            float radius, Color4 col, int seg) {
    constexpr float pi = 3.14159265f;

    if (col.a < 1.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }

    glColor4f(col.r, col.g, col.b, col.a);
    for (int i = 0; i < seg; ++i) {
        float lat0 = pi * (-0.5f + static_cast<float>(i) / seg);
        float lat1 = pi * (-0.5f + static_cast<float>(i + 1) / seg);
        float y0 = std::sin(lat0), yr0 = std::cos(lat0);
        float y1 = std::sin(lat1), yr1 = std::cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= seg; ++j) {
            float lng = 2.0f * pi * static_cast<float>(j) / seg;
            float xl = std::cos(lng), zl = std::sin(lng);
            glNormal3f(xl * yr0, y0, zl * yr0);
            glVertex3f(cx + radius * xl * yr0, cy + radius * y0, cz + radius * zl * yr0);
            glNormal3f(xl * yr1, y1, zl * yr1);
            glVertex3f(cx + radius * xl * yr1, cy + radius * y1, cz + radius * zl * yr1);
        }
        glEnd();
    }

    if (col.a < 1.0f) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

inline void BeadInspectorView::draw_line(float x0, float y0, float z0,
                                          float x1, float y1, float z1,
                                          Color3 col, float width) {
    glLineWidth(width);
    glBegin(GL_LINES);
    glColor3f(col.r, col.g, col.b);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y1, z1);
    glEnd();
}

inline void BeadInspectorView::draw_disk(float cx, float cy, float cz,
                                          float nx, float ny, float nz,
                                          float radius, Color4 col, int seg) {
    constexpr float pi = 3.14159265f;

    if (col.a < 1.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Build tangent frame from normal
    float ax, ay, az, bx, by, bz;
    if (std::abs(ny) < 0.9f) {
        // Cross with Y-up
        ax = nz; ay = 0.0f; az = -nx;
    } else {
        ax = 0.0f; ay = -nz; az = ny;
    }
    float alen = std::sqrt(ax*ax + ay*ay + az*az);
    if (alen > 1e-6f) { ax /= alen; ay /= alen; az /= alen; }
    bx = ny*az - nz*ay; by = nz*ax - nx*az; bz = nx*ay - ny*ax;

    glColor4f(col.r, col.g, col.b, col.a);
    glNormal3f(nx, ny, nz);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(cx, cy, cz);
    for (int i = 0; i <= seg; ++i) {
        float angle = 2.0f * pi * static_cast<float>(i) / seg;
        float c_a = std::cos(angle), s_a = std::sin(angle);
        glVertex3f(cx + radius * (c_a * ax + s_a * bx),
                   cy + radius * (c_a * ay + s_a * by),
                   cz + radius * (c_a * az + s_a * bz));
    }
    glEnd();

    if (col.a < 1.0f) glDisable(GL_BLEND);
}

inline void BeadInspectorView::draw_cone(float bx, float by, float bz,
                                          float tx, float ty, float tz,
                                          float radius, Color3 col, int seg) {
    constexpr float pi = 3.14159265f;
    float dx = tx - bx, dy = ty - by, dz = tz - bz;
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-6f) return;
    dx /= len; dy /= len; dz /= len;

    // Build tangent frame
    float ax, ay, az;
    if (std::abs(dy) < 0.9f) {
        ax = dz; ay = 0.0f; az = -dx;
    } else {
        ax = 0.0f; ay = -dz; az = dy;
    }
    float alen = std::sqrt(ax*ax + ay*ay + az*az);
    if (alen > 1e-6f) { ax /= alen; ay /= alen; az /= alen; }
    float cx2 = dy*az - dz*ay, cy2 = dz*ax - dx*az, cz2 = dx*ay - dy*ax;

    glColor3f(col.r, col.g, col.b);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(tx, ty, tz);  // tip
    for (int i = 0; i <= seg; ++i) {
        float angle = 2.0f * pi * static_cast<float>(i) / seg;
        float ca = std::cos(angle), sa = std::sin(angle);
        glVertex3f(bx + radius * (ca * ax + sa * cx2),
                   by + radius * (ca * ay + sa * cy2),
                   bz + radius * (ca * az + sa * cz2));
    }
    glEnd();
}

// ============================================================================
// Implementation — Composite Rendering
// ============================================================================

inline void BeadInspectorView::render_shell(const BeadVisualRecord& rec,
                                             float alpha) {
    float cx = static_cast<float>(rec.center_world.x);
    float cy = static_cast<float>(rec.center_world.y);
    float cz = static_cast<float>(rec.center_world.z);
    float r  = static_cast<float>(rec.effective_radius);

    // Shell opacity encodes mapping confidence
    float shell_alpha = alpha * static_cast<float>(
        0.3 + 0.7 * rec.mapping_confidence);

    Color4 shell_col;
    if (rec.is_aromatic) {
        shell_col = {0.4f, 0.3f, 0.7f, shell_alpha};  // Purple for aromatics
    } else if (rec.is_metal_centered) {
        shell_col = {0.7f, 0.5f, 0.2f, shell_alpha};  // Gold for metals
    } else if (rec.is_cyclic) {
        shell_col = {0.3f, 0.6f, 0.4f, shell_alpha};  // Teal for cyclic
    } else {
        shell_col = {0.3f, 0.5f, 0.8f, shell_alpha};  // Blue default
    }

    draw_sphere(cx, cy, cz, r, shell_col, 24);
}

inline void BeadInspectorView::render_frame_triad(const BeadVisualRecord& rec) {
    if (!rec.frame_valid) return;

    float cx = static_cast<float>(rec.center_world.x);
    float cy = static_cast<float>(rec.center_world.y);
    float cz = static_cast<float>(rec.center_world.z);
    float axis_len = static_cast<float>(rec.effective_radius * 1.5);

    // Frame axes in world space (columns of local_frame matrix)
    float ax1_x = static_cast<float>(rec.local_frame(0, 0));
    float ax1_y = static_cast<float>(rec.local_frame(1, 0));
    float ax1_z = static_cast<float>(rec.local_frame(2, 0));

    float ax2_x = static_cast<float>(rec.local_frame(0, 1));
    float ax2_y = static_cast<float>(rec.local_frame(1, 1));
    float ax2_z = static_cast<float>(rec.local_frame(2, 1));

    float ax3_x = static_cast<float>(rec.local_frame(0, 2));
    float ax3_y = static_cast<float>(rec.local_frame(1, 2));
    float ax3_z = static_cast<float>(rec.local_frame(2, 2));

    // Line width based on frame confidence
    float line_w = (rec.frame_confidence > 0.5) ? 3.0f : 1.5f;

    glDisable(GL_LIGHTING);

    // Axis 1 (red) — smallest moment
    draw_line(cx, cy, cz,
              cx + ax1_x * axis_len, cy + ax1_y * axis_len, cz + ax1_z * axis_len,
              {0.9f, 0.2f, 0.2f}, line_w);
    // Arrow tip
    float tip_len = axis_len * 0.15f;
    draw_cone(cx + ax1_x * (axis_len - tip_len), cy + ax1_y * (axis_len - tip_len),
              cz + ax1_z * (axis_len - tip_len),
              cx + ax1_x * axis_len, cy + ax1_y * axis_len, cz + ax1_z * axis_len,
              tip_len * 0.3f, {0.9f, 0.2f, 0.2f});

    // Axis 2 (green) — middle moment
    draw_line(cx, cy, cz,
              cx + ax2_x * axis_len, cy + ax2_y * axis_len, cz + ax2_z * axis_len,
              {0.2f, 0.9f, 0.2f}, line_w);
    draw_cone(cx + ax2_x * (axis_len - tip_len), cy + ax2_y * (axis_len - tip_len),
              cz + ax2_z * (axis_len - tip_len),
              cx + ax2_x * axis_len, cy + ax2_y * axis_len, cz + ax2_z * axis_len,
              tip_len * 0.3f, {0.2f, 0.9f, 0.2f});

    // Axis 3 (blue) — largest moment (normal)
    draw_line(cx, cy, cz,
              cx + ax3_x * axis_len, cy + ax3_y * axis_len, cz + ax3_z * axis_len,
              {0.2f, 0.4f, 0.9f}, line_w);
    draw_cone(cx + ax3_x * (axis_len - tip_len), cy + ax3_y * (axis_len - tip_len),
              cz + ax3_z * (axis_len - tip_len),
              cx + ax3_x * axis_len, cy + ax3_y * axis_len, cz + ax3_z * axis_len,
              tip_len * 0.3f, {0.2f, 0.4f, 0.9f});

    glEnable(GL_LIGHTING);
}

inline void BeadInspectorView::render_scaffold(const BeadVisualRecord& rec) {
    if (rec.solved_directions_local.empty()) return;

    float cx = static_cast<float>(rec.center_world.x);
    float cy = static_cast<float>(rec.center_world.y);
    float cz = static_cast<float>(rec.center_world.z);
    float arm_len = static_cast<float>(rec.effective_radius * 0.8);

    glDisable(GL_LIGHTING);

    // Center node
    draw_sphere(cx, cy, cz, arm_len * 0.08f,
                {0.9f, 0.9f, 0.9f, 1.0f}, 8);

    // Direction arms — each is a solved vector transformed to world space
    Color3 arm_col = {0.8f, 0.6f, 0.2f};  // Warm gold for structural arms
    for (const auto& dir_local : rec.solved_directions_local) {
        // Transform local direction to world space
        float wx, wy, wz;
        if (rec.frame_valid) {
            wx = static_cast<float>(
                rec.local_frame(0,0)*dir_local.x +
                rec.local_frame(0,1)*dir_local.y +
                rec.local_frame(0,2)*dir_local.z);
            wy = static_cast<float>(
                rec.local_frame(1,0)*dir_local.x +
                rec.local_frame(1,1)*dir_local.y +
                rec.local_frame(1,2)*dir_local.z);
            wz = static_cast<float>(
                rec.local_frame(2,0)*dir_local.x +
                rec.local_frame(2,1)*dir_local.y +
                rec.local_frame(2,2)*dir_local.z);
        } else {
            wx = static_cast<float>(dir_local.x);
            wy = static_cast<float>(dir_local.y);
            wz = static_cast<float>(dir_local.z);
        }

        float ex = cx + wx * arm_len;
        float ey = cy + wy * arm_len;
        float ez = cz + wz * arm_len;

        draw_line(cx, cy, cz, ex, ey, ez, arm_col, 2.0f);

        // Small sphere at arm tip
        draw_sphere(ex, ey, ez, arm_len * 0.05f,
                    {arm_col.r, arm_col.g, arm_col.b, 1.0f}, 6);
    }

    // Planar ring disk (if applicable)
    if (rec.has_planar_component && rec.frame_valid) {
        float nx = static_cast<float>(
            rec.local_frame(0,0)*rec.planar_normal_local.x +
            rec.local_frame(0,1)*rec.planar_normal_local.y +
            rec.local_frame(0,2)*rec.planar_normal_local.z);
        float ny = static_cast<float>(
            rec.local_frame(1,0)*rec.planar_normal_local.x +
            rec.local_frame(1,1)*rec.planar_normal_local.y +
            rec.local_frame(1,2)*rec.planar_normal_local.z);
        float nz = static_cast<float>(
            rec.local_frame(2,0)*rec.planar_normal_local.x +
            rec.local_frame(2,1)*rec.planar_normal_local.y +
            rec.local_frame(2,2)*rec.planar_normal_local.z);

        Color4 disk_col = rec.is_aromatic ?
            Color4{0.5f, 0.3f, 0.8f, 0.25f} :  // Purple for aromatic
            Color4{0.3f, 0.7f, 0.5f, 0.20f};    // Teal for cyclic

        draw_disk(cx, cy, cz, nx, ny, nz,
                  arm_len * 0.7f, disk_col, 32);
    }

    glEnable(GL_LIGHTING);
}

inline void BeadInspectorView::render_descriptor_patches(
    const BeadVisualRecord& rec)
{
    if (!rec.has_descriptor || rec.descriptor_field.empty()) return;

    float cx = static_cast<float>(rec.center_world.x);
    float cy = static_cast<float>(rec.center_world.y);
    float cz = static_cast<float>(rec.center_world.z);
    float r  = static_cast<float>(rec.effective_radius * 1.02);

    // Find value range for steric channel
    double vmin = 1e30, vmax = -1e30;
    for (const auto& s : rec.descriptor_field) {
        if (s.steric_value < vmin) vmin = s.steric_value;
        if (s.steric_value > vmax) vmax = s.steric_value;
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.0f);

    glBegin(GL_POINTS);
    for (const auto& sample : rec.descriptor_field) {
        // Transform direction from local to world
        float wx, wy, wz;
        if (rec.frame_valid) {
            wx = static_cast<float>(
                rec.local_frame(0,0)*sample.direction_local.x +
                rec.local_frame(0,1)*sample.direction_local.y +
                rec.local_frame(0,2)*sample.direction_local.z);
            wy = static_cast<float>(
                rec.local_frame(1,0)*sample.direction_local.x +
                rec.local_frame(1,1)*sample.direction_local.y +
                rec.local_frame(1,2)*sample.direction_local.z);
            wz = static_cast<float>(
                rec.local_frame(2,0)*sample.direction_local.x +
                rec.local_frame(2,1)*sample.direction_local.y +
                rec.local_frame(2,2)*sample.direction_local.z);
        } else {
            wx = static_cast<float>(sample.direction_local.x);
            wy = static_cast<float>(sample.direction_local.y);
            wz = static_cast<float>(sample.direction_local.z);
        }

        Color3 col = scalar_to_color(sample.steric_value, vmin, vmax);
        glColor4f(col.r, col.g, col.b, 0.7f);
        glVertex3f(cx + wx * r, cy + wy * r, cz + wz * r);
    }
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

inline void BeadInspectorView::render_residual_halo(
    const BeadVisualRecord& rec)
{
    if (rec.descriptor_residual < 0.01 && rec.mapping_residual < 0.01) return;

    float cx = static_cast<float>(rec.center_world.x);
    float cy = static_cast<float>(rec.center_world.y);
    float cz = static_cast<float>(rec.center_world.z);

    // Residual halo: larger, redder, more transparent with higher error
    float halo_r = static_cast<float>(rec.effective_radius * (1.1 + rec.descriptor_residual * 0.3));
    float halo_alpha = static_cast<float>(0.1 + std::min(0.3, rec.descriptor_residual));

    draw_sphere(cx, cy, cz, halo_r,
                {0.9f, 0.3f, 0.1f, halo_alpha}, 16);

    // Mapping residual: COM→COG displacement vector
    if (rec.source_fragment.populated && rec.mapping_residual > 0.01) {
        glDisable(GL_LIGHTING);
        float com_x = static_cast<float>(rec.source_fragment.center_of_mass.x);
        float com_y = static_cast<float>(rec.source_fragment.center_of_mass.y);
        float com_z = static_cast<float>(rec.source_fragment.center_of_mass.z);
        float cog_x = static_cast<float>(rec.source_fragment.center_of_geometry.x);
        float cog_y = static_cast<float>(rec.source_fragment.center_of_geometry.y);
        float cog_z = static_cast<float>(rec.source_fragment.center_of_geometry.z);
        draw_line(com_x, com_y, com_z, cog_x, cog_y, cog_z,
                  {0.9f, 0.4f, 0.1f}, 2.5f);
        glEnable(GL_LIGHTING);
    }
}

inline void BeadInspectorView::render_source_fragment(
    const BeadVisualRecord& rec)
{
    if (!rec.source_fragment.populated) return;

    // Draw source atoms as small spheres
    for (const auto& atom : rec.source_fragment.atoms) {
        float ax = static_cast<float>(atom.position.x);
        float ay = static_cast<float>(atom.position.y);
        float az = static_cast<float>(atom.position.z);
        float ar = atom.radius * 0.25f;  // Reduced size for miniature

        Color3 ec = element_color(atom.atomic_number);
        draw_sphere(ax, ay, az, ar, {ec.r, ec.g, ec.b, 0.9f}, 10);
    }

    // Draw bonds as lines
    glDisable(GL_LIGHTING);
    for (const auto& bond : rec.source_fragment.bonds) {
        if (bond.i < 0 || bond.i >= static_cast<int>(rec.source_fragment.atoms.size()) ||
            bond.j < 0 || bond.j >= static_cast<int>(rec.source_fragment.atoms.size()))
            continue;

        const auto& a = rec.source_fragment.atoms[bond.i];
        const auto& b = rec.source_fragment.atoms[bond.j];
        float width = (bond.order >= 2) ? 2.5f : 1.5f;
        draw_line(static_cast<float>(a.position.x),
                  static_cast<float>(a.position.y),
                  static_cast<float>(a.position.z),
                  static_cast<float>(b.position.x),
                  static_cast<float>(b.position.y),
                  static_cast<float>(b.position.z),
                  {0.6f, 0.6f, 0.6f}, width);
    }
    glEnable(GL_LIGHTING);
}

// ============================================================================
// Implementation — Layer 2: Effective Surface-State Heatmap
// ============================================================================

inline void BeadInspectorView::render_surface_state_layer(
    const BeadVisualRecord& rec)
{
    if (!rec.surface_state_grid.populated) return;

    const auto& grid = rec.surface_state_grid;
    float cx = static_cast<float>(rec.center_world.x);
    float cy = static_cast<float>(rec.center_world.y);
    float cz = static_cast<float>(rec.center_world.z);
    float r  = static_cast<float>(rec.effective_radius * 1.01);

    // Get value range for the active channel
    double vmin = 0.0, vmax = 0.0;
    grid.channel_range(rec.active_surface_channel, vmin, vmax);

    float alpha = rec.surface_state_opacity;
    float r_exag = rec.radial_exaggeration;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);

    // Render as quad strips, one strip per theta band
    for (int it = 0; it < grid.n_theta; ++it) {
        glBegin(GL_QUAD_STRIP);
        for (int ip = 0; ip <= grid.n_phi; ++ip) {
            int jp = ip % grid.n_phi;

            // Bottom vertex (theta band it)
            const auto& v0 = grid.at(it, jp);
            double val0 = grid.channel_value(v0, rec.active_surface_channel);
            Color3 c0 = scalar_to_color(val0, vmin, vmax);

            // Radial scaling: optionally exaggerate radius by value
            float r0 = r;
            if (r_exag > 0.0f && vmax > vmin) {
                float t0 = static_cast<float>((val0 - vmin) / (vmax - vmin));
                r0 = r * (1.0f + r_exag * (t0 - 0.5f));
            }

            // Transform direction from local to world
            float wx0, wy0, wz0;
            if (rec.frame_valid) {
                wx0 = static_cast<float>(
                    rec.local_frame(0,0)*v0.direction_local.x +
                    rec.local_frame(0,1)*v0.direction_local.y +
                    rec.local_frame(0,2)*v0.direction_local.z);
                wy0 = static_cast<float>(
                    rec.local_frame(1,0)*v0.direction_local.x +
                    rec.local_frame(1,1)*v0.direction_local.y +
                    rec.local_frame(1,2)*v0.direction_local.z);
                wz0 = static_cast<float>(
                    rec.local_frame(2,0)*v0.direction_local.x +
                    rec.local_frame(2,1)*v0.direction_local.y +
                    rec.local_frame(2,2)*v0.direction_local.z);
            } else {
                wx0 = static_cast<float>(v0.direction_local.x);
                wy0 = static_cast<float>(v0.direction_local.y);
                wz0 = static_cast<float>(v0.direction_local.z);
            }

            glColor4f(c0.r, c0.g, c0.b, alpha);
            glNormal3f(wx0, wy0, wz0);
            glVertex3f(cx + wx0 * r0, cy + wy0 * r0, cz + wz0 * r0);

            // Top vertex (theta band it+1)
            const auto& v1 = grid.at(it + 1, jp);
            double val1 = grid.channel_value(v1, rec.active_surface_channel);
            Color3 c1 = scalar_to_color(val1, vmin, vmax);

            float r1 = r;
            if (r_exag > 0.0f && vmax > vmin) {
                float t1 = static_cast<float>((val1 - vmin) / (vmax - vmin));
                r1 = r * (1.0f + r_exag * (t1 - 0.5f));
            }

            float wx1, wy1, wz1;
            if (rec.frame_valid) {
                wx1 = static_cast<float>(
                    rec.local_frame(0,0)*v1.direction_local.x +
                    rec.local_frame(0,1)*v1.direction_local.y +
                    rec.local_frame(0,2)*v1.direction_local.z);
                wy1 = static_cast<float>(
                    rec.local_frame(1,0)*v1.direction_local.x +
                    rec.local_frame(1,1)*v1.direction_local.y +
                    rec.local_frame(1,2)*v1.direction_local.z);
                wz1 = static_cast<float>(
                    rec.local_frame(2,0)*v1.direction_local.x +
                    rec.local_frame(2,1)*v1.direction_local.y +
                    rec.local_frame(2,2)*v1.direction_local.z);
            } else {
                wx1 = static_cast<float>(v1.direction_local.x);
                wy1 = static_cast<float>(v1.direction_local.y);
                wz1 = static_cast<float>(v1.direction_local.z);
            }

            glColor4f(c1.r, c1.g, c1.b, alpha);
            glNormal3f(wx1, wy1, wz1);
            glVertex3f(cx + wx1 * r1, cy + wy1 * r1, cz + wz1 * r1);
        }
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ============================================================================
// Implementation — Layer 3: Internal Structural Reference
// ============================================================================

inline void BeadInspectorView::render_internal_reference(
    const BeadVisualRecord& rec)
{
    if (!rec.source_fragment.populated) return;

    const auto& opts = rec.internal_ref_options;

    // Draw source atoms as small spheres
    if (opts.show_atoms) {
        for (const auto& atom : rec.source_fragment.atoms) {
            float ax = static_cast<float>(atom.position.x);
            float ay = static_cast<float>(atom.position.y);
            float az = static_cast<float>(atom.position.z);
            float ar = atom.radius * opts.atom_scale;

            Color3 ec = opts.element_coloring ?
                element_color(atom.atomic_number) :
                Color3{0.6f, 0.6f, 0.6f};

            draw_sphere(ax, ay, az, ar, {ec.r, ec.g, ec.b, 0.9f}, 10);
        }
    }

    // Draw bonds as sticks
    if (opts.show_bonds) {
        glDisable(GL_LIGHTING);
        for (const auto& bond : rec.source_fragment.bonds) {
            if (bond.i < 0 || bond.i >= static_cast<int>(rec.source_fragment.atoms.size()) ||
                bond.j < 0 || bond.j >= static_cast<int>(rec.source_fragment.atoms.size()))
                continue;

            const auto& a = rec.source_fragment.atoms[bond.i];
            const auto& b = rec.source_fragment.atoms[bond.j];
            float width = opts.bond_width;
            if (bond.order >= 3) width = opts.bond_width * 2.0f;
            else if (bond.order >= 2) width = opts.bond_width * 1.5f;

            // Colour by midpoint of two element colours
            Color3 ca = opts.element_coloring ?
                element_color(a.atomic_number) : Color3{0.6f, 0.6f, 0.6f};
            Color3 cb = opts.element_coloring ?
                element_color(b.atomic_number) : Color3{0.6f, 0.6f, 0.6f};

            // Draw two half-bonds, each in its atom's colour
            float mx = 0.5f * (static_cast<float>(a.position.x) +
                               static_cast<float>(b.position.x));
            float my = 0.5f * (static_cast<float>(a.position.y) +
                               static_cast<float>(b.position.y));
            float mz = 0.5f * (static_cast<float>(a.position.z) +
                               static_cast<float>(b.position.z));

            draw_line(static_cast<float>(a.position.x),
                      static_cast<float>(a.position.y),
                      static_cast<float>(a.position.z),
                      mx, my, mz, ca, width);
            draw_line(mx, my, mz,
                      static_cast<float>(b.position.x),
                      static_cast<float>(b.position.y),
                      static_cast<float>(b.position.z),
                      cb, width);
        }
        glEnable(GL_LIGHTING);
    }

    // Draw fragment orientation markers (local frame axes at COM)
    if (opts.show_orientation_markers && rec.frame_valid) {
        float com_x = static_cast<float>(rec.source_fragment.center_of_mass.x);
        float com_y = static_cast<float>(rec.source_fragment.center_of_mass.y);
        float com_z = static_cast<float>(rec.source_fragment.center_of_mass.z);
        float marker_len = static_cast<float>(rec.effective_radius * 0.5);

        glDisable(GL_LIGHTING);

        // Axis 1 (red) — smallest moment
        float a1x = static_cast<float>(rec.local_frame(0, 0));
        float a1y = static_cast<float>(rec.local_frame(1, 0));
        float a1z = static_cast<float>(rec.local_frame(2, 0));
        draw_line(com_x, com_y, com_z,
                  com_x + a1x * marker_len, com_y + a1y * marker_len,
                  com_z + a1z * marker_len,
                  {0.9f, 0.3f, 0.3f}, 1.5f);

        // Axis 2 (green) — middle moment
        float a2x = static_cast<float>(rec.local_frame(0, 1));
        float a2y = static_cast<float>(rec.local_frame(1, 1));
        float a2z = static_cast<float>(rec.local_frame(2, 1));
        draw_line(com_x, com_y, com_z,
                  com_x + a2x * marker_len, com_y + a2y * marker_len,
                  com_z + a2z * marker_len,
                  {0.3f, 0.9f, 0.3f}, 1.5f);

        // Axis 3 (blue) — largest moment / normal
        float a3x = static_cast<float>(rec.local_frame(0, 2));
        float a3y = static_cast<float>(rec.local_frame(1, 2));
        float a3z = static_cast<float>(rec.local_frame(2, 2));
        draw_line(com_x, com_y, com_z,
                  com_x + a3x * marker_len, com_y + a3y * marker_len,
                  com_z + a3z * marker_len,
                  {0.3f, 0.4f, 0.9f}, 1.5f);

        glEnable(GL_LIGHTING);
    }
}

inline void BeadInspectorView::render_clipping_plane(
    const BeadVisualRecord& rec, float clip_alpha)
{
    if (clip_alpha <= 0.0f) return;

    // Use the clipping plane to slice the shell
    // Plane equation: ax + by + cz + d = 0
    // Use the frame axis1 as clip direction, offset by clip_alpha
    float cx = static_cast<float>(rec.center_world.x);
    float clip_offset = static_cast<float>(rec.effective_radius) *
                        (1.0f - clip_alpha * 2.0f);

    GLdouble plane[] = {1.0, 0.0, 0.0, -(cx + clip_offset)};
    glClipPlane(GL_CLIP_PLANE0, plane);
    glEnable(GL_CLIP_PLANE0);
}

// ============================================================================
// Implementation — Main Render Entry
// ============================================================================

inline void BeadInspectorView::render_bead(
    const BeadVisualRecord& rec,
    ViewMode view_mode,
    float clip_alpha)
{
    // Apply clipping if cutaway mode
    if (view_mode == ViewMode::Cutaway) {
        render_clipping_plane(rec, clip_alpha);
    }

    switch (view_mode) {
    case ViewMode::Shell:
        render_shell(rec, 0.6f);
        break;

    case ViewMode::Scaffold:
        render_shell(rec, 0.25f);       // Translucent shell
        render_frame_triad(rec);        // Local coordinate frame
        render_scaffold(rec);           // Solved direction vectors
        render_descriptor_patches(rec); // Surface descriptor field
        break;

    case ViewMode::SurfaceState:
        render_shell(rec, 0.10f);            // Ghost shell
        render_surface_state_layer(rec);     // Layer 2: spherical heatmap
        render_frame_triad(rec);
        break;

    case ViewMode::InternalRef:
        render_shell(rec, 0.12f);            // Ghost shell
        render_internal_reference(rec);      // Layer 3: atomistic fragment
        render_frame_triad(rec);
        break;

    case ViewMode::Skeleton:
        render_shell(rec, 0.15f);       // Very translucent shell
        render_source_fragment(rec);    // Atomistic stick model inside
        render_frame_triad(rec);
        break;

    case ViewMode::Cutaway:
        render_shell(rec, 0.5f);        // Clipped shell
        render_frame_triad(rec);
        render_scaffold(rec);
        render_descriptor_patches(rec);
        break;

    case ViewMode::Residual:
        render_shell(rec, 0.3f);
        render_residual_halo(rec);
        render_frame_triad(rec);
        break;

    case ViewMode::Comparison:
        render_shell(rec, 0.15f);       // Ghost shell
        render_surface_state_layer(rec);// Layer 2: heatmap overlay
        render_internal_reference(rec); // Layer 3: atomistic fragment
        render_frame_triad(rec);
        render_residual_halo(rec);
        break;
    }

    // Disable clipping plane
    if (view_mode == ViewMode::Cutaway) {
        glDisable(GL_CLIP_PLANE0);
    }
}

// ============================================================================
// Implementation — ImGui Inspector Panel
// ============================================================================

inline void BeadInspectorView::draw_inspector_panel(
    const BeadVisualRecord& rec,
    ViewMode& view_mode,
    float& clip_alpha)
{
    ImGui::Begin("Bead Inspector — Detail", nullptr, ImGuiWindowFlags_NoCollapse);

    // --- View mode selector ---
    {
        const char* modes[] = {"Shell", "Scaffold", "Surface State",
                                "Internal Ref", "Skeleton",
                                "Cutaway", "Residual", "Comparison"};
        int mode_idx = static_cast<int>(view_mode);
        if (ImGui::Combo("View Mode", &mode_idx, modes, 8)) {
            view_mode = static_cast<ViewMode>(mode_idx);
        }
        if (view_mode == ViewMode::Cutaway) {
            ImGui::SliderFloat("Clip", &clip_alpha, 0.0f, 1.0f, "%.2f");
        }
    }

    ImGui::Separator();

    // --- Section 1: Identity ---
    if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Bead ID:   %d", rec.bead_id);
        ImGui::Text("Class:     %s", rec.bead_class.empty() ? "(untyped)" : rec.bead_class.c_str());
        ImGui::Text("Fragment:  %s", rec.fragment_name.empty() ? "(none)" : rec.fragment_name.c_str());
        if (rec.is_aromatic) ImGui::TextColored(ImVec4(0.6f, 0.3f, 0.9f, 1.0f), "  aromatic");
        if (rec.is_cyclic)   ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.5f, 1.0f), "  cyclic");
        if (rec.is_metal_centered) ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "  metal center");
    }

    // --- Section 2: Geometry ---
    if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Center (A): (%.3f, %.3f, %.3f)",
                    rec.center_world.x, rec.center_world.y, rec.center_world.z);
        ImGui::Text("Eff. radius: %.3f A", rec.effective_radius);
        ImGui::Text("Anisotropy:  (%.3f, %.3f, %.3f)",
                    rec.anisotropy_axes.x, rec.anisotropy_axes.y, rec.anisotropy_axes.z);
        ImGui::Separator();
        ImGui::Text("Frame: %s", frame_method_name(rec.frame_method));
        ImGui::Text("  confidence: %.3f", rec.frame_confidence);
        ImGui::Text("  valid:      %s", rec.frame_valid ? "yes" : "no");

        // Frame confidence bar
        float conf = static_cast<float>(rec.frame_confidence);
        ImVec4 bar_col = conf > 0.5f ?
            ImVec4(0.2f, 0.8f, 0.3f, 1.0f) : ImVec4(0.9f, 0.4f, 0.1f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_col);
        ImGui::ProgressBar(conf, ImVec2(-1, 0), "");
        ImGui::PopStyleColor();
    }

    // --- Section 3: Structural Solution ---
    if (ImGui::CollapsingHeader("Structural Solution")) {
        ImGui::Text("Direction count: %d", rec.coordination_number);
        ImGui::Text("Planar:   %s", rec.has_planar_component ? "yes" : "no");
        ImGui::Text("Cyclic:   %s", rec.is_cyclic ? "yes" : "no");
        ImGui::Text("Aromatic: %s", rec.is_aromatic ? "yes" : "no");
        ImGui::Text("Metal:    %s", rec.is_metal_centered ? "yes" : "no");

        if (!rec.solved_directions_local.empty()) {
            ImGui::Separator();
            ImGui::Text("Solved directions (local frame):");
            for (int i = 0; i < static_cast<int>(rec.solved_directions_local.size()) && i < 12; ++i) {
                const auto& d = rec.solved_directions_local[i];
                ImGui::Text("  [%d] (%.3f, %.3f, %.3f)", i, d.x, d.y, d.z);
            }
            if (static_cast<int>(rec.solved_directions_local.size()) > 12) {
                ImGui::Text("  ... +%d more",
                    static_cast<int>(rec.solved_directions_local.size()) - 12);
            }
        }
    }

    // --- Section 4: Descriptors ---
    if (ImGui::CollapsingHeader("Descriptors")) {
        if (rec.has_descriptor) {
            ImGui::Text("l_max: %d", rec.descriptor_l_max);
            ImGui::Text("Resolution: %s",
                        resolution_level_name(classify_resolution(rec.descriptor_l_max)));
            ImGui::Text("Surface samples: %d",
                        static_cast<int>(rec.descriptor_field.size()));

            // Per-channel info (from descriptor_field stats)
            if (!rec.descriptor_field.empty()) {
                double s_min = 1e30, s_max = -1e30;
                double e_min = 1e30, e_max = -1e30;
                double d_min = 1e30, d_max = -1e30;
                for (const auto& s : rec.descriptor_field) {
                    if (s.steric_value < s_min) s_min = s.steric_value;
                    if (s.steric_value > s_max) s_max = s.steric_value;
                    if (s.electrostatic_value < e_min) e_min = s.electrostatic_value;
                    if (s.electrostatic_value > e_max) e_max = s.electrostatic_value;
                    if (s.dispersion_value < d_min) d_min = s.dispersion_value;
                    if (s.dispersion_value > d_max) d_max = s.dispersion_value;
                }
                ImGui::Text("Steric:  [%.4f, %.4f]", s_min, s_max);
                ImGui::Text("Elec:    [%.4f, %.4f]", e_min, e_max);
                ImGui::Text("Disp:    [%.4f, %.4f]", d_min, d_max);
            }
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(no descriptor data)");
        }
    }

    // --- Section 5: Layer 2 — Surface State ---
    if (ImGui::CollapsingHeader("Layer 2: Surface State")) {
        if (rec.surface_state_grid.populated) {
            ImGui::Text("Grid: %d x %d (%d vertices)",
                        rec.surface_state_grid.n_theta,
                        rec.surface_state_grid.n_phi,
                        static_cast<int>(rec.surface_state_grid.vertices.size()));
            ImGui::Text("Active channel: %s",
                        surface_state_channel_name(rec.active_surface_channel));
            ImGui::Separator();
            ImGui::Text("Steric range:  [%.4f, %.4f]",
                        rec.surface_state_grid.steric_min,
                        rec.surface_state_grid.steric_max);
            ImGui::Text("Elec range:    [%.4f, %.4f]",
                        rec.surface_state_grid.elec_min,
                        rec.surface_state_grid.elec_max);
            ImGui::Text("Disp range:    [%.4f, %.4f]",
                        rec.surface_state_grid.disp_min,
                        rec.surface_state_grid.disp_max);
            ImGui::Text("Synth range:   [%.4f, %.4f]",
                        rec.surface_state_grid.synth_min,
                        rec.surface_state_grid.synth_max);
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "(no surface-state data)");
        }
    }

    // --- Section 6: Layer 3 — Internal Reference ---
    if (ImGui::CollapsingHeader("Layer 3: Internal Reference")) {
        if (rec.source_fragment.populated) {
            ImGui::Text("Source atoms: %d",
                        static_cast<int>(rec.source_fragment.atoms.size()));
            ImGui::Text("Source bonds: %d",
                        static_cast<int>(rec.source_fragment.bonds.size()));
            ImGui::Text("COM: (%.3f, %.3f, %.3f)",
                        rec.source_fragment.center_of_mass.x,
                        rec.source_fragment.center_of_mass.y,
                        rec.source_fragment.center_of_mass.z);
            ImGui::Text("COG: (%.3f, %.3f, %.3f)",
                        rec.source_fragment.center_of_geometry.x,
                        rec.source_fragment.center_of_geometry.y,
                        rec.source_fragment.center_of_geometry.z);
            ImGui::Text("|COM-COG|: %.4f A", rec.mapping_residual);
            ImGui::Separator();
            // Element census
            int n_H = 0, n_C = 0, n_N = 0, n_O = 0, n_other = 0;
            for (const auto& a : rec.source_fragment.atoms) {
                switch (a.atomic_number) {
                    case 1:  ++n_H; break;
                    case 6:  ++n_C; break;
                    case 7:  ++n_N; break;
                    case 8:  ++n_O; break;
                    default: ++n_other; break;
                }
            }
            ImGui::Text("Elements: C=%d H=%d N=%d O=%d other=%d",
                        n_C, n_H, n_N, n_O, n_other);
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "(no internal reference data)");
        }
    }

    // --- Section 7: Residuals ---
    if (ImGui::CollapsingHeader("Residuals")) {
        ImGui::Text("Mapping residual: %.4f A", rec.mapping_residual);
        ImGui::Text("Descriptor residual: %.4f", rec.descriptor_residual);
        ImGui::Text("Frame ambiguity: %.4f", rec.frame_ambiguity);
        ImGui::Text("Mapping confidence: %.4f", rec.mapping_confidence);

        // Residual quality bar
        float qual = static_cast<float>(1.0 - rec.descriptor_residual);
        ImVec4 q_col = qual > 0.8f ?
            ImVec4(0.2f, 0.8f, 0.3f, 1.0f) :
            (qual > 0.5f ?
                ImVec4(0.9f, 0.7f, 0.1f, 1.0f) :
                ImVec4(0.9f, 0.2f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, q_col);
        ImGui::ProgressBar(qual, ImVec2(-1, 0), "descriptor quality");
        ImGui::PopStyleColor();
    }

    // --- Section 8: Connectivity ---
    if (ImGui::CollapsingHeader("Connectivity")) {
        ImGui::Text("Neighbours: %d", static_cast<int>(rec.neighbours.size()));
        ImGui::Text("Coordination C: %.4f", rec.coordination_C);
        ImGui::Separator();

        // Environment snapshot
        ImGui::Text("Environment:");
        ImGui::Text("  eta:      %.6f", rec.eta);
        ImGui::Text("  rho:      %.6f", rec.rho);
        ImGui::Text("  P2:       %.6f", rec.P2);
        ImGui::Text("  target_f: %.6f", rec.target_f);

        if (!rec.neighbours.empty()) {
            ImGui::Separator();
            ImGui::Text("Neighbour details:");
            for (int i = 0; i < static_cast<int>(rec.neighbours.size()) && i < 10; ++i) {
                const auto& nb = rec.neighbours[i];
                ImGui::Text("  [%d] d=%.2f A  %s",
                            nb.neighbour_bead_id, nb.distance,
                            nb.interaction_type.c_str());
            }
        }
    }

    // --- Source fragment stats ---
    if (rec.source_fragment.populated && ImGui::CollapsingHeader("Source Fragment")) {
        ImGui::Text("Atoms: %d", static_cast<int>(rec.source_fragment.atoms.size()));
        ImGui::Text("Bonds: %d", static_cast<int>(rec.source_fragment.bonds.size()));
        ImGui::Text("COM: (%.3f, %.3f, %.3f)",
                    rec.source_fragment.center_of_mass.x,
                    rec.source_fragment.center_of_mass.y,
                    rec.source_fragment.center_of_mass.z);
        ImGui::Text("COG: (%.3f, %.3f, %.3f)",
                    rec.source_fragment.center_of_geometry.x,
                    rec.source_fragment.center_of_geometry.y,
                    rec.source_fragment.center_of_geometry.z);
        ImGui::Text("|COM-COG|: %.4f A", rec.mapping_residual);
    }

    ImGui::End();
}

} // namespace vis
} // namespace coarse_grain

#endif // BUILD_VISUALIZATION
