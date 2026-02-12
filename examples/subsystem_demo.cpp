/**
 * VSEPR-Sim Subsystem Integration Demo
 * Demonstrates deterministic metallic simulation calls from main system
 * MatLabForC++ style external package integration
 */

#include "subsystem/metallic_sim.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

using namespace vsepr::subsystem;

// ============================================================================
// Demo Utilities
// ============================================================================

void print_header(const std::string& title) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(62) << title << " ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
}

void print_properties(const MechanicalProperties& props) {
    std::cout << "  Material:        " << props.material << "\n";
    std::cout << "  Tensile (σ_b):   " << std::fixed << std::setprecision(1) 
              << props.tensile_strength_MPa << " MPa\n";
    std::cout << "  Yield (σ_0.2):   " << props.yield_strength_MPa << " MPa\n";
    std::cout << "  Elongation (σ_s): " << props.elongation_percent << " %\n";
    std::cout << "  Hardness:        " << props.hardness_HRB << " HRB\n";
}

void print_failure_analysis(const MetallicSimulator::FailureAnalysis& analysis) {
    std::cout << "  Applied Stress:     " << std::fixed << std::setprecision(2) 
              << analysis.max_stress_MPa << " MPa\n";
    std::cout << "  Von Mises Stress:   " << analysis.von_mises_stress_MPa << " MPa\n";
    std::cout << "  Safety Factor:      " << analysis.safety_factor << "\n";
    std::cout << "  Status:             " << (analysis.will_fail ? "⚠️  WILL FAIL" : "✅ SAFE") << "\n";
    std::cout << "  Failure Mode:       " << analysis.failure_mode << "\n";
}

// ============================================================================
// Demo 1: Database Query (Deterministic Lookup)
// ============================================================================

void demo_database_queries() {
    print_header("Demo 1: Database Queries (Deterministic Lookup)");
    
    MetallicSimulator sim;
    
    std::cout << "\n[Query 1] Get specific material: Hastelloy C-276\n";
    auto props = sim.get_material("Hastelloy C-276");
    print_properties(props);
    
    std::cout << "\n[Query 2] Search materials with tensile > 750 MPa\n";
    auto high_strength = sim.search_materials(750.0, 1000.0, 0.0, 500.0);
    std::cout << "  Found " << high_strength.size() << " materials:\n";
    for (const auto& mat : high_strength) {
        std::cout << "    - " << mat.material << " (σ_b = " << mat.tensile_strength_MPa << " MPa)\n";
    }
    
    std::cout << "\n[Query 3] List all Hastelloy alloys\n";
    auto all_materials = sim.get_all_materials();
    std::cout << "  Total materials in database: " << all_materials.size() << "\n";
    std::cout << "  Hastelloy alloys:\n";
    for (const auto& mat : all_materials) {
        if (mat.material.find("Hastelloy") != std::string::npos) {
            std::cout << "    - " << std::setw(20) << std::left << mat.material 
                      << " | σ_b: " << std::setw(6) << std::right << mat.tensile_strength_MPa 
                      << " MPa | Elongation: " << mat.elongation_percent << "%\n";
        }
    }
}

// ============================================================================
// Demo 2: Property Prediction (MATLAB-style)
// ============================================================================

