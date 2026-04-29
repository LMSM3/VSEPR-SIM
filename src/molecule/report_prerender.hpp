#pragma once
// ============================================================================
// report_prerender.hpp - Glass Module: Offline SVG Report Renderer
// ============================================================================
// Software-only projection of PrerenderBuffers into publication-quality SVG.
// No OpenGL required. Designed for LaTeX \includegraphics integration.
//
// Usage:
//   ReportRenderer rr(report_settings);
//   std::string svg = rr.render_svg(buffers);
//   rr.write_svg("molecule.svg", buffers);
// ============================================================================

#include "prerender_buffers.hpp"
#include <string>
#include <cstdint>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// ReportCamera - lightweight software camera for 2D projection
// -----------------------------------------------------------------------
struct ReportCamera {
    Vec3f  eye       {4.0f, 3.0f, 5.0f};
    Vec3f  target    {0.0f, 0.0f, 0.0f};
    Vec3f  up        {0.0f, 1.0f, 0.0f};
    bool   ortho     = true;
    float  ortho_scale = 6.0f;      // world units visible in half-width
    float  fov_deg     = 45.0f;     // only used when ortho == false
    float  near_plane  = 0.1f;
    float  far_plane   = 100.0f;
};

// -----------------------------------------------------------------------
// ReportSettings - controls SVG output style
// -----------------------------------------------------------------------
struct ReportSettings {
    float  canvas_width   = 600.0f;  // SVG width in px
    float  canvas_height  = 600.0f;  // SVG height in px
    float  atom_scale     = 12.0f;   // screen-space radius multiplier
    float  bond_width     = 2.5f;    // stroke width for single bonds
    float  outline_width  = 0.8f;    // dark outline around atoms
    bool   depth_shading  = true;    // darken atoms further from camera
    float  depth_dim      = 0.35f;   // max darkening factor (0=none, 1=black)
    bool   label_atoms    = false;   // overlay element symbols
    float  label_font_size= 9.0f;
    ReportCamera camera;
};

// -----------------------------------------------------------------------
// ReportRenderer - offline SVG generator from PrerenderBuffers
// -----------------------------------------------------------------------
class ReportRenderer {
public:
    explicit ReportRenderer(ReportSettings s = {});

    // Generate SVG string from prerender buffers
    std::string render_svg(const PrerenderBuffers& buffers) const;

    // Convenience: render + write to file
    bool write_svg(const std::string& path, const PrerenderBuffers& buffers) const;

    // Access/modify settings
    ReportSettings&       settings()       { return settings_; }
    const ReportSettings& settings() const { return settings_; }

private:
    ReportSettings settings_;

    struct Projected {
        float sx, sy;     // screen x,y
        float depth;      // camera-space z (for sorting)
        float radius;     // screen radius (atoms) or 0 (bonds)
        uint32_t type;    // atomic number or bond order
        uint32_t flags;
        bool is_bond;
        // bond endpoints (screen space) - only valid when is_bond==true
        float ax, ay, bx, by;
    };

    // Project a 3D point to screen coordinates
    void project(const Vec3f& world, float& sx, float& sy, float& depth) const;

    // CPK element colour lookup
    static const char* cpk_color(uint32_t Z);

    // Depth-dimmed colour string
    std::string depth_color(const char* base_hex, float depth,
                            float min_depth, float max_depth) const;
};

} // namespace glass
} // namespace vsepr
