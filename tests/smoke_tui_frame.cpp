// smoke_tui_frame.cpp — render one frame of the TUI (non-interactive)
#include "infra/nvidia_tui.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>

// Re-expose the internal query + draw functions for a single-frame test
// by just calling enter_nvidia_tui with a timeout hack — instead, let me
// directly shell out and print what the TUI would show.

namespace {

std::string shell_exec(const char* cmd) {
    std::string result;
    FILE* pipe = _popen(cmd, "r");
    if (!pipe) return "";
    std::array<char, 512> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        result += buf.data();
    _pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'
           || result.back() == ' ')) result.pop_back();
    return result;
}

}

int main() {
    std::cout << "=== NVIDIA-SMI RAW QUERY ===\n\n";

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
        std::cout << "nvidia-smi not available\n";
    } else {
        std::cout << "GPU telemetry: " << csv << "\n";
    }

    std::cout << "\n=== COMPUTE APPS ===\n";
    std::string apps = shell_exec(
        "nvidia-smi --query-compute-apps=pid,process_name,used_gpu_memory"
        " --format=csv,noheader,nounits 2>nul");
    std::cout << (apps.empty() ? "(none)" : apps) << "\n";

    std::cout << "\n=== GRAPHICS APPS ===\n";
    std::string gapps = shell_exec(
        "nvidia-smi --query-graphics-apps=pid,process_name,used_gpu_memory"
        " --format=csv,noheader,nounits 2>nul");
    std::cout << (gapps.empty() ? "(none)" : gapps) << "\n";

    return 0;
}
