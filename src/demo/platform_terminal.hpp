#pragma once

#include <string>
#include <vector>

namespace vsepr {
namespace demo {

enum class Platform {
    Windows_PowerShell,
    Windows_CMD,
    Windows_WSL,
    Linux_Terminal,
    MacOS_Terminal,
    Unknown
};

class PlatformTerminal {
public:
    // Detect current platform
    static Platform detect_platform();
    
    // Get platform name
    static std::string platform_name(Platform p);
    
    // Launch command in appropriate terminal
    static bool launch_command(const std::string& command, bool wait = false);
    
    // Launch command with custom title
    static bool launch_command_titled(const std::string& title, 
                                       const std::string& command, 
                                       bool wait = false);
    
    // Open terminal at specific directory
    static bool launch_terminal(const std::string& working_dir = ".");
    
private:
    static bool launch_windows_powershell(const std::string& command, bool wait);
    static bool launch_windows_cmd(const std::string& command, bool wait);
    static bool launch_linux_terminal(const std::string& command, bool wait);
    static bool launch_macos_terminal(const std::string& command, bool wait);
};

} // namespace demo
} // namespace vsepr
