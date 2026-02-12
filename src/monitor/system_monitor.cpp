/**
 * system_monitor.cpp
 * ===================
 * Implementation of system monitoring
 */

#include "monitor/system_monitor.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <array>

#ifdef __linux__
#include <sys/statvfs.h>
#include <unistd.h>
#endif

namespace vsepr {
namespace monitor {

// ============================================================================
// MiniGraph Implementation
// ============================================================================

std::string MiniGraph::render(size_t width) const {
    if (history_.empty()) return std::string(width, ' ');
    
    // Sparkline characters: ▁▂▃▄▅▆▇█
    static const char* blocks[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    
    // Determine range
    double min_v = min_val_;
    double max_v = max_val_;
    if (max_v <= min_v) max_v = min_v + 1.0;
    
    std::string result;
    result.reserve(width * 3); // UTF-8 chars can be 3 bytes
    
    // Sample history to fit width
    size_t step = std::max(size_t(1), history_.size() / width);
    for (size_t i = 0; i < width && i * step < history_.size(); ++i) {
        double val = history_[i * step];
        double norm = (val - min_v) / (max_v - min_v);
        norm = std::clamp(norm, 0.0, 1.0);
        
        int block_idx = static_cast<int>(norm * 8.0);
        block_idx = std::clamp(block_idx, 0, 8);
        result += blocks[block_idx];
    }
    
    return result;
}

std::string MiniGraph::render_bar(double percent, size_t width) {
    if (width == 0) return "";
    
    double clamped = std::clamp(percent, 0.0, 100.0);
    size_t filled = static_cast<size_t>((clamped / 100.0) * width);
    size_t empty = width - filled;
    
    std::string bar = "[";
    bar += std::string(filled, '█');
    bar += std::string(empty, '░');
    bar += "] ";
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << clamped << "%";
    bar += ss.str();
    
    return bar;
}

double MiniGraph::average() const {
    if (history_.empty()) return 0.0;
    return std::accumulate(history_.begin(), history_.end(), 0.0) / history_.size();
}

double MiniGraph::min() const {
    if (history_.empty()) return 0.0;
    return *std::min_element(history_.begin(), history_.end());
}

double MiniGraph::max() const {
    if (history_.empty()) return 0.0;
    return *std::max_element(history_.begin(), history_.end());
}

// ============================================================================
// Utility: Execute Command
// ============================================================================

std::string exec_command(const char* cmd) {
#ifdef _WIN32
    // Windows: use popen
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif
    
    if (!pipe) return "";
    
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    return result;
}

std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double val = static_cast<double>(bytes);
    
    while (val >= 1024.0 && unit < 4) {
        val /= 1024.0;
        ++unit;
    }
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << val << " " << units[unit];
    return ss.str();
}

std::string format_rate(double mbps) {
    if (mbps < 1.0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (mbps * 1000.0) << " Kbps";
        return ss.str();
    } else if (mbps < 1000.0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << mbps << " Mbps";
        return ss.str();
    } else {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (mbps / 1000.0) << " Gbps";
        return ss.str();
    }
}

// ============================================================================
// NVIDIA-SMI Parser
// ============================================================================

bool NvidiaSMIParser::is_available() {
    std::string output = exec_command("which nvidia-smi 2>/dev/null");
    return !output.empty();
}

std::vector<GPUStats> NvidiaSMIParser::query() {
    // Query nvidia-smi with CSV format
    std::string cmd = "nvidia-smi --query-gpu=index,name,utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw "
                      "--format=csv,noheader,nounits 2>/dev/null";
    std::string output = exec_command(cmd.c_str());
    return parse(output);
}

std::vector<GPUStats> NvidiaSMIParser::parse(const std::string& output) {
    std::vector<GPUStats> gpus;
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        GPUStats gpu;
        std::istringstream line_stream(line);
        std::string token;
        
        // Parse CSV: index, name, util, mem_used, mem_total, temp, power
        int field = 0;
        while (std::getline(line_stream, token, ',')) {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t\n\r"));
            token.erase(token.find_last_not_of(" \t\n\r") + 1);
            
            switch (field) {
                case 0: gpu.device_id = std::stoi(token); break;
                case 1: gpu.name = token; break;
                case 2: gpu.utilization_percent = std::stod(token); break;
                case 3: gpu.memory_used_mb = std::stod(token); break;
                case 4: gpu.memory_total_mb = std::stod(token); break;
                case 5: gpu.temperature_celsius = std::stod(token); break;
                case 6: gpu.power_watts = std::stod(token); break;
            }
            ++field;
        }
        
        gpu.timestamp = std::time(nullptr);
        gpus.push_back(gpu);
    }
    
