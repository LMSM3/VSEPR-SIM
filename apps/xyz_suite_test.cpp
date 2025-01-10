/**
 * XYZ Suite Tester
 * 
 * Production test harness for I/O API
 * - Loads real molecules from benchmark_results/
 * - Validates all API operations
 * - Shows backend data structures
 * - Monitors CPU/GPU resource usage
 */

#include "api/io_api.hpp"
#include "core/error.hpp"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace vsepr;
using namespace vsepr::api;

// Add 3 colours
#define RED "\033[31m]"
#define BLUE "\033[32m]"
#define GREEN "\033[33m]"

// ============================================================================
// Resource Monitor
// ============================================================================

struct ResourceMetrics {
    double cpu_percent = 0.0;
    size_t memory_mb = 0;
    double gpu_utilization = 0.0;  // Placeholder - actual GPU query requires vendor libs
    std::chrono::microseconds elapsed_time{0};
};

class ResourceMonitor {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        #ifdef _WIN32
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc_start_, sizeof(pmc_start_));
        #endif
    }
    
    ResourceMetrics stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        ResourceMetrics metrics;
        
        metrics.elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_
        );
        
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        metrics.memory_mb = pmc.WorkingSetSize / (1024 * 1024);
        
        // CPU usage estimation (simplified)
        FILETIME creation, exit, kernel, user;
        if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
            ULARGE_INTEGER k, u;
            k.LowPart = kernel.dwLowDateTime;
            k.HighPart = kernel.dwHighDateTime;
            u.LowPart = user.dwLowDateTime;
            u.HighPart = user.dwHighDateTime;
            
            double total_time = (k.QuadPart + u.QuadPart) / 10000.0; // to ms
            double elapsed_ms = metrics.elapsed_time.count() / 1000.0;
            metrics.cpu_percent = (elapsed_ms > 0) ? (total_time / elapsed_ms) * 100.0 : 0.0;
        }
        #else
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        metrics.memory_mb = usage.ru_maxrss / 1024; // KB to MB
        
        double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
        double sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
        double elapsed_sec = metrics.elapsed_time.count() / 1e6;
        metrics.cpu_percent = (elapsed_sec > 0) ? ((user_time + sys_time) / elapsed_sec) * 100.0 : 0.0;
        #endif
        
        // GPU utilization placeholder (requires CUDA/OpenCL/Vulkan query)
        metrics.gpu_utilization = 0.0;
        
        return metrics;
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    #ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc_start_;
    #endif
};

// ============================================================================
// Output Formatting
// ============================================================================

void print_header(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(62) << title << " ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
}

void print_section(const std::string& title) {
    std::cout << "\n┌─ " << title << " " << std::string(60 - title.length(), '─') << "\n";
}

void print_metrics(const ResourceMetrics& metrics) {
    std::cout << "│ Performance Metrics:\n";
    std::cout << "│   Time:   " << std::fixed << std::setprecision(2) 
              << metrics.elapsed_time.count() / 1000.0 << " ms\n";
    std::cout << "│   CPU:    " << std::setprecision(1) << metrics.cpu_percent << "%\n";
    std::cout << "│   Memory: " << metrics.memory_mb << " MB\n";
    if (metrics.gpu_utilization > 0.0) {
        std::cout << "│   GPU:    " << metrics.gpu_utilization << "%\n";
    }
    std::cout << "└" << std::string(64, '─') << "\n";
}

// ============================================================================
// Backend Data Visualization
// ============================================================================

