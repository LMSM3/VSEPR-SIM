#include "platform_terminal.hpp"
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace vsepr {
namespace demo {

Platform PlatformTerminal::detect_platform() {
#ifdef _WIN32
    // Check if running in WSL
    const char* wsl_distro = std::getenv("WSL_DISTRO_NAME");
    if (wsl_distro) {
        return Platform::Windows_WSL;
    }
    
    // Check if PowerShell is preferred (PSModulePath exists)
    const char* ps_path = std::getenv("PSModulePath");
    if (ps_path) {
        return Platform::Windows_PowerShell;
    }
    
    return Platform::Windows_CMD;
#elif __APPLE__
    return Platform::MacOS_Terminal;
#elif __linux__
    return Platform::Linux_Terminal;
#else
    return Platform::Unknown;
#endif
}

std::string PlatformTerminal::platform_name(Platform p) {
    switch (p) {
        case Platform::Windows_PowerShell: return "Windows PowerShell";
        case Platform::Windows_CMD: return "Windows CMD";
        case Platform::Windows_WSL: return "Windows WSL";
        case Platform::Linux_Terminal: return "Linux Terminal";
        case Platform::MacOS_Terminal: return "macOS Terminal";
        case Platform::Unknown: return "Unknown Platform";
    }
    return "Unknown";
}

bool PlatformTerminal::launch_command(const std::string& command, bool wait) {
    Platform platform = detect_platform();
    
    switch (platform) {
        case Platform::Windows_PowerShell:
            return launch_windows_powershell(command, wait);
        case Platform::Windows_CMD:
            return launch_windows_cmd(command, wait);
        case Platform::Windows_WSL:
            return launch_linux_terminal(command, wait);
        case Platform::Linux_Terminal:
            return launch_linux_terminal(command, wait);
        case Platform::MacOS_Terminal:
            return launch_macos_terminal(command, wait);
        default:
            return false;
    }
}

bool PlatformTerminal::launch_command_titled(const std::string& title, 
                                               const std::string& command, 
                                               bool wait) {
    Platform platform = detect_platform();
    
    std::string titled_command;
    
    switch (platform) {
        case Platform::Windows_PowerShell:
            titled_command = "$host.ui.RawUI.WindowTitle = '" + title + "'; " + command;
            return launch_windows_powershell(titled_command, wait);
            
        case Platform::Windows_CMD:
            titled_command = "title " + title + " && " + command;
            return launch_windows_cmd(titled_command, wait);
            
        case Platform::Windows_WSL:
        case Platform::Linux_Terminal:
            // Most Linux terminals support --title
            titled_command = command;
            return launch_linux_terminal(titled_command, wait);
            
        case Platform::MacOS_Terminal:
            return launch_macos_terminal(command, wait);
            
        default:
            return false;
    }
}

bool PlatformTerminal::launch_terminal(const std::string& working_dir) {
    Platform platform = detect_platform();
    
    switch (platform) {
        case Platform::Windows_PowerShell:
            return launch_windows_powershell("cd '" + working_dir + "'; $host", false);
            
        case Platform::Windows_CMD:
            return launch_windows_cmd("cd /d \"" + working_dir + "\"", false);
            
        case Platform::Windows_WSL:
        case Platform::Linux_Terminal:
            return launch_linux_terminal("cd " + working_dir + " && bash", false);
            
        case Platform::MacOS_Terminal:
            return launch_macos_terminal("cd " + working_dir + " && bash", false);
            
        default:
            return false;
    }
}

bool PlatformTerminal::launch_windows_powershell(const std::string& command, bool wait) {
#ifdef _WIN32
    std::stringstream ss;
    ss << "powershell.exe -NoExit -Command \"" << command << "\"";
    std::string full_cmd = ss.str();
    
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    
    BOOL success = CreateProcessA(
        nullptr,
        const_cast<char*>(full_cmd.c_str()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &si,
        &pi
    );
    
    if (!success) {
        return false;
    }
    
    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
#else
    return false;
#endif
}

bool PlatformTerminal::launch_windows_cmd(const std::string& command, bool wait) {
#ifdef _WIN32
    std::stringstream ss;
    ss << "cmd.exe /K \"" << command << "\"";
    std::string full_cmd = ss.str();
    
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    
    BOOL success = CreateProcessA(
        nullptr,
        const_cast<char*>(full_cmd.c_str()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &si,
        &pi
    );
    
    if (!success) {
        return false;
    }
    
    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
#else
    return false;
#endif
}

bool PlatformTerminal::launch_linux_terminal(const std::string& command, bool wait) {
#ifndef _WIN32
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        // Try common terminal emulators in order of preference
        const char* terminals[] = {
            "gnome-terminal",
            "konsole",
            "xfce4-terminal",
            "xterm",
            nullptr
        };
        
        for (int i = 0; terminals[i] != nullptr; ++i) {
            execlp(terminals[i], terminals[i], "-e", command.c_str(), nullptr);
        }
        
        // If all fail, exit
        _exit(1);
    } else if (pid > 0) {
        // Parent process
        if (wait) {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        return true;
    }
    
    return false;
#else
    return false;
#endif
}

bool PlatformTerminal::launch_macos_terminal(const std::string& command, bool wait) {
#ifdef __APPLE__
    std::stringstream ss;
    ss << "osascript -e 'tell application \"Terminal\" to do script \"" 
       << command << "\"' -e 'tell application \"Terminal\" to activate'";
    
    int result = system(ss.str().c_str());
    
    return (result == 0);
#else
    return false;
#endif
}

} // namespace demo
} // namespace vsepr
