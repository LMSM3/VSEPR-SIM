/**
 * bootstrap_probe.cpp
 * -------------------
 * Implementation of hardware check and infrastructure initialization.
 * Cross-platform: Windows (primary), Linux, macOS fallback.
 */

#include "infra/bootstrap_probe.hpp"

#include <thread>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <algorithm>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__linux__)
#  include <sys/sysinfo.h>
#  include <sys/statvfs.h>
#  include <unistd.h>
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#  include <sys/statvfs.h>
#  include <mach/mach.h>
#endif

namespace fs = std::filesystem;

namespace vsepr {
namespace infra {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

constexpr double GB = 1024.0 * 1024.0 * 1024.0;

// Thresholds (from Read_ME.bin specification)
constexpr double RAM_WARN_GB  = 4.0;
constexpr double RAM_FAIL_GB  = 1.5;
constexpr double DISK_WARN_GB = 10.0;
constexpr double DISK_FAIL_GB = 2.0;

double total_ram_gb() {
#ifdef _WIN32
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return static_cast<double>(ms.ullTotalPhys) / GB;
#elif defined(__linux__)
    struct sysinfo si{};
    if (sysinfo(&si) == 0)
        return static_cast<double>(si.totalram) * si.mem_unit / GB;
#elif defined(__APPLE__)
    int64_t mem = 0;
    size_t  len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0)
        return static_cast<double>(mem) / GB;
#endif
    return 0.0;
}

double free_ram_gb() {
#ifdef _WIN32
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return static_cast<double>(ms.ullAvailPhys) / GB;
#elif defined(__linux__)
    struct sysinfo si{};
    if (sysinfo(&si) == 0)
        return static_cast<double>(si.freeram) * si.mem_unit / GB;
#elif defined(__APPLE__)
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm{};
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS) {
        int64_t page = sysconf(_SC_PAGESIZE);
        return static_cast<double>(vm.free_count * page) / GB;
    }
#endif
    return 0.0;
}

double disk_free_gb() {
    std::error_code ec;
    auto info = fs::space(fs::current_path(), ec);
    if (!ec)
        return static_cast<double>(info.available) / GB;
    return 0.0;
}

// ---- Shell execution helper ------------------------------------------------
// Runs a command via popen, captures first line of stdout, trims whitespace.
// Returns empty string on any failure.  Never throws.
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
    // Trim trailing whitespace / newlines
    while (!result.empty() &&
           (result.back() == '\n' || result.back() == '\r' ||
            result.back() == ' '  || result.back() == '\t')) {
        result.pop_back();
    }
    return result;
}

// ---- GPU detection ---------------------------------------------------------
// Strategy (ordered by specificity):
//   1. nvidia-smi  — fast, reliable when NVIDIA driver is installed
//   2. Windows: powershell Get-CimInstance (replaces deprecated wmic)
//      Fallback: wmic path win32_VideoController
//   3. Linux: lspci grep VGA
//   4. macOS: system_profiler SPDisplaysDataType
//   5. Environment variable heuristic (last resort)
//
// Each attempt is guarded — if the command is missing or fails, we move on.

std::string detect_gpu() {
    std::string name;

    // --- nvidia-smi (all platforms where NVIDIA driver is present) ---
    name = shell_exec("nvidia-smi --query-gpu=name --format=csv,noheader,nounits 2>nul");
    if (name.empty())
        name = shell_exec("nvidia-smi --query-gpu=name --format=csv,noheader,nounits 2>/dev/null");
    if (!name.empty()) {
        // nvidia-smi may return multiple lines for multi-GPU; take first
        auto nl = name.find('\n');
        if (nl != std::string::npos) name = name.substr(0, nl);
        return name;
    }

#ifdef _WIN32
    // --- PowerShell Get-CimInstance (modern Windows) ---
    name = shell_exec(
        "powershell -NoProfile -Command "
        "\"(Get-CimInstance Win32_VideoController | Select-Object -First 1).Name\" 2>nul");
    if (!name.empty()) return name;

    // --- wmic fallback (deprecated but still present on older builds) ---
    name = shell_exec("wmic path win32_VideoController get Name /value 2>nul");
    if (!name.empty()) {
        // wmic output is "Name=NVIDIA GeForce RTX 4080\r\n" — extract value
        auto eq = name.find('=');
        if (eq != std::string::npos) {
            name = name.substr(eq + 1);
            // Trim again after extraction
            while (!name.empty() &&
                   (name.back() == '\n' || name.back() == '\r' ||
                    name.back() == ' '  || name.back() == '\t')) {
                name.pop_back();
            }
        }
        if (!name.empty()) return name;
    }

#elif defined(__linux__)
    // --- lspci VGA (Linux) ---
    name = shell_exec("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1");
    if (!name.empty()) {
        // lspci output: "01:00.0 VGA compatible controller: NVIDIA Corporation ..."
        // Strip the PCI address prefix up to the colon-space after the class
        auto colon = name.find(": ");
        if (colon != std::string::npos) {
            auto second = name.find(": ", colon + 2);
            if (second != std::string::npos)
                name = name.substr(second + 2);
            else
                name = name.substr(colon + 2);
        }
        if (!name.empty()) return name;
    }

#elif defined(__APPLE__)
    // --- system_profiler (macOS) ---
    name = shell_exec(
        "system_profiler SPDisplaysDataType 2>/dev/null "
        "| grep 'Chipset Model' | head -1 | sed 's/.*: //'");
    if (!name.empty()) return name;
#endif

    // --- Last resort: environment variable heuristic ---
    const char* nv = std::getenv("NVIDIA_VISIBLE_DEVICES");
    if (nv) return "NVIDIA GPU (detected via env)";

    return "";  // genuinely not detected
}

