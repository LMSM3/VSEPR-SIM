/**
 * crystal_tui.cpp  —  Terminal crystal lattice viewer (implementation)
 * VSEPR-SIM 3.0.0
 */

#include "crystal_tui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace atomistic {
namespace tui {

// ============================================================================
// Helpers
// ============================================================================

static std::string fmt(double v, int prec = 4) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(prec) << v;
    return os.str();
}

static std::string sci(double v, int prec = 2) {
    std::ostringstream os;
    os << std::scientific << std::setprecision(prec) << v;
    return os.str();
}

static const char* arrow_char(double dx, double dy) {
    // 8-direction arrow from displacement
    if (std::abs(dx) < 1e-12 && std::abs(dy) < 1e-12) return ".";
    double angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979;
    if (angle < 0) angle += 360.0;
    if (angle <  22.5) return ">";
    if (angle <  67.5) return "/";
    if (angle < 112.5) return "^";
    if (angle < 157.5) return "\\";
    if (angle < 202.5) return "<";
    if (angle < 247.5) return "/";
    if (angle < 292.5) return "v";
    if (angle < 337.5) return "\\";
    return ">";
}

// ============================================================================
// TUISnapshot::capture
// ============================================================================

TUISnapshot TUISnapshot::capture(const State& s,
                                  const crystal::Lattice& lat,
                                  int step, double dt,
                                  const perturbation::WindParticle* wind) {
    TUISnapshot snap;

    // Lattice info
    snap.a = lat.a_len();
    snap.b = lat.b_len();
    snap.c = lat.c_len();
    snap.alpha_deg = lat.alpha_deg();
    snap.beta_deg  = lat.beta_deg();
    snap.gamma_deg = lat.gamma_deg();
    snap.volume    = lat.V;

    // Atoms
    snap.positions.assign(s.X.begin(), s.X.end());
    snap.types.assign(s.type.begin(), s.type.end());

    // Forces
    snap.forces.assign(s.F.begin(), s.F.end());
    double frms2 = 0.0;
    double fmax  = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        double fi = norm(s.F[i]);
        frms2 += fi * fi;
        if (fi > fmax) fmax = fi;
    }
    snap.Frms = (s.N > 0) ? std::sqrt(frms2 / s.N) : 0.0;
    snap.Fmax = fmax;

    // Energy
    snap.U_total = s.E.total();
    snap.U_bond  = s.E.Ubond + s.E.Uangle + s.E.Utors;
    snap.U_vdw   = s.E.UvdW;
    snap.U_coul  = s.E.UCoul;
    snap.U_ext   = s.E.Uext;

    // Kinetic / temperature (approximate)
    double ke = 0.0;
    for (uint32_t i = 0; i < s.N; ++i)
        ke += 0.5 * s.M[i] * dot(s.V[i], s.V[i]);
    snap.KE = ke;
    snap.T  = (s.N > 0) ? (2.0 * ke) / (3.0 * s.N * 0.0019872041) : 0.0;

    // Integration
    snap.step   = step;
    snap.dt     = dt;
    snap.dt_eff = dt;

    // Wind
    if (wind) {
        snap.wind_active    = true;
        snap.wind_dir       = wind->params.direction;
        snap.wind_strength  = wind->params.strength;
        snap.wind_ramp      = wind->ramp_fraction();
        snap.wind_peak_force = wind->peak_force();
        snap.wind_headroom  = wind->headroom_ratio();
        snap.dt_eff         = wind->effective_dt(dt);
    }

    return snap;
}

// ============================================================================
// Projection
// ============================================================================

int CrystalTUI::proj_x(double cx, double x_min, double x_range) const {
    if (x_range < 1e-12) return config.lattice_w / 2 + 2;
    double t = (cx - x_min) / x_range;
    return 2 + static_cast<int>(t * (config.lattice_w - 4));
}

int CrystalTUI::proj_y(double cy, double y_min, double y_range) const {
    if (y_range < 1e-12) return config.lattice_h / 2 + 2;
    double t = (cy - y_min) / y_range;
    // Invert Y so +y is up
    return 2 + static_cast<int>((1.0 - t) * (config.lattice_h - 4));
}

// ============================================================================
// Sub-renderers
// ============================================================================

void CrystalTUI::draw_title_bar(const TUISnapshot& snap) {
    Colour title_c = {80, 220, 255};
    std::string title = " VSEPR-SIM 3.0.0 :: Crystal Lattice TUI ";
    fb.put_string(2, 0, title, title_c, true);

    Colour dim_c = {100, 100, 100};
    std::string step_str = "step " + std::to_string(snap.step);
    fb.put_string(config.width - (int)step_str.size() - 3, 0, step_str, dim_c);
}

void CrystalTUI::draw_border(const TUISnapshot& /*snap*/) {
    Colour border_c = {60, 60, 80};
    // Lattice viewport
    fb.box(1, 1, config.lattice_w, config.lattice_h, border_c);
    // Math panel
    fb.box(config.panel_x, 1, config.panel_w, config.lattice_h, border_c);
}

