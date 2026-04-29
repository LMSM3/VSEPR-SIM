/**
 * live_server.hpp
 * ---------------
 * Zero-Input HTTP Live Analysis Server.
 *
 * Starts on port 99998 with zero configuration. Immediately begins
 * generating random molecular analysis and streaming results as:
 *   - Server-Sent Events (SSE) at /stream
 *   - JSON snapshots at /snapshot
 *   - HTML dashboard at /
 *   - Server status at /status
 *
 * Uses raw POSIX/Winsock sockets — no external HTTP library needed.
 * The analysis loop runs the report engine and gas module to produce
 * scientifically interesting random output every cycle.
 *
 * Anti-black-box: every analysis has seed, timestamp, and full provenance.
 */

#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <atomic>
#include <vector>

namespace vsepr {
namespace live {

// ============================================================================
// Analysis snapshot (one cycle of random analysis)
// ============================================================================

struct AnalysisSnapshot {
    uint64_t    cycle;
    uint64_t    seed;
    std::string timestamp;       // ISO 8601
    std::string formula;         // random formula analyzed
    double      temperature_K;
    double      pressure_atm;

    // Gas properties
    double ideal_volume_L;
    double rms_speed_ms;
    double mean_free_path_nm;
    double avg_ke_eV;

    // Classification
    std::string gas_type;        // "noble", "diatomic", "polyatomic"
    double      molar_mass_g;

    // Deterministic provenance
    std::string trail_id;

    // Serialization
    std::string to_json() const;
    std::string to_sse_event() const;  // "data: {...}\n\n"
};

// ============================================================================
// Server configuration
// ============================================================================

struct ServerConfig {
    int         port            = 9998;  // TCP port (user intent: 99998, clamped to valid range)
    double      cycle_interval_ms = 3000.0;  // analysis every 3 seconds
    uint64_t    base_seed       = 0;         // 0 = use time-based seed
    int         max_clients     = 32;
    bool        verbose         = false;
    std::string log_path;                    // JSONL capture file (empty = no logging)
};

// ============================================================================
// Live server (blocking run)
// ============================================================================

class LiveServer {
public:
    explicit LiveServer(const ServerConfig& cfg = {});
    ~LiveServer();

    // Blocking: starts the server, runs forever (Ctrl+C to stop)
    int run();

    // Thread-safe: request graceful shutdown
    void request_shutdown();

    // Get latest snapshot (thread-safe copy)
    AnalysisSnapshot latest_snapshot() const;

    // Get server uptime in seconds
    double uptime_seconds() const;

    // Get total cycles completed
    uint64_t total_cycles() const;

private:
    ServerConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> cycle_count_{0};

    // Platform socket handle
    int server_fd_ = -1;

    // Latest analysis (guarded by atomic flag)
    mutable std::atomic<bool> snapshot_lock_{false};
    AnalysisSnapshot latest_;

    // Timing
    double start_time_ = 0.0;

    // Internal methods
    AnalysisSnapshot generate_analysis(uint64_t cycle, uint64_t seed);
    std::string build_dashboard_html() const;
    std::string build_status_json() const;
    void handle_request(int client_fd);
    bool init_socket();
    void cleanup_socket();
};

// ============================================================================
// CLI entry: vsepr serve [--port P] [--interval MS] [--seed S]
// ============================================================================

int serve_dispatch(int argc, char** argv);

} // namespace live
} // namespace vsepr