bool cuda_available() {
#ifdef VSEPR_HAS_CUDA
    return true;
#else
    return false;
#endif
}

bool gui_capable() {
#ifdef _WIN32
    return true;  // Windows always has a display backend
#elif defined(__APPLE__)
    return true;  // macOS always has a display backend
#else
    return std::getenv("DISPLAY") != nullptr ||
           std::getenv("WAYLAND_DISPLAY") != nullptr;
#endif
}

// Session root: <cwd>/.vsepr-session  (lightweight, local, disposable)
fs::path session_root() {
    return fs::current_path() / ".vsepr-session";
}

} // anonymous namespace

// ============================================================================
// run_hardware_check
// ============================================================================

HardwareReport run_hardware_check(bool need_gui, bool need_cuda) {
    HardwareReport r;

    r.cpu_cores    = static_cast<int>(std::thread::hardware_concurrency());
    r.total_ram_gb = total_ram_gb();
    r.free_ram_gb  = free_ram_gb();
    r.disk_free_gb = disk_free_gb();
    r.gpu_name     = detect_gpu();
    r.cuda_available = cuda_available();
    r.gui_capable  = gui_capable();

    // Build formatted entries
    auto push = [&](const std::string& k, const std::string& v,
                    StatusLevel lv = StatusLevel::Pass,
                    const std::string& reason = "") {
        r.entries.push_back({k, v, lv, reason});
    };

    auto fmt_gb = [](double v) -> std::string {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f GB", v);
        return buf;
    };

    push("CPU cores",    std::to_string(r.cpu_cores));
    push("Total RAM",    fmt_gb(r.total_ram_gb));

    // Free RAM thresholds
    if (r.free_ram_gb < RAM_FAIL_GB) {
        push("Free RAM", fmt_gb(r.free_ram_gb), StatusLevel::Fail,
             "Free RAM below " + fmt_gb(RAM_FAIL_GB) + " minimum");
        r.level = StatusLevel::Fail;
        r.fail_reason = "Insufficient free RAM";
    } else if (r.free_ram_gb < RAM_WARN_GB) {
        push("Free RAM", fmt_gb(r.free_ram_gb), StatusLevel::Warn,
             "Free RAM below " + fmt_gb(RAM_WARN_GB) + " recommended");
        if (r.level < StatusLevel::Warn) r.level = StatusLevel::Warn;
    } else {
        push("Free RAM", fmt_gb(r.free_ram_gb));
    }

    // Disk thresholds
    if (r.disk_free_gb < DISK_FAIL_GB) {
        push("Disk free", fmt_gb(r.disk_free_gb), StatusLevel::Fail,
             "Disk free below " + fmt_gb(DISK_FAIL_GB) + " minimum");
        r.level = StatusLevel::Fail;
        r.fail_reason = "Insufficient disk space";
    } else if (r.disk_free_gb < DISK_WARN_GB) {
        push("Disk free", fmt_gb(r.disk_free_gb), StatusLevel::Warn,
             "Disk free below " + fmt_gb(DISK_WARN_GB) + " recommended");
        if (r.level < StatusLevel::Warn) r.level = StatusLevel::Warn;
    } else {
        push("Disk free", fmt_gb(r.disk_free_gb));
    }

    // GPU
    if (r.gpu_name.empty()) {
        push("GPU detected", "no", StatusLevel::Warn,
             "Falling back to CPU execution path");
        if (r.level < StatusLevel::Warn) r.level = StatusLevel::Warn;
    } else {
        push("GPU detected", r.gpu_name);
    }

    // CUDA
    push("CUDA available", r.cuda_available ? "yes" : "no",
         (!r.cuda_available && need_cuda) ? StatusLevel::Fail :
         (!r.cuda_available)              ? StatusLevel::Warn : StatusLevel::Pass,
         (!r.cuda_available && need_cuda) ? "CUDA required but unavailable" :
         (!r.cuda_available)              ? "Falling back to CPU execution path" : "");

    if (!r.cuda_available && need_cuda) {
        r.level = StatusLevel::Fail;
        r.fail_reason = "CUDA required but unavailable";
    } else if (!r.cuda_available && r.level < StatusLevel::Warn) {
        r.level = StatusLevel::Warn;
    }

    // GUI
    push("GUI capability", r.gui_capable ? "yes" : "no",
         (!r.gui_capable && need_gui) ? StatusLevel::Fail :
         (!r.gui_capable)             ? StatusLevel::Warn : StatusLevel::Pass,
         (!r.gui_capable && need_gui) ? "GUI requested but no display backend" : "");

    if (!r.gui_capable && need_gui) {
        r.level = StatusLevel::Fail;
        r.fail_reason = "GUI requested but no display backend available";
    }

    // Final status entry
    push("Status", status_label(r.level), r.level, r.fail_reason);

    return r;
}

