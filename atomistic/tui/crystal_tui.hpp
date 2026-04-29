#pragma once
/**
 * crystal_tui.hpp  —  Terminal crystal lattice viewer
 * ====================================================
 * VSEPR-SIM 3.0.0
 *
 * "Looking through the glass to the core of a reaction"
 *
 * Renders a live TUI (Text User Interface) that visualises:
 *   1. Crystal lattice sites as a 2D projection (XY or XZ slice)
 *   2. Per-atom force vectors (magnitude-coloured arrows)
 *   3. Wind particle field overlay (directional flow)
 *   4. Running mathematics panel (energy, temperature, forces, convergence)
 *   5. Lattice metric sidebar (a, b, c, α, β, γ, V)
 *
 * The TUI is a pure-text renderer that writes ANSI-coloured output
 * to stdout.  No GUI dependencies.  Works in any modern terminal
 * (Windows Terminal, iTerm2, GNOME Terminal, xterm-256color).
 *
 * Architecture:
 *   CrystalTUI owns a FrameBuffer (2D character grid with colour).
 *   Each render pass:
 *     1. Clear buffer
 *     2. Draw lattice cell outline
 *     3. Project atom positions onto grid
 *     4. Overlay force arrows
 *     5. Overlay wind field streamlines
 *     6. Draw math panel
 *     7. Flush to stdout
 *
 * This is designed to be called once per integration step from a
 * simulation loop, at a throttled frame rate (e.g., every 50 steps).
 */

#include "atomistic/core/state.hpp"
#include "atomistic/crystal/lattice.hpp"
#include "atomistic/models/wind_particle.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <ostream>
#include <cmath>

namespace atomistic {
namespace tui {

// ============================================================================
// ANSI colour helpers
// ============================================================================

struct Colour {
    uint8_t r{}, g{}, b{};

    std::string fg() const {
        return "\033[38;2;" + std::to_string(r) + ";" +
               std::to_string(g) + ";" + std::to_string(b) + "m";
    }
    std::string bg() const {
        return "\033[48;2;" + std::to_string(r) + ";" +
               std::to_string(g) + ";" + std::to_string(b) + "m";
    }

