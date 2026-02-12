/**
 * system_monitor.hpp
 * ===================
 * Real-time system monitoring for GPU, Network, Disk with CLI graphs
 * Integrates with DataPipe for reactive updates
 */

#pragma once

#include "gui/data_pipe.hpp"
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <optional>
#include <cstdint>
#include <ctime>

namespace vsepr {
namespace monitor {

// ============================================================================
// GPU Stats (NVIDIA via nvidia-smi)
// ============================================================================

struct GPUStats {
    int device_id;
    std::string name;
    double utilization_percent;      // 0-100
    double memory_used_mb;
    double memory_total_mb;
    double temperature_celsius;
    double power_watts;
    std::time_t timestamp;
    
    double memory_percent() const {
        return memory_total_mb > 0 ? (memory_used_mb / memory_total_mb) * 100.0 : 0.0;
    }
};

// ============================================================================
// Network Stats
// ============================================================================

struct NetworkStats {
    std::string interface;           // e.g., "eth0", "wlan0"
    uint64_t rx_bytes;               // Total received bytes
    uint64_t tx_bytes;               // Total transmitted bytes
    double rx_rate_mbps;             // Current receive rate (Mbps)
    double tx_rate_mbps;             // Current transmit rate (Mbps)
    std::time_t timestamp;
};

// ============================================================================
// Disk Stats
// ============================================================================

struct DiskStats {
    std::string mount_point;         // e.g., "/", "/home"
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    double usage_percent;
    std::time_t timestamp;
    
    double total_gb() const { return total_bytes / (1024.0 * 1024.0 * 1024.0); }
    double used_gb() const { return used_bytes / (1024.0 * 1024.0 * 1024.0); }
    double free_gb() const { return free_bytes / (1024.0 * 1024.0 * 1024.0); }
};

// ============================================================================
// System Summary
// ============================================================================

struct SystemSnapshot {
    std::vector<GPUStats> gpus;
    std::vector<NetworkStats> networks;
    std::vector<DiskStats> disks;
    double cpu_percent;              // Overall CPU usage
    double ram_used_gb;
    double ram_total_gb;
    std::time_t timestamp;
    
    double ram_percent() const {
        return ram_total_gb > 0 ? (ram_used_gb / ram_total_gb) * 100.0 : 0.0;
    }
};

// ============================================================================
// CLI Graph Renderer (mini sparklines)
// ============================================================================

class MiniGraph {
    std::vector<double> history_;
    size_t max_points_;
    double min_val_, max_val_;
    
public:
    explicit MiniGraph(size_t max_points = 50, double min_val = 0.0, double max_val = 100.0)
        : max_points_(max_points), min_val_(min_val), max_val_(max_val) {}
    
    void push(double value) {
        history_.push_back(value);
        if (history_.size() > max_points_) {
            history_.erase(history_.begin());
        }
    }
    
    void clear() {
        history_.clear();
    }
    
    // Render as ASCII sparkline: ▁▂▃▄▅▆▇█
    std::string render(size_t width = 40) const;
    
    // Render as bar: [████████░░] 80%
    static std::string render_bar(double percent, size_t width = 20);
    
    const std::vector<double>& history() const { return history_; }
    double latest() const { return history_.empty() ? 0.0 : history_.back(); }
    double average() const;
    double min() const;
    double max() const;
};

// ============================================================================
// System Monitor (polls system stats)
// ============================================================================

class SystemMonitor {
    bool running_;
    std::shared_ptr<gui::DataPipe<SystemSnapshot>> system_pipe_;
    std::shared_ptr<gui::DataPipe<GPUStats>> gpu_pipe_;
    std::shared_ptr<gui::DataPipe<std::string>> status_pipe_;
    
    // History for graphing
    MiniGraph gpu_utilization_graph_;
    MiniGraph network_rx_graph_;
    MiniGraph disk_usage_graph_;
    MiniGraph cpu_graph_;
    
public:
    SystemMonitor();
    
    // Start/stop monitoring
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Get current snapshot
    SystemSnapshot get_snapshot();
    
    // Query specific subsystems
    std::vector<GPUStats> query_gpus();
    std::vector<NetworkStats> query_networks();
    std::vector<DiskStats> query_disks();
    double query_cpu_usage();
    std::pair<double, double> query_ram_usage(); // (used_gb, total_gb)
    
    // CLI rendering
    std::string render_gpu_status() const;
    std::string render_network_status() const;
    std::string render_disk_status() const;
    std::string render_full_status() const;
    
    // Data pipes
    std::shared_ptr<gui::DataPipe<SystemSnapshot>> system_pipe() { return system_pipe_; }
    std::shared_ptr<gui::DataPipe<GPUStats>> gpu_pipe() { return gpu_pipe_; }
    std::shared_ptr<gui::DataPipe<std::string>> status_pipe() { return status_pipe_; }
    
    // Graphs
    const MiniGraph& gpu_graph() const { return gpu_utilization_graph_; }
    const MiniGraph& network_graph() const { return network_rx_graph_; }
    const MiniGraph& disk_graph() const { return disk_usage_graph_; }
    const MiniGraph& cpu_graph() const { return cpu_graph_; }
};

// ============================================================================
// NVIDIA-SMI Parser
// ============================================================================

class NvidiaSMIParser {
public:
    // Parse nvidia-smi output
    static std::vector<GPUStats> parse(const std::string& nvidia_smi_output);
    
    // Execute nvidia-smi and parse
    static std::vector<GPUStats> query();
    
    // Check if nvidia-smi is available
    static bool is_available();
};

// ============================================================================
// Network Stats Reader (Linux /proc/net/dev)
// ============================================================================

class NetworkStatsReader {
    struct InterfaceState {
        uint64_t rx_bytes;
        uint64_t tx_bytes;
        std::time_t timestamp;
    };
    
    std::unordered_map<std::string, InterfaceState> last_state_;
    
public:
    std::vector<NetworkStats> query();
    
private:
    std::vector<NetworkStats> parse_proc_net_dev();
};

// ============================================================================
// Disk Stats Reader (Linux /proc/mounts + statvfs)
// ============================================================================

class DiskStatsReader {
public:
    std::vector<DiskStats> query();
    
private:
    DiskStats query_mount(const std::string& mount_point);
};

// ============================================================================
// Utility Functions
// ============================================================================

// Format bytes as human-readable (e.g., "1.5 GB")
std::string format_bytes(uint64_t bytes);

// Format rate as human-readable (e.g., "10.5 Mbps")
std::string format_rate(double mbps);

// Execute shell command and capture output
std::string exec_command(const char* cmd);

} // namespace monitor
} // namespace vsepr
