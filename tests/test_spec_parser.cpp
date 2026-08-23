/**
 * @file test_spec_parser.cpp
 * @brief Test suite for DSL parser and spec system
 */

#include "vsepr/spec_parser.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace vsepr;

void test_single_molecule() {
    std::cout << "Test: Single molecule...\n";
    
    auto spec = parse_dsl("CH12CaO9");
    
    assert(spec.is_single_molecule());
    assert(spec.get_single().formula == "CH12CaO9");
    assert(spec.get_single().count == 1);
    assert(!spec.get_single().temperature.has_value());
    
    std::cout << "  ✓ Passed\n";
}

void test_temperature_modifier() {
    std::cout << "Test: Temperature modifier...\n";
    
    auto spec = parse_dsl("H2O --T=273.15");
    
    assert(spec.is_single_molecule());
    assert(spec.get_single().formula == "H2O");
    assert(spec.get_single().temperature.has_value());
    assert(std::abs(spec.get_single().temperature.value() - 273.15) < 0.01);
    
    std::cout << "  ✓ Passed\n";
}

void test_count_modifier() {
    std::cout << "Test: Count modifier...\n";
    
    auto spec = parse_dsl("H2O -n=5");
    
    assert(spec.is_single_molecule());
    assert(spec.get_single().formula == "H2O");
    assert(spec.get_single().count == 5);
    
    std::cout << "  ✓ Passed\n";
}

void test_position_random() {
    std::cout << "Test: Random position...\n";
    
    auto spec = parse_dsl("CO2 -pos{random}");
    
    assert(spec.is_single_molecule());
    assert(spec.get_single().position.has_value());
    assert(std::holds_alternative<RandomPosition>(spec.get_single().position.value()));
    
    std::cout << "  ✓ Passed\n";
}

void test_position_fixed() {
    std::cout << "Test: Fixed position...\n";
    
    auto spec = parse_dsl("H2O -pos{fixed:1.5,2.5,3.5}");
    
    assert(spec.is_single_molecule());
    assert(spec.get_single().position.has_value());
    
    const auto& pos = spec.get_single().position.value();
    assert(std::holds_alternative<FixedPosition>(pos));
    
    const auto& fp = std::get<FixedPosition>(pos);
    assert(std::abs(fp.x - 1.5) < 0.01);
    assert(std::abs(fp.y - 2.5) < 0.01);
    assert(std::abs(fp.z - 3.5) < 0.01);
    
    std::cout << "  ✓ Passed\n";
}

void test_position_seeded() {
    std::cout << "Test: Seeded position...\n";
    
    auto spec = parse_dsl("CH4 -pos{seeded:42:10,20,30}");
    
    assert(spec.is_single_molecule());
    assert(spec.get_single().position.has_value());
    
    const auto& pos = spec.get_single().position.value();
    assert(std::holds_alternative<SeededPosition>(pos));
    
    const auto& sp = std::get<SeededPosition>(pos);
    assert(sp.seed == 42);
    assert(std::abs(sp.box_x - 10.0) < 0.01);
    assert(std::abs(sp.box_y - 20.0) < 0.01);
    assert(std::abs(sp.box_z - 30.0) < 0.01);
    
    std::cout << "  ✓ Passed\n";
}

void test_simple_mixture() {
    std::cout << "Test: Simple mixture...\n";
    
    auto spec = parse_dsl("H2O, CO2");
    
    assert(!spec.is_single_molecule());
    assert(spec.mixture.components.size() == 2);
    assert(spec.mixture.components[0].formula == "H2O");
    assert(spec.mixture.components[1].formula == "CO2");
    assert(spec.mixture.percentages.empty());
    
    std::cout << "  ✓ Passed\n";
}

void test_mixture_with_percentages() {
    std::cout << "Test: Mixture with percentages...\n";
    
    auto spec = parse_dsl("H2O, CO2 -per{80,20}");
    
    assert(!spec.is_single_molecule());
    assert(spec.mixture.components.size() == 2);
    assert(spec.mixture.percentages.size() == 2);
    assert(std::abs(spec.mixture.percentages[0] - 80.0) < 0.01);
    assert(std::abs(spec.mixture.percentages[1] - 20.0) < 0.01);
    
    std::cout << "  ✓ Passed\n";
}