void show_backend_structure(const io::XYZMolecule& mol) {
    print_section("Backend Data Structure");
    
    // Atom data
    std::cout << "│ Atoms (" << mol.atoms.size() << "):\n";
    std::cout << "│   Idx | Element | Position (Å)                    | ${RED}Charge\n";
    std::cout << "│   " << std::string(60, '─') << "\n";
    
    size_t display_limit = std::min(mol.atoms.size(), size_t(10));
    for (size_t i = 0; i < display_limit; ++i) {
        const auto& atom = mol.atoms[i];
        std::cout << "│   " << std::setw(3) << i << " | "
                  << std::setw(7) << atom.element << " | ("
                  << std::setw(7) << std::fixed << std::setprecision(3) << atom.position[0] << ", "
                  << std::setw(7) << atom.position[1] << ", "
                  << std::setw(7) << atom.position[2] << ") | "
                  << std::setw(6) << std::setprecision(2) << atom.charge << "\n";
    }
    if (mol.atoms.size() > display_limit) {
        std::cout << "│   ... (" << (mol.atoms.size() - display_limit) << " more)\n";
    }
    
    // Bond data
    if (!mol.bonds.empty()) {
        std::cout << "│\n│ Bonds (" << mol.bonds.size() << "):\n";
        std::cout << "│   Idx | Atom I | Atom J | Order | Length (Å)\n";
        std::cout << "│   " << std::string(60, '─') << "\n";
        
        size_t bond_limit = std::min(mol.bonds.size(), size_t(10));
        for (size_t i = 0; i < bond_limit; ++i) {
            const auto& bond = mol.bonds[i];
            const auto& a1 = mol.atoms[bond.atom_i];
            const auto& a2 = mol.atoms[bond.atom_j];
            
            double dx = a1.position[0] - a2.position[0];
            double dy = a1.position[1] - a2.position[1];
            double dz = a1.position[2] - a2.position[2];
            double length = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            std::cout << "│   " << std::setw(3) << i << " | "
                      << std::setw(6) << bond.atom_i << " | "
                      << std::setw(6) << bond.atom_j << " | "
                      << std::setw(5) << std::setprecision(1) << bond.bond_order << " | "
                      << std::setw(9) << std::setprecision(3) << length << "\n";
        }
        if (mol.bonds.size() > bond_limit) {
            std::cout << "│   ... (" << (mol.bonds.size() - bond_limit) << " more)\n";
        }
    }
    
    // Computed properties
    std::cout << "│\n│ Properties:\n";
    std::cout << "│   Formula:     " << compute_formula(mol) << "\n";
    std::cout << "│   Mol. Mass:   " << std::fixed << std::setprecision(3) 
              << compute_molecular_mass(mol) << " amu\n";
    
    auto com = compute_center_of_mass(mol);
    std::cout << "│   Center Mass: (" 
              << std::setprecision(3) << com[0] << ", " 
              << com[1] << ", " << com[2] << ") Å\n";
    
    auto center = mol.get_center();
    std::cout << "│   Centroid:    (" 
              << std::setprecision(3) << center[0] << ", " 
              << center[1] << ", " << center[2] << ") Å\n";
    
    std::cout << "└" << std::string(64, '─') << "\n";
}

// ============================================================================
// Test Cases
// ============================================================================

bool test_load_and_validate(const std::string& filepath, ResourceMonitor& monitor) {
    std::cout << "\n📄 Testing: " << fs::path(filepath).filename().string() << "\n";
    
    monitor.start();
    auto result = load_molecule(filepath, true);
    auto metrics = monitor.stop();
    
    if (!result.is_ok()) {
        std::cout << "❌ FAILED: " << result.error().to_string() << "\n";
        return false;
    }
    
    std::cout << "✓ Load successful\n";
    
    // Validate geometry
    auto validation = validate_geometry(result.value());
    if (!validation.is_ok()) {
        std::cout << "⚠ Validation: " << validation.error().to_string() << "\n";
    } else {
        std::cout << "✓ Geometry valid\n";
    }
    
    // Validate units
    auto unit_check = validate_units_assumed(result.value());
    if (!unit_check.is_ok()) {
        std::cout << "⚠ Unit check: " << unit_check.message() << "\n";
    } else {
        std::cout << "✓ Units within expected ranges\n";
    }
    
    // Validate bonds
    if (!result.value().bonds.empty()) {
        auto bond_check = validate_bonds(result.value());
        if (!bond_check.is_ok()) {
            std::cout << "⚠ Bond validation: " << bond_check.message() << "\n";
        } else {
            std::cout << "✓ Bonds valid\n";
        }
    }
    
    show_backend_structure(result.value());
    print_metrics(metrics);
    
    return true;
}

bool test_round_trip(const std::string& filepath, ResourceMonitor& monitor) {
    print_section("Round-Trip Test (Load → Save → Reload)");
    
    // Load original
    auto original = load_molecule(filepath, true);
    if (!original.is_ok()) {
        std::cout << "❌ Failed to load original\n";
        return false;
    }
    
    // Save to temporary file
    std::string temp_file = "test_output_temp.xyz";
    
    monitor.start();
    auto save_status = save_molecule(temp_file, original.value(), false);
    auto save_metrics = monitor.stop();
    
    if (!save_status.is_ok()) {
        std::cout << "❌ Save failed: " << save_status.message() << "\n";
        return false;
    }
    std::cout << "│ Save: " << save_metrics.elapsed_time.count() / 1000.0 << " ms\n";
    
    // Reload
    monitor.start();
    auto reloaded = load_molecule(temp_file, true);
    auto load_metrics = monitor.stop();
    
    if (!reloaded.is_ok()) {
        std::cout << "❌ Reload failed: " << reloaded.error().to_string() << "\n";
        fs::remove(temp_file);
        return false;
    }
    std::cout << "│ Reload: " << load_metrics.elapsed_time.count() / 1000.0 << " ms\n";
    
    // Compare
    bool atoms_match = (original.value().atoms.size() == reloaded.value().atoms.size());
    bool formula_match = (compute_formula(original.value()) == compute_formula(reloaded.value()));
    
    std::cout << "│\n│ Verification:\n";
    std::cout << "│   Atom count:  " << (atoms_match ? "✓" : "❌") << "\n";
    std::cout << "│   Formula:     " << (formula_match ? "✓" : "❌") << "\n";
    
    fs::remove(temp_file);
    std::cout << "└" << std::string(64, '─') << "\n";
    
    return atoms_match && formula_match;
}