void demo_property_prediction() {
    print_header("Demo 2: Property Prediction (MATLAB-style Interpolation)");
    
    MetallicSimulator sim;
    
    // Typical Hastelloy C-276 composition: 57% Ni, 16% Cr, 16% Mo, 11% Fe
    std::map<std::string, double> composition1;
    composition1["Ni"] = 57.0;
    composition1["Cr"] = 16.0;
    composition1["Mo"] = 16.0;
    composition1["Fe"] = 11.0;
    
    std::cout << "\n[Prediction 1] Ni-Cr-Mo Alloy (57-16-16)\n";
    auto result1 = sim.predict_properties(composition1, 298.15);
    std::cout << "  Composition: Ni=" << composition1["Ni"] << "%, Cr=" 
              << composition1["Cr"] << "%, Mo=" << composition1["Mo"] << "%\n";
    std::cout << "  Confidence: " << std::fixed << std::setprecision(2) 
              << (result1.confidence * 100.0) << "%\n";
    std::cout << "  Method: " << result1.method << "\n";
    print_properties(result1.properties);
    
    // Modified composition: higher Mo content
    std::map<std::string, double> composition2;
    composition2["Ni"] = 50.0;
    composition2["Cr"] = 20.0;
    composition2["Mo"] = 18.0;
    composition2["Fe"] = 12.0;
    
    std::cout << "\n[Prediction 2] Modified Alloy (50-20-18) at 500K\n";
    auto result2 = sim.predict_properties(composition2, 500.0);
    std::cout << "  Composition: Ni=" << composition2["Ni"] << "%, Cr=" 
              << composition2["Cr"] << "%, Mo=" << composition2["Mo"] << "%\n";
    std::cout << "  Temperature: 500 K (227°C)\n";
    std::cout << "  Confidence: " << (result2.confidence * 100.0) << "%\n";
    print_properties(result2.properties);
}

// ============================================================================
// Demo 3: Failure Analysis (Deterministic FEA-lite)
// ============================================================================

void demo_failure_analysis() {
    print_header("Demo 3: Failure Analysis (Deterministic Safety Assessment)");
    
    MetallicSimulator sim;
    auto material = sim.get_material("Hastelloy C-276");
    
    std::cout << "\n[Analysis 1] Safe loading condition\n";
    std::cout << "  Material: " << material.material << "\n";
    auto analysis1 = sim.analyze_failure(material, 150.0, 298.15);
    print_failure_analysis(analysis1);
    
    std::cout << "\n[Analysis 2] Yield condition\n";
    std::cout << "  Material: " << material.material << "\n";
    auto analysis2 = sim.analyze_failure(material, 300.0, 298.15);
    print_failure_analysis(analysis2);
    
    std::cout << "\n[Analysis 3] Tensile failure condition\n";
    std::cout << "  Material: " << material.material << "\n";
    auto analysis3 = sim.analyze_failure(material, 700.0, 298.15);
    print_failure_analysis(analysis3);
    
    std::cout << "\n[Analysis 4] High temperature (800K) effect\n";
    std::cout << "  Material: " << material.material << "\n";
    auto analysis4 = sim.analyze_failure(material, 300.0, 800.0);
    print_failure_analysis(analysis4);
}

// ============================================================================
// Demo 4: Matrix Operations (MATLAB-style)
// ============================================================================

void demo_matrix_operations() {
    print_header("Demo 4: Matrix Operations (MATLAB-style Linear Algebra)");
    
    std::cout << "\n[Operation 1] Matrix addition\n";
    Matrix A({{1.0, 2.0}, {3.0, 4.0}});
    Matrix B({{5.0, 6.0}, {7.0, 8.0}});
    Matrix C = A + B;
    std::cout << "  A + B:\n";
    std::cout << "    [" << C(0,0) << ", " << C(0,1) << "]\n";
    std::cout << "    [" << C(1,0) << ", " << C(1,1) << "]\n";
    
    std::cout << "\n[Operation 2] Matrix multiplication\n";
    Matrix D = A * B;
    std::cout << "  A * B:\n";
    std::cout << "    [" << D(0,0) << ", " << D(0,1) << "]\n";
    std::cout << "    [" << D(1,0) << ", " << D(1,1) << "]\n";
    
    std::cout << "\n[Operation 3] Transpose\n";
    Matrix E = A.transpose();
    std::cout << "  A':\n";
    std::cout << "    [" << E(0,0) << ", " << E(0,1) << "]\n";
    std::cout << "    [" << E(1,0) << ", " << E(1,1) << "]\n";
}

// ============================================================================
// Demo 5: Subsystem Interface (External Package Registration)
// ============================================================================

