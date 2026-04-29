// ============================================================================
// report_prerender.cpp - Glass Module: Offline SVG Report Renderer
// ============================================================================

#include "report_prerender.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iomanip>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// CPK colour table (hex) for Z=0..54
// -----------------------------------------------------------------------
static const char* s_cpk_table[] = {
    "#808080",  // 0  dummy
    "#FFFFFF",  // 1  H
    "#D9FFFF",  // 2  He
    "#CC80FF",  // 3  Li
    "#C2FF00",  // 4  Be
    "#FFB5B5",  // 5  B
    "#909090",  // 6  C
    "#3050F8",  // 7  N
    "#FF0D0D",  // 8  O
    "#90E050",  // 9  F
    "#B3E3F5",  // 10 Ne
    "#AB5CF2",  // 11 Na
    "#8AFF00",  // 12 Mg
    "#BFA6A6",  // 13 Al
    "#F0C8A0",  // 14 Si
    "#FF8000",  // 15 P
    "#FFFF30",  // 16 S
    "#1FF01F",  // 17 Cl
    "#80D1E3",  // 18 Ar
    "#8F40D4",  // 19 K
    "#3DFF00",  // 20 Ca
    "#E6E6E6",  // 21 Sc
    "#BFC2C7",  // 22 Ti
    "#A6A6AB",  // 23 V
    "#8A99C7",  // 24 Cr
    "#9C7AC7",  // 25 Mn
    "#E06633",  // 26 Fe
    "#F090A0",  // 27 Co
    "#50D050",  // 28 Ni
    "#C88033",  // 29 Cu
    "#7D80B0",  // 30 Zn
    "#C28F8F",  // 31 Ga
    "#668F8F",  // 32 Ge
    "#BD80E3",  // 33 As
    "#FFA100",  // 34 Se
    "#A62929",  // 35 Br
    "#5CB8D1",  // 36 Kr
    "#702EB0",  // 37 Rb
    "#00FF00",  // 38 Sr
    "#94FFFF",  // 39 Y
    "#94E0E0",  // 40 Zr
    "#73C2C9",  // 41 Nb
    "#54B5B5",  // 42 Mo
    "#3B9E9E",  // 43 Tc
    "#248F8F",  // 44 Ru
    "#0A7D8C",  // 45 Rh
    "#006985",  // 46 Pd
    "#C0C0C0",  // 47 Ag
    "#FFD98F",  // 48 Cd
    "#A67573",  // 49 In
    "#668080",  // 50 Sn
    "#9E63B5",  // 51 Sb
    "#D47A00",  // 52 Te
    "#940094",  // 53 I
    "#429EB0",  // 54 Xe
};
static constexpr size_t s_cpk_count = sizeof(s_cpk_table) / sizeof(s_cpk_table[0]);

const char* ReportRenderer::cpk_color(uint32_t Z) {
    if (Z < s_cpk_count) return s_cpk_table[Z];
    return "#808080";
}

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
ReportRenderer::ReportRenderer(ReportSettings s)
    : settings_(std::move(s)) {}

// -----------------------------------------------------------------------
// Software projection (orthographic or perspective)
// -----------------------------------------------------------------------
void ReportRenderer::project(const Vec3f& world,
                             float& sx, float& sy, float& depth) const
{
    const auto& cam = settings_.camera;
    Vec3f fwd = normalize3f(cam.target - cam.eye);
    Vec3f right = normalize3f(cross3f(fwd, cam.up));
    Vec3f up    = cross3f(right, fwd);

    Vec3f rel = world - cam.eye;
    float cx = dot3f(rel, right);
    float cy = dot3f(rel, up);
    float cz = dot3f(rel, fwd);

    depth = cz;

    float hw = settings_.canvas_width  * 0.5f;
    float hh = settings_.canvas_height * 0.5f;

    if (cam.ortho) {
        float scale = hw / cam.ortho_scale;
        sx = hw + cx * scale;
        sy = hh - cy * scale;
    } else {
        float d = (cz > 0.001f) ? cz : 0.001f;
        float fov_rad = cam.fov_deg * 3.14159265f / 180.0f;
        float scale = hw / std::tan(fov_rad * 0.5f);
        sx = hw + (cx / d) * scale;
        sy = hh - (cy / d) * scale;
    }
}

// -----------------------------------------------------------------------
// Depth-dimmed colour
// -----------------------------------------------------------------------
std::string ReportRenderer::depth_color(const char* base_hex,
                                        float depth,
                                        float min_depth,
                                        float max_depth) const
{
    if (!settings_.depth_shading || max_depth <= min_depth)
        return base_hex;

    float t = (depth - min_depth) / (max_depth - min_depth);
    t = std::clamp(t, 0.0f, 1.0f);
    float dim = 1.0f - t * settings_.depth_dim;

    // Parse hex colour
    unsigned r = 0, g = 0, b = 0;
    if (base_hex[0] == '#' && std::strlen(base_hex) >= 7) {
        auto hex2 = [](const char* s) -> unsigned {
            unsigned v = 0;
            for (int i = 0; i < 2; ++i) {
                v <<= 4;
                char c = s[i];
                if (c >= '0' && c <= '9') v += c - '0';
                else if (c >= 'A' && c <= 'F') v += 10 + c - 'A';
                else if (c >= 'a' && c <= 'f') v += 10 + c - 'a';
            }
            return v;
        };
        r = hex2(base_hex + 1);
        g = hex2(base_hex + 3);
        b = hex2(base_hex + 5);
    }

    r = static_cast<unsigned>(r * dim);
    g = static_cast<unsigned>(g * dim);
    b = static_cast<unsigned>(b * dim);

    std::ostringstream oss;
    oss << '#' << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << r << std::setw(2) << g << std::setw(2) << b;
    return oss.str();
}

