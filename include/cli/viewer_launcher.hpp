#pragma once
/**
 * viewer_launcher.hpp
 * ====================
 * VSEPR-SIM  |  WO-85Z  |  Viewer Launcher
 *
 * Launches the supported live `vsepr-view` process for simulation artifacts.
 * Historical standalone, VTK, and daemon viewers are archived and are not
 * considered launch candidates.
 */

#include <string>

namespace vsepr {
namespace cli {

struct ViewerLaunchConfig {
    std::string artifact_path;
    bool uless_indicator_enabled = false;
    std::string uless_label = "obs";
    double uless_value = 0.0;
    double uless_max = 1.0;
};

class ViewerLauncher {
public:
    // Launch a live viewer seeded from an artifact path.
    static void launch_static(const std::string& artifact_path);

    // Launch with explicit Uless indicator configuration.
    static void launch_with_config(const ViewerLaunchConfig& config);

    // Launch a live viewer from the latest artifact state. The simulation loop
    // remains fixed-timestep and independent of render cadence.
    static void launch_watch(const std::string& artifact_path);

private:
    // Resolve only the supported live viewer.
    static std::string resolve_viewer_binary();

    static void launch_process(const std::string& command);
};

} // namespace cli
} // namespace vsepr