    return gpus;
}

// ============================================================================
// Network Stats Reader
// ============================================================================

std::vector<NetworkStats> NetworkStatsReader::query() {
    return parse_proc_net_dev();
}

std::vector<NetworkStats> NetworkStatsReader::parse_proc_net_dev() {
    std::vector<NetworkStats> stats;
    
#ifdef __linux__
    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) return stats;
    
    std::string line;
    // Skip header lines
    std::getline(file, line);
    std::getline(file, line);
    
    std::time_t now = std::time(nullptr);
    
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string iface;
        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;
        
        // Parse: "  eth0: 12345 ..."
        ss >> iface >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo 
           >> rx_frame >> rx_compressed >> rx_multicast
           >> tx_bytes >> tx_packets >> tx_errs >> tx_drop >> tx_fifo 
           >> tx_colls >> tx_carrier >> tx_compressed;
        
        // Remove trailing colon from interface name
        if (!iface.empty() && iface.back() == ':') {
            iface.pop_back();
        }
        
        // Skip loopback
        if (iface == "lo") continue;
        
        NetworkStats net;
        net.interface = iface;
        net.rx_bytes = rx_bytes;
        net.tx_bytes = tx_bytes;
        net.timestamp = now;
        
        // Calculate rates if we have previous state
        auto it = last_state_.find(iface);
        if (it != last_state_.end()) {
            auto& prev = it->second;
            double dt = std::difftime(now, prev.timestamp);
            if (dt > 0) {
                uint64_t rx_delta = rx_bytes - prev.rx_bytes;
                uint64_t tx_delta = tx_bytes - prev.tx_bytes;
                
                // Convert to Mbps
                net.rx_rate_mbps = (rx_delta * 8.0) / (dt * 1000000.0);
                net.tx_rate_mbps = (tx_delta * 8.0) / (dt * 1000000.0);
            }
        } else {
            net.rx_rate_mbps = 0.0;
            net.tx_rate_mbps = 0.0;
        }
        
        // Update last state
        last_state_[iface] = {rx_bytes, tx_bytes, now};
        
        stats.push_back(net);
    }
#endif
    
    return stats;
}

// ============================================================================
// Disk Stats Reader
// ============================================================================

std::vector<DiskStats> DiskStatsReader::query() {
    std::vector<DiskStats> stats;
    
    // Query common mount points
    std::vector<std::string> mounts = {"/", "/home", "/tmp"};
    for (const auto& mount : mounts) {
        DiskStats ds = query_mount(mount);
        if (ds.total_bytes > 0) {
            stats.push_back(ds);
        }
    }
    
    return stats;
}

DiskStats DiskStatsReader::query_mount(const std::string& mount_point) {
    DiskStats ds;
    ds.mount_point = mount_point;
    ds.timestamp = std::time(nullptr);
    
#ifdef __linux__
    struct statvfs vfs;
    if (statvfs(mount_point.c_str(), &vfs) == 0) {
        ds.total_bytes = vfs.f_blocks * vfs.f_frsize;
        ds.free_bytes = vfs.f_bfree * vfs.f_frsize;
        ds.used_bytes = ds.total_bytes - ds.free_bytes;
        ds.usage_percent = ds.total_bytes > 0 ? 
            (ds.used_bytes * 100.0) / ds.total_bytes : 0.0;
    }
#endif
    
    return ds;
}

// ============================================================================
// System Monitor
// ============================================================================

SystemMonitor::SystemMonitor()
    : running_(false),
      gpu_utilization_graph_(50, 0.0, 100.0),
      network_rx_graph_(50, 0.0, 1000.0),
      disk_usage_graph_(50, 0.0, 100.0),
      cpu_graph_(50, 0.0, 100.0) {
    
    system_pipe_ = std::make_shared<gui::DataPipe<SystemSnapshot>>("system_monitor");
    gpu_pipe_ = std::make_shared<gui::DataPipe<GPUStats>>("gpu_stats");
    status_pipe_ = std::make_shared<gui::DataPipe<std::string>>("monitor_status");
}

void SystemMonitor::start() {
    running_ = true;
    status_pipe_->push("System monitor started");
}

void SystemMonitor::stop() {
    running_ = false;
    status_pipe_->push("System monitor stopped");
}

