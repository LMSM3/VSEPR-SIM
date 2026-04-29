/**
 * live_server.cpp
 * ---------------
 * Zero-input HTTP live analysis server implementation.
 *
 * Uses Winsock2 on Windows, POSIX sockets on Unix.
 * Streams random molecular analysis as SSE + JSON + HTML dashboard.
 */

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   using socket_t = SOCKET;
#  define CLOSE_SOCKET closesocket
#  define SOCKET_INVALID INVALID_SOCKET
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <arpa/inet.h>
   using socket_t = int;
#  define CLOSE_SOCKET close
#  define SOCKET_INVALID (-1)
#endif

#include "core/live_server.hpp"
#include "core/gas_module.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>
#include <cstring>
#include <csignal>
#include <algorithm>
#include <fstream>

namespace vsepr {
namespace live {

// ============================================================================
// Global shutdown signal
// ============================================================================

static std::atomic<bool> g_shutdown_requested{false};

#ifdef _WIN32
static BOOL WINAPI console_handler(DWORD) {
    g_shutdown_requested.store(true);
    return TRUE;
}
#else
static void signal_handler(int) {
    g_shutdown_requested.store(true);
}
#endif

// ============================================================================
// AnalysisSnapshot serialization
// ============================================================================

std::string AnalysisSnapshot::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << "{"
       << "\"cycle\":" << cycle << ","
       << "\"seed\":" << seed << ","
       << "\"timestamp\":\"" << timestamp << "\","
       << "\"formula\":\"" << formula << "\","
       << "\"temperature_K\":" << temperature_K << ","
       << "\"pressure_atm\":" << pressure_atm << ","
       << "\"ideal_volume_L\":" << ideal_volume_L << ","
       << "\"rms_speed_ms\":" << rms_speed_ms << ","
       << "\"mean_free_path_nm\":" << mean_free_path_nm << ","
       << "\"avg_ke_eV\":" << std::setprecision(8) << avg_ke_eV << ","
       << "\"gas_type\":\"" << gas_type << "\","
       << "\"molar_mass_g\":" << std::setprecision(3) << molar_mass_g << ","
       << "\"trail_id\":\"" << trail_id << "\""
       << "}";
    return ss.str();
}

std::string AnalysisSnapshot::to_sse_event() const {
    return "data: " + to_json() + "\n\n";
}

// ============================================================================
// Helpers
// ============================================================================

static std::string now_iso() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

static double now_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ============================================================================
// LiveServer implementation
// ============================================================================

LiveServer::LiveServer(const ServerConfig& cfg) : config_(cfg) {}

LiveServer::~LiveServer() {
    cleanup_socket();
}

bool LiveServer::init_socket() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }
#endif

    server_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd_ == static_cast<int>(SOCKET_INVALID)) {
        std::cerr << "Socket creation failed\n";
        return false;
    }

    // Allow port reuse
    int opt = 1;
#ifdef _WIN32
    setsockopt(static_cast<socket_t>(server_fd_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(config_.port));

    if (bind(static_cast<socket_t>(server_fd_),
             reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Bind failed on port " << config_.port << "\n";
        cleanup_socket();
        return false;
    }

    if (listen(static_cast<socket_t>(server_fd_), config_.max_clients) < 0) {
        std::cerr << "Listen failed\n";
        cleanup_socket();
        return false;
    }

    // Set non-blocking for accept timeout
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(static_cast<socket_t>(server_fd_), FIONBIO, &mode);
#else
    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);
#endif

    return true;
}