void CrystalTUI::draw_lattice(const TUISnapshot& snap) {
    // Draw unit cell edges as dotted lines on the lattice viewport
    // Simplified: draw axis indicators at bottom-left
    Colour axis_c = {100, 140, 180};
    fb.put_string(3, config.lattice_h - 1, "a->", axis_c);
    if (!config.xz_slice)
        fb.put_string(3, config.lattice_h - 2, "b ^", axis_c);
    else
        fb.put_string(3, config.lattice_h - 2, "c ^", axis_c);

    // Volume label
    Colour vol_c = {120, 120, 120};
    fb.put_string(3, config.lattice_h, "V=" + fmt(snap.volume, 1) + " A^3", vol_c);
}

void CrystalTUI::draw_atoms(const TUISnapshot& snap) {
    if (snap.positions.empty()) return;

    // Compute bounding box
    double x_min = 1e30, x_max = -1e30;
    double y_min = 1e30, y_max = -1e30;
    for (auto& p : snap.positions) {
        double px = p.x;
        double py = config.xz_slice ? p.z : p.y;
        if (px < x_min) x_min = px;
        if (px > x_max) x_max = px;
        if (py < y_min) y_min = py;
        if (py > y_max) y_max = py;
    }
    double pad = 1.0;
    x_min -= pad; x_max += pad;
    y_min -= pad; y_max += pad;
    double x_range = x_max - x_min;
    double y_range = y_max - y_min;

    // Element chars by Z
    auto elem_char = [](uint32_t Z) -> char {
        // Simple: use first letter of common elements
        switch (Z) {
            case 1:  return 'H';
            case 6:  return 'C';
            case 7:  return 'N';
            case 8:  return 'O';
            case 9:  return 'F';
            case 11: return 'a'; // Na
            case 12: return 'g'; // Mg
            case 13: return 'l'; // Al
            case 14: return 'i'; // Si
            case 15: return 'P';
            case 16: return 'S';
            case 17: return 'l'; // Cl
            case 19: return 'K';
            case 20: return 'a'; // Ca
            case 26: return 'e'; // Fe
            case 29: return 'u'; // Cu
            case 30: return 'n'; // Zn
            case 47: return 'g'; // Ag
            case 79: return 'u'; // Au
            default: return 'o';
        }
    };

    for (size_t i = 0; i < snap.positions.size(); ++i) {
        double px = snap.positions[i].x;
        double py = config.xz_slice ? snap.positions[i].z : snap.positions[i].y;
        int gx = proj_x(px, x_min, x_range);
        int gy = proj_y(py, y_min, y_range);

        uint32_t Z = (i < snap.types.size()) ? snap.types[i] : 0;
        Colour col = colour_for_type(Z);
        fb.put(gx, gy, elem_char(Z), col, true);
    }
}

void CrystalTUI::draw_forces(const TUISnapshot& snap) {
    if (!config.show_forces || snap.forces.empty()) return;

    double x_min = 1e30, x_max = -1e30;
    double y_min = 1e30, y_max = -1e30;
    for (auto& p : snap.positions) {
        double px = p.x;
        double py = config.xz_slice ? p.z : p.y;
        if (px < x_min) x_min = px;
        if (px > x_max) x_max = px;
        if (py < y_min) y_min = py;
        if (py > y_max) y_max = py;
    }
    double pad = 1.0;
    x_min -= pad; x_max += pad;
    y_min -= pad; y_max += pad;
    double x_range = x_max - x_min;
    double y_range = y_max - y_min;

    for (size_t i = 0; i < snap.forces.size() && i < snap.positions.size(); ++i) {
        double fx = snap.forces[i].x;
        double fy = config.xz_slice ? snap.forces[i].z : snap.forces[i].y;
        double fmag = norm(snap.forces[i]);
        if (fmag < 1e-6) continue;

        double px = snap.positions[i].x;
        double py = config.xz_slice ? snap.positions[i].z : snap.positions[i].y;
        int gx = proj_x(px, x_min, x_range);
        int gy = proj_y(py, y_min, y_range);

        // Place arrow one cell away from atom in force direction
        int ax = gx + (fx > 0.3 ? 1 : (fx < -0.3 ? -1 : 0));
        int ay = gy + (fy > 0.3 ? -1 : (fy < -0.3 ? 1 : 0)); // inverted y

        Colour fc = force_colour(fmag, snap.Fmax);
        const char* arrow = arrow_char(fx, fy);
        fb.put(ax, ay, arrow[0], fc);
    }
}

void CrystalTUI::draw_wind(const TUISnapshot& snap) {
    if (!config.show_wind || !snap.wind_active) return;

    Colour wind_c = {60, 180, 130};
    double wx = snap.wind_dir.x;
    double wy = config.xz_slice ? snap.wind_dir.z : snap.wind_dir.y;
    const char* arrow = arrow_char(wx, wy);

    // Draw sparse wind field across viewport
    for (int y = 4; y < config.lattice_h - 2; y += 3) {
        for (int x = 4; x < config.lattice_w - 2; x += 6) {
            // Only draw if cell is empty
            if (fb.at(x, y).ch == ' ')
                fb.put(x, y, arrow[0], wind_c);
        }
    }

    // Wind label
    Colour label_c = {40, 150, 110};
    std::string wlabel = "wind " + fmt(snap.wind_strength, 2) + " ramp=" + fmt(snap.wind_ramp * 100, 0) + "%";
    fb.put_string(config.lattice_w - (int)wlabel.size() - 2, config.lattice_h, wlabel, label_c);
}