void test_complex_mixture() {
    std::cout << "Test: Complex mixture (your example)...\n";
    
    auto spec = parse_dsl("H2O, H2O --T=289, CO2 -pos{random} -per{80,16.7,3.3}");
    
    assert(!spec.is_single_molecule());
    assert(spec.mixture.components.size() == 3);
    
    // First component: H2O (no modifiers)
    assert(spec.mixture.components[0].formula == "H2O");
    assert(!spec.mixture.components[0].temperature.has_value());
    
    // Second component: H2O with temperature
    assert(spec.mixture.components[1].formula == "H2O");
    assert(spec.mixture.components[1].temperature.has_value());
    assert(std::abs(spec.mixture.components[1].temperature.value() - 289.0) < 0.01);
    
    // Third component: CO2 with random position
    assert(spec.mixture.components[2].formula == "CO2");
    assert(spec.mixture.components[2].position.has_value());
    
    // Percentages
    assert(spec.mixture.percentages.size() == 3);
    assert(std::abs(spec.mixture.percentages[0] - 80.0) < 0.01);
    assert(std::abs(spec.mixture.percentages[1] - 16.7) < 0.01);
    assert(std::abs(spec.mixture.percentages[2] - 3.3) < 0.01);
    
    std::cout << "  ✓ Passed\n";
}

void test_combined_modifiers() {
    std::cout << "Test: Combined modifiers...\n";
    
    auto spec = parse_dsl("H2O -n=100 --T=298 -pos{random}");
    
    assert(spec.is_single_molecule());
    const auto& mol = spec.get_single();
    
    assert(mol.formula == "H2O");
    assert(mol.count == 100);
    assert(mol.temperature.has_value());
    assert(std::abs(mol.temperature.value() - 298.0) < 0.01);
    assert(mol.position.has_value());
    
    std::cout << "  ✓ Passed\n";
}

void test_json_output() {
    std::cout << "Test: JSON output...\n";
    
    auto spec = parse_dsl("H2O --T=273");
    std::string json = to_json(spec);
    
    // Basic validation - check for expected strings
    assert(json.find("\"formula\"") != std::string::npos);
    assert(json.find("\"H2O\"") != std::string::npos);
    assert(json.find("\"T\"") != std::string::npos);
    assert(json.find("273") != std::string::npos);
    
    std::cout << "  JSON: " << json << "\n";
    std::cout << "  ✓ Passed\n";
}

void test_run_plan_expansion() {
    std::cout << "Test: Run plan expansion...\n";
    
    auto spec = parse_dsl("H2O, CO2 -per{80,20}");
    auto plan = expand_to_run_plan(spec, 100);
    
    assert(plan.size() == 2);
    assert(plan[0].formula == "H2O");
    assert(plan[0].count == 80);
    assert(plan[1].formula == "CO2");
    assert(plan[1].count == 20);
    
    std::cout << "  ✓ Passed\n";
}

void test_to_string() {
    std::cout << "Test: Pretty print...\n";
    
    auto spec = parse_dsl("H2O --T=273, CO2 -per{50,50}");
    std::string str = to_string(spec);
    
    std::cout << str << "\n";
    
    assert(str.find("H2O") != std::string::npos);
    assert(str.find("CO2") != std::string::npos);
    assert(str.find("273") != std::string::npos);
    assert(str.find("50%") != std::string::npos);
    
    std::cout << "  ✓ Passed\n";
}

int main() {
    std::cout << "=================================\n";
    std::cout << "DSL Parser Test Suite\n";
    std::cout << "=================================\n\n";
    
    try {
        test_single_molecule();
        test_temperature_modifier();
        test_count_modifier();
        test_position_random();
        test_position_fixed();
        test_position_seeded();
        test_simple_mixture();
        test_mixture_with_percentages();
        test_complex_mixture();
        test_combined_modifiers();
        test_json_output();
        test_run_plan_expansion();
        test_to_string();
        
        std::cout << "\n=================================\n";
        std::cout << "All tests passed! ✓\n";
        std::cout << "=================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed: " << e.what() << "\n";
        return 1;
    }
}