void demo_subsystem_interface() {
    print_header("Demo 5: Subsystem Interface (External Package Calls)");
    
    auto& interface = SubsystemInterface::instance();
    
    // Register materials subsystem
    interface.register_subsystem("materials", [](const std::string& input) -> std::string {
        MetallicSimulator sim;
        auto props = sim.get_material(input);
        
        std::ostringstream oss;
        oss << "Material: " << props.material << "\n";
        oss << "Tensile: " << props.tensile_strength_MPa << " MPa\n";
        oss << "Yield: " << props.yield_strength_MPa << " MPa\n";
        return oss.str();
    });
    
    // Register failure analysis subsystem
    interface.register_subsystem("failure", [](const std::string& input) -> std::string {
        // Parse input: "material,stress,temp"
        // Simplified: just use Hastelloy C-276 with input as stress
        MetallicSimulator sim;
        auto material = sim.get_material("Hastelloy C-276");
        double stress = std::stod(input);
        auto analysis = sim.analyze_failure(material, stress, 298.15);
        
        std::ostringstream oss;
        oss << "Safety Factor: " << analysis.safety_factor << "\n";
        oss << "Status: " << (analysis.will_fail ? "FAIL" : "SAFE") << "\n";
        return oss.str();
    });
    
    std::cout << "\n[Call 1] Query materials subsystem\n";
    std::cout << interface.call_subsystem("materials", "Hastelloy C-22");
    
    std::cout << "\n[Call 2] Query failure subsystem\n";
    std::cout << interface.call_subsystem("failure", "350.0");
    
    std::cout << "\n[Call 3] List registered subsystems\n";
    auto subsystems = interface.list_subsystems();
    std::cout << "  Registered: " << subsystems.size() << " subsystems\n";
    for (const auto& name : subsystems) {
        std::cout << "    - " << name << "\n";
    }
}

// ============================================================================
// Demo 6: Batch Processing (Deterministic Workflow)
// ============================================================================

void demo_batch_processing() {
    print_header("Demo 6: Batch Processing (Deterministic Material Screening)");
    
    MetallicSimulator sim;
    
    std::vector<std::string> candidates = {
        "Hastelloy B-2",
        "Hastelloy C-276",
        "Hastelloy C-4",
        "Hastelloy G-30"
    };
    
    double required_stress = 300.0;  // MPa
    double temp = 400.0;  // K
    
    std::cout << "\n[Batch Analysis] Screen materials for σ = " << required_stress 
              << " MPa at T = " << temp << " K\n\n";
    
    std::cout << "  " << std::setw(20) << std::left << "Material" 
              << " | " << std::setw(8) << "SF" 
              << " | " << std::setw(10) << "Status" 
              << " | " << "Recommendation\n";
    std::cout << "  " << std::string(70, '-') << "\n";
    
    for (const auto& name : candidates) {
        auto props = sim.get_material(name);
        auto analysis = sim.analyze_failure(props, required_stress, temp);
        
        std::string status = analysis.will_fail ? "❌ FAIL" : "✅ PASS";
        std::string recommendation;
        
        if (analysis.safety_factor >= 2.0) {
            recommendation = "Excellent choice";
        } else if (analysis.safety_factor >= 1.5) {
            recommendation = "Acceptable";
        } else if (analysis.safety_factor >= 1.0) {
            recommendation = "Marginal - review";
        } else {
            recommendation = "Reject";
        }
        
        std::cout << "  " << std::setw(20) << std::left << name 
                  << " | " << std::setw(8) << std::fixed << std::setprecision(2) 
                  << analysis.safety_factor 
                  << " | " << std::setw(10) << status 
                  << " | " << recommendation << "\n";
    }
}

// ============================================================================
// Main Demo Entry Point
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    std::cout << "█                                                              █\n";
    std::cout << "█   VSEPR-Sim Subsystem Integration Demo                      █\n";
    std::cout << "█   Deterministic Metallic Simulation Package                 █\n";
    std::cout << "█   MatLabForC++ Style External Package Integration           █\n";
    std::cout << "█                                                              █\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    
    try {
        demo_database_queries();
        demo_property_prediction();
        demo_failure_analysis();
        demo_matrix_operations();
        demo_subsystem_interface();
        demo_batch_processing();
        
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║ All Demos Completed Successfully! ✅                          ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