void LiveServer::cleanup_socket() {
    if (server_fd_ != static_cast<int>(SOCKET_INVALID)) {
        CLOSE_SOCKET(static_cast<socket_t>(server_fd_));
        server_fd_ = static_cast<int>(SOCKET_INVALID);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

AnalysisSnapshot LiveServer::generate_analysis(uint64_t cycle, uint64_t seed) {
    AnalysisSnapshot snap{};
    snap.cycle = cycle;
    snap.seed = seed;
    snap.timestamp = now_iso();

    // Random formula selection
    static const std::vector<std::string> formulas = {
        "H2", "He", "N2", "O2", "Ar", "CO2", "H2O", "CH4",
        "Ne", "Kr", "Xe", "NH3", "Cl2", "SO2"
    };
    static const std::vector<std::string> types = {
        "diatomic", "noble", "diatomic", "diatomic", "noble", "polyatomic",
        "polyatomic", "polyatomic", "noble", "noble", "noble", "polyatomic",
        "diatomic", "polyatomic"
    };

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> form_dist(0, formulas.size() - 1);
    std::uniform_real_distribution<double> temp_dist(77.0, 2000.0);
    std::uniform_real_distribution<double> pres_dist(0.01, 100.0);

    size_t idx = form_dist(rng);
    snap.formula = formulas[idx];
    snap.gas_type = types[idx];
    snap.temperature_K = temp_dist(rng);
    snap.pressure_atm = pres_dist(rng);

    // Compute gas properties
    auto gp = gas::compute_properties(snap.formula, snap.temperature_K,
                                       snap.pressure_atm);

    snap.ideal_volume_L = gp.ideal_volume_m3 * 1000.0;
    snap.rms_speed_ms = gp.rms_speed_ms;
    snap.mean_free_path_nm = gp.mean_free_path_m * 1e9;
    snap.avg_ke_eV = gp.avg_kinetic_energy_J / 1.602176634e-19;
    snap.molar_mass_g = gp.molar_mass_kg * 1000.0;

    // Trail ID (deterministic hash)
    std::ostringstream tid;
    tid << "LVS-" << std::hex << std::setfill('0') << std::setw(8) << (seed & 0xFFFFFFFF)
        << "-" << std::setw(4) << (cycle & 0xFFFF);
    snap.trail_id = tid.str();

    return snap;
}

std::string LiveServer::build_dashboard_html() const {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<title>VSEPR-SIM Live Analysis</title>
<style>
  body { background: #1a1a2e; color: #e0e0e0; font-family: 'Consolas', monospace; margin: 20px; }
  h1 { color: #e94560; border-bottom: 2px solid #e94560; padding-bottom: 10px; }
  .card { background: #16213e; border: 1px solid #0f3460; border-radius: 8px;
          padding: 15px; margin: 10px 0; }
  .label { color: #e94560; font-weight: bold; }
  .value { color: #00ff88; }
  .meta { color: #666; font-size: 0.85em; }
  #data { white-space: pre-wrap; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  @keyframes pulse { 0%,100% { opacity:1; } 50% { opacity:0.5; } }
  .live { animation: pulse 2s infinite; color: #ff0000; }
</style>
</head><body>
<h1>VSEPR-SIM <span class="live">● LIVE</span> Analysis Stream</h1>
<p class="meta">Port )" << config_.port << R"( | Auto-refreshing via Server-Sent Events</p>
<div class="grid">
  <div class="card">
    <div class="label">Current Analysis</div>
    <div id="formula" style="font-size:2em; color:#00ff88;">--</div>
    <div id="gastype" class="meta">--</div>
  </div>
  <div class="card">
    <div class="label">Conditions</div>
    <div>T = <span id="temp" class="value">--</span> K</div>
    <div>P = <span id="pres" class="value">--</span> atm</div>
    <div>V = <span id="vol" class="value">--</span> L</div>
  </div>
  <div class="card">
    <div class="label">Kinetic Theory</div>
    <div>v_rms = <span id="rms" class="value">--</span> m/s</div>
    <div>MFP = <span id="mfp" class="value">--</span> nm</div>
    <div>KE = <span id="ke" class="value">--</span> eV</div>
  </div>
  <div class="card">
    <div class="label">Provenance</div>
    <div>Cycle: <span id="cycle" class="value">--</span></div>
    <div>Seed: <span id="seed" class="value">--</span></div>
    <div>Trail: <span id="trail" class="value">--</span></div>
  </div>
</div>
<div class="card"><div class="label">Event Log</div><div id="log" style="max-height:300px;overflow-y:auto;font-size:0.8em;"></div></div>
<script>
const es = new EventSource('/stream');
es.onmessage = function(e) {
  const d = JSON.parse(e.data);
  document.getElementById('formula').textContent = d.formula;
  document.getElementById('gastype').textContent = d.gas_type + ' | M=' + d.molar_mass_g.toFixed(1) + ' g/mol';
  document.getElementById('temp').textContent = d.temperature_K.toFixed(1);
  document.getElementById('pres').textContent = d.pressure_atm.toFixed(2);
  document.getElementById('vol').textContent = d.ideal_volume_L.toFixed(3);
  document.getElementById('rms').textContent = d.rms_speed_ms.toFixed(1);
  document.getElementById('mfp').textContent = d.mean_free_path_nm.toFixed(1);
  document.getElementById('ke').textContent = d.avg_ke_eV.toFixed(6);
  document.getElementById('cycle').textContent = d.cycle;
  document.getElementById('seed').textContent = d.seed;
  document.getElementById('trail').textContent = d.trail_id;
  const log = document.getElementById('log');
  log.innerHTML = '[' + d.timestamp + '] ' + d.formula + ' @ ' +
    d.temperature_K.toFixed(0) + 'K, ' + d.pressure_atm.toFixed(1) + 'atm → v_rms=' +
    d.rms_speed_ms.toFixed(0) + ' m/s\n' + log.innerHTML;
};
</script>
</body></html>)";
    return html.str();
}

std::string LiveServer::build_status_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "{"
       << "\"server\":\"VSEPR-SIM Live Analysis\","
       << "\"version\":\"3.0.1\","
       << "\"port\":" << config_.port << ","
       << "\"uptime_s\":" << uptime_seconds() << ","
       << "\"cycles\":" << total_cycles() << ","
       << "\"interval_ms\":" << config_.cycle_interval_ms << ","
       << "\"status\":\"running\""
       << "}";
    return ss.str();
}

void LiveServer::handle_request(int client_fd) {
    char buf[4096];
    int n = recv(static_cast<socket_t>(client_fd), buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        CLOSE_SOCKET(static_cast<socket_t>(client_fd));
        return;
    }
    buf[n] = '\0';

    // Parse HTTP request line
    std::string request(buf);
    std::string path = "/";
    if (request.size() > 4) {
        size_t start = request.find(' ');
        size_t end = request.find(' ', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            path = request.substr(start + 1, end - start - 1);
        }
    }

    std::string response;

    if (path == "/stream") {
        // SSE: send headers then keep connection open for one event
        std::string headers =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n";
        send(static_cast<socket_t>(client_fd), headers.c_str(),
             static_cast<int>(headers.size()), 0);

        // Stream events until client disconnects or shutdown
        while (!g_shutdown_requested.load() && running_.load()) {
            std::string event = latest_.to_sse_event();
            int sent = send(static_cast<socket_t>(client_fd), event.c_str(),
                           static_cast<int>(event.size()), 0);
            if (sent <= 0) break;
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(config_.cycle_interval_ms)));
        }
        CLOSE_SOCKET(static_cast<socket_t>(client_fd));
        return;
    }

    if (path == "/snapshot") {
        std::string body = latest_.to_json();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "\r\n" << body;
        response = resp.str();
    } else if (path == "/status") {
        std::string body = build_status_json();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "\r\n" << body;
        response = resp.str();
    } else {
        // Dashboard HTML
        std::string body = build_dashboard_html();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html; charset=UTF-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "\r\n" << body;
        response = resp.str();
    }

    send(static_cast<socket_t>(client_fd), response.c_str(),
         static_cast<int>(response.size()), 0);
    CLOSE_SOCKET(static_cast<socket_t>(client_fd));
}

int LiveServer::run() {
    if (!init_socket()) return 1;

    // Set up signal handlers
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#endif

    running_.store(true);
    start_time_ = now_seconds();

    // JSONL capture log
    std::ofstream log_file;
    if (!config_.log_path.empty()) {
        log_file.open(config_.log_path, std::ios::app);
        if (log_file.is_open()) {
            std::cout << "  Log capture: " << config_.log_path << "\n";
        } else {
            std::cerr << "  WARNING: Could not open log file: " << config_.log_path << "\n";
        }
    }

    // Seed
    uint64_t base_seed = config_.base_seed;
    if (base_seed == 0) {
        base_seed = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    std::cout << "\033[1;35m"
              << "╔════════════════════════════════════════════════════════════════╗\n"
              << "║  VSEPR-SIM Live Analysis Server                               ║\n"
              << "╚════════════════════════════════════════════════════════════════╝\n"
              << "\033[0m\n";
    std::cout << "  \033[0;32m●\033[0m Listening on http://localhost:" << config_.port << "\n";
    std::cout << "  \033[0;36mℹ\033[0m Dashboard:  http://localhost:" << config_.port << "/\n";
    std::cout << "  \033[0;36mℹ\033[0m SSE stream: http://localhost:" << config_.port << "/stream\n";
    std::cout << "  \033[0;36mℹ\033[0m Snapshot:   http://localhost:" << config_.port << "/snapshot\n";
    std::cout << "  \033[0;36mℹ\033[0m Status:     http://localhost:" << config_.port << "/status\n";
    std::cout << "  \033[0;36mℹ\033[0m Interval:   " << config_.cycle_interval_ms << " ms\n";
    std::cout << "  \033[0;36mℹ\033[0m Base seed:  " << base_seed << "\n";
    std::cout << "\n  Press Ctrl+C to stop.\n\n";

    // Analysis + accept loop
    auto last_analysis = std::chrono::steady_clock::now();
    uint64_t cycle = 0;

    // Generate first analysis immediately
    latest_ = generate_analysis(cycle, base_seed ^ cycle);
    cycle_count_.store(++cycle);
    if (log_file.is_open()) { log_file << latest_.to_json() << "\n"; log_file.flush(); }

    std::cout << "  [" << latest_.timestamp << "] "
              << "\033[1m" << latest_.formula << "\033[0m"
              << " @ " << std::fixed << std::setprecision(0)
              << latest_.temperature_K << "K, "
              << std::setprecision(1) << latest_.pressure_atm << "atm"
              << " → v_rms=" << std::setprecision(0)
              << latest_.rms_speed_ms << " m/s\n";

    while (!g_shutdown_requested.load()) {
        // Accept clients (non-blocking)
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = static_cast<int>(
            accept(static_cast<socket_t>(server_fd_),
                   reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len));

        if (client_fd != static_cast<int>(SOCKET_INVALID)) {
            // Handle in same thread (simple sequential model)
            handle_request(client_fd);
        }

        // Generate new analysis on interval
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_analysis).count();

        if (elapsed >= static_cast<long long>(config_.cycle_interval_ms)) {
            latest_ = generate_analysis(cycle, base_seed ^ (cycle * 2654435761ULL));
            cycle_count_.store(++cycle);
            last_analysis = now;
            if (log_file.is_open()) { log_file << latest_.to_json() << "\n"; log_file.flush(); }

            std::cout << "  [" << latest_.timestamp << "] "
                      << "\033[1m" << latest_.formula << "\033[0m"
                      << " @ " << std::fixed << std::setprecision(0)
                      << latest_.temperature_K << "K, "
                      << std::setprecision(1) << latest_.pressure_atm << "atm"
                      << " → v_rms=" << std::setprecision(0)
                      << latest_.rms_speed_ms << " m/s\n";
        }

        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "\n  \033[1;33m⚠\033[0m Shutting down... "
              << cycle << " analyses completed in "
              << std::fixed << std::setprecision(1)
              << uptime_seconds() << " seconds.\n";

    running_.store(false);
    cleanup_socket();
    return 0;
}

void LiveServer::request_shutdown() {
    g_shutdown_requested.store(true);
}

AnalysisSnapshot LiveServer::latest_snapshot() const {
    return latest_;
}

double LiveServer::uptime_seconds() const {
    return now_seconds() - start_time_;
}

uint64_t LiveServer::total_cycles() const {
    return cycle_count_.load();
}

// ============================================================================
// CLI dispatch: vsepr serve [--port P] [--interval MS] [--seed S]
// ============================================================================

int serve_dispatch(int argc, char** argv) {
    ServerConfig cfg;
    std::string log_path = "live_log.jsonl";  // default capture file

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--log") && i + 1 < argc) {
            log_path = argv[++i];
        } else if (arg == "--no-log") {
            log_path.clear();
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            cfg.port = std::stoi(argv[++i]);
            if (cfg.port < 1 || cfg.port > 65535) {
                std::cerr << "WARNING: Port " << cfg.port << " outside valid TCP range (1-65535). Using 9998.\n";
                cfg.port = 9998;
            }
        } else if ((arg == "--interval" || arg == "-i") && i + 1 < argc) {
            cfg.cycle_interval_ms = std::stod(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            cfg.base_seed = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--verbose" || arg == "-v") {
            cfg.verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << R"(
VSEPR-SIM Live Analysis Server
═══════════════════════════════

USAGE:
    vsepr serve [options]
    vsepr_live  [options]

OPTIONS:
    -p, --port <PORT>       Listen port (default: 99998)
    -i, --interval <MS>     Analysis cycle interval in ms (default: 3000)
    --seed <SEED>           Base RNG seed (default: time-based)
    -v, --verbose           Verbose logging
    -h, --help              Show this help

ENDPOINTS:
    GET /              HTML dashboard with live auto-updating display
    GET /stream        Server-Sent Events (text/event-stream)
    GET /snapshot      Latest analysis as JSON
    GET /status        Server uptime and statistics

EXAMPLES:
    vsepr serve                     # Start on port 99998, zero config
    vsepr serve --port 8080         # Custom port
    vsepr serve --interval 1000     # Faster updates (1 second)
    vsepr serve --seed 42           # Deterministic sequence

)";
            return 0;
        }
    }

    cfg.log_path = log_path;

    LiveServer server(cfg);
    return server.run();
}

} // namespace live
} // namespace vsepr