// ============================================================================
// initialize_infrastructure
// ============================================================================

BootstrapReport initialize_infrastructure() {
    BootstrapReport r;

    auto push = [&](const std::string& k, const std::string& v,
                    StatusLevel lv = StatusLevel::Pass,
                    const std::string& reason = "") {
        r.entries.push_back({k, v, lv, reason});
    };

    // --- Config ---
    fs::path cfg = session_root() / "config.ini";
    if (fs::exists(cfg)) {
        r.config_loaded = true;
        push("Config loaded", "yes");
    } else {
        // Generate default config — never crash
        std::error_code ec;
        fs::create_directories(session_root(), ec);
        if (!ec) {
            std::ofstream f(cfg);
            if (f.is_open()) {
                f << "# VSEPR-SIM default config (auto-generated)\n"
                  << "[general]\n"
                  << "mode = desktop\n";
                f.close();
                r.config_loaded = true;
                push("Config loaded", "no (generated default)", StatusLevel::Warn,
                     "generated default config");
                if (r.level < StatusLevel::Warn) r.level = StatusLevel::Warn;
            } else {
                push("Config loaded", "no", StatusLevel::Warn,
                     "could not write default config");
                if (r.level < StatusLevel::Warn) r.level = StatusLevel::Warn;
            }
        } else {
            push("Config loaded", "no", StatusLevel::Warn,
                 "could not create session directory for config");
            if (r.level < StatusLevel::Warn) r.level = StatusLevel::Warn;
        }
    }

    // --- Session directory ---
    {
        std::error_code ec;
        fs::create_directories(session_root(), ec);
        r.session_created = !ec;
        push("Session directory", r.session_created ? "created" : "failed",
             r.session_created ? StatusLevel::Pass : StatusLevel::Warn,
             r.session_created ? "" : "could not create session directory");
        if (!r.session_created && r.level < StatusLevel::Warn)
            r.level = StatusLevel::Warn;
    }

    // --- Log file ---
    {
        fs::path logpath = session_root() / "session.log";
        std::ofstream f(logpath, std::ios::app);
        r.log_ready = f.is_open();
        push("Log file", r.log_ready ? "ready" : "failed",
             r.log_ready ? StatusLevel::Pass : StatusLevel::Warn);
        if (!r.log_ready && r.level < StatusLevel::Warn)
            r.level = StatusLevel::Warn;
    }

    // --- Cache path ---
    {
        std::error_code ec;
        fs::create_directories(session_root() / "cache", ec);
        r.cache_ready = !ec;
        push("Cache path", r.cache_ready ? "ready" : "failed",
             r.cache_ready ? StatusLevel::Pass : StatusLevel::Warn);
        if (!r.cache_ready && r.level < StatusLevel::Warn)
            r.level = StatusLevel::Warn;
    }

    // --- Render queue (logical init, no GPU needed) ---
    r.render_queue_ok = true;
    push("Render queue", "initialized");

    // --- Export spool ---
    {
        std::error_code ec;
        fs::create_directories(session_root() / "export", ec);
        r.export_spool_ok = !ec;
        push("Export spool", r.export_spool_ok ? "initialized" : "failed",
             r.export_spool_ok ? StatusLevel::Pass : StatusLevel::Warn);
        if (!r.export_spool_ok && r.level < StatusLevel::Warn)
            r.level = StatusLevel::Warn;
    }

    // --- IPC channel (standby — placeholder for future IPC) ---
    r.ipc_channel_ok = true;
    push("IPC channel", "standby");

    // Final status entry
    push("Status", status_label(r.level), r.level, r.fail_reason);

    return r;
}

// ============================================================================
// combine_reports
// ============================================================================

MotdReport combine_reports(HardwareReport hw, BootstrapReport boot) {
    MotdReport m;
    m.hardware  = std::move(hw);
    m.bootstrap = std::move(boot);

    if (m.hardware.level == StatusLevel::Fail ||
        m.bootstrap.level == StatusLevel::Fail) {
        m.overall = StatusLevel::Fail;
    } else if (m.hardware.level == StatusLevel::Warn ||
               m.bootstrap.level == StatusLevel::Warn) {
        m.overall = StatusLevel::Warn;
    } else {
        m.overall = StatusLevel::Pass;
    }

    return m;
}

} // namespace infra
} // namespace vsepr
