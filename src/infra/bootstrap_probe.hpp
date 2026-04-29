#pragma once
/**
 * bootstrap_probe.hpp
 * -------------------
 * Hardware check and convenience infrastructure initialization for the
 * VSEPR-SIM boot sequence.
 *
 * Namespace: vsepr::infra
 * Internal name: bootstrap_probe
 * User-facing name: VSEPR-MOTD
 *
 * Thresholds:
 *   RAM warning   : free RAM  < 4 GB
 *   RAM fail      : free RAM  < 1.5 GB
 *   Disk warning  : free disk < 10 GB
 *   Disk fail     : free disk < 2 GB
 *   GUI fail      : GUI requested, no display backend
 *   CUDA warning  : CUDA requested but unavailable, CPU fallback possible
 *   CUDA fail     : CUDA required, unavailable
 */

#include <string>
#include <vector>

namespace vsepr {
namespace infra {

// ============================================================================
// Status levels
// ============================================================================

enum class StatusLevel { Pass, Warn, Fail };

inline const char* status_label(StatusLevel s) {
    switch (s) {
        case StatusLevel::Pass: return "PASS";
        case StatusLevel::Warn: return "WARN";
        case StatusLevel::Fail: return "FAIL";
    }
    return "UNKNOWN";
}

// ============================================================================
// Diagnostic entry (one line of [HW] or [INFRA] output)
// ============================================================================

struct DiagEntry {
    std::string key;
    std::string value;
    StatusLevel level = StatusLevel::Pass;
    std::string reason;  // non-empty when level != Pass
};

// ============================================================================
// Hardware report
// ============================================================================

struct HardwareReport {
    int         cpu_cores       = 0;
    double      total_ram_gb    = 0.0;
    double      free_ram_gb     = 0.0;
    double      disk_free_gb    = 0.0;
    std::string gpu_name;           // empty = not detected
    bool        cuda_available  = false;
    bool        gui_capable     = false;

    StatusLevel level           = StatusLevel::Pass;
    std::string fail_reason;

    std::vector<DiagEntry> entries;  // formatted lines
};

// ============================================================================
// Bootstrap (infrastructure) report
// ============================================================================

struct BootstrapReport {
    bool config_loaded      = false;
    bool session_created    = false;
    bool log_ready          = false;
    bool cache_ready        = false;
    bool render_queue_ok    = false;
    bool export_spool_ok    = false;
    bool ipc_channel_ok     = false;

    StatusLevel level       = StatusLevel::Pass;
    std::string fail_reason;

    std::vector<DiagEntry> entries;  // formatted lines
};

// ============================================================================
// Combined MOTD report
// ============================================================================

struct MotdReport {
    HardwareReport   hardware;
    BootstrapReport  bootstrap;
    StatusLevel      overall = StatusLevel::Pass;
};

// ============================================================================
// Probe API
// ============================================================================

/**
 * Run hardware check.
 * @param need_gui   true if the application will attempt to open a window
 * @param need_cuda  true if CUDA is required (not just preferred)
 */
HardwareReport run_hardware_check(bool need_gui, bool need_cuda);

/**
 * Initialize convenience infrastructure (session dir, log, cache, etc.).
 * Creates default config if none exists.  Never crashes.
 */
BootstrapReport initialize_infrastructure();

/**
 * Combine hardware + bootstrap into a final MotdReport with overall status.
 */
MotdReport combine_reports(HardwareReport hw, BootstrapReport boot);

} // namespace infra
} // namespace vsepr