// -----------------------------------------------------------------------
// render_svg - main entry
// -----------------------------------------------------------------------
std::string ReportRenderer::render_svg(const PrerenderBuffers& buffers) const {
    std::vector<Projected> items;
    items.reserve(buffers.atom_count() + buffers.bond_count());

    // Project bonds
    for (const auto& bi : buffers.bond_instances) {
        Projected p{};
        p.is_bond = true;
        float da, db;
        project(bi.endpoint_a, p.ax, p.ay, da);
        project(bi.endpoint_b, p.bx, p.by, db);
        p.depth = (da + db) * 0.5f;
        p.type  = bi.bond_order;
        p.flags = bi.style_flags;
        items.push_back(p);
    }

    // Project atoms
    for (const auto& ai : buffers.atom_instances) {
        Projected p{};
        p.is_bond = false;
        project(ai.position, p.sx, p.sy, p.depth);
        p.radius = ai.radius * settings_.atom_scale;
        p.type   = ai.atom_type;
        p.flags  = ai.style_flags;
        items.push_back(p);
    }

    // Depth sort: back to front (higher depth = further = drawn first)
    std::sort(items.begin(), items.end(),
              [](const Projected& a, const Projected& b) {
                  return a.depth > b.depth;
              });

    // Find depth range
    float min_d = 1e30f, max_d = -1e30f;
    for (const auto& it : items) {
        min_d = std::min(min_d, it.depth);
        max_d = std::max(max_d, it.depth);
    }

    // Build SVG
    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);

    svg << R"(<?xml version="1.0" encoding="UTF-8"?>)" << '\n';
    svg << R"(<svg xmlns="http://www.w3.org/2000/svg" )"
        << "width=\"" << settings_.canvas_width << "\" "
        << "height=\"" << settings_.canvas_height << "\" "
        << "viewBox=\"0 0 " << settings_.canvas_width
        << " " << settings_.canvas_height << "\">\n";
    svg << R"(<rect width="100%" height="100%" fill="#FAFAFA"/>)" << '\n';

    // Element symbols for labels
    static const char* elem_sym[] = {
        "?","H","He","Li","Be","B","C","N","O","F","Ne",
        "Na","Mg","Al","Si","P","S","Cl","Ar","K","Ca",
        "Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn",
        "Ga","Ge","As","Se","Br","Kr","Rb","Sr","Y","Zr",
        "Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd","In","Sn",
        "Sb","Te","I","Xe"
    };
    static constexpr size_t elem_count = sizeof(elem_sym) / sizeof(elem_sym[0]);

    for (const auto& it : items) {
        if (it.is_bond) {
            float w = settings_.bond_width * it.type;
            std::string col = depth_color("#555555", it.depth, min_d, max_d);

            if (it.type >= 2) {
                // Multi-bond: draw parallel lines
                float dx = it.bx - it.ax;
                float dy = it.by - it.ay;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len < 0.001f) continue;
                float nx = -dy / len;
                float ny =  dx / len;
                float off = 2.0f * (it.type == 3 ? 1.5f : 1.0f);

                for (uint32_t k = 0; k < it.type; ++k) {
                    float t = (it.type <= 1) ? 0.0f
                        : (-0.5f + static_cast<float>(k) / (it.type - 1)) * off * 2.0f;
                    svg << "<line x1=\"" << it.ax + nx*t << "\" y1=\"" << it.ay + ny*t
                         << "\" x2=\"" << it.bx + nx*t << "\" y2=\"" << it.by + ny*t
                         << "\" stroke=\"" << col
                         << "\" stroke-width=\"" << settings_.bond_width
                         << "\" stroke-linecap=\"round\"/>\n";
                }
            } else {
                svg << "<line x1=\"" << it.ax << "\" y1=\"" << it.ay
                     << "\" x2=\"" << it.bx << "\" y2=\"" << it.by
                     << "\" stroke=\"" << col
                     << "\" stroke-width=\"" << w
                     << "\" stroke-linecap=\"round\"/>\n";
            }
        } else {
            const char* base = cpk_color(it.type);
            std::string fill = depth_color(base, it.depth, min_d, max_d);

            // Outline
            if (settings_.outline_width > 0.0f) {
                svg << "<circle cx=\"" << it.sx << "\" cy=\"" << it.sy
                     << "\" r=\"" << it.radius + settings_.outline_width
                     << "\" fill=\"#222222\"/>\n";
            }
            // Fill
            svg << "<circle cx=\"" << it.sx << "\" cy=\"" << it.sy
                 << "\" r=\"" << it.radius
                 << "\" fill=\"" << fill << "\"/>\n";

            // Label
            if (settings_.label_atoms && it.type < elem_count) {
                svg << "<text x=\"" << it.sx << "\" y=\"" << it.sy + settings_.label_font_size * 0.35f
                     << "\" text-anchor=\"middle\" font-size=\"" << settings_.label_font_size
                     << "\" font-family=\"sans-serif\" fill=\"#111111\">"
                     << elem_sym[it.type] << "</text>\n";
            }
        }
    }

    svg << "</svg>\n";
    return svg.str();
}

// -----------------------------------------------------------------------
// write_svg - file output convenience
// -----------------------------------------------------------------------
bool ReportRenderer::write_svg(const std::string& path,
                               const PrerenderBuffers& buffers) const {
    std::string content = render_svg(buffers);
    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << content;
    return ofs.good();
}

} // namespace glass
} // namespace vsepr

