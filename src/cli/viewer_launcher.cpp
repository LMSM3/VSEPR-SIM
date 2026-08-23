/**
 * viewer_launcher.cpp
 * ====================
 * VSEPR-SIM  |  WO-85Z  |  Viewer Launcher
 *
 * Implements ViewerLauncher for the supported fixed-timestep live viewer.
 */

#include "cli/viewer_launcher.hpp"
#include "vis/supported_viewer.hpp"
#include "vis/uless_indicator.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <shellapi.h>
#else
#  include <unistd.h>
#endif

namespace vsepr {
namespace cli {

namespace fs = std::filesystem;

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// Quote a path for use in a shell command string.
static std::string quoted(const std::string& s) {
    return "\"" + s + "\"";
}

// Extract the --artifact argument from a viewer argument string.
static std::string parse_artifact_path(const std::string& args_str) {
    const std::string needle = "--artifact ";
    auto pos = args_str.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    // Skip leading quote.
    if (pos < args_str.size() && args_str[pos] == '"') ++pos;
    auto end = args_str.find('"', pos);
    if (end == std::string::npos) end = args_str.size();
    return args_str.substr(pos, end - pos);
}

#ifdef _WIN32
static std::string find_chrome_binary()
{
    // Common Chrome locations on Windows.
    const char* localappdata = std::getenv("LOCALAPPDATA");
    const char* programfiles = std::getenv("PROGRAMFILES");
    const char* programfiles_x86 = std::getenv("PROGRAMFILES(X86)");
    std::vector<std::string> candidates;
    if (localappdata) {
        candidates.push_back(std::string(localappdata) + R"(\Google\Chrome\Application\chrome.exe)");
    }
    if (programfiles) {
        candidates.push_back(std::string(programfiles) + R"(\Google\Chrome\Application\chrome.exe)");
    }
    if (programfiles_x86) {
        candidates.push_back(std::string(programfiles_x86) + R"(\Google\Chrome\Application\chrome.exe)");
    }
    candidates.emplace_back("chrome.exe");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(c, ec) && !ec) return c;
    }
    return "chrome.exe";
}

static void shell_open_file(const std::string& path)
{
    const std::string chrome = find_chrome_binary();
    std::cout << "[view] Chrome fallback: " << path << "\n";
    const std::string params = "\"" + path + "\"";
    SHELLEXECUTEINFOA sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "open";
    sei.lpFile = chrome.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow  = SW_SHOWNORMAL;
    if (!ShellExecuteExA(&sei)) {
        std::cerr << "[view] Failed to open artifact in Chrome"
                  << " (error " << GetLastError() << ")\n";
    }
}
#endif

static std::string find_repo_root_from_binary()
{
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, buf, MAX_PATH) == 0) return "";
    std::error_code ec;
    auto p = std::filesystem::path(buf).parent_path(); // build/
    auto root = p.parent_path();                       // repo root
    if (std::filesystem::is_regular_file(root / "CMakeLists.txt", ec)) {
        return root.string();
    }
#endif
    return "";
}

static std::string png_to_pdf(const std::string& png_path)
{
    std::error_code ec;
    const fs::path pdf_path = fs::path(png_path).replace_extension(".pdf");
    if (fs::is_regular_file(pdf_path, ec) && !ec) {
        return pdf_path.string();
    }
    const std::string repo = find_repo_root_from_binary();
    if (repo.empty()) return "";
    fs::path helper = fs::path(repo) / "tools" / "vsepr_png_to_pdf.py";
    if (!fs::is_regular_file(helper, ec) || ec) return "";
    const std::string cmd = "python \"" + helper.string() + "\" \"" + png_path + "\"";
    int rc = std::system(cmd.c_str());
    if (rc != 0) return "";
    if (fs::is_regular_file(pdf_path, ec) && !ec) return pdf_path.string();
    return "";
}

static bool try_visual_fallback(const std::string& args_str)
{
#ifndef _WIN32
    // Linux/macOS fallback: keep the existing xr/open behavior; not implemented here.
    (void)args_str;
    return false;
#else
    const std::string artifact = parse_artifact_path(args_str);
    if (artifact.empty()) return false;
    std::error_code ec;
    if (!fs::is_regular_file(artifact, ec) || ec) return false;
    const std::string ext = fs::path(artifact).extension().string();
    if (ext == ".pdf") {
        shell_open_file(artifact);
        return true;
    }
    if (ext == ".png") {
        // Try quick embedded PDF conversion; if it fails, open PNG directly.
        std::string pdf = png_to_pdf(artifact);
        if (!pdf.empty()) {
            shell_open_file(pdf);
        } else {
            shell_open_file(artifact);
        }
        return true;
    }
    // Not a static image/PDF we can rescue.
    return false;
#endif
}