bool test_bond_detection(const std::string& filepath, ResourceMonitor& monitor) {
    print_section("Bond Detection Test");
    
    auto mol_result = load_molecule(filepath, false);  // Don't auto-detect
    if (!mol_result.is_ok()) {
        std::cout << "❌ Load failed\n";
        return false;
    }
    
    auto mol = mol_result.value();
    
    std::cout << "│ Before detection: " << mol.bonds.size() << " bonds\n";
    
    monitor.start();
    int num_bonds = detect_bonds(mol, 1.2);
    auto metrics = monitor.stop();
    
    std::cout << "│ After detection:  " << num_bonds << " bonds\n";
    std::cout << "│ Detection time:   " << metrics.elapsed_time.count() / 1000.0 << " ms\n";
    std::cout << "└" << std::string(64, '─') << "\n";
    
    return true;
}

// ============================================================================
// Main Test Suite
// ============================================================================

int main(int argc, char* argv[]) {
    print_header("XYZ Suite Tester - Production I/O Validation");
    
    std::cout << "\nTest Configuration:\n";
    std::cout << "  Standard: C++20\n";
    std::cout << "  API Layer: vsepr::api\n";
    std::cout << "  Backend: vsepr::io\n";
    std::cout << "  Units: Ångström, elementary charge, amu\n";
    
    ResourceMonitor monitor;
    int passed = 0;
    int failed = 0;
    
    // Find test molecules
    std::vector<std::string> test_files;
    
    if (argc > 1) {
        // User-specified files
        for (int i = 1; i < argc; ++i) {
            test_files.push_back(argv[i]);
        }
    } else {
        // Scan benchmark_results/
        std::string bench_dir = "benchmark_results";
        if (fs::exists(bench_dir)) {
            for (const auto& entry : fs::directory_iterator(bench_dir)) {
                if (entry.path().extension() == ".xyz") {
                    test_files.push_back(entry.path().string());
                }
            }
        }
        
        // Fallback: look in current directory
        if (test_files.empty()) {
            std::cout << "\n⚠ No benchmark_results/ found, scanning current directory...\n";
            for (const auto& entry : fs::directory_iterator(".")) {
                if (entry.path().extension() == ".xyz") {
                    test_files.push_back(entry.path().string());
                }
            }
        }
    }
    
    if (test_files.empty()) {
        std::cout << "\n❌ No .xyz files found!\n";
        std::cout << "Usage: " << argv[0] << " [file1.xyz file2.xyz ...]\n";
        return 1;
    }
    
    std::cout << "\nFound " << test_files.size() << " molecule(s) to test\n";
    
    // Run tests
    for (const auto& filepath : test_files) {
        print_header("Test Molecule: " + fs::path(filepath).filename().string());
        
        bool success = true;
        
        // Test 1: Load and validate
        if (!test_load_and_validate(filepath, monitor)) {
            success = false;
        }
        
        // Test 2: Round-trip
        if (success && !test_round_trip(filepath, monitor)) {
            success = false;
        }
        
        // Test 3: Bond detection
        if (success && !test_bond_detection(filepath, monitor)) {
            success = false;
        }
        
        if (success) {
            passed++;
            std::cout << "\n✓ All tests passed for " << fs::path(filepath).filename().string() << "\n";
        } else {
            failed++;
            std::cout << "\n❌ Some tests failed for " << fs::path(filepath).filename().string() << "\n";
        }
    }
    
    // Summary
    print_header("Test Summary");
    std::cout << "\nResults:\n";
    std::cout << "  Passed: " << passed << "/" << test_files.size() << "\n";
    std::cout << "  Failed: " << failed << "/" << test_files.size() << "\n";
    std::cout << "  Success Rate: " << std::fixed << std::setprecision(1) 
              << (100.0 * passed / test_files.size()) << "%\n\n";
    
    return (failed == 0) ? 0 : 1;
}
