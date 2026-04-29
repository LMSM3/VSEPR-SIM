/**
 * nvidia_tui.cpp
 * ---------------
 * Secret NVIDIA subsystem monitoring TUI and secret key handler.
 * Shells out to nvidia-smi for real-time GPU telemetry.
 * Cross-platform: Windows (primary), Linux, macOS.
 *
 * This module adds zero weight to the normal boot path — the TUI
 * code is only entered if the user presses a secret key during the
 * 2-second boot window.
 */

#include "infra/nvidia_tui.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_set>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <conio.h>
#  include <io.h>
#else
#  include <termios.h>
#  include <unistd.h>
#  include <sys/select.h>
#endif

namespace fs = std::filesystem;

namespace vsepr {
namespace infra {

namespace {

// ============================================================================
// ANSI escape sequences
// ============================================================================

const char* ESC_CLEAR    = "\033[2J\033[H";
const char* ESC_HIDE_CUR = "\033[?25l";
const char* ESC_SHOW_CUR = "\033[?25h";

const char* COL_RESET   = "\033[0m";
const char* COL_BOLD    = "\033[1m";
const char* COL_DIM     = "\033[2m";
const char* COL_CYAN    = "\033[0;36m";
const char* COL_GREEN   = "\033[0;32m";
const char* COL_YELLOW  = "\033[1;33m";
const char* COL_RED     = "\033[0;31m";
const char* COL_MAGENTA = "\033[0;35m";
const char* COL_GRAY    = "\033[0;90m";

// ============================================================================
// Platform: terminal input (non-blocking)
// ============================================================================

#ifdef _WIN32

bool is_terminal() { return _isatty(_fileno(stdin)) != 0; }

void setup_terminal()   { /* Windows console needs no mode change for _kbhit */ }
void restore_terminal() { /* no-op */ }

bool has_keypress() { return _kbhit() != 0; }

char read_keypress() { return static_cast<char>(_getch()); }

void sleep_ms(int ms) { Sleep(ms); }

#else  // POSIX

static termios g_saved_termios;
static bool    g_termios_saved = false;

bool is_terminal() { return isatty(STDIN_FILENO) != 0; }

void setup_terminal() {
    if (!is_terminal()) return;
    tcgetattr(STDIN_FILENO, &g_saved_termios);
    g_termios_saved = true;
    termios raw = g_saved_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void restore_terminal() {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_termios);
        g_termios_saved = false;
    }
}

bool has_keypress() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    timeval tv = {0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

char read_keypress() {
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#endif

// ============================================================================
// Shell execution (local copy — matches bootstrap_probe.cpp)
// ============================================================================

std::string shell_exec(const char* cmd) {
    std::string result;
    try {
#ifdef _WIN32
        FILE* pipe = _popen(cmd, "r");
#else
        FILE* pipe = popen(cmd, "r");
#endif
        if (!pipe) return "";
        std::array<char, 512> buf{};
        while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
            result += buf.data();
        }
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
    } catch (...) {
        return "";
    }
    while (!result.empty() &&
           (result.back() == '\n' || result.back() == '\r' ||
            result.back() == ' '  || result.back() == '\t')) {
        result.pop_back();
    }
    return result;
}

std::vector<std::string> shell_lines(const char* cmd) {
    std::string raw;
    try {
#ifdef _WIN32
        FILE* pipe = _popen(cmd, "r");
#else
        FILE* pipe = popen(cmd, "r");
#endif
        if (!pipe) return {};
        std::array<char, 512> buf{};
        while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
            raw += buf.data();
        }
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
    } catch (...) {
        return {};
    }
    std::vector<std::string> lines;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

// ============================================================================
// Parsing helpers
// ============================================================================

std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        fields.push_back(trim(field));
    }
    return fields;
}

double safe_double(const std::string& s) {
    try { return std::stod(s); } catch (...) { return 0.0; }
}

int safe_int(const std::string& s) {
    try { return std::stoi(s); } catch (...) { return 0; }
}

std::string basename(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

// ============================================================================
// GPU data structures
// ============================================================================

struct GpuSnapshot {
    std::string name;
    std::string driver_version;
    int    temperature_c   = 0;
    int    gpu_util_pct    = 0;
    int    mem_util_pct    = 0;
    double mem_used_mb     = 0;
    double mem_total_mb    = 0;
    double power_draw_w    = 0;
    double power_limit_w   = 0;
    int    fan_speed_pct   = 0;
    int    clock_gpu_mhz   = 0;
    int    clock_mem_mhz   = 0;
    bool   valid           = false;
    std::string error_msg;
};

struct GpuProcess {
    int         pid     = 0;
    std::string name;
    double      mem_mb  = 0;
};

struct SubsystemSnapshot {
    bool        session_active  = false;
    std::string session_path;
    int         cache_files     = 0;
    double      cache_size_mb   = 0;
    int         export_pending  = 0;
    bool        cuda_compiled   = false;
    std::string render_status;
    std::string log_tail;        // last line of session.log

    // Tracker integration (populated from .vsepr-session/tracker.csv if present)
    bool        tracker_active      = false;
    int         tracker_particles   = 0;
    int         tracker_active_p    = 0;
    int         tracker_events      = 0;
    int         tracker_elements    = 0;    // unique Z count
    int         tracker_children    = 0;    // particles with parent_id
    uint64_t    tracker_highest_id  = 0;
    std::string tracker_summary;            // one-line summary
};

// ============================================================================
// nvidia-smi queries
// ============================================================================

GpuSnapshot query_gpu() {
    GpuSnapshot g;

    // Try Windows stderr redirect first, then POSIX
    std::string csv = shell_exec(
        "nvidia-smi --query-gpu="
        "name,driver_version,temperature.gpu,"
        "utilization.gpu,utilization.memory,"
        "memory.used,memory.total,"
        "power.draw,power.limit,"
        "fan.speed,"
        "clocks.current.graphics,clocks.current.memory"
        " --format=csv,noheader,nounits 2>nul");
    if (csv.empty()) {
        csv = shell_exec(
            "nvidia-smi --query-gpu="
            "name,driver_version,temperature.gpu,"
            "utilization.gpu,utilization.memory,"
            "memory.used,memory.total,"
            "power.draw,power.limit,"
            "fan.speed,"
            "clocks.current.graphics,clocks.current.memory"
            " --format=csv,noheader,nounits 2>/dev/null");
    }

    if (csv.empty()) {
        g.error_msg = "nvidia-smi not available or no NVIDIA GPU detected";
        return g;
    }

    // Take first line (first GPU for multi-GPU)
    auto nl = csv.find('\n');
    if (nl != std::string::npos) csv = csv.substr(0, nl);

    auto f = split_csv(csv);
    if (f.size() < 12) {
        g.error_msg = "Unexpected nvidia-smi output (" + std::to_string(f.size()) + " fields)";
        return g;
    }

    g.name           = f[0];
    g.driver_version = f[1];
    g.temperature_c  = safe_int(f[2]);
    g.gpu_util_pct   = safe_int(f[3]);
    g.mem_util_pct   = safe_int(f[4]);
    g.mem_used_mb    = safe_double(f[5]);
    g.mem_total_mb   = safe_double(f[6]);
    g.power_draw_w   = safe_double(f[7]);
    g.power_limit_w  = safe_double(f[8]);
    g.fan_speed_pct  = safe_int(f[9]);
    g.clock_gpu_mhz  = safe_int(f[10]);
    g.clock_mem_mhz  = safe_int(f[11]);
    g.valid          = true;
    return g;
}

std::vector<GpuProcess> query_gpu_processes() {
    std::vector<GpuProcess> procs;

    auto parse_lines = [&](const std::vector<std::string>& lines) {
        for (auto& line : lines) {
            // Skip error lines from nvidia-smi
            if (line.find("ERROR") != std::string::npos) continue;
            auto f = split_csv(line);
            if (f.size() >= 3) {
                GpuProcess p;
                p.pid    = safe_int(f[0]);
                p.name   = basename(f[1]);
                p.mem_mb = safe_double(f[2]);  // [N/A] → 0
                if (p.pid <= 0) continue;
                // Skip permission-denied entries
                if (p.name.find("Insufficient") != std::string::npos) continue;
                // Truncate long names
                if (p.name.size() > 38) p.name = p.name.substr(0, 35) + "...";
                // Deduplicate by PID
                bool dup = false;
                for (auto& e : procs) { if (e.pid == p.pid) { dup = true; break; } }
                if (!dup) procs.push_back(p);
            }
        }
    };

    // Compute apps
    auto compute = shell_lines(
        "nvidia-smi --query-compute-apps=pid,process_name,used_gpu_memory"
        " --format=csv,noheader,nounits 2>nul");
    if (compute.empty()) {
        compute = shell_lines(
            "nvidia-smi --query-compute-apps=pid,process_name,used_gpu_memory"
            " --format=csv,noheader,nounits 2>/dev/null");
    }
    parse_lines(compute);

    // Graphics apps (newer drivers)
    auto graphics = shell_lines(
        "nvidia-smi --query-graphics-apps=pid,process_name,used_gpu_memory"
        " --format=csv,noheader,nounits 2>nul");
    if (graphics.empty()) {
        graphics = shell_lines(
            "nvidia-smi --query-graphics-apps=pid,process_name,used_gpu_memory"
            " --format=csv,noheader,nounits 2>/dev/null");
    }
    parse_lines(graphics);

    return procs;
}

SubsystemSnapshot query_subsystem() {
    SubsystemSnapshot s;

    fs::path root = fs::current_path() / ".vsepr-session";
    s.session_path   = root.string();
    s.session_active = fs::exists(root);
    s.render_status  = "idle";

#ifdef VSEPR_HAS_CUDA
    s.cuda_compiled = true;
#else
    s.cuda_compiled = false;
#endif

    std::error_code ec;

    // Cache stats
    fs::path cache = root / "cache";
    if (fs::exists(cache, ec)) {
        for (auto& entry : fs::directory_iterator(cache, ec)) {
            if (entry.is_regular_file(ec)) {
                s.cache_files++;
                s.cache_size_mb += static_cast<double>(entry.file_size(ec)) / (1024.0 * 1024.0);
            }
        }
    }

    // Export pending
    fs::path exports = root / "export";
    if (fs::exists(exports, ec)) {
        for (auto& entry : fs::directory_iterator(exports, ec)) {
            if (entry.is_regular_file(ec))
                s.export_pending++;
        }
    }

    // Last log line
    fs::path logpath = root / "session.log";
    if (fs::exists(logpath, ec)) {
        std::ifstream f(logpath);
        std::string line, last;
        while (std::getline(f, line)) {
            if (!line.empty()) last = line;
        }
        if (last.size() > 56) last = last.substr(0, 53) + "...";
        s.log_tail = last;
    }

    // Tracker state (probed from .vsepr-session/tracker.csv or tracker.state)
    fs::path tracker_csv = root / "tracker.csv";
    fs::path tracker_state = root / "tracker.state";
    if (fs::exists(tracker_csv, ec) || fs::exists(tracker_state, ec)) {
        s.tracker_active = true;

        // Try to read the state file (lightweight metrics dump)
        fs::path state_file = fs::exists(tracker_state, ec) ? tracker_state : tracker_csv;
        std::ifstream tf(state_file);
        std::string tline;
        int line_count = 0;
        std::unordered_set<std::string> elements;
        int active = 0, total = 0, children = 0;
        uint64_t max_id = 0;

        while (std::getline(tf, tline)) {
            line_count++;
            if (line_count == 1) continue; // skip header

            // CSV: persistent_id,current_index,Z,symbol,...,active,...
            auto fields = split_csv(tline);
            if (fields.size() >= 9) {
                total++;
                uint64_t pid = static_cast<uint64_t>(safe_double(fields[0]));
                if (pid > max_id) max_id = pid;
                elements.insert(fields[3]); // symbol
                if (fields[8] == "true") active++;
                if (fields.size() >= 5 && fields[4] != "0") children++;
            }
        }

        s.tracker_particles  = total;
        s.tracker_active_p   = active;
        s.tracker_elements   = static_cast<int>(elements.size());
        s.tracker_children   = children;
        s.tracker_highest_id = max_id;

        // Also count event log if present
        fs::path events_log = root / "tracker.events.log";
        if (fs::exists(events_log, ec)) {
            std::ifstream ef(events_log);
            std::string eline;
            int ev_count = 0;
            while (std::getline(ef, eline)) { if (!eline.empty()) ev_count++; }
            s.tracker_events = ev_count;
        }

        std::ostringstream ts;
        ts << active << "/" << total << " active"
           << " | " << elements.size() << " elements"
           << " | " << children << " children";
        s.tracker_summary = ts.str();
    }

    return s;
}

// ============================================================================
// Bar rendering + color helpers
// ============================================================================

std::string render_bar(int percent, int width = 20) {
    if (percent < 0)   percent = 0;
    if (percent > 100)  percent = 100;
    int filled = (percent * width + 50) / 100;
    std::string bar;
    for (int i = 0; i < width; i++) {
        // U+2588 FULL BLOCK = \xe2\x96\x88
        // U+2591 LIGHT SHADE = \xe2\x96\x91
        bar += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";
    }
    return bar;
}

const char* temp_color(int c) {
    if (c < 65) return COL_GREEN;
    if (c < 80) return COL_YELLOW;
    return COL_RED;
}

const char* util_color(int pct) {
    if (pct < 50) return COL_GREEN;
    if (pct < 80) return COL_YELLOW;
    return COL_RED;
}

const char* mem_color(double used, double total) {
    if (total <= 0) return COL_CYAN;
    double pct = (used / total) * 100.0;
    if (pct < 75) return COL_GREEN;
    if (pct < 90) return COL_YELLOW;
    return COL_RED;
}

const char* power_color(double draw, double limit) {
    if (limit <= 0) return COL_CYAN;
    double pct = (draw / limit) * 100.0;
    if (pct < 65) return COL_GREEN;
    if (pct < 85) return COL_YELLOW;
    return COL_RED;
}

const char* fan_color(int pct) {
    if (pct < 50) return COL_GREEN;
    if (pct < 75) return COL_YELLOW;
    return COL_RED;
}

// ============================================================================
// Drawing functions
// ============================================================================

void draw_separator() {
    std::cout << COL_CYAN;
    for (int i = 0; i < 64; i++) std::cout << "\xe2\x94\x80";  // ─ × 64
    std::cout << "\n" << COL_RESET;
}

constexpr int KW = 16;  // key width

void draw_header() {
    std::cout
        << COL_CYAN << COL_BOLD
        << "================================================================\n"
        << " VSEPR-SIM :: NVIDIA Subsystem Monitor              [q] Exit\n"
        << "================================================================\n"
        << COL_RESET << "\n";
}

void draw_gpu_panel(const GpuSnapshot& g) {
    if (!g.valid) {
        std::cout << COL_RED << " " << g.error_msg << COL_RESET << "\n\n";
        return;
    }

    // Device + Driver
    std::cout << COL_CYAN << " " << std::left << std::setw(KW) << "Device"
              << COL_RESET << ": " << g.name << "\n"
              << COL_CYAN << " " << std::setw(KW) << "Driver"
              << COL_RESET << ": " << g.driver_version << "\n\n";

    // Temperature
    int temp_pct = std::min(std::max(g.temperature_c, 0), 100);
    const char* tc = temp_color(g.temperature_c);
    std::cout << COL_CYAN << " " << std::setw(KW) << "Temperature" << COL_RESET
              << ": " << tc << render_bar(temp_pct) << "  " << g.temperature_c
              << "\xc2\xb0" << "C" << COL_RESET << "\n";

    // GPU Load
    const char* gc = util_color(g.gpu_util_pct);
    std::cout << COL_CYAN << " " << std::setw(KW) << "GPU Load" << COL_RESET
              << ": " << gc << render_bar(g.gpu_util_pct) << "  " << g.gpu_util_pct
              << "%" << COL_RESET << "\n";

    // VRAM
    int vram_pct = (g.mem_total_mb > 0)
        ? static_cast<int>((g.mem_used_mb / g.mem_total_mb) * 100.0) : 0;
    const char* mc = mem_color(g.mem_used_mb, g.mem_total_mb);
    char vbuf[64];
    std::snprintf(vbuf, sizeof(vbuf), "%.1f / %.1f GB  (%d%%)",
                  g.mem_used_mb / 1024.0, g.mem_total_mb / 1024.0, vram_pct);
    std::cout << COL_CYAN << " " << std::setw(KW) << "VRAM" << COL_RESET
              << ": " << mc << render_bar(vram_pct) << "  " << vbuf
              << COL_RESET << "\n";

    // Power
    int pwr_pct = (g.power_limit_w > 0)
        ? static_cast<int>((g.power_draw_w / g.power_limit_w) * 100.0) : 0;
    const char* pc = power_color(g.power_draw_w, g.power_limit_w);
    char pbuf[64];
    std::snprintf(pbuf, sizeof(pbuf), "%.0f / %.0f W", g.power_draw_w, g.power_limit_w);
    std::cout << COL_CYAN << " " << std::setw(KW) << "Power" << COL_RESET
              << ": " << pc << render_bar(pwr_pct) << "  " << pbuf
              << COL_RESET << "\n";

    // Fan
    const char* fc = fan_color(g.fan_speed_pct);
    std::cout << COL_CYAN << " " << std::setw(KW) << "Fan" << COL_RESET
              << ": " << fc << render_bar(g.fan_speed_pct) << "  "
              << g.fan_speed_pct << "%" << COL_RESET << "\n";

    // Clocks
    std::cout << COL_CYAN << " " << std::setw(KW) << "Clocks" << COL_RESET
              << ": " << g.clock_gpu_mhz << " MHz (GPU) / "
              << g.clock_mem_mhz << " MHz (Mem)\n";

    std::cout << "\n";
}

void draw_process_panel(const std::vector<GpuProcess>& procs) {
    draw_separator();

    std::cout << COL_CYAN << COL_BOLD << " GPU Processes";
    if (!procs.empty())
        std::cout << " (" << procs.size() << ")";
    std::cout << COL_RESET << "\n";

    draw_separator();

    if (procs.empty()) {
        std::cout << COL_GRAY << "  (none)\n" << COL_RESET;
    } else {
        // Header row
        std::cout << COL_DIM << "  " << std::left
                  << std::setw(10) << "PID"
                  << std::setw(40) << "Process"
                  << "VRAM\n" << COL_RESET;
        for (auto& p : procs) {
            std::cout << "  " << std::left
                      << std::setw(10) << p.pid
                      << std::setw(40) << p.name;
            if (p.mem_mb > 0) {
                char mbuf[32];
                std::snprintf(mbuf, sizeof(mbuf), "%.0f MiB", p.mem_mb);
                std::cout << mbuf;
            } else {
                std::cout << COL_GRAY << "N/A" << COL_RESET;
            }
            std::cout << "\n";
        }
    }
    std::cout << "\n";
}

void draw_subsystem_panel(const SubsystemSnapshot& s) {
    draw_separator();

    std::cout << COL_CYAN << COL_BOLD << " VSEPR Subsystem"
              << COL_RESET << "\n";

    draw_separator();

    std::cout << COL_CYAN << "  " << std::left << std::setw(KW) << "Session"
              << COL_RESET << ": "
              << (s.session_active ? COL_GREEN : COL_YELLOW)
              << (s.session_active ? "active" : "none")
              << COL_RESET << "\n";

    if (s.session_active) {
        std::cout << COL_CYAN << "  " << std::setw(KW) << "Path"
                  << COL_RESET << ": " << COL_DIM << s.session_path
                  << COL_RESET << "\n";
    }

    char cbuf[64];
    std::snprintf(cbuf, sizeof(cbuf), "%d files, %.1f MB", s.cache_files, s.cache_size_mb);
    std::cout << COL_CYAN << "  " << std::setw(KW) << "Cache"
              << COL_RESET << ": " << cbuf << "\n";

    std::cout << COL_CYAN << "  " << std::setw(KW) << "Export"
              << COL_RESET << ": " << s.export_pending << " pending\n";

    std::cout << COL_CYAN << "  " << std::setw(KW) << "Render Q"
              << COL_RESET << ": " << s.render_status << "\n";

    std::cout << COL_CYAN << "  " << std::setw(KW) << "CUDA"
              << COL_RESET << ": "
              << (s.cuda_compiled ? (COL_GREEN + std::string("compiled in"))
                                 : (COL_YELLOW + std::string("not compiled (CPU fallback)")))
              << COL_RESET << "\n";

    if (!s.log_tail.empty()) {
        std::cout << COL_CYAN << "  " << std::setw(KW) << "Last log"
                  << COL_RESET << ": " << COL_DIM << s.log_tail
                  << COL_RESET << "\n";
    }

    // --- Particle Tracker panel ---
    if (s.tracker_active) {
        std::cout << "\n";
        draw_separator();

        std::cout << COL_CYAN << COL_BOLD << " Particle Tracker"
                  << COL_RESET << "\n";

        draw_separator();

        // Active / total
        char pbuf[64];
        std::snprintf(pbuf, sizeof(pbuf), "%d / %d active",
                      s.tracker_active_p, s.tracker_particles);
        const char* p_color = (s.tracker_active_p > 0) ? COL_GREEN : COL_YELLOW;
        std::cout << COL_CYAN << "  " << std::left << std::setw(KW) << "Particles"
                  << COL_RESET << ": " << p_color << pbuf << COL_RESET << "\n";

        // Unique elements
        std::cout << COL_CYAN << "  " << std::setw(KW) << "Elements"
                  << COL_RESET << ": " << s.tracker_elements << " unique Z values\n";

        // Child particles (lineage)
        if (s.tracker_children > 0) {
            std::cout << COL_CYAN << "  " << std::setw(KW) << "Children"
                      << COL_RESET << ": " << s.tracker_children
                      << " (decay/split/transmutation)\n";
        }

        // Events
        if (s.tracker_events > 0) {
            std::cout << COL_CYAN << "  " << std::setw(KW) << "Events"
                      << COL_RESET << ": " << s.tracker_events << " logged\n";
        }

        // Highest ID (monotonic sequence indicator)
        if (s.tracker_highest_id > 0) {
            std::cout << COL_CYAN << "  " << std::setw(KW) << "Highest ID"
                      << COL_RESET << ": " << s.tracker_highest_id << "\n";
        }

        // Summary line
        if (!s.tracker_summary.empty()) {
            std::cout << COL_CYAN << "  " << std::setw(KW) << "Summary"
                      << COL_RESET << ": " << COL_DIM << s.tracker_summary
                      << COL_RESET << "\n";
        }
    }

    std::cout << "\n";
}

void draw_footer() {
    draw_separator();

    std::cout << COL_GRAY
              << " Auto-refresh 2s  |  [q] Exit  |  [r] Refresh"
              << COL_RESET << "\n";
}

// ============================================================================
// Secret help (key '0')
// ============================================================================

void draw_secret_help() {
    std::cout << "\n";
    draw_separator();

    std::cout << COL_MAGENTA << COL_BOLD
              << " VSEPR-SIM :: Secret Keys\n"
              << COL_RESET;

    draw_separator();

    std::cout << "  " << COL_BOLD << "[1]" << COL_RESET
              << "  NVIDIA Subsystem Monitor (live TUI)\n"
              << "  " << COL_BOLD << "[0]" << COL_RESET
              << "  This help\n\n";
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

char check_secret_keys(int timeout_ms) {
    if (!is_terminal()) return '\0';

    setup_terminal();

    char result = '\0';
    int elapsed = 0;
    const int step_ms = 50;

    while (elapsed < timeout_ms) {
        if (has_keypress()) {
            result = read_keypress();
            break;
        }
        sleep_ms(step_ms);
        elapsed += step_ms;
    }

    restore_terminal();
    return result;
}

bool dispatch_secret_key(char key) {
    switch (key) {
        case '1':
            enter_nvidia_tui();
            return true;
        case '0':
            draw_secret_help();
            return true;
        default:
            return false;
    }
}

void enter_nvidia_tui() {
    setup_terminal();
    std::cout << ESC_HIDE_CUR << std::flush;

    bool running = true;
    while (running) {
        auto gpu   = query_gpu();
        auto procs = query_gpu_processes();
        auto sub   = query_subsystem();

        std::cout << ESC_CLEAR;
        draw_header();
        draw_gpu_panel(gpu);
        draw_process_panel(procs);
        draw_subsystem_panel(sub);
        draw_footer();
        std::cout << std::flush;

        // Wait ~2s, polling for input every 100ms
        for (int i = 0; i < 20 && running; i++) {
            if (has_keypress()) {
                char c = read_keypress();
                if (c == 'q' || c == 'Q' || c == 27) {
                    running = false;
                } else if (c == 'r' || c == 'R') {
                    break;  // force immediate refresh
                }
            }
            sleep_ms(100);
        }
    }

    std::cout << ESC_SHOW_CUR << ESC_CLEAR;
    std::cout << COL_GREEN << "[BOOT] Returning to boot sequence...\n"
              << COL_RESET << "\n" << std::flush;

    restore_terminal();
}

} // namespace infra
} // namespace vsepr
