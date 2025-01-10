/**
 * demo_temperature_heat_mapping.cpp
 * ----------------------------------
 * Demonstration of temperature-to-heat conversion (Item #7).
 * 
 * Shows how MD thermostat temperature maps to heat parameter h,
 * which controls which reaction templates are active.
 * 
 * Build:
 *   g++ -std=c++20 -I../atomistic -o demo_temperature_heat_mapping \
 *       demo_temperature_heat_mapping.cpp \
 *       ../atomistic/reaction/heat_gate.cpp
 * 
 * Run:
 *   ./demo_temperature_heat_mapping
 */

#include "atomistic/reaction/heat_gate.hpp"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace atomistic::reaction;

void print_header() {
    std::cout << "╔════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Temperature → Heat Parameter Mapping (Formation Engine v0.1)     ║\n";
    std::cout << "║  Implements: Section 8b, Item #7                                   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════╝\n\n";
}

void print_separator() {
    std::cout << "────────────────────────────────────────────────────────────────────\n";
}

void demo_basic_mapping() {
    std::cout << "🔹 Basic Mapping (default slope λ = 1.5)\n\n";
    
    std::vector<double> test_temps = {
        77.0,    // Liquid nitrogen
        100.0,   // Cryogenic
        167.0,   // Organic/bio threshold
        273.0,   // Ice point
        298.0,   // Room temperature
        373.0,   // Boiling point
        433.0,   // Bio/full threshold
        500.0,   // Elevated
        666.0,   // Saturation point
        800.0    // Beyond saturation
    };
    
    std::cout << std::setw(12) << "T (K)" 
              << std::setw(10) << "h" 
              << std::setw(20) << "Mode"
              << std::setw(30) << "Active Templates\n";
    print_separator();
    
    for (double T : test_temps) {
        uint16_t h = temperature_to_heat(T);
        
        std::string mode;
        std::string templates;
        if (h < 250) {
            mode = "Organic";
            templates = "Base only";
        } else if (h < 650) {
            mode = "Transitional";
            templates = "Base + partial bio";
        } else {
            mode = "Full Biochemical";
            templates = "Base + full bio";
        }
        
        std::cout << std::setw(12) << std::fixed << std::setprecision(1) << T
                  << std::setw(10) << h
                  << std::setw(20) << mode
                  << std::setw(30) << templates << "\n";
    }
    std::cout << "\n";
}

void demo_controller_integration() {
    std::cout << "🔹 HeatGateController Integration\n\n";
    
    double T_room = 298.0;  // Room temperature
    std::cout << "Creating controller at T = " << T_room << " K...\n";
    
    HeatGateController ctrl;
    ctrl.set_heat_from_temperature(T_room);
    
    std::cout << "  ✓ Heat parameter: " << ctrl.config().heat_3 << "\n";
    std::cout << "  ✓ Normalized: " << std::fixed << std::setprecision(3) 
              << ctrl.config().x_normalized << "\n";
    std::cout << "  ✓ Mode index: " << ctrl.mode_index() << "\n\n";
    
    std::cout << "Active bio templates:\n";
    std::cout << std::setw(25) << "Template" 
              << std::setw(15) << "Enable Weight"
              << std::setw(10) << "Active?\n";
    print_separator();
    
    for (int i = 0; i < static_cast<int>(BioTemplateId::COUNT); ++i) {
        auto id = static_cast<BioTemplateId>(i);
        double w = ctrl.enable_weight(id);
        bool active = ctrl.is_active(id);
        
        std::cout << std::setw(25) << bio_template_name(id)
                  << std::setw(15) << std::setprecision(3) << w
                  << std::setw(10) << (active ? "Yes" : "No") << "\n";
    }
    std::cout << "\n";
}

void demo_temperature_sweep() {
    std::cout << "🔹 Temperature Sweep: Mode Transitions\n\n";
    
    std::cout << "Sweeping T = 0 → 700 K in 50 K steps:\n\n";
    std::cout << std::setw(10) << "T (K)"
              << std::setw(8) << "h"
              << std::setw(12) << "m(h)"
              << std::setw(12) << "w_peptide"
              << std::setw(12) << "w_ester"
              << std::setw(12) << "w_disulf\n";
    print_separator();
    
    for (double T = 0; T <= 700; T += 50) {
        HeatGateController ctrl;
        ctrl.set_heat_from_temperature(T);
        
        double mode = ctrl.mode_index();
        double w_peptide = ctrl.enable_weight(BioTemplateId::PEPTIDE_BOND);
        double w_ester = ctrl.enable_weight(BioTemplateId::ESTER);
        double w_disulf = ctrl.enable_weight(BioTemplateId::DISULFIDE);
        
        std::cout << std::setw(10) << std::fixed << std::setprecision(1) << T
                  << std::setw(8) << ctrl.config().heat_3
                  << std::setw(12) << std::setprecision(3) << mode
                  << std::setw(12) << w_peptide
                  << std::setw(12) << w_ester
                  << std::setw(12) << w_disulf << "\n";
    }
    std::cout << "\n";
}

void demo_custom_slope() {
    std::cout << "🔹 Custom Slope: Tuning Temperature Mapping\n\n";
    
    double T_test = 300.0;
    std::vector<double> slopes = {0.5, 1.0, 1.5, 2.0, 3.0};
    
    std::cout << "At T = " << T_test << " K with different slopes:\n\n";
    std::cout << std::setw(12) << "Slope λ"
              << std::setw(10) << "h"
              << std::setw(20) << "Mode\n";
    print_separator();
    
    for (double slope : slopes) {
        uint16_t h = temperature_to_heat(T_test, slope);
        
        std::string mode;
        if (h < 250) mode = "Organic";
        else if (h < 650) mode = "Transitional";
        else mode = "Full Biochemical";
        
        std::cout << std::setw(12) << std::fixed << std::setprecision(1) << slope
                  << std::setw(10) << h
                  << std::setw(20) << mode << "\n";
    }
    std::cout << "\n";
    std::cout << "📝 Note: Higher slope → earlier transition to biochemical mode\n\n";
}

void demo_inverse_mapping() {
    std::cout << "🔹 Inverse Mapping: Heat → Temperature (for reporting)\n\n";
    
    std::vector<uint16_t> test_heats = {0, 250, 450, 650, 999};
    
    std::cout << std::setw(10) << "h"
              << std::setw(15) << "T (K)"
              << std::setw(20) << "Mode\n";
    print_separator();
    
    for (uint16_t h : test_heats) {
        double T = heat_to_temperature(h);
        
        std::string mode;
        if (h < 250) mode = "Organic";
        else if (h < 650) mode = "Transitional";
        else mode = "Full Biochemical";
        
        std::cout << std::setw(10) << h
                  << std::setw(15) << std::fixed << std::setprecision(1) << T
                  << std::setw(20) << mode << "\n";
    }
    std::cout << "\n";
}

int main() {
    print_header();
    
    demo_basic_mapping();
    print_separator();
    std::cout << "\n";
    
    demo_controller_integration();
    print_separator();
    std::cout << "\n";
    
    demo_temperature_sweep();
    print_separator();
    std::cout << "\n";
    
    demo_custom_slope();
    print_separator();
    std::cout << "\n";
    
    demo_inverse_mapping();
    print_separator();
    std::cout << "\n";
    
    std::cout << "✅ All demonstrations complete!\n";
    std::cout << "📖 See docs/section8b_heat_gated_reaction_control.tex for theory\n";
    std::cout << "🧪 Run tests/test_heat_gate.cpp for validation\n\n";
    
    return 0;
}
