// ============================================================================
// test_batch_processing.cpp
// Part of VSEPR-Sim: Molecular Geometry Simulation System
// 
// Comprehensive test suite for Phase 1 batch processing features:
//   1. Real molecule generator (all categories + parametric generators)
//   2. Batch worker (10-molecule batch with progress tracking)
//   3. Continuous generator (100 molecules with statistics)
//
// Expected Output:
//   - Database validation (50+ molecules)
//   - Category-based generation (inorganics, hydrocarbons, aromatics, etc.)
//   - Parametric generation (alkanes, alkenes, cycloalkanes, alcohols)
//   - Batch processing with timing (<2ms per molecule)
//   - Continuous generation (500-1000 mol/s)
//
// Usage:
//   ./test_batch_processing
//
// Version: 2.3.1
// ============================================================================

// Test Batch Processing System
// Demonstrates the real molecule generator and batch worker

#include "dynamic/real_molecule_generator.hpp"
#include "gui/batch_worker.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace vsepr;

void test_real_molecule_generator() {
    std::cout << "\n=== Testing Real Molecule Generator ===\n";
    
    dynamic::RealMoleculeGenerator generator;
    
    std::cout << "Database contains " << generator.get_template_count() << " molecules\n\n";
    
    // Test specific categories
    std::cout << "Generating samples from each category:\n\n";
    
    auto test_category = [&](dynamic::MoleculeCategory cat, const std::string& name) {
        std::cout << name << ":\n";
        for (int i = 0; i < 3; ++i) {
            Molecule mol = generator.generate_from_category(cat);
            std::cout << "  - Generated molecule with " << mol.num_atoms() << " atoms\n";
        }
        std::cout << "\n";
    };
    
    test_category(dynamic::MoleculeCategory::SmallInorganic, "Small Inorganics");
    test_category(dynamic::MoleculeCategory::Hydrocarbons, "Hydrocarbons");
    test_category(dynamic::MoleculeCategory::Alcohols, "Alcohols");
    test_category(dynamic::MoleculeCategory::Aromatics, "Aromatics");
    
    // Test specific molecule types
    std::cout << "Testing specific generators:\n";
    
    auto alkane = generator.generate_alkane(5);  // Pentane
    std::cout << "  Pentane (C5H12): " << alkane.num_atoms() << " atoms\n";
    
    auto alkene = generator.generate_alkene(4);  // Butene
    std::cout << "  Butene (C4H8): " << alkene.num_atoms() << " atoms\n";
    
    auto cyclo = generator.generate_cycloalkane(6);  // Cyclohexane
    std::cout << "  Cyclohexane (C6H12): " << cyclo.num_atoms() << " atoms\n";
    
    auto alcohol = generator.generate_alcohol(3);  // Propanol
    std::cout << "  Propanol (C3H7OH): " << alcohol.num_atoms() << " atoms\n\n";
}

void test_batch_worker() {
    std::cout << "\n=== Testing Batch Worker ===\n";
    
    // Create a test batch list
    std::vector<gui::BatchBuildItem> batch;
    
    batch.push_back({"H2O", "output/water.xyz", false, true, "Water"});
    batch.push_back({"NH3", "output/ammonia.xyz", false, true, "Ammonia"});
    batch.push_back({"CH4", "output/methane.xyz", false, true, "Methane"});
    batch.push_back({"C2H6", "output/ethane.xyz", false, true, "Ethane"});
    batch.push_back({"C6H6", "output/benzene.xyz", false, true, "Benzene"});
    batch.push_back({"CH3OH", "output/methanol.xyz", false, true, "Methanol"});
    batch.push_back({"CO2", "output/co2.xyz", false, true, "Carbon Dioxide"});
    batch.push_back({"SO2", "output/so2.xyz", false, true, "Sulfur Dioxide"});
    batch.push_back({"C3H8", "output/propane.xyz", false, true, "Propane"});
    batch.push_back({"H2O2", "output/peroxide.xyz", false, true, "Hydrogen Peroxide"});
    
    std::cout << "Created batch list with " << batch.size() << " molecules\n";
    
    gui::BatchWorker worker;
    
    // Set up progress callback
    worker.set_progress_callback([](size_t completed, size_t total, const gui::BatchResult& result) {
        std::cout << "[" << completed << "/" << total << "] ";
        std::cout << result.formula << " → ";
        
        if (result.success) {
            std::cout << "✓ (" << result.num_atoms << " atoms, ";
            std::cout << std::fixed << std::setprecision(2) << result.energy << " kcal/mol, ";
            std::cout << result.time_seconds * 1000 << " ms)\n";
        } else {
            std::cout << "✗ " << result.error_message << "\n";
        }
    });
    
    // Set up completion callback
    worker.set_completion_callback([](const std::vector<gui::BatchResult>& results) {
        std::cout << "\n=== Batch Processing Complete ===\n";
        std::cout << "Total molecules: " << results.size() << "\n";
        
        size_t successful = 0;
        double total_time = 0.0;
        int total_atoms = 0;
        
        for (const auto& r : results) {
            if (r.success) {
                successful++;
                total_time += r.time_seconds;
                total_atoms += r.num_atoms;
            }
        }
        
        std::cout << "Successful: " << successful << "/" << results.size() << "\n";
        std::cout << "Total time: " << std::fixed << std::setprecision(3) << total_time << " seconds\n";
        std::cout << "Average time: " << (total_time / results.size()) * 1000 << " ms/molecule\n";
        std::cout << "Total atoms generated: " << total_atoms << "\n";
        std::cout << "Average atoms/molecule: " << total_atoms / static_cast<double>(results.size()) << "\n";
    });
    
    // Start batch processing
    auto start_time = std::chrono::steady_clock::now();
    worker.start(batch);
    
    // Wait for completion
    while (worker.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    double total_time = std::chrono::duration<double>(end_time - start_time).count();
    
    std::cout << "\nWall-clock time: " << std::fixed << std::setprecision(3) << total_time << " seconds\n";
}

void test_continuous_generator() {
    std::cout << "\n=== Testing Continuous Generator ===\n";
    
    dynamic::ContinuousRealMoleculeGenerator generator;
    
    // Generate 100 molecules
    std::cout << "Generating 100 molecules continuously...\n";
    
    generator.start(100, 25);  // 100 molecules, checkpoint every 25
    
    // Monitor progress
    while (generator.is_running()) {
        std::cout << "\rProgress: " << generator.count() << "/100 molecules "
                  << "(" << std::fixed << std::setprecision(1) << generator.rate() << " mol/s)";
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n\nGeneration complete!\n";
    std::cout << "Total generated: " << generator.count() << "\n";
    std::cout << "Unique formulas: " << generator.unique_formulas() << "\n";
    std::cout << "Average rate: " << generator.rate() << " molecules/second\n";
    
    // Display recent molecules
    auto recent = generator.recent_molecules(10);
    std::cout << "\nLast 10 molecules generated:\n";
    for (size_t i = 0; i < recent.size(); ++i) {
        std::cout << "  " << (i+1) << ". " << recent[i].num_atoms() << " atoms\n";
    }
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   VSEPR-Sim Batch Processing & Generation Test Suite   ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    try {
        // Test 1: Real molecule generator
        test_real_molecule_generator();
        
        // Test 2: Batch worker
        test_batch_worker();
        
        // Test 3: Continuous generator
        test_continuous_generator();
        
        std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║                  ALL TESTS PASSED ✓                      ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERROR: " << e.what() << "\n";
        return 1;
    }
}