SystemSnapshot SystemMonitor::get_snapshot() {
    SystemSnapshot snap;
    snap.timestamp = std::time(nullptr);
    
    snap.gpus = query_gpus();
    snap.networks = query_networks();
    snap.disks = query_disks();
    snap.cpu_percent = query_cpu_usage();
    
    auto [ram_used, ram_total] = query_ram_usage();
    snap.ram_used_gb = ram_used;
    snap.ram_total_gb = ram_total;
    
    // Update graphs
    if (!snap.gpus.empty()) {
        gpu_utilization_graph_.push(snap.gpus[0].utilization_percent);
    }
    if (!snap.networks.empty()) {
        network_rx_graph_.push(snap.networks[0].rx_rate_mbps);
    }
    if (!snap.disks.empty()) {
        disk_usage_graph_.push(snap.disks[0].usage_percent);
    }
    cpu_graph_.push(snap.cpu_percent);
    
    // Push to pipe
    system_pipe_->push(snap);
    
    return snap;
}

std::vector<GPUStats> SystemMonitor::query_gpus() {
    if (!NvidiaSMIParser::is_available()) {
        return {};
    }
    return NvidiaSMIParser::query();
}

std::vector<NetworkStats> SystemMonitor::query_networks() {
    static NetworkStatsReader reader;
    return reader.query();
}

std::vector<DiskStats> SystemMonitor::query_disks() {
    DiskStatsReader reader;
    return reader.query();
}

double SystemMonitor::query_cpu_usage() {
    // Simplified CPU usage query
#ifdef __linux__
    static uint64_t prev_total = 0, prev_idle = 0;
    
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0.0;
    
    std::string line;
    std::getline(file, line);
    
    std::istringstream ss(line);
    std::string cpu;
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    
    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
    uint64_t idle_time = idle + iowait;
    
    uint64_t total_delta = total - prev_total;
    uint64_t idle_delta = idle_time - prev_idle;
    
    prev_total = total;
    prev_idle = idle_time;
    
    if (total_delta == 0) return 0.0;
    return 100.0 * (1.0 - static_cast<double>(idle_delta) / total_delta);
#else
    return 0.0;
#endif
}

std::pair<double, double> SystemMonitor::query_ram_usage() {
#ifdef __linux__
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return {0.0, 0.0};
    
    uint64_t total = 0, available = 0;
    std::string line, key, unit;
    uint64_t value;
    
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        ss >> key >> value >> unit;
        
        if (key == "MemTotal:") total = value;
        else if (key == "MemAvailable:") available = value;
    }
    
    double total_gb = total / (1024.0 * 1024.0);
    double used_gb = (total - available) / (1024.0 * 1024.0);
    
    return {used_gb, total_gb};
#else
    return {0.0, 0.0};
#endif
}

std::string SystemMonitor::render_gpu_status() const {
    std::ostringstream ss;
    ss << "GPU: ";
    if (gpu_utilization_graph_.history().empty()) {
        ss << "N/A";
    } else {
        ss << MiniGraph::render_bar(gpu_utilization_graph_.latest(), 15);
        ss << " " << gpu_utilization_graph_.render(30);
    }
    return ss.str();
}

std::string SystemMonitor::render_network_status() const {
    std::ostringstream ss;
    ss << "NET: ";
    if (network_rx_graph_.history().empty()) {
        ss << "N/A";
    } else {
        ss << std::fixed << std::setprecision(1) << network_rx_graph_.latest() << " Mbps ";
        ss << network_rx_graph_.render(30);
    }
    return ss.str();
}

std::string SystemMonitor::render_disk_status() const {
    std::ostringstream ss;
    ss << "DISK: ";
    if (disk_usage_graph_.history().empty()) {
        ss << "N/A";
    } else {
        ss << MiniGraph::render_bar(disk_usage_graph_.latest(), 15);
    }
    return ss.str();
}

std::string SystemMonitor::render_full_status() const {
    std::ostringstream ss;
    ss << "┌─ System Monitor ─────────────────────────────────┐\n";
    ss << "│ " << render_gpu_status() << "\n";
    ss << "│ " << render_network_status() << "\n";
    ss << "│ " << render_disk_status() << "\n";
    ss << "│ CPU: " << MiniGraph::render_bar(cpu_graph_.latest(), 15);
    ss << " " << cpu_graph_.render(20) << "\n";
    ss << "└──────────────────────────────────────────────────┘";
    return ss.str();
}

} // namespace monitor
} // namespace vsepr