void CrystalTUI::draw_math_panel(const TUISnapshot& snap) {
    int x0 = config.panel_x + 2;
    int y  = 2;
    Colour hdr = {80, 220, 255};
    Colour val = {200, 200, 200};
    Colour dim = {120, 120, 120};
    Colour hot = {255, 100, 60};
    Colour cool = {60, 180, 255};
    Colour grn = {80, 230, 80};

    // Title
    fb.put_string(x0, y++, "=== MATHEMATICS ===", hdr, true);
    y++;

    // Energy
    fb.put_string(x0, y++, "Energy (kcal/mol)", hdr);
    fb.put_string(x0, y++, "  U_total  = " + fmt(snap.U_total, 4), val);
    fb.put_string(x0, y++, "  U_bond   = " + fmt(snap.U_bond, 4), dim);
    fb.put_string(x0, y++, "  U_vdW    = " + fmt(snap.U_vdw, 4), dim);
    fb.put_string(x0, y++, "  U_Coul   = " + fmt(snap.U_coul, 4), dim);
    fb.put_string(x0, y++, "  U_ext    = " + fmt(snap.U_ext, 4),
                  snap.U_ext != 0.0 ? hot : dim);
    fb.put_string(x0, y++, "  KE       = " + fmt(snap.KE, 4), dim);
    y++;

    // Temperature
    fb.put_string(x0, y++, "Temperature", hdr);
    fb.put_string(x0, y++, "  T = " + fmt(snap.T, 1) + " K",
                  snap.T > 500 ? hot : (snap.T < 100 ? cool : val));
    y++;

    // Forces
    fb.put_string(x0, y++, "Forces (kcal/mol/A)", hdr);
    fb.put_string(x0, y++, "  F_rms = " + sci(snap.Frms),
                  snap.Frms < 1e-4 ? grn : val);
    fb.put_string(x0, y++, "  F_max = " + sci(snap.Fmax), val);
    y++;

    // Integration
    fb.put_string(x0, y++, "Integration", hdr);
    fb.put_string(x0, y++, "  dt     = " + sci(snap.dt) + " fs", dim);
    fb.put_string(x0, y++, "  dt_eff = " + sci(snap.dt_eff) + " fs",
                  snap.dt_eff > snap.dt ? grn : dim);
    fb.put_string(x0, y++, "  N_atom = " + std::to_string(snap.positions.size()), dim);
    y++;

    // Wind
    if (snap.wind_active) {
        fb.put_string(x0, y++, "Wind Particle", hdr);
        fb.put_string(x0, y++, "  strength = " + fmt(snap.wind_strength, 3), val);
        fb.put_string(x0, y++, "  ramp     = " + fmt(snap.wind_ramp * 100, 0) + " %",
                      snap.wind_ramp < 1.0 ? cool : grn);
        fb.put_string(x0, y++, "  F_peak   = " + fmt(snap.wind_peak_force, 4), val);
        fb.put_string(x0, y++, "  headroom = " + fmt(snap.wind_headroom, 1) + "x",
                      snap.wind_headroom >= 1.5 ? grn : hot);
        fb.put_string(x0, y++, "  dt_factor= " + fmt(snap.dt_eff / std::max(snap.dt, 1e-30), 1) + "x", grn);
    }
}

void CrystalTUI::draw_lattice_info(const TUISnapshot& snap) {
    int x0 = config.panel_x + 2;
    int y  = config.lattice_h - 6;
    Colour hdr = {80, 220, 255};
    Colour val = {180, 180, 180};

    fb.put_string(x0, y++, "Lattice", hdr);
    fb.put_string(x0, y++, "  a=" + fmt(snap.a, 2) + " b=" + fmt(snap.b, 2) + " c=" + fmt(snap.c, 2), val);
    fb.put_string(x0, y++, "  alpha=" + fmt(snap.alpha_deg, 1) + " beta=" + fmt(snap.beta_deg, 1), val);
    fb.put_string(x0, y++, "  gamma=" + fmt(snap.gamma_deg, 1), val);
    fb.put_string(x0, y++, "  V = " + fmt(snap.volume, 2) + " A^3", val);
}

// ============================================================================
// Main render
// ============================================================================

std::string CrystalTUI::render(const TUISnapshot& snap) {
    fb = FrameBuffer(config.width, config.height);
    fb.clear();

    draw_title_bar(snap);
    draw_border(snap);
    draw_lattice(snap);
    draw_atoms(snap);
    draw_forces(snap);
    draw_wind(snap);
    draw_math_panel(snap);
    draw_lattice_info(snap);

    return fb.render();
}

void CrystalTUI::display(const TUISnapshot& snap) {
    std::string frame = render(snap);
    std::fputs(frame.c_str(), stdout);
    std::fflush(stdout);
}

} // namespace tui
} // namespace atomistic
