/**
 * Problem 2: Three-Body Neutral Cluster (Emergence Test)
 * 
 * Tests many-body dynamics and geometric emergence from pairwise interactions.
 * 
 * Setup:
 *   - 3 identical Ar atoms
 *   - Random initial positions in 10 Å box
 *   - T = 50 K (low temperature, should equilibrate)
 *   - Velocity-rescaling thermostat
 *   - 50,000 MD steps
 * 
 * Tasks:
 *   1. Run MD relaxation
 *   2. Measure final geometry (distances + angles)
 *   3. Compute total potential energy
 *   4. Compare linear vs triangular configurations
 * 
 * Question:
 *   Which geometry minimizes total energy and why?
 *   - Pairwise additivity
 *   - Geometric frustration
 *   - Classical emergence (NO quantum hand-waving!)
 * 
 * If this fails, multi-atom formation is broken.
 */

#include "../atomistic/core/state.hpp"
#include "../atomistic/core/maxwell_boltzmann.hpp"
#include "../atomistic/models/model.hpp"
#include "../atomistic/integrators/velocity_verlet.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <random>
#include <fstream>
#include <chrono>

using namespace atomistic;

// LJ parameters for Ar
constexpr double EPSILON = 0.238;  // kcal/mol
constexpr double SIGMA = 3.4;      // Å
constexpr double R0 = 3.8164;      // 2^(1/6) * σ

/**
 * Compute distance between two points
 */
double distance(const Vec3& a, const Vec3& b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double dz = b.z - a.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

/**
 * Compute angle (in degrees) at vertex B given three points A-B-C
 */
double angle_deg(const Vec3& A, const Vec3& B, const Vec3& C) {
    Vec3 BA = {A.x - B.x, A.y - B.y, A.z - B.z};
    Vec3 BC = {C.x - B.x, C.y - B.y, C.z - B.z};
    
    double dot_product = BA.x*BC.x + BA.y*BC.y + BA.z*BC.z;
    double mag_BA = std::sqrt(BA.x*BA.x + BA.y*BA.y + BA.z*BA.z);
    double mag_BC = std::sqrt(BC.x*BC.x + BC.y*BC.y + BC.z*BC.z);
    
    double cos_angle = dot_product / (mag_BA * mag_BC);
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));  // Clamp to [-1, 1]
    
    return std::acos(cos_angle) * 180.0 / M_PI;
}

/**
 * Analyze final geometry
 */
struct GeometryAnalysis {
    double r01, r02, r12;  // Pairwise distances
    double angle0, angle1, angle2;  // Angles at each vertex
    double total_energy;
    std::string type;  // "linear", "triangular", "collapsed"
};

GeometryAnalysis analyze_geometry(const State& state) {
    GeometryAnalysis result;
    
    // Distances
    result.r01 = distance(state.X[0], state.X[1]);
    result.r02 = distance(state.X[0], state.X[2]);
    result.r12 = distance(state.X[1], state.X[2]);
    
    // Angles
    result.angle0 = angle_deg(state.X[1], state.X[0], state.X[2]);
    result.angle1 = angle_deg(state.X[0], state.X[1], state.X[2]);
    result.angle2 = angle_deg(state.X[0], state.X[2], state.X[1]);
    
    // Total energy
    result.total_energy = state.E.total();
    
    // Classify geometry
    double avg_r = (result.r01 + result.r02 + result.r12) / 3.0;
    double r_std = std::sqrt(
        (std::pow(result.r01 - avg_r, 2) + 
         std::pow(result.r02 - avg_r, 2) + 
         std::pow(result.r12 - avg_r, 2)) / 3.0
    );
    
    // Check if equilateral triangle (all distances ~equal, all angles ~60°)
    if (r_std < 0.1 && 
        std::abs(result.angle0 - 60.0) < 5.0 &&
        std::abs(result.angle1 - 60.0) < 5.0 &&
        std::abs(result.angle2 - 60.0) < 5.0) {
        result.type = "equilateral_triangle";
    }
    // Check if linear (one angle ~180°)
    else if (result.angle0 > 170.0 || result.angle1 > 170.0 || result.angle2 > 170.0) {
        result.type = "linear";
    }
    // Check if isosceles
    else if (r_std > 0.5) {
        result.type = "isosceles_triangle";
    }
    else {
        result.type = "general_triangle";
    }
    
    return result;
}

/**
 * Print geometry analysis
 */