// Spawn a process detached from the current terminal.
static void spawn_detached(const std::string& viewer_bin,
                           const std::string& args_str)
{
    // Fail cleanly if the viewer binary is not present (e.g. BUILD_VIS=OFF).
    // Avoids the Windows "cannot find ..." dialog when vsepr-view.exe is absent.
    std::error_code ec;
    if (!std::filesystem::is_regular_file(viewer_bin, ec) || ec) {
        std::cerr << "[view] Viewer binary not found: " << viewer_bin
                  << " — trying static Chrome/PDF fallback\n";
        if (try_visual_fallback(args_str)) {
            return;
        }
        std::cerr << "[view] No static fallback available for this artifact"
                  << " — skipping live viewer launch\n";
        return;
    }

#ifdef _WIN32
    // Keep strings alive across the ShellExecuteExA call.
    const std::string file = viewer_bin;
    const std::string params = args_str;
    SHELLEXECUTEINFOA sei = {};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = "open";
    sei.lpFile       = file.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow        = SW_SHOWNORMAL;
    if (!ShellExecuteExA(&sei)) {
        std::cerr << "[view] Failed to launch live viewer"
                  << " (error " << GetLastError() << ")\n";
    }
#else
    if (fork() == 0) {
        // Build argv from the args string; simpler than full shell-parse.
        std::string cmd = viewer_bin + " " + args_str + " &";
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
#endif
}

} // anonymous namespace

// ============================================================================
// ViewerLauncher::resolve_viewer_binary()
// ============================================================================

std::string ViewerLauncher::resolve_viewer_binary() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    fs::path self(buf);
#else
    fs::path self;
    if (fs::exists("/proc/self/exe")) {
        try { self = fs::read_symlink("/proc/self/exe"); } catch (...) {}
    }
#endif

    auto try_candidate = [&](const fs::path& p) -> std::string {
        if (!p.empty() && vis::is_supported_viewer_candidate(p.string()) &&
            fs::exists(p)) {
            return p.string();
        }
        return {};
    };

    if (!self.empty()) {
        // Prefer the supported live viewer in development and installed layouts.
        // With VTK + Qt3D, the same vsepr-view binary lives in the active build
        // directory or alongside the launcher when installed.
        const fs::path active_build = self.parent_path().parent_path();
        std::string r = try_candidate(
            active_build / std::string(vis::kSupportedViewerExecutable));
        if (!r.empty()) return r;
        r = try_candidate(
            self.parent_path() / std::string(vis::kSupportedViewerExecutable));
        if (!r.empty()) return r;
    }

    return std::string(vis::kSupportedViewerExecutable);
}

// ============================================================================
// ViewerLauncher public interface
// ============================================================================

static std::string build_uless_args(const ViewerLaunchConfig& cfg) {
    if (!cfg.uless_indicator_enabled) return "";
    std::ostringstream oss;
    oss << " --uless-indicator --uless-label " << quoted(cfg.uless_label)
        << " --uless-value " << cfg.uless_value
        << " --uless-max " << cfg.uless_max;
    return oss.str();
}

void ViewerLauncher::launch_static(const std::string& artifact_path) {
    launch_with_config({artifact_path, false, "obs", 0.0, 1.0});
}

void ViewerLauncher::launch_with_config(const ViewerLaunchConfig& config) {
    const std::string viewer = resolve_viewer_binary();
    std::cout << "[view] Opening: " << config.artifact_path << "\n";
    if (config.uless_indicator_enabled) {
        std::cout << "[view] Uless indicator enabled: " << config.uless_label
                  << " " << config.uless_value << " / " << config.uless_max << "\n";
    }
    spawn_detached(viewer, "--artifact " + quoted(config.artifact_path) + build_uless_args(config));
}

void ViewerLauncher::launch_watch(const std::string& artifact_path) {
    const std::string viewer = resolve_viewer_binary();
    std::cout << "[view] Opening (live-watch): " << artifact_path << "\n";
    spawn_detached(viewer, "--artifact " + quoted(artifact_path));
}

// ============================================================================
// Private
// ============================================================================

void ViewerLauncher::launch_process(const std::string& command) {
    // Legacy shim — kept for backwards compat; delegates to system().
    std::system(command.c_str());   // NOLINT
}

} // namespace cli
} // namespace vsepr
