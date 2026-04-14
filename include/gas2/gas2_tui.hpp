/**
 * gas2_tui.hpp
 * ------------
 * Pure-ANSI Interactive TUI for Gas2/Gas3.
 *
 * No ncurses. No PDCurses. No new dependencies.
 * Uses raw terminal input (conio.h on Windows, termios on POSIX).
 * Renders with the same ANSI 256-color + box-drawing style as gas2_engine.cpp.
 *
 * Screens:
 *   MAIN_MENU      — Top-level nav: Species / Analyze / Sweep / Compare / Help / Quit
 *   SPECIES_SELECT — Scrollable species browser with property preview
 *   PARAM_EDIT     — T / P / n parameter editor
 *   ANALYSIS_VIEW  — Full gas2 report rendered inline
 *   EOS_COMPARE    — Side-by-side EOS comparison with live bar chart
 *   SWEEP_LIVE     — Gas3 adaptive sweep streaming view
 *   HELP           — Key binding reference
 *
 * Entry points:
 *   gas2_tui_run()   — called by `vsepr gas2 tui`
 *   gas3_tui_run()   — called by `vsepr gas3 tui`
 *
 * Anti-black-box: every screen is a standalone render function.
 * Every key binding is explicit. No hidden state.
 */

#pragma once

#include "gas2_engine.hpp"
#include "gas2_species.hpp"
#include "gas3/gas3_engine.hpp"
#include "gas3/gas3_sweep.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <functional>
#include <chrono>
#include <thread>

