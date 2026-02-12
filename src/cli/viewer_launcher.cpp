#include "cli/viewer_launcher.hpp"
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vsepr {
namespace cli {

void ViewerLauncher::launch_static(const std::string& xyz_path) {
    std::cout << "Launching viewer for: " << xyz_path << "\n";
    
#ifdef _WIN32
    // Windows: Use start to launch in background
    std::string command = "start simple-viewer.exe \"" + xyz_path + "\"";
    system(command.c_str());
#else
    // Unix/WSL: Launch in background
    std::string command = "./simple-viewer \"" + xyz_path + "\" &";
    int result = system(command.c_str());
    if (result != 0) {
        std::cerr << "Warning: Failed to launch viewer (is simple-viewer in PATH?)\n";
    }
#endif
}

void ViewerLauncher::launch_watch(const std::string& xyz_path) {
    std::cout << "Launching live viewer (--watch mode) for: " << xyz_path << "\n";
    std::cout << "Viewer will update automatically as simulation progresses.\n";
    
#ifdef _WIN32
    // Windows: Launch with --watch flag
    std::string command = "start simple-viewer.exe \"" + xyz_path + "\" --watch";
    system(command.c_str());
#else
    // Unix/WSL: Launch in background with watch mode
    std::string command = "./simple-viewer \"" + xyz_path + "\" --watch &";
    int result = system(command.c_str());
    if (result != 0) {
        std::cerr << "Warning: Failed to launch viewer (is simple-viewer in PATH?)\n";
    }
#endif
}

void ViewerLauncher::launch_process(const std::string& command) {
#ifdef _WIN32
    // Windows: Use CreateProcess for better control
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    // Create mutable copy for CreateProcess
    char* cmd = new char[command.length() + 1];
    strcpy(cmd, command.c_str());
    
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Warning: Failed to launch viewer\n";
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    delete[] cmd;
#else
    // Unix: Use system()
    system(command.c_str());
#endif
}

}} // namespace vsepr::cli
