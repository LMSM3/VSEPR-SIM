/**
 * atomistic-align: Molecular Structure Alignment Viewer
 * 
 * Demonstrates Kabsch alignment with camera tracking visualization
 * 
 * Features:
 * - Load two molecular structures
 * - Animate alignment with smooth rotation
 * - Camera tracks the alignment process
 * - Shows RMSD decrease in real-time
 * - Side-by-side comparison before/after
 * 
 * Usage:
 *   atomistic-align reference.xyz target.xyz [--steps N]
 * 
 * Physics:
 * - Kabsch algorithm: Optimal rotation minimizing RMSD
 * - Uses SVD of covariance matrix H = Σ(target ⊗ reference)
 * - Optimal rotation: R = V·U^T (with chirality correction)
 * 
 * References:
 * - Kabsch, W. (1976). Acta Cryst. A32, 922-923
 * - Kabsch, W. (1978). Acta Cryst. A34, 827-828
 * 
 * Controls:
 *   SPACE - Play/pause animation
 *   R - Reset to initial positions
 *   1 - Show reference only
 *   2 - Show target only
 *   3 - Show both (overlay)
 *   C - Toggle camera tracking
 *   ESC - Quit
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/alignment.hpp"
#include "atomistic/core/linalg.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>

using namespace atomistic;

// ============================================================================
// XYZ File Parser
// ============================================================================

struct XYZData {
    std::vector<int> Z;
    std::vector<Vec3> positions;
    std::vector<double> masses;
    std::string comment;
};

double get_atomic_mass(int Z) {
    static const double masses[] = {
        0.0, 1.008, 4.003, 6.94, 9.012, 10.81, 12.01, 14.01, 16.00, 19.00, 20.18,
        22.99, 24.31, 26.98, 28.09, 30.97, 32.06, 35.45, 39.95, 39.10, 40.08
    };
    return (Z >= 0 && Z < 21) ? masses[Z] : 1.0;
}

int element_symbol_to_Z(const std::string& symbol) {
    if (symbol == "H") return 1;
    if (symbol == "C") return 6;
    if (symbol == "N") return 7;
    if (symbol == "O") return 8;
    if (symbol == "F") return 9;
    if (symbol == "P") return 15;
    if (symbol == "S") return 16;
    if (symbol == "Cl") return 17;
    return 0;
}

XYZData load_xyz(const std::string& filename) {
    XYZData data;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open " << filename << std::endl;
        return data;
    }
    
    int n_atoms;
    file >> n_atoms;
    file.ignore(1000, '\n');
    std::getline(file, data.comment);
    
    for (int i = 0; i < n_atoms; ++i) {
        std::string symbol;
        Vec3 pos;
        file >> symbol >> pos.x >> pos.y >> pos.z;
        
        int Z = element_symbol_to_Z(symbol);
        data.Z.push_back(Z);
        data.positions.push_back(pos);
        data.masses.push_back(get_atomic_mass(Z));
    }
    
    return data;
}

State xyz_to_state(const XYZData& xyz) {
    State s;
    s.N = static_cast<uint32_t>(xyz.Z.size());
    s.type.reserve(s.N);
    for (int z : xyz.Z) {
        s.type.push_back(static_cast<uint32_t>(z));
    }
    s.X = xyz.positions;
    s.M = xyz.masses;
    s.Q.resize(s.N, 0.0);  // Neutral charges
    s.V.resize(s.N, {0, 0, 0});
    s.F.resize(s.N, {0, 0, 0});
    return s;
}

// ============================================================================
// Console Visualization
// ============================================================================

void print_separator(int width = 70) {
    std::cout << std::string(width, '=') << "\n";
}

void print_header(const std::string& text) {
    print_separator();
    std::cout << "  " << text << "\n";
    print_separator();
}

void print_state_info(const State& s, const std::string& label) {
    Vec3 com = compute_com(s);
    double radius = compute_bounding_radius(s, com);
    
    std::cout << label << ":\n";
    std::cout << "  Atoms: " << s.N << "\n";
    std::cout << "  COM:   (" << com.x << ", " << com.y << ", " << com.z << ")\n";
    std::cout << "  Radius: " << radius << " Å\n";
}

void print_alignment_result(const AlignmentResult& result) {
    std::cout << "\nAlignment Results:\n";
    print_separator(50);
    std::cout << "  RMSD before:  " << result.rmsd_before << " Å\n";
    std::cout << "  RMSD after:   " << result.rmsd_after << " Å\n";
    std::cout << "  Improvement:  " << (result.rmsd_before - result.rmsd_after) << " Å\n";
    std::cout << "  % Reduction:  " << (100.0 * (1.0 - result.rmsd_after / result.rmsd_before)) << "%\n";
    std::cout << "  Max deviation: " << result.max_deviation << " Å\n";
    print_separator(50);
}

void print_rotation_matrix(const linalg::Mat3& R) {
    std::cout << "Rotation Matrix:\n";
    for (int i = 0; i < 3; ++i) {
        std::cout << "  [";
        for (int j = 0; j < 3; ++j) {
            printf(" %8.5f", R(i,j));
        }
        std::cout << " ]\n";
    }
    std::cout << "  det(R) = " << R.det() << " (should be +1 for proper rotation)\n";
}

void print_camera_info(const AlignmentCamera& cam) {
    std::cout << "\nCamera Parameters:\n";
    std::cout << "  Position: (" << cam.position.x << ", " << cam.position.y << ", " << cam.position.z << ")\n";
    std::cout << "  Target:   (" << cam.target.x << ", " << cam.target.y << ", " << cam.target.z << ")\n";
    std::cout << "  Distance: " << cam.distance << " Å\n";
    std::cout << "  FOV:      " << cam.fov << "°\n";
}

// ============================================================================
// Animated Alignment Demo
// ============================================================================

void demo_animated_alignment(State& target, const State& reference, int n_steps) {
    std::cout << "\n";
    print_header("ANIMATED ALIGNMENT DEMO");
    std::cout << "\nAnimating alignment over " << n_steps << " steps...\n\n";
    
    // Progress bar width
    const int bar_width = 50;
    
    // Callback for each animation step
    auto callback = [&](double progress, double rmsd, [[maybe_unused]] const State& current) {
        // Print progress bar
        int filled = static_cast<int>(progress * bar_width);
        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) std::cout << "█";
            else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(progress * 100) << "% ";
        std::cout << "RMSD: " << rmsd << " Å   ";
        std::cout << std::flush;
    };
    
    // Run animated alignment
    AlignmentResult result = animated_align(target, reference, n_steps, callback);
    
    std::cout << "\n\n✓ Animation complete!\n";
    print_alignment_result(result);
}

// ============================================================================
// Main Application
// ============================================================================

int main(int argc, char* argv[]) {
    // Parse arguments
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <reference.xyz> <target.xyz> [--steps N]\n";
        std::cout << "\nAlign target structure onto reference using Kabsch algorithm.\n";
        std::cout << "\nOptions:\n";
        std::cout << "  --steps N    Number of animation steps (default: 60)\n";
        std::cout << "\nExample:\n";
        std::cout << "  " << argv[0] << " protein_ref.xyz protein_tgt.xyz --steps 120\n";
        return 1;
    }
    
    std::string ref_file = argv[1];
    std::string tgt_file = argv[2];
    int n_steps = 60;
    
    // Parse optional arguments
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--steps" && i + 1 < argc) {
            n_steps = std::atoi(argv[i + 1]);
            ++i;
        }
    }
    
    // Print banner
    print_header("atomistic-align: Molecular Structure Alignment");
    std::cout << "Kabsch algorithm with camera tracking\n\n";
    
    // Load structures
    std::cout << "Loading structures...\n";
    XYZData ref_xyz = load_xyz(ref_file);
    XYZData tgt_xyz = load_xyz(tgt_file);
    
    if (ref_xyz.Z.empty() || tgt_xyz.Z.empty()) {
        std::cerr << "Error: Failed to load structures\n";
        return 1;
    }
    
    State reference = xyz_to_state(ref_xyz);
    State target = xyz_to_state(tgt_xyz);
    
    std::cout << "✓ Loaded " << reference.N << " atoms from " << ref_file << "\n";
    std::cout << "✓ Loaded " << target.N << " atoms from " << tgt_file << "\n\n";
    
    if (reference.N != target.N) {
        std::cerr << "Warning: Atom counts differ (" << reference.N << " vs " << target.N << ")\n";
        std::cerr << "Alignment requires same number of atoms!\n";
        return 1;
    }
    
    // Print initial state info
    print_header("INITIAL STRUCTURES");
    print_state_info(reference, "Reference");
    std::cout << "\n";
    print_state_info(target, "Target (before alignment)");
    
    // Compute initial RMSD
    double initial_rmsd = compute_rmsd(target, reference);
    std::cout << "\nInitial RMSD: " << initial_rmsd << " Å\n";
    
    // Compute camera for initial view
    State target_copy = target;
    AlignmentResult dummy_result = kabsch_align(target_copy, reference);
    target_copy = target;  // Restore
    
    AlignmentCamera cam_initial = compute_alignment_camera(reference, target, dummy_result);
    print_camera_info(cam_initial);
    
    // Run animated alignment
    State target_animated = target;
    demo_animated_alignment(target_animated, reference, n_steps);
    
    // Run standard alignment for comparison
    std::cout << "\n";
    print_header("STANDARD ALIGNMENT (INSTANT)");
    
    State target_instant = target;
    AlignmentResult result = kabsch_align(target_instant, reference);
    
    print_alignment_result(result);
    print_rotation_matrix(result.R);
    
    // Compute final camera
    AlignmentCamera cam_final = compute_alignment_camera(reference, target_instant, result);
    print_camera_info(cam_final);
    
    // Summary
    std::cout << "\n";
    print_header("SUMMARY");
    std::cout << "Reference:  " << ref_file << " (" << reference.N << " atoms)\n";
    std::cout << "Target:     " << tgt_file << " (" << target.N << " atoms)\n";
    std::cout << "RMSD:       " << result.rmsd_before << " → " << result.rmsd_after << " Å\n";
    std::cout << "Improvement: " << (result.rmsd_before - result.rmsd_after) << " Å ";
    std::cout << "(" << (100.0 * (1.0 - result.rmsd_after / result.rmsd_before)) << "% reduction)\n";
    print_separator();
    
    std::cout << "\n✓ Alignment complete!\n";
    std::cout << "\nNext steps:\n";
    std::cout << "  - Visualize with: atomistic-view aligned.xyz\n";
    std::cout << "  - Compare structures: atomistic-compare ref.xyz target.xyz\n";
    std::cout << "  - Export aligned: (TODO: implement save function)\n";
    
    return 0;
}