    static Colour lerp(Colour a, Colour b, double t) {
        t = (t < 0.0) ? 0.0 : (t > 1.0 ? 1.0 : t);
        return {
            static_cast<uint8_t>(a.r + t * (b.r - a.r)),
            static_cast<uint8_t>(a.g + t * (b.g - a.g)),
            static_cast<uint8_t>(a.b + t * (b.b - a.b))
        };
    }
};

inline const char* RESET = "\033[0m";
inline const char* BOLD  = "\033[1m";
inline const char* DIM   = "\033[2m";

// Element palette (by block)
inline Colour colour_for_type(uint32_t Z) {
    if (Z <= 2)   return {255, 255, 255};  // s-block light: white
    if (Z <= 10)  return {100, 200, 255};  // p-block period 2: cyan
    if (Z <= 18)  return {100, 255, 100};  // p-block period 3: green
    if (Z <= 36)  return {255, 200, 80};   // d-block: gold
    if (Z <= 54)  return {200, 100, 255};  // heavier p: violet
    if (Z <= 86)  return {255, 120, 80};   // lanthanides/heavy: orange
    return {180, 180, 180};                // actinides+: grey
}

// Force magnitude heat map: blue (cold) → red (hot)
inline Colour force_colour(double magnitude, double max_force) {
    if (max_force < 1e-20) return {80, 80, 80};
    double t = magnitude / max_force;
    Colour cold = {40, 80, 200};
    Colour hot  = {255, 60, 30};
    return Colour::lerp(cold, hot, t);
}

// ============================================================================
// Universal colour gradients
// All functions are deterministic: same input → same colour.
// ============================================================================

// cold_hot: scalar in [0,1] → dark-blue → cyan → yellow → red
inline Colour cold_hot(double t) {
    t = (t < 0.0) ? 0.0 : (t > 1.0 ? 1.0 : t);
    if (t < 0.333) {
        double u = t / 0.333;
        return Colour::lerp({20, 40, 180}, {40, 200, 220}, u);
    } else if (t < 0.667) {
        double u = (t - 0.333) / 0.334;
        return Colour::lerp({40, 200, 220}, {240, 220, 30}, u);
    } else {
        double u = (t - 0.667) / 0.333;
        return Colour::lerp({240, 220, 30}, {255, 30, 20}, u);
    }
}

// black_body_temp: T in Kelvin → approximate black-body colour
// 0 K → black, 1000 K → deep red, 4000 K → orange, 6500 K → white, >8000 K → blue-white
inline Colour black_body_temp(double T_K) {
    if (T_K <= 0.0)    return {0, 0, 0};
    if (T_K < 1000.0)  return Colour::lerp({0, 0, 0}, {180, 20, 0}, T_K / 1000.0);
    if (T_K < 4000.0)  return Colour::lerp({180, 20, 0}, {255, 140, 40}, (T_K - 1000.0) / 3000.0);
    if (T_K < 6500.0)  return Colour::lerp({255, 140, 40}, {255, 255, 255}, (T_K - 4000.0) / 2500.0);
    return Colour::lerp({255, 255, 255}, {160, 180, 255}, std::min((T_K - 6500.0) / 3500.0, 1.0));
}

// force_gradient: blue → cyan → yellow → red (alias with explicit Fmax normalisation)
inline Colour force_gradient(double F, double Fmax) {
    return cold_hot((Fmax > 1e-20) ? F / Fmax : 0.0);
}

// energy_gradient: violet → green → yellow (potential energy range)
inline Colour energy_gradient(double U, double Umin, double Umax) {
    double span = Umax - Umin;
    double t = (span > 1e-20) ? (U - Umin) / span : 0.0;
    t = (t < 0.0) ? 0.0 : (t > 1.0 ? 1.0 : t);
    if (t < 0.5) return Colour::lerp({140, 0, 200}, {40, 200, 80}, t * 2.0);
    return Colour::lerp({40, 200, 80}, {240, 230, 20}, (t - 0.5) * 2.0);
}

// damage_colour: health fraction [0,1] → green → yellow → red → dark-grey
inline Colour damage_colour(double health01) {
    health01 = (health01 < 0.0) ? 0.0 : (health01 > 1.0 ? 1.0 : health01);
    double t = 1.0 - health01;
    if (t < 0.5) return Colour::lerp({40, 200, 40}, {240, 200, 20}, t * 2.0);
    if (t < 0.85) return Colour::lerp({240, 200, 20}, {220, 30, 20}, (t - 0.5) / 0.35);
    return Colour::lerp({220, 30, 20}, {80, 80, 80}, (t - 0.85) / 0.15);
}

// decay_colour: decay fraction [0,1] → purple → magenta → white (radiation glow)
inline Colour decay_colour(double decay01) {
    decay01 = (decay01 < 0.0) ? 0.0 : (decay01 > 1.0 ? 1.0 : decay01);
    if (decay01 < 0.5) return Colour::lerp({80, 0, 160}, {220, 0, 220}, decay01 * 2.0);
    return Colour::lerp({220, 0, 220}, {255, 255, 255}, (decay01 - 0.5) * 2.0);
}

// velocity_colour: speed fraction [0,1] → dim-grey → blue → white
inline Colour velocity_colour(double speed01) {
    speed01 = (speed01 < 0.0) ? 0.0 : (speed01 > 1.0 ? 1.0 : speed01);
    if (speed01 < 0.5) return Colour::lerp({60, 60, 60}, {30, 80, 220}, speed01 * 2.0);
    return Colour::lerp({30, 80, 220}, {255, 255, 255}, (speed01 - 0.5) * 2.0);
}

// pressure_colour: pressure fraction [0,1] → dark-cyan → yellow → red
inline Colour pressure_colour(double p01) {
    p01 = (p01 < 0.0) ? 0.0 : (p01 > 1.0 ? 1.0 : p01);
    if (p01 < 0.5) return Colour::lerp({0, 120, 140}, {240, 220, 20}, p01 * 2.0);
    return Colour::lerp({240, 220, 20}, {220, 30, 20}, (p01 - 0.5) * 2.0);
}

// health_colour: health fraction [0,1] → red → yellow → green
inline Colour health_colour(double health01) {
    health01 = (health01 < 0.0) ? 0.0 : (health01 > 1.0 ? 1.0 : health01);
    if (health01 < 0.5) return Colour::lerp({210, 30, 20}, {240, 200, 20}, health01 * 2.0);
    return Colour::lerp({240, 200, 20}, {40, 200, 40}, (health01 - 0.5) * 2.0);
}

// ============================================================================
// Cell: one character slot in the TUI frame buffer
// ============================================================================

struct Cell {
    char    ch = ' ';
    Colour  fg = {180, 180, 180};
    bool    bold = false;
};

// ============================================================================
// FrameBuffer: 2D grid of cells
// ============================================================================

class FrameBuffer {
public:
    int W{}, H{};
    std::vector<Cell> cells;

    FrameBuffer() = default;
    FrameBuffer(int w, int h) : W(w), H(h), cells(w * h) {}

    void clear() { for (auto& c : cells) c = Cell{}; }

    Cell& at(int x, int y) {
        static Cell dummy;
        if (x < 0 || x >= W || y < 0 || y >= H) return dummy;
        return cells[y * W + x];
    }

    void put(int x, int y, char ch, Colour fg, bool bold = false) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        auto& c = at(x, y);
        c.ch = ch;
        c.fg = fg;
        c.bold = bold;
    }

    void put_string(int x, int y, const std::string& s, Colour fg, bool bold = false) {
        for (size_t i = 0; i < s.size(); ++i)
            put(x + (int)i, y, s[i], fg, bold);
    }