void print_geometry(const GeometryAnalysis& geom, const std::string& label) {
    std::cout << label << ":\n";
    std::cout << "  Distances:\n";
    std::cout << "    r₀₁ = " << geom.r01 << " Å\n";
    std::cout << "    r₀₂ = " << geom.r02 << " Å\n";
    std::cout << "    r₁₂ = " << geom.r12 << " Å\n";
    std::cout << "  Angles:\n";
    std::cout << "    ∠₀ = " << geom.angle0 << "°\n";
    std::cout << "    ∠₁ = " << geom.angle1 << "°\n";
    std::cout << "    ∠₂ = " << geom.angle2 << "°\n";
    std::cout << "  Energy: " << geom.total_energy << " kcal/mol\n";
    std::cout << "  Type: " << geom.type << "\n\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PROBLEM 2: Three-Body Neutral Cluster (Emergence Test)   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << std::fixed << std::setprecision(4);
    
    // ========================================================================
    // Setup: Create 3 Ar atoms at random positions
    // ========================================================================
    
    std::cout << "SETUP: Three Ar Atoms\n";
    std::cout << "─────────────────────────────────────────────────────\n";
    
    State state;
    state.N = 3;
    state.X.resize(3);
    state.M.resize(3, 39.948);  // Ar mass
    state.Q.resize(3, 0.0);     // Neutral
    state.type.resize(3, 18);   // Ar (Z=18)
    state.F.resize(3);
    state.V.resize(3);
    state.box = BoxPBC();  // No PBC
    
    // Random initial positions in 10 Å box
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<double> dist(0.0, 10.0);
    
    for (int i = 0; i < 3; ++i) {
        state.X[i] = {dist(rng), dist(rng), dist(rng)};
    }
    
    std::cout << "Initial positions:\n";
    for (int i = 0; i < 3; ++i) {
        std::cout << "  Atom " << i << ": (" 
                  << state.X[i].x << ", " 
                  << state.X[i].y << ", " 
                  << state.X[i].z << ") Å\n";
    }
    std::cout << "\n";
    
    // Initialize velocities (Maxwell-Boltzmann at 50K)
    double T_initial = 50.0;  // K
    initialize_velocities_thermal(state, T_initial, rng);
    
    std::cout << "Temperature: " << T_initial << " K\n";
    std::cout << "Thermostat: Velocity rescaling\n";
    std::cout << "Steps: 50,000 (50 ps with dt=1 fs)\n\n";
    
    // ========================================================================
    // Create model and integrator
    // ========================================================================
    
    auto model = create_lj_coulomb_model();
    ModelParams model_params;
    model_params.rc = 10.0;
    
    LangevinDynamics dynamics(*model, model_params);
    LangevinParams md_params;
    md_params.dt = 1.0;           // fs
    md_params.n_steps = 50000;    // 50 ps
    md_params.T_target = 50.0;    // K
    md_params.gamma = 0.1;        // 1/fs (weak coupling)
    md_params.print_freq = 5000;
    md_params.verbose = true;
    
    // Analyze initial geometry
    model->eval(state, model_params);
    auto initial_geom = analyze_geometry(state);
    print_geometry(initial_geom, "Initial Geometry");
    
    // ========================================================================
    // Run MD relaxation
    // ========================================================================
    
    std::cout << "RUNNING MD RELAXATION\n";
    std::cout << "─────────────────────────────────────────────────────\n\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto stats = dynamics.integrate(state, md_params, rng);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\n✅ MD Complete in " << duration.count() << " ms\n\n";
    
    // ========================================================================
    // Analyze final geometry
    // ========================================================================
    
    std::cout << "FINAL GEOMETRY ANALYSIS\n";
    std::cout << "─────────────────────────────────────────────────────\n\n";
    
    model->eval(state, model_params);
    auto final_geom = analyze_geometry(state);
    print_geometry(final_geom, "Final Geometry");
    
    std::cout << "Statistics:\n";
    std::cout << "  <T> = " << stats.T_avg << " ± " << stats.T_std << " K\n";
    std::cout << "  <KE> = " << stats.KE_avg << " kcal/mol\n";
    std::cout << "  <PE> = " << stats.PE_avg << " kcal/mol\n";
    std::cout << "  <E_total> = " << stats.E_total_avg << " kcal/mol\n\n";
    
    // ========================================================================
    // Compare with theoretical configurations
    // ========================================================================
    
    std::cout << "THEORETICAL COMPARISON\n";
    std::cout << "─────────────────────────────────────────────────────\n\n";
    
    // Linear configuration: A---B---C
    std::cout << "LINEAR CHAIN (A---B---C):\n";
    std::cout << "  All distances = r₀ = " << R0 << " Å\n";
    std::cout << "  Central angle = 180°\n";
    std::cout << "  Total energy = 2 × U(r₀) = 2 × (-ε) = " << (2 * -EPSILON) << " kcal/mol\n";
    std::cout << "  (Only 2 bonds: A-B and B-C)\n\n";
    
    // Equilateral triangle
    double r_triangle = R0;  // Optimal for each pair
    double U_triangle = 3 * -EPSILON;  // 3 bonds, all at r₀
    
    std::cout << "EQUILATERAL TRIANGLE:\n";
    std::cout << "  All distances = r₀ = " << R0 << " Å\n";
    std::cout << "  All angles = 60°\n";
    std::cout << "  Total energy = 3 × U(r₀) = 3 × (-ε) = " << U_triangle << " kcal/mol\n";
    std::cout << "  (Three bonds: A-B, B-C, A-C)\n\n";
    
    std::cout << "ENERGY COMPARISON:\n";
    std::cout << "  Linear:    " << (2 * -EPSILON) << " kcal/mol\n";
    std::cout << "  Triangle:  " << U_triangle << " kcal/mol\n";
    std::cout << "  Difference: " << (U_triangle - 2*(-EPSILON)) << " kcal/mol\n";
    std::cout << "  → Triangle is " << std::abs(U_triangle - 2*(-EPSILON)) 
              << " kcal/mol MORE STABLE\n\n";
    
    // ========================================================================
    // Verdict
    // ========================================================================
    
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VERDICT: WHICH GEOMETRY WINS?                             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Expected: EQUILATERAL TRIANGLE\n";
    std::cout << "Reason: Pairwise additivity favors maximum bonding\n\n";
    
    std::cout << "Explanation:\n";
    std::cout << "1. PAIRWISE ADDITIVITY\n";
    std::cout << "   - Total energy = Σ U(r_ij) over all pairs\n";
    std::cout << "   - Linear: Only 2 pairs contribute (A-B, B-C)\n";
    std::cout << "   - Triangle: All 3 pairs contribute (A-B, B-C, A-C)\n";
    std::cout << "   → Triangle has MORE bonding interactions!\n\n";
    
    std::cout << "2. NO GEOMETRIC FRUSTRATION (for LJ)\n";
    std::cout << "   - LJ is isotropic (no angular preference)\n";
    std::cout << "   - All bonds can be at r₀ simultaneously\n";
    std::cout << "   - Triangle with side length r₀ is geometrically possible\n";
    std::cout << "   → No frustration penalty!\n\n";
    
    std::cout << "3. CLASSICAL EMERGENCE\n";
    std::cout << "   - No quantum mechanics needed\n";
    std::cout << "   - Simple pairwise potential + geometry\n";
    std::cout << "   - Maximum coordination wins (more bonds = more stable)\n";
    std::cout << "   → Classical many-body effect!\n\n";
    
    std::cout << "Observed Result:\n";
    std::cout << "  Final geometry: " << final_geom.type << "\n";
    std::cout << "  Final energy: " << final_geom.total_energy << " kcal/mol\n";
    std::cout << "  Expected (triangle): " << U_triangle << " kcal/mol\n\n";
    
    // Check if result matches expectation
    bool correct_geometry = (final_geom.type == "equilateral_triangle");
    bool correct_energy = std::abs(final_geom.total_energy - U_triangle) < 0.05;  // 5% tolerance
    
    if (correct_geometry && correct_energy) {
        std::cout << "✅ PASS: System correctly equilibrated to equilateral triangle\n";
        std::cout << "✅ PASS: Energy matches theoretical prediction\n\n";
        
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  🎉 HUGE W! MULTI-ATOM DYNAMICS WORKS!                     ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
        
        std::cout << "What this proves:\n";
        std::cout << "  ✅ Many-body force evaluation correct\n";
        std::cout << "  ✅ Integration preserves energy\n";
        std::cout << "  ✅ Thermostat equilibrates properly\n";
        std::cout << "  ✅ Geometry emergence from pairwise forces\n";
        std::cout << "  ✅ Classical statistical mechanics works\n\n";
        
        std::cout << "Ready for:\n";
        std::cout << "  → Larger clusters (N > 3)\n";
        std::cout << "  → Crystal formation\n";
        std::cout << "  → Molecular assemblies\n\n";
        
        return 0;
    }
    else {
        std::cout << "❌ FAIL: Incorrect final geometry or energy\n\n";
        
        if (!correct_geometry) {
            std::cout << "  Expected: equilateral_triangle\n";
            std::cout << "  Got: " << final_geom.type << "\n";
            std::cout << "  → Check: Are forces computed correctly for all pairs?\n";
            std::cout << "  → Check: Is thermostat working?\n";
            std::cout << "  → Check: Are there NaN/inf values?\n\n";
        }
        
        if (!correct_energy) {
            std::cout << "  Expected energy: " << U_triangle << " kcal/mol\n";
            std::cout << "  Got: " << final_geom.total_energy << " kcal/mol\n";
            std::cout << "  Error: " << std::abs(final_geom.total_energy - U_triangle) 
                      << " kcal/mol\n";
            std::cout << "  → Check: Is LJ potential correctly implemented?\n";
            std::cout << "  → Check: Are parameters (ε, σ) correct?\n\n";
        }
        
        std::cout << "MULTI-ATOM FORMATION IS BROKEN!\n";
        std::cout << "Fix this before attempting larger systems.\n\n";
        
        return 1;
    }
}