// ── Platform raw input ────────────────────────────────────────────────────────
#ifdef _WIN32
#  include <conio.h>
#  include <windows.h>
static void tui_raw_mode_on()  {}
static void tui_raw_mode_off() {}
static int  tui_kbhit()  { return _kbhit(); }
static int  tui_getch()  { return _getch(); }
#else
#  include <termios.h>
#  include <unistd.h>
#  include <sys/select.h>
static struct termios tui_orig_termios;
static void tui_raw_mode_on() {
    struct termios raw = tui_orig_termios;
    tcgetattr(STDIN_FILENO, &tui_orig_termios);
    raw = tui_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
static void tui_raw_mode_off() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tui_orig_termios);
}
static int tui_kbhit() {
    fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    struct timeval tv{0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}
static int tui_getch() {
    unsigned char c = 0;
    return (read(STDIN_FILENO, &c, 1) == 1) ? c : -1;
}
#endif

namespace vsepr {
namespace gas2 {

// ============================================================================
// ANSI helpers
// ============================================================================

namespace tui_ansi {

inline void clear()          { std::cout << "\033[2J\033[H"; }
inline void home()           { std::cout << "\033[H"; }
inline void hide_cursor()    { std::cout << "\033[?25l"; }
inline void show_cursor()    { std::cout << "\033[?25h"; }
inline void reset()          { std::cout << "\033[0m"; }
inline void bold()           { std::cout << "\033[1m"; }
inline void move(int r,int c){ std::cout << "\033[" << r << ";" << c << "H"; }

inline std::string fg(int c)        { return "\033[38;5;" + std::to_string(c) + "m"; }
inline std::string bg(int c)        { return "\033[48;5;" + std::to_string(c) + "m"; }
inline std::string bold_fg(int c)   { return "\033[1;38;5;" + std::to_string(c) + "m"; }
inline std::string reset_str()      { return "\033[0m"; }

// Palette
static constexpr int COL_HEADER  = 213;  // magenta
static constexpr int COL_SECTION = 75;   // cyan-blue
static constexpr int COL_PIPE    = 250;  // light grey
static constexpr int COL_VALUE   = 214;  // amber
static constexpr int COL_SPECIES = 219;  // lavender
static constexpr int COL_GOOD    = 48;   // green
static constexpr int COL_WARN    = 226;  // yellow
static constexpr int COL_BAD     = 196;  // red
static constexpr int COL_DIM     = 245;  // dim grey
static constexpr int COL_SELECT  = 33;   // bright blue (selected row)
static constexpr int COL_KEY     = 156;  // key hint green

// Print a horizontal rule
inline void rule(int width = 72, int col = 240) {
    std::cout << fg(col);
    for (int i = 0; i < width; ++i) std::cout << "─";
    std::cout << reset_str() << "\n";
}

// Bar of `len` filled blocks at colour `col`
inline std::string bar(double value, double max_val, int bar_width, int col) {
    int len = (max_val > 0) ? static_cast<int>(value / max_val * bar_width) : 0;
    len = std::max(1, std::min(len, bar_width));
    std::ostringstream s;
    s << fg(col);
    for (int i = 0; i < len; ++i) s << "█";
    s << reset_str();
    return s.str();
}

// Sparkline chars
static constexpr const char* SPARK[] = {"▁","▂","▃","▄","▅","▆","▇","█"};

} // namespace tui_ansi

// ============================================================================
// TUI state
// ============================================================================

enum class TuiScreen {
    MAIN_MENU,
    SPECIES_SELECT,
    PARAM_EDIT,
    ANALYSIS_VIEW,
    EOS_COMPARE,
    SWEEP_LIVE,
    HELP,
    QUIT
};

struct TuiState {
    TuiScreen screen    = TuiScreen::MAIN_MENU;

    // Species selection
    std::vector<std::string> species_keys;
    int species_cursor  = 0;
    int species_scroll  = 0;

    // Parameters
    std::string formula = "Ar";
    double T_K          = 298.15;
    double P_atm        = 1.0;
    double n_mol        = 1.0;
    int param_cursor    = 0;    // 0=T, 1=P, 2=n

    // Main menu cursor
    int menu_cursor     = 0;

    // Cached analysis
    bool analysis_valid = false;
    Gas2Analysis analysis;

    // Sweep state
    std::vector<gas3::GasStateRecord> sweep_records;
    int sweep_scroll    = 0;
    bool sweep_running  = false;

    // Status message (bottom line)
    std::string status;
};

// ============================================================================
// Screen: HEADER BANNER
// ============================================================================

static void render_header(const std::string& title, const TuiState& st) {
    using namespace tui_ansi;
    std::cout << bold_fg(COL_HEADER)
              << "╔══════════════════════════════════════════════════════════════════════╗\n"
              << "║  ⚛  VSEPR-SIM  │  Gas Analysis TUI  │  "
              << std::left << std::setw(29) << title << "║\n"
              << "╚══════════════════════════════════════════════════════════════════════╝"
              << reset_str() << "\n";
    // Species + conditions bar
    std::cout << fg(COL_DIM) << "  Species: " << reset_str()
              << bold_fg(COL_SPECIES) << std::setw(6) << st.formula << reset_str()
              << fg(COL_DIM) << "  T=" << reset_str()
              << fg(COL_VALUE) << std::fixed << std::setprecision(1) << st.T_K << " K" << reset_str()
              << fg(COL_DIM) << "  P=" << reset_str()
              << fg(COL_VALUE) << st.P_atm << " atm" << reset_str()
              << fg(COL_DIM) << "  n=" << reset_str()
              << fg(COL_VALUE) << st.n_mol << " mol" << reset_str()
              << "\n";
    rule();
}

// ============================================================================
// Screen: MAIN MENU
// ============================================================================

static const char* MENU_ITEMS[] = {
    "  [S]  Species Browser    — browse all 14 species with properties",
    "  [A]  Analyze            — full gas2 analysis report",
    "  [E]  EOS Compare        — Ideal / VdW / Redlich-Kwong side-by-side",
    "  [P]  Parameters         — edit T, P, n",
    "  [W]  Sweep (gas3)       — live quality sweep across T/P grid",
    "  [H]  Help               — key bindings and navigation",
    "  [Q]  Quit",
};
static constexpr int MENU_COUNT = 7;

static void render_main_menu(const TuiState& st) {
    using namespace tui_ansi;
    clear();
    render_header("Main Menu", st);
    std::cout << "\n";
    for (int i = 0; i < MENU_COUNT; ++i) {
        if (i == st.menu_cursor) {
            std::cout << bg(COL_SELECT) << bold_fg(COL_GOOD) << "▶" << MENU_ITEMS[i]
                      << std::string(72 - 1 - strlen(MENU_ITEMS[i]), ' ') << reset_str() << "\n";
        } else {
            std::cout << fg(COL_DIM) << " " << reset_str()
                      << fg(255) << MENU_ITEMS[i] << reset_str() << "\n";
        }
    }
    std::cout << "\n";
    rule();
    std::cout << fg(COL_KEY) << "  ↑/↓ navigate   Enter/letter select   Q quit" << reset_str() << "\n";
    if (!st.status.empty())
        std::cout << fg(COL_WARN) << "  " << st.status << reset_str() << "\n";
}

// ============================================================================
// Screen: SPECIES SELECT
// ============================================================================

static void render_species_select(const TuiState& st) {
    using namespace tui_ansi;
    clear();
    render_header("Species Browser", st);

    constexpr int VISIBLE = 12;

    std::cout << "\n";
    std::cout << fg(COL_SECTION) << "  "
              << std::setw(6) << std::left << "Form"
              << std::setw(22) << "Name"
              << std::setw(10) << "M(g/mol)"
              << std::setw(9)  << "γ"
              << std::setw(12) << "η(μPa·s)"
              << std::setw(10) << "Tc(K)"
              << "\n" << reset_str();
    rule(72, 238);

    int total = static_cast<int>(st.species_keys.size());
    int end   = std::min(st.species_scroll + VISIBLE, total);

    for (int i = st.species_scroll; i < end; ++i) {
        const auto& key = st.species_keys[i];
        const auto* sp  = find_species(key);
        if (!sp) continue;

        bool selected = (i == st.species_cursor);
        if (selected) {
            std::cout << bg(COL_SELECT) << bold_fg(COL_GOOD) << "▶ ";
        } else {
            std::cout << fg(COL_DIM) << "  " << reset_str();
        }

        std::cout << bold_fg(COL_SPECIES) << std::setw(6) << std::left << sp->formula
                  << reset_str()
                  << fg(255) << std::setw(22) << sp->name
                  << fg(COL_VALUE) << std::fixed
                  << std::setw(10) << std::setprecision(3) << sp->molar_mass_g
                  << std::setw(9)  << std::setprecision(3) << sp->gamma
                  << std::setw(12) << std::setprecision(1) << sp->viscosity_uPas
                  << std::setw(10) << std::setprecision(1) << sp->Tc_K
                  << reset_str();
        if (selected) std::cout << reset_str();
        std::cout << "\n";
    }

    // Scroll indicator
    if (total > VISIBLE) {
        std::cout << fg(COL_DIM) << "  (" << (st.species_scroll + 1)
                  << "–" << end << " of " << total << ")" << reset_str() << "\n";
    }

    // Property detail panel for selected species
    const auto* sel = find_species(st.species_keys[st.species_cursor]);
    if (sel) {
        rule(72, 238);
        std::cout << fg(COL_SECTION) << "┌─ Detail: " << bold_fg(COL_SPECIES) << sel->name << reset_str() << "\n";
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  Cv=" << std::setprecision(2) << sel->Cv_Jmol << " J/mol·K"
                  << "  Cp=" << sel->Cp_Jmol << " J/mol·K"
                  << "  d_kin=" << std::setprecision(0) << sel->d_kinetic_pm << " pm"
                  << "  Pc=" << std::setprecision(1) << sel->Pc_atm << " atm\n";
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  VdW a=" << std::setprecision(4) << sel->vdw_a
                  << " Pa·m⁶/mol²   b=" << std::setprecision(4) << sel->vdw_b * 1e5
                  << "×10⁻⁵ m³/mol"
                  << "  Hf⁰=" << std::setprecision(1) << sel->Hf0_kJmol << " kJ/mol\n";
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  η(tab)=" << std::setprecision(1) << sel->viscosity_uPas
                  << " μPa·s   κ=" << sel->k_thermal_mWmK << " mW/m·K\n";
        std::cout << fg(COL_SECTION) << "└" << reset_str() << "\n";
    }

    rule();
    std::cout << fg(COL_KEY)
              << "  ↑/↓ scroll   Enter=select+analyze   Esc/M=menu"
              << reset_str() << "\n";
}

// ============================================================================
// Screen: PARAM EDIT
// ============================================================================

static void render_param_edit(const TuiState& st) {
    using namespace tui_ansi;
    clear();
    render_header("Parameters", st);

    const char* labels[] = {"Temperature (K)", "Pressure (atm)", "Amount (mol)"};
    double vals[]        = {st.T_K, st.P_atm, st.n_mol};
    const char* hints[]  = {"[100 – 5000]", "[0.01 – 500]", "[0.001 – 100]"};

    std::cout << "\n";
    for (int i = 0; i < 3; ++i) {
        bool active = (i == st.param_cursor);
        if (active) {
            std::cout << bg(COL_SELECT) << bold_fg(COL_GOOD) << "▶ ";
        } else {
            std::cout << fg(COL_DIM) << "  " << reset_str();
        }
        std::cout << bold_fg(COL_SECTION) << std::setw(22) << std::left << labels[i]
                  << reset_str()
                  << fg(COL_VALUE) << std::fixed << std::setprecision(4) << std::setw(14) << vals[i]
                  << reset_str()
                  << fg(COL_DIM) << hints[i] << reset_str();
        if (active) std::cout << reset_str();
        std::cout << "\n";
    }

    std::cout << "\n";
    rule(72, 238);

    // Quick presets
    struct Preset { const char* label; double T; double P; };
    static const Preset presets[] = {
        {"STP  (273.15K, 1 atm)",   273.15, 1.0},
        {"NTP  (293.15K, 1 atm)",   293.15, 1.0},
        {"Room (298.15K, 1 atm)",   298.15, 1.0},
        {"Hot  (1000K,   1 atm)",  1000.0,  1.0},
        {"HP   (298.15K, 50 atm)", 298.15, 50.0},
        {"Cryo (77K,     1 atm)",    77.0,  1.0},
    };
    std::cout << fg(COL_SECTION) << "┌─ Quick Presets\n" << reset_str();
    for (int i = 0; i < 6; ++i) {
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  [" << (i+1) << "] " << fg(COL_DIM) << presets[i].label << reset_str() << "\n";
    }
    std::cout << fg(COL_SECTION) << "└" << reset_str() << "\n";

    rule();
    std::cout << fg(COL_KEY)
              << "  ↑/↓ field   +/- adjust ×0.1   1-6 preset   A=analyze   Esc/M=menu"
              << reset_str() << "\n";
    if (!st.status.empty())
        std::cout << fg(COL_WARN) << "  " << st.status << reset_str() << "\n";
}

// ============================================================================
// Screen: ANALYSIS VIEW
// ============================================================================

static void render_analysis_view(const TuiState& st) {
    using namespace tui_ansi;
    clear();
    render_header("Full Analysis", st);

    if (!st.analysis_valid) {
        std::cout << "\n" << fg(COL_WARN)
                  << "  No analysis cached.  Press [A] from the menu, or [Enter] in Species Browser.\n"
                  << reset_str();
    } else {
        // Delegate to the engine's own renderer — already rich ANSI output
        std::cout << st.analysis.format_full_report();
    }

    rule();
    std::cout << fg(COL_KEY)
              << "  [R] rerun   [E] EOS compare   [W] sweep   Esc/M=menu"
              << reset_str() << "\n";
}

// ============================================================================
// Screen: EOS COMPARE
// ============================================================================

static void render_eos_compare(const TuiState& st) {
    using namespace tui_ansi;
    clear();
    render_header("EOS Comparison", st);

    if (!st.analysis_valid) {
        std::cout << "\n" << fg(COL_WARN) << "  No analysis cached — run Analyze first.\n" << reset_str();
        rule();
        std::cout << fg(COL_KEY) << "  A=analyze   Esc/M=menu" << reset_str() << "\n";
        return;
    }

    const auto& a = st.analysis;

    // --- Volume comparison ---
    std::cout << "\n" << fg(COL_SECTION) << "┌─ Molar Volume (L/mol)\033[0m\n";
    double vmax = std::max({a.eos_ideal.V_L(), a.eos_vdw.V_L(), a.eos_rk.V_L(), 0.001});
    constexpr int BW = 32;

    auto eos_row = [&](const char* name, const EOSResult& r, int col) {
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  " << bold_fg(col) << std::setw(14) << std::left << name << reset_str()
                  << fg(col) << std::setw(10) << std::fixed << std::setprecision(5) << r.V_L() << reset_str()
                  << " L   " << bar(r.V_L(), vmax, BW, col)
                  << "  Z=" << fg(COL_VALUE) << std::setprecision(6) << r.Z << reset_str()
                  << "  (" << r.iterations << " iter)\n";
    };
    eos_row("Ideal Gas",     a.eos_ideal, 75);
    eos_row("Van der Waals", a.eos_vdw,   156);
    eos_row("Redlich-Kwong", a.eos_rk,    213);
    std::cout << fg(COL_SECTION) << "└" << reset_str() << "\n\n";

    // --- Deviation from ideal ---
    std::cout << fg(COL_SECTION) << "┌─ Deviation from Ideal\033[0m\n";
    auto dev_row = [&](const char* name, double V_real, int col) {
        double dev = (V_real - a.eos_ideal.V_L()) / a.eos_ideal.V_L() * 100.0;
        int bar_len = std::min(static_cast<int>(std::abs(dev) / 20.0 * 24), 24);
        std::string bar_str;
        for (int i = 0; i < bar_len; ++i) bar_str += (dev < 0) ? "◀" : "▶";
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  " << bold_fg(col) << std::setw(14) << std::left << name << reset_str()
                  << fg(dev < 0 ? COL_GOOD : COL_BAD) << std::showpos << std::fixed
                  << std::setprecision(3) << dev << "%" << std::noshowpos << reset_str()
                  << "   " << fg(col) << bar_str << reset_str() << "\n";
    };
    dev_row("Van der Waals", a.eos_vdw.V_L(), 156);
    dev_row("Redlich-Kwong", a.eos_rk.V_L(),  213);
    std::cout << fg(COL_SECTION) << "└" << reset_str() << "\n\n";

    // --- Kinetic summary ---
    std::cout << fg(COL_SECTION) << "┌─ Kinetic Theory\033[0m\n";
    double vscale = std::max(a.v_rms, 1.0);
    auto speed_row = [&](const char* name, double v, int col) {
        int slen = std::max(1, static_cast<int>(v / vscale * 20));
        std::string gauge;
        for (int i = 0; i < slen; ++i) gauge += "━";
        gauge += "▸";
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  " << fg(col) << std::setw(10) << std::left << name << reset_str()
                  << fg(COL_VALUE) << std::setw(10) << std::fixed << std::setprecision(1) << v << " m/s  " << reset_str()
                  << fg(col) << gauge << reset_str() << "\n";
    };
    speed_row("v_mp",   a.v_mp,   226);
    speed_row("v_mean", a.v_mean, 48);
    speed_row("v_rms",  a.v_rms,  196);
    std::cout << fg(COL_PIPE) << "│" << reset_str()
              << "  MFP=" << std::setprecision(2) << (a.mean_free_path_m * 1e9) << " nm"
              << "   η=" << std::setprecision(2) << (a.viscosity * 1e6) << " μPa·s";
    if (a.species) {
        std::cout << fg(COL_DIM) << "  [tab: " << a.species->viscosity_uPas << "]" << reset_str();
    }
    std::cout << "\n" << fg(COL_SECTION) << "└" << reset_str() << "\n\n";

    // --- Thermal ---
    std::cout << fg(COL_SECTION) << "┌─ Thermal Properties\033[0m\n";
    std::cout << fg(COL_PIPE) << "│" << reset_str()
              << "  Cv=" << std::setprecision(3) << a.Cv_calc << " J/mol·K";
    if (a.species) std::cout << fg(COL_DIM) << "  [tab: " << a.species->Cv_Jmol << "]" << reset_str();
    std::cout << "\n";
    std::cout << fg(COL_PIPE) << "│" << reset_str()
              << "  Cp=" << a.Cp_calc << " J/mol·K";
    if (a.species) std::cout << fg(COL_DIM) << "  [tab: " << a.species->Cp_Jmol << "]" << reset_str();
    std::cout << "\n";
    std::cout << fg(COL_PIPE) << "│" << reset_str()
              << "  γ=" << std::setprecision(4) << a.gamma_calc;
    if (a.species) std::cout << fg(COL_DIM) << "  [tab: " << a.species->gamma << "]" << reset_str();
    std::cout << "\n";
    std::cout << fg(COL_PIPE) << "│" << reset_str()
              << "  c_sound=" << std::setprecision(1) << a.c_sound << " m/s\n";
    if (a.species) {
        std::cout << fg(COL_PIPE) << "│" << reset_str()
                  << "  T_inv=" << std::setprecision(0) << a.T_inversion << " K";
        if (a.T_K < a.T_inversion)
            std::cout << fg(COL_SECTION) << "  ❄ cooling on expansion" << reset_str();
        else
            std::cout << fg(COL_BAD) << "  🔥 heating on expansion" << reset_str();
        std::cout << "\n";
    }
    std::cout << fg(COL_SECTION) << "└" << reset_str() << "\n";

    rule();
    std::cout << fg(COL_KEY)
              << "  [R] rerun   [A] full report   [W] sweep   Esc/M=menu"
              << reset_str() << "\n";
}

// ============================================================================
// Screen: SWEEP LIVE (Gas3 quick linear sweep, single species)
// ============================================================================

static void render_sweep_live(const TuiState& st) {
    using namespace tui_ansi;
    clear();
    render_header("Gas3 Sweep — " + st.formula, st);

    const auto& recs = st.sweep_records;
    if (recs.empty()) {
        std::cout << "\n" << fg(COL_WARN)
                  << (st.sweep_running ? "  Sweeping…\n" : "  Press [R] to run sweep.\n")
                  << reset_str();
        rule();
        std::cout << fg(COL_KEY) << "  [R] run sweep   Esc/M=menu" << reset_str() << "\n";
        return;
    }

    // Summary bar
    int total = static_cast<int>(recs.size());
    int q4=0, q3=0, q2=0, q1=0, q0=0;
    double sum_q = 0.0;
    for (const auto& r : recs) {
        switch (r.quality_tier) {
            case gas3::QualityTier::Q4: ++q4; break;
            case gas3::QualityTier::Q3: ++q3; break;
            case gas3::QualityTier::Q2: ++q2; break;
            case gas3::QualityTier::Q1: ++q1; break;
            case gas3::QualityTier::Q0: ++q0; break;
        }
        sum_q += r.quality_score;
    }
    double avg_q = (total > 0) ? sum_q / total : 0.0;

    std::cout << "\n" << fg(COL_SECTION) << "┌─ Summary  (" << total << " records)\033[0m\n";
    std::cout << fg(COL_PIPE) << "│" << reset_str()
              << "  " << bold_fg(COL_GOOD) << "Q4:" << q4 << reset_str()
              << fg(75)  << "  Q3:" << q3 << reset_str()
              << fg(COL_VALUE) << "  Q2:" << q2 << reset_str()
              << fg(COL_WARN) << "  Q1:" << q1 << reset_str()
              << fg(COL_BAD)  << "  Q0:" << q0 << reset_str()
              << fg(COL_DIM)  << "  avg=" << std::fixed << std::setprecision(1) << avg_q << reset_str()
              << "\n";

    // Quality sparkline across the whole sweep
    std::cout << fg(COL_PIPE) << "│" << reset_str() << "  ";
    int spark_w = std::min(total, 60);
    for (int i = 0; i < spark_w; ++i) {
        int idx = static_cast<int>(i * static_cast<double>(total) / spark_w);
        double q = recs[idx].quality_score;
        int level = static_cast<int>(q / 100.0 * 7.0);
        level = std::max(0, std::min(7, level));
        int col = (q >= 75) ? COL_GOOD : (q >= 55) ? 75 : (q >= 25) ? COL_WARN : COL_BAD;
        std::cout << fg(col) << tui_ansi::SPARK[level] << reset_str();
    }
    std::cout << "  quality\n";
    std::cout << fg(COL_SECTION) << "└" << reset_str() << "\n\n";

    // Scrollable record table
    constexpr int VISIBLE = 14;
    int scroll = st.sweep_scroll;
    int end = std::min(scroll + VISIBLE, total);

    std::cout << fg(COL_SECTION)
              << "  " << std::setw(8) << std::left << "T(K)"
              << std::setw(10) << "P(atm)"
              << std::setw(10) << "Model"
              << std::setw(10) << "V(L)"
              << std::setw(8)  << "Z"
              << std::setw(8)  << "Score"
              << std::setw(4)  << "Tier"
              << "\n" << reset_str();    rule(72, 238);

    for (int i = scroll; i < end; ++i) {
        const auto& r = recs[i];
        int tier_col = (r.quality_tier == gas3::QualityTier::Q4) ? COL_GOOD :
                       (r.quality_tier == gas3::QualityTier::Q3) ? 75 :
                       (r.quality_tier == gas3::QualityTier::Q2) ? COL_VALUE :
                       (r.quality_tier == gas3::QualityTier::Q1) ? COL_WARN : COL_BAD;
        std::cout << fg(255)
                      << "  " << std::setw(8) << std::fixed << std::setprecision(1) << r.T_K
                      << std::setw(10) << std::setprecision(2) << r.P_atm()
                      << std::setw(10) << r.model_name
                      << std::setw(10) << std::setprecision(4) << (r.V_m3 * 1000.0)
                      << std::setw(8)  << std::setprecision(4) << r.Z
                  << fg(tier_col)  << std::setw(8) << std::setprecision(1) << r.quality_score
                  << "  " << gas3::tier_name(r.quality_tier)
                  << reset_str() << "\n";
    }

    if (total > VISIBLE) {
        std::cout << fg(COL_DIM) << "  (" << (scroll + 1) << "–" << end
                  << " of " << total << ")   ↑/↓ scroll" << reset_str() << "\n";
    }

    rule();
    std::cout << fg(COL_KEY)
              << "  [R] rerun sweep   ↑/↓ scroll   Esc/M=menu"
              << reset_str() << "\n";
}

// ============================================================================
// Screen: HELP
// ============================================================================

static void render_help(const TuiState& st) {
    using namespace tui_ansi;
    clear();
    render_header("Help — Key Bindings", st);

    struct Section { const char* title; std::vector<std::pair<const char*, const char*>> keys; };
    const Section sections[] = {
        {"Global", {
            {"Q / Esc",  "Return to main menu"},
            {"M",        "Main menu (any screen)"},
            {"A",        "Run analysis with current params"},
            {"R",        "Rerun / refresh current view"},
            {"?",        "Toggle help"},
        }},
        {"Navigation", {
            {"↑ / k",    "Move cursor up"},
            {"↓ / j",    "Move cursor down"},
            {"Enter",    "Select / confirm"},
            {"Tab",      "Next screen"},
        }},
        {"Parameters", {
            {"+",        "Increase active param ×1.1"},
            {"-",        "Decrease active param ×0.9"},
            {"1-6",      "Quick preset (STP/NTP/Room/Hot/HP/Cryo)"},
        }},
        {"Screens", {
            {"S",        "Species Browser"},
            {"E",        "EOS Compare"},
            {"P",        "Parameter Editor"},
            {"W",        "Gas3 Sweep"},
            {"H / ?",    "This help screen"},
        }},
    };

    std::cout << "\n";
    for (const auto& sec : sections) {
        std::cout << fg(COL_SECTION) << "┌─ " << sec.title << "\033[0m\n";
        for (const auto& [key, desc] : sec.keys) {
            std::cout << fg(COL_PIPE) << "│" << reset_str()
                      << "  " << bold_fg(COL_KEY) << std::setw(12) << std::left << key
                      << reset_str() << fg(255) << desc << reset_str() << "\n";
        }
        std::cout << fg(COL_SECTION) << "└\033[0m\n\n";
    }

    rule();
    std::cout << fg(COL_KEY) << "  Any key to return" << reset_str() << "\n";
}

// ============================================================================
// Sweep runner (blocking, updates state in place)
// ============================================================================

static void run_sweep(TuiState& st) {
    gas3::SweepConfig cfg;
    cfg.T_min_K  = 100.0;
    cfg.T_max_K  = 1500.0;
    cfg.T_step_K = 100.0;
    cfg.P_grid_atm = {0.5, 1.0, 5.0, 10.0, 50.0};
    cfg.species_list = {st.formula};

    gas3::SweepStats stats;
    st.sweep_records.clear();
    st.sweep_running = true;
    st.sweep_records = gas3::linear_sweep(cfg, stats, nullptr);
    st.sweep_running = false;
    st.sweep_scroll  = 0;
    st.status = "Sweep complete: " + std::to_string(st.sweep_records.size()) + " records";
}

// ============================================================================
// Input handling helpers
// ============================================================================

static void adjust_param(TuiState& st, bool increase) {
    double factor = increase ? 1.1 : 0.9;
    switch (st.param_cursor) {
        case 0: st.T_K   = std::max(10.0,  std::min(5000.0, st.T_K   * factor)); break;
        case 1: st.P_atm = std::max(0.01,  std::min(500.0,  st.P_atm * factor)); break;
        case 2: st.n_mol = std::max(0.001, std::min(100.0,  st.n_mol * factor)); break;
    }
    st.analysis_valid = false;
}

static void apply_preset(TuiState& st, int idx) {
    struct P { double T; double P; };
    static const P presets[] = {
        {273.15, 1.0}, {293.15, 1.0}, {298.15, 1.0},
        {1000.0, 1.0}, {298.15, 50.0}, {77.0, 1.0}
    };
    if (idx >= 0 && idx < 6) {
        st.T_K   = presets[idx].T;
        st.P_atm = presets[idx].P;
        st.analysis_valid = false;
    }
}

static void do_analyze(TuiState& st) {
    st.analysis       = analyze(st.formula, st.T_K, st.P_atm, st.n_mol);
    st.analysis_valid = true;
    st.status         = "Analysis complete: " + st.formula
                        + " @ " + std::to_string(static_cast<int>(st.T_K)) + "K";
}

// ============================================================================
// Main TUI event loop
// ============================================================================

inline int gas2_tui_run(const std::string& initial_formula = "Ar",
                        double initial_T = 298.15,
                        double initial_P = 1.0,
                        double initial_n = 1.0) {
    using namespace tui_ansi;

    // Build state
    TuiState st;
    st.formula = initial_formula;
    st.T_K     = initial_T;
    st.P_atm   = initial_P;
    st.n_mol   = initial_n;

    // Populate species list
    for (const auto& [key, _] : species_database())
        st.species_keys.push_back(key);
    std::sort(st.species_keys.begin(), st.species_keys.end());

    // Set species cursor to initial formula
    for (int i = 0; i < static_cast<int>(st.species_keys.size()); ++i) {
        if (st.species_keys[i] == st.formula) { st.species_cursor = i; break; }
    }

    // Run initial analysis
    do_analyze(st);

    tui_raw_mode_on();
    hide_cursor();

    auto render = [&]() {
        switch (st.screen) {
            case TuiScreen::MAIN_MENU:     render_main_menu(st);     break;
            case TuiScreen::SPECIES_SELECT:render_species_select(st); break;
            case TuiScreen::PARAM_EDIT:    render_param_edit(st);    break;
            case TuiScreen::ANALYSIS_VIEW: render_analysis_view(st); break;
            case TuiScreen::EOS_COMPARE:   render_eos_compare(st);   break;
            case TuiScreen::SWEEP_LIVE:    render_sweep_live(st);    break;
            case TuiScreen::HELP:          render_help(st);          break;
            case TuiScreen::QUIT:          break;
        }
        std::cout.flush();
    };

    render();

    while (st.screen != TuiScreen::QUIT) {
        if (!tui_kbhit()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        int ch = tui_getch();

        // Arrow key sequences: ESC [ A/B/C/D
        if (ch == 27) {
            int ch2 = tui_getch();
            if (ch2 == '[') {
                int ch3 = tui_getch();
                if      (ch3 == 'A') ch = 1001; // up
                else if (ch3 == 'B') ch = 1002; // down
                else if (ch3 == 'C') ch = 1003; // right
                else if (ch3 == 'D') ch = 1004; // left
                else ch = 27; // bare ESC
            } else if (ch2 == -1 || ch2 == 0) {
                ch = 27; // bare ESC
            } else {
                ch = 27;
            }
        }
#ifdef _WIN32
        // conio extended keys: first byte 0 or 224
        else if (ch == 0 || ch == 224) {
            int ch2 = tui_getch();
            if      (ch2 == 72) ch = 1001; // up
            else if (ch2 == 80) ch = 1002; // down
            else if (ch2 == 77) ch = 1003; // right
            else if (ch2 == 75) ch = 1004; // left
        }
#endif
        // Normalise to uppercase for letter shortcuts (leave digits and specials)
        int letter = (ch >= 'a' && ch <= 'z') ? (ch - 32) : ch;

        // ── Global shortcuts ──────────────────────────────────────────────
        if (letter == 'Q' || (ch == 27 && st.screen == TuiScreen::MAIN_MENU)) {
            st.screen = TuiScreen::QUIT;
            break;
        }
        if (ch == 27 || letter == 'M') {
            st.screen = TuiScreen::MAIN_MENU;
            render(); continue;
        }
        if (letter == 'A') {
            do_analyze(st);
            st.screen = TuiScreen::ANALYSIS_VIEW;
            render(); continue;
        }
        if (letter == 'H' || ch == '?') {
            st.screen = TuiScreen::HELP;
            render(); continue;
        }
        if (letter == 'S') { st.screen = TuiScreen::SPECIES_SELECT; render(); continue; }
        if (letter == 'E') { st.screen = TuiScreen::EOS_COMPARE;    render(); continue; }
        if (letter == 'P') { st.screen = TuiScreen::PARAM_EDIT;     render(); continue; }
        if (letter == 'W') { st.screen = TuiScreen::SWEEP_LIVE;     render(); continue; }

        // ── Screen-specific input ─────────────────────────────────────────
        switch (st.screen) {

            // ---- MAIN MENU ----
            case TuiScreen::MAIN_MENU: {
                if (ch == 1001) st.menu_cursor = std::max(0, st.menu_cursor - 1);
                if (ch == 1002) st.menu_cursor = std::min(MENU_COUNT - 1, st.menu_cursor + 1);
                if (ch == '\r' || ch == '\n' || ch == ' ') {
                    switch (st.menu_cursor) {
                        case 0: st.screen = TuiScreen::SPECIES_SELECT; break;
                        case 1: do_analyze(st); st.screen = TuiScreen::ANALYSIS_VIEW; break;
                        case 2: st.screen = TuiScreen::EOS_COMPARE;    break;
                        case 3: st.screen = TuiScreen::PARAM_EDIT;     break;
                        case 4: st.screen = TuiScreen::SWEEP_LIVE;     break;
                        case 5: st.screen = TuiScreen::HELP;           break;
                        case 6: st.screen = TuiScreen::QUIT;           break;
                    }
                }
                break;
            }

            // ---- SPECIES SELECT ----
            case TuiScreen::SPECIES_SELECT: {
                int total_sp = static_cast<int>(st.species_keys.size());
                if (ch == 1001) {
                    if (st.species_cursor > 0) --st.species_cursor;
                    if (st.species_cursor < st.species_scroll) st.species_scroll = st.species_cursor;
                }
                if (ch == 1002) {
                    if (st.species_cursor < total_sp - 1) ++st.species_cursor;
                    if (st.species_cursor >= st.species_scroll + 12) ++st.species_scroll;
                }
                if (ch == '\r' || ch == '\n') {
                    st.formula       = st.species_keys[st.species_cursor];
                    st.analysis_valid = false;
                    do_analyze(st);
                    st.screen = TuiScreen::ANALYSIS_VIEW;
                }
                break;
            }

            // ---- PARAM EDIT ----
            case TuiScreen::PARAM_EDIT: {
                if (ch == 1001) st.param_cursor = std::max(0, st.param_cursor - 1);
                if (ch == 1002) st.param_cursor = std::min(2, st.param_cursor + 1);
                if (ch == '+' || ch == '=') { adjust_param(st, true);  }
                if (ch == '-' || ch == '_') { adjust_param(st, false); }
                if (ch >= '1' && ch <= '6') { apply_preset(st, ch - '1'); }
                break;
            }

            // ---- ANALYSIS VIEW ----
            case TuiScreen::ANALYSIS_VIEW: {
                if (letter == 'R') { do_analyze(st); }
                break;
            }

            // ---- EOS COMPARE ----
            case TuiScreen::EOS_COMPARE: {
                if (letter == 'R') { do_analyze(st); }
                break;
            }

            // ---- SWEEP LIVE ----
            case TuiScreen::SWEEP_LIVE: {
                if (letter == 'R') { run_sweep(st); }
                int total_r = static_cast<int>(st.sweep_records.size());
                if (ch == 1001) st.sweep_scroll = std::max(0, st.sweep_scroll - 1);
                if (ch == 1002) st.sweep_scroll = std::min(std::max(0, total_r - 14), st.sweep_scroll + 1);
                break;
            }

            // ---- HELP ----
            case TuiScreen::HELP: {
                // Any key returns to main menu
                st.screen = TuiScreen::MAIN_MENU;
                break;
            }

            default: break;
        }

        if (st.screen != TuiScreen::QUIT) render();
    }

    // Cleanup
    show_cursor();
    tui_raw_mode_off();
    clear();
    std::cout << tui_ansi::fg(tui_ansi::COL_HEADER)
              << "VSEPR-SIM Gas TUI exited.\n"
              << tui_ansi::reset_str();
    return 0;
}

} // namespace gas2
} // namespace vsepr