    // Set cell only if it is currently empty (space glyph)
    void set_if_empty(int x, int y, char ch, Colour fg, bool bold = false) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        auto& c = at(x, y);
        if (c.ch == ' ') { c.ch = ch; c.fg = fg; c.bold = bold; }
    }

    // Bresenham line between two terminal cells
    void draw_line(int x0, int y0, int x1, int y1, char ch, Colour fg) {
        int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;
        while (true) {
            put(x0, y0, ch, fg);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 <  dx) { err += dx; y0 += sy; }
        }
    }

    // Horizontal fill bar — value01 in [0,1] maps to filled width
    void draw_bar(int x, int y, int width, double value01, Colour fg,
                  char fill = '#', char empty = '.') {
        if (width <= 0) return;
        value01 = (value01 < 0.0) ? 0.0 : (value01 > 1.0 ? 1.0 : value01);
        int filled = static_cast<int>(value01 * width);
        for (int i = 0; i < width; ++i)
            put(x + i, y, (i < filled) ? fill : empty, fg);
    }

    // Flush rendered ANSI frame to any ostream
    void flush(std::ostream& out) const {
        out << render();
        out.flush();
    }

    // Draw a box outline
    void box(int x0, int y0, int w, int h, Colour fg) {
        for (int x = x0; x < x0 + w; ++x) {
            put(x, y0,         '-', fg);
            put(x, y0 + h - 1, '-', fg);
        }
        for (int y = y0; y < y0 + h; ++y) {
            put(x0,         y, '|', fg);
            put(x0 + w - 1, y, '|', fg);
        }
        put(x0,         y0,         '+', fg);
        put(x0 + w - 1, y0,         '+', fg);
        put(x0,         y0 + h - 1, '+', fg);
        put(x0 + w - 1, y0 + h - 1, '+', fg);
    }

    // Flush to string with ANSI codes
    std::string render() const {
        std::string out;
        out.reserve(W * H * 20);
        out += "\033[H";  // cursor home
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const auto& c = cells[y * W + x];
                if (c.bold) out += BOLD;
                out += c.fg.fg();
                out += c.ch;
                out += RESET;
            }
            out += '\n';
        }
        return out;
    }
};

// ============================================================================
// TUI Configuration
// ============================================================================

struct TUIConfig {
    int  width       = 120;   // terminal columns
    int  height      = 40;    // terminal rows
    int  lattice_w   = 60;    // lattice viewport width
    int  lattice_h   = 30;    // lattice viewport height
    int  panel_x     = 65;    // math panel x-offset
    int  panel_w     = 52;    // math panel width
    bool show_forces = true;
    bool show_wind   = true;
    bool xz_slice    = false; // false = XY projection, true = XZ
};

// ============================================================================
// Snapshot: all data needed for one TUI frame
// ============================================================================

struct TUISnapshot {
    // Lattice metadata
    double a{}, b{}, c{};
    double alpha_deg{}, beta_deg{}, gamma_deg{};
    double volume{};

    // Atom positions (Cartesian, Å)
    std::vector<Vec3>     positions;
    std::vector<uint32_t> types;

    // Forces (kcal/(mol·Å))
    std::vector<Vec3>     forces;
    double                Frms{};
    double                Fmax{};

    // Energy ledger
    double U_total{};
    double U_bond{};
    double U_vdw{};
    double U_coul{};
    double U_ext{};
    double KE{};
    double T{};

    // Integration state
    int    step{};
    double dt{};
    double dt_eff{};   // effective dt (with wind headroom)

    // Wind diagnostics
    bool   wind_active = false;
    Vec3   wind_dir{};
    double wind_strength{};
    double wind_ramp{};
    double wind_peak_force{};
    double wind_headroom{};

    // Build from live state
    static TUISnapshot capture(const State& s,
                               const crystal::Lattice& lat,
                               int step, double dt,
                               const perturbation::WindParticle* wind = nullptr);
};

// ============================================================================
// CrystalTUI: the main renderer
// ============================================================================

class CrystalTUI {
public:
    TUIConfig config;

    explicit CrystalTUI(const TUIConfig& cfg = {}) : config(cfg), fb(cfg.width, cfg.height) {}

    // Render one frame and return ANSI string
    std::string render(const TUISnapshot& snap);

    // Convenience: render + print to stdout
    void display(const TUISnapshot& snap);

private:
    FrameBuffer fb;

    // Sub-renderers
    void draw_border(const TUISnapshot& snap);
    void draw_lattice(const TUISnapshot& snap);
    void draw_atoms(const TUISnapshot& snap);
    void draw_forces(const TUISnapshot& snap);
    void draw_wind(const TUISnapshot& snap);
    void draw_math_panel(const TUISnapshot& snap);
    void draw_lattice_info(const TUISnapshot& snap);
    void draw_title_bar(const TUISnapshot& snap);

    // Projection helpers
    int proj_x(double cartesian_x, double x_min, double x_range) const;
    int proj_y(double cartesian_y, double y_min, double y_range) const;
};

} // namespace tui
} // namespace atomistic
