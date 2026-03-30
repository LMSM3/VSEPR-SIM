#include "io_api.h"
#include "xyz_format.h"
#include "xyzc_format.h"
#include <iostream>
#include <chrono>
#include <cstring>

// XYZ Suite Test - Production API validation
// Tests the complete I/O pipeline for XYZ and XYZC formats

void print_usage() {
    std::cout << "XYZ Suite Test - Production API Validator\n";
    std::cout << "Usage: xyz_suite_test [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --test-xyz         Test XYZ format I/O\n";
    std::cout << "  --test-xyzc        Test XYZC thermal format I/O\n";
    std::cout << "  --test-api         Test C API\n";
    std::cout << "  --benchmark        Run performance benchmarks\n";
    std::cout << "  --all              Run all tests (default)\n";
}

bool test_xyz_format() {
    std::cout << "\n=== Testing XYZ Format ===\n";
    
    // Create test molecule
    vsepr::io::XYZMolecule mol;
    mol.comment = "Test molecule - Water";
    
    vsepr::io::XYZAtom o;
    o.element = "O";
    o.position = {0.0, 0.0, 0.0};
    mol.atoms.push_back(o);
    
    vsepr::io::XYZAtom h1;
    h1.element = "H";
    h1.position = {0.96, 0.0, 0.0};
    mol.atoms.push_back(h1);
    
    vsepr::io::XYZAtom h2;
    h2.element = "H";
    h2.position = {-0.24, 0.93, 0.0};
    mol.atoms.push_back(h2);
    
    // Write test
    std::cout << "Writing test molecule to test_output.xyz...\n";
    if (!vsepr::io::write_xyz("test_output.xyz", mol)) {
        std::cerr << "FAILED: Could not write XYZ file\n";
        return false;
    }
    
    // Read test
    std::cout << "Reading test molecule from test_output.xyz...\n";
    vsepr::io::XYZMolecule mol_read;
    if (!vsepr::io::read_xyz("test_output.xyz", mol_read)) {
        std::cerr << "FAILED: Could not read XYZ file\n";
        return false;
    }
    
    // Validate
    if (mol_read.atoms.size() != 3) {
        std::cerr << "FAILED: Expected 3 atoms, got " << mol_read.atoms.size() << "\n";
        return false;
    }
    
    std::cout << "PASSED: XYZ format test\n";
    return true;
}

bool test_xyzc_format() {
    std::cout << "\n=== Testing XYZC Thermal Format ===\n";
    
    // Create test trajectory
    vsepr::thermal::XYZCTrajectory traj;
    
    for (int frame = 0; frame < 3; ++frame) {
        vsepr::thermal::XYZCFrame f;
        f.comment = "Frame " + std::to_string(frame);
        f.total_energy = 100.0 + frame * 10.0;
        f.temperature = 300.0 + frame * 5.0;
        
        vsepr::thermal::XYZCAtom atom;
        atom.element = "C";
        atom.position = {double(frame), 0.0, 0.0};
        atom.energy_transfer = frame * 0.5;
        f.atoms.push_back(atom);
        
        traj.frames.push_back(f);
    }
    
    // Write test
    std::cout << "Writing test trajectory to test_output.xyzc...\n";
    if (!vsepr::thermal::write_xyzc("test_output.xyzc", traj)) {
        std::cerr << "FAILED: Could not write XYZC file\n";
        return false;
    }
    
    // Read test
    std::cout << "Reading test trajectory from test_output.xyzc...\n";
    vsepr::thermal::XYZCTrajectory traj_read;
    if (!vsepr::thermal::read_xyzc("test_output.xyzc", traj_read)) {
        std::cerr << "FAILED: Could not read XYZC file\n";
        return false;
    }
    
    // Validate
    if (traj_read.frames.size() != 3) {
        std::cerr << "FAILED: Expected 3 frames, got " << traj_read.frames.size() << "\n";
        return false;
    }
    
    std::cout << "PASSED: XYZC format test\n";
    return true;
}

bool test_c_api() {
    std::cout << "\n=== Testing C API ===\n";
    
    // Test error handling
    void* mol = nullptr;
    int result = io_read_xyz(nullptr, &mol);
    if (result == 0) {
        std::cerr << "FAILED: Should have failed with nullptr filename\n";
        return false;
    }
    
    std::cout << "PASSED: C API error handling test\n";
    return true;
}

void run_benchmarks() {
    std::cout << "\n=== Running Benchmarks ===\n";
    std::cout << "[TODO] Benchmarks not yet implemented\n";
}

int main(int argc, char* argv[]) {
    std::cout << "XYZ Suite Test v2.0.0\n";
    std::cout << "Production API Validation Suite\n";
    
    bool test_all = true;
    bool test_xyz = false;
    bool test_xyzc = false;
    bool test_api = false;
    bool benchmark = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-xyz") == 0) {
            test_xyz = true;
            test_all = false;
        } else if (std::strcmp(argv[i], "--test-xyzc") == 0) {
            test_xyzc = true;
            test_all = false;
        } else if (std::strcmp(argv[i], "--test-api") == 0) {
            test_api = true;
            test_all = false;
        } else if (std::strcmp(argv[i], "--benchmark") == 0) {
            benchmark = true;
            test_all = false;
        } else if (std::strcmp(argv[i], "--all") == 0) {
            test_all = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage();
            return 1;
        }
    }
    
    int failures = 0;
    
    if (test_all || test_xyz) {
        if (!test_xyz_format()) {
            failures++;
        }
    }
    
    if (test_all || test_xyzc) {
        if (!test_xyzc_format()) {
            failures++;
        }
    }
    
    if (test_all || test_api) {
        if (!test_c_api()) {
            failures++;
        }
    }
    
    if (benchmark) {
        run_benchmarks();
    }
    
    std::cout << "\n=== Test Summary ===\n";
    if (failures == 0) {
        std::cout << "All tests PASSED!\n";
        return 0;
    } else {
        std::cout << failures << " test(s) FAILED\n";
        return 1;
    }
}
